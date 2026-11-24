#!/bin/sh
# Compila el núcleo (core/) a WebAssembly para el simulador.
# Requiere clang con soporte wasm32 (viene en clang >= 8) y wasm-ld.
set -e
cd "$(dirname "$0")"
clang++ --target=wasm32 -O2 -nostdlib -fno-exceptions -fno-rtti -ffreestanding \
  -std=c++17 -Wall -Wextra -Wno-unused-parameter \
  -Wl,--no-entry -Wl,--allow-undefined -Wl,--export-memory \
  -o simulador/core.wasm core/*.cpp core/ti/*.cpp
echo "OK -> simulador/core.wasm ($(wc -c < simulador/core.wasm) bytes)"
