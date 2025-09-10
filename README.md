# WebGL Cube in Pure C → WebAssembly (No Emscripten)

This project demonstrates how to render a spinning 3D cube in the browser using **WebGL2**, with the logic written in **pure C** and compiled directly to **WebAssembly**.

- No Emscripten or heavyweight runtime.
- No libc, no WASI.
- Just `clang` → `.wasm` + a minimal JavaScript shim to call WebGL.

## Features

- Pure C code for math, vertex/index data, and render loop.
- WebGL2 bindings exposed manually via a thin JS shim.
- Column-major matrix math consistent with GLSL uniforms.
- Demonstrates vertex attributes, element indices, uniform matrices, and color interpolation.

## Files

- **index.html** — minimal HTML page with `<canvas>`
- **main.js** — JavaScript shim exposing WebGL2 calls to WASM
- **main.c** — C program that compiles to WebAssembly and issues GL-style calls
- **main.wasm** — compiled output (not tracked in repo)

## Build

Requires a recent `clang` with `wasm32` target.

```bash
clang --target=wasm32 -O3 -nostdlib \
  -Wl,--no-entry \
  -Wl,--export=init -Wl,--export=frame -Wl,--export=set_viewport \
  -Wl,--export-memory \
  -Wl,--initial-memory=1048576 -Wl,--max-memory=16777216 \
  -Wl,--export-table \
  -Wl,--allow-undefined \
  -o main.wasm main.c
