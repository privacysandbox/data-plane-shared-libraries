Install rust: `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh`

Install wasm-gc: `cargo install wasm-gc`

Add WASM: `rustup target add wasm32-unknown-unknown`

Compile and optimize: `./build.sh`
