This file was compiled into WASM using emscripten and then following command:

Note that the `-sINITIAL_MEMORY` is setting the memory size to be 10MB

`emcc allocate_memory.cc -sINITIAL_MEMORY=10mb -sEXPORTED_FUNCTIONS="['_Handler']" -Wl,--no-entry -o allocate_memory.wasm`
