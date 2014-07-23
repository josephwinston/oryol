//------------------------------------------------------------------------------
//  Keyboard.cc
//------------------------------------------------------------------------------
#include "Pre.h"
#include "Keyboard.h"
#include "Core/Memory/Memory.h"

namespace Oryol {
namespace Input {

using namespace Core;
    
//------------------------------------------------------------------------------
Keyboard::Keyboard() :
charIndex(0),
attached(false) {
    Memory::Clear(&this->chars, sizeof(this->chars));
}

//------------------------------------------------------------------------------
void
Keyboard::setAttached(bool b) {
    this->attached = b;
}

//------------------------------------------------------------------------------
void
Keyboard::onKeyDown(Key::Code key) {
    o_assert_range_dbg(key, Key::NumKeys);
    this->down[key] = true;
    this->pressed[key] = true;
}

//------------------------------------------------------------------------------
void
Keyboard::onKeyUp(Key::Code key) {
    o_assert_range_dbg(key, Key::NumKeys);
    this->up[key] = true;
    this->pressed[key] = false;
}

//------------------------------------------------------------------------------
void
Keyboard::onChar(wchar_t c) {
    if (this->charIndex < MaxNumChars) {
        this->chars[charIndex++] = c;
        this->chars[charIndex] = 0;
    }
}

//------------------------------------------------------------------------------
void
Keyboard::reset() {
    this->charIndex = 0;
    this->chars[0] = 0;
    this->down.reset();
    this->up.reset();
    this->pressed.reset();
}

} // namespace Input
} // namespace Oryol