oryol_begin_app(DDSTextureLoading windowed)
    oryol_sources(.)
    oryol_deps(Render)
    oryol_add_web_sample(DDSTextureLoading "Load and render various DDS texture formats" emscripten)    
    oryol_add_web_sample(DDSTextureLoading "Load and render various DDS texture formats" pnacl)
oryol_end_app()
