This file was compiled into WASM using emscripten and then following command:

`emcc list_of_string_in_list_of_string_out.cc -Oz -DNDEBUG -sEXPORTED_FUNCTIONS="['_Handler']" -Wl,--no-entry -o list_of_string_in_list_of_string_out.wasm`
