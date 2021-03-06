# The Oryol Messaging Module

Disclaimer: the Messaging module is work-in-progress and stuff will change.

## Overview

The Messaging module implements a simple message-passing framework for the higher-level modules of Oryol. It
is very flexible, easy to use and reasonably fast, but it may be too heavy-weight for some scenarios. Specifically,
if the number of messages per frame ~~is more then a few hundred~~ (update: it looks like the messaging framework won't be the bottleneck, it can currently create, forward into a thread, and call a simple handler function at a rate of about 10 million per second, without any performance optimizations).

Good use cases for the messaging frame-work are:

- handling asynchronous IO requests
- a multiplayer networking system
- simple HTTP services (responding to JSON-RPC calls, etc...)

I wouldn't recommend the messaging framework for offloading heavy compute tasks to threads if this means
a lot of messages need to be passed between the threads.

## Concepts

The Messaging module is based on 3 simple concepts:

- **Message**: an actual message object
- **Protocol**: a group of related messages
- **Port**: message sender/receiver/handler/forwarder objects


### Messages

A Message is a simple, ref-counted C++ object used to hold the message parameters, and to communicate the 
current state of the message. Message classes are usually not written by hand, but their C++ source is 
generated from XML files.

Message classes have the following attributes:

- a numeric message id (auto-generated), message ids are only unique within their protocol
- a protocol id (this is a FourCC uint32)
- a current state (Initial, Pending, Handled)
- a set of member variables with their respective setter/getter methods

Messages are generally created through pool-allocators. When a message is created, a pointer is popped from
a lock-free forward-linked list, and placement-new is called on the memory block (so, no expensive "dynamic 
allocation" happens, but a constructor is called). Messages are ref-counted. When a message is destroyed, the
destructor is called on the object, and the memory block is put back into the pool-allocator's free-list.

Message classes can be derived from other message classes, the parent message class can be either
from the same Protocol, or from the parent Protocol (see below in the Protocol section).

Messages can serialize/deserialize themselves to and from plain-old-data (POD) representation. Serialization
to POD is only used when the message needs to cross process boundaries, otherwise a (smart-)pointer to the
message is passed around.

Care has been taken that message-pointers are either passed by reference or moved, so that no
unnecessary copying or ref-count-bumping happens.

The disadvantage with passing messages around by pointers is that - even though they are pooled - once
the content of the message needs to be accessed, a cache miss is likely, since different message objects
can be far away from each other in memory. This is why a more low-level system which works with 
plain memory-buffers for passing data back and forth may be more efficient in some use cases.

### Protocols

A Protocol is a group of related messages. Technically it is a C++ class with a bunch of static methods,
and more embedded classes. There are no Protocol objects.

The following classes are embedded in the Protocol class:

- a MessageId class, this holds an enum of all MessageIds in the protocol
- a Factory class, this can create a Message object from its MessageId
- the actual Message classes

Protocols can be derived from one another. The first MessageId of a derived protocol will be the last 
MessageId of the parent protocol + 1, so that MessageIds are unique within a Protocol's ancestor chain. This
makes it possible to build jump-tables as simple linear arrays with the MessageId as index.

The C++ source code for protocols is usually not written by hand, but generated from XML files. The 
conversion from XML source code to C++ happens transparently during the build process (you 
can simply edit an XML file in your favourite IDE, hit the "Build" button, and the right thing will happen).

The magic behind the source code generation process is handled by python scripts called *generators*
(see the *oryol/generators* directory, and the integration into the build process is handled through
the cmake-scripts under *oryol/cmake*)

Here's a simple message protocol XML file (NOTE: syntax details are very likely to change):

    <!-- 
      - type: conversion happens through the MessageProtocol.py python script
      - ns: the C++ namespace of the protocol
      - name: the C++ class name of the protocol
      - id: the protocolId (FourCC code)
    -->
    <Generator type="MessageProtocol" ns="Messaging" name="TestProtocol" id="TSTP" >
        
        <!-- the required C++ headers for non-trivial types -->
        <Header path="Core/String/StringAtom.h"/>
        
        <!-- 
          A simple message with one 32bit-integer, and one 32-bit-float value.
          - the C++ class name is "TestMsg" and has the following setters/getters:
            - void SetHitpoints(int32 val)
            - int32 GetHitpoints() const
            - void SetDuration(float32 val)
            - float32 GetDuration() const
          - the Hitpoints value will be set to 10 in the constructor
          - the Duration value will be set to 0.0 (0 is the default for all
            simple numeric types
        -->
        <Message name="TestMsg">
            <Attr name="Hitpoints" type="int32" def="10" />
            <Attr name="Duration" type="float32" />
        </Message>
        
        <!-- 
          A derived message with TestMsg as parent class, this only
          adds a StringAtom attribute 'Name'. To set the default value,
          we need to use XML's escape mechanism (&quot; is ")
        -->
        <Message name="DerivedMsg" parent="TestMsg">
            <Attr name="Name" type="Core::StringAtom" def="&quot;Test&quot;"/>
        </Message>

    </Generator>

### Ports

A Port is a generic base class which accepts a Message in its Put() method and "does something" with
the message. What happens to the message is implemented in subclasses of Port, and this is where it 
gets interesting:

There are Port subclasses which broadcast a message to other ports (**Broadcaster**),
invoke a handler method when a specific message arrives (**Dispatcher**), queue messages, and only forward
them to another port when a special ForwardMessages() method is called (**AsyncQueue**), forward messages to
another Port running in a worker thread (**ThreadedQueue**), forward messages to different ports based
on round-robin scheme (**RoundRobinForwarder**), and so on...

Ports are basically simple building blocks which make it easy to construct different message-passing and
-processing scenarios, and they are meant to be subclassed for new scenarios (such as message
transfer over a network connection).

Ports have a **DoWork()** which is used in some port types to trigger per-frame work. Only
"front-end" ports are usually connected to the thread's main RunLoop, the DoWork call
will be forwarded to connected ports by the front-end port. This makes sure that the cascade
of the DoWork() calls happens in the right order.

Here's an extremely simple code-sample to delegate message processing to another thread, for this
we need a ThreadedQueue-port and a Dispatcher-port which runs in the thread created by the
ThreadedQueue:

    // this is a message handler function which is invoked by the Dispatcher 
    void HandleTestMsg(const Ptr<TestMsg>& msg) {
      Log::Info("TestMsg received: Hitpoints=%d, Duration=%f\n", msg->GetHitpoints() msg->GetDuration());
      msg->SetHandled();
    }

    ...
    {
      // first setup the dispatcher which runs in the work-thread
      // and handles the 'TestMsg' message of protocol 'TestProtocol'
      Ptr<Dispatcher<TestProtocol>> dispatcher = Dispatcher<TestProtocol>::Create("dispatcher");
      
      // subscribe the HandleTestMsg function to the TestMsg message:
      dispatcher->Subscribe<TestMsg>(&HandleTestMsg);

      // create the threaded-message-queue port, set the Dispatcher
      // as forwarding port (which runs in the thread)
      Ptr<ThreadedQueue> threadedQueue = ThreadedQueue::Create("thread", dispatcher);
      
      // create and send a few messages to the to the thread, these will be
      // queued on the main-threaf side until DoWork() is called (which normally happens automatically
      // as part of the run-loop)
      Ptr<TestMsg> msg0 = TestMsg::Create();
      msg0->SetHitpoints(20);
      msg0->SetDuration(100.0f);
      threadedQueue->Put(msg0);
      
      Ptr<TestMsg> msg1 = TestMsg::Create();
      msg0->SetHitpoints(100);
      msg0->SetDuration(10.0f);
      threadedQueue->Put(msg1);
      
      // So far nothing has happened, since the messages have been queued up,
      // and the worker thread is sleeping... calling DoWork will internally
      // move the messages queued up in the main thread to the worker thread's queue
      // and wake up the worker thread. DoWork() is normally called once per frame
      // as part of the RunLoop. This has the advantage, that locking only happens
      // once per frame for a very short amount of time, instead of once for
      // each message, but the disadvantage is, that message handling will be delayed
      // by one frame.
      threadedQueue->DoWork();
      
      // The work thread should now wake up, and the HandleTestMsg function
      // should be called exactly twice. Once the work thread has handled the
      // messages, the message state should switch to Handled, which can check
      // here on the main-thread.

### More on Dispatchers

A Dispatcher is a Port subclass which calls a handler function when a specific message is received.
One Dispatcher object can only handle Messages from a single Protocol, and only one handler function
can be associated with one MessageId (this is because MessageIds are only unique within one Protocol,
and they are used to index into a jump-table).

The Protocol must be provided as a template argument on creation:

    Ptr<Dispatcher<MyProtocol>> dispatcher = Dispatcher<MyProtocol>::Create("MyDispatcher");

The main reason why the protocol is a template parameter is that the Dispatcher object directly
embeds a jumptable with one entry for each MessageId of the protocol.

Once the Dispatcher object is created, message handler functions can be attached. The simple case
is to attach a global function or static class method:

    // the handler function's signature must match the expected message class:
    void HandlerFunc(const Ptr<TestMsg>& msg) {
      //... do something, and set the message to handled
      msg->SetHandled();
    }

    ...
    // subscribe HandlerFunc() to TestMsg:
    dispatcher->Subscribe<TestMsg>(&HandlerFunc);
    ...

From now on, when a message object of class TestMsg is passed to the Dispatcher's Put() method, the
function HandlerFunc() will be called.

The parameter of the Subscribe() method is a std::function object. This means we can also pass a
"real" method call to an object, although the syntax looks a bit arcane:

    // declare a simple class with a handler-method:
    class HandlerClass {
    public:
      void Handle(const Ptr<TestMsg>& msg) {
        this->value += msg->GetHitpoints();
      }
      int32 value = 0;
    };

    ...
    // create an object of class HandlerClass, and subscribe its Handle() method 
    // to message class TestMsg:
    HandlerClass obj;
    using namespace std::placeholders;
    dispatcher->Subscribe<TestMsg>(std::bind(&HandlerClass::Handle, &handlerObj, _1));
    ...
