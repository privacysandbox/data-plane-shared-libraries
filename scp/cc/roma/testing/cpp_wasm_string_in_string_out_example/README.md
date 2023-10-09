This file was compiled into WASM using emscripten and then following command:

`emcc string_in_string_out.cc -Oz -DNDEBUG -sEXPORTED_FUNCTIONS="['_Handler']" -Wl,--no-entry -o string_in_string_out.wasm`
