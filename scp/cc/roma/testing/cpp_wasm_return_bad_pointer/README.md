This file was compiled into WASM using emscripten and then following command:

Note that the `-sINITIAL_MEMORY` is setting the memory size to be 10MB

`emcc return_bad_pointer.cc -sINITIAL_MEMORY=10mb -sEXPORTED_FUNCTIONS="['_Handler']" -Wl,--no-entry -o return_bad_pointer.wasm`
