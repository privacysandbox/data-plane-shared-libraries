The WASM that's generated has a dependency on WASI proc_exit

This file was compiled into WASM using emscripten and then following command:

`emcc wasi_dependency.cc -Oz -DNDEBUG -sEXPORTED_FUNCTIONS="['_Handler']" -Wl,--no-entry -o wasi_dependency.wasm`
