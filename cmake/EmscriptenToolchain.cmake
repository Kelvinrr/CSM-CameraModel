# Emscripten toolchain configuration for WebAssembly builds
# Usage: emcmake cmake -DCMAKE_TOOLCHAIN_FILE=cmake/EmscriptenToolchain.cmake ..

set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_VERSION 1)

set(CMAKE_C_COMPILER "emcc")
set(CMAKE_CXX_COMPILER "em++")
set(CMAKE_AR "emar")
set(CMAKE_RANLIB "emranlib")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Disable threading by default for WASM
set(THREADS_PREFER_PTHREAD_FLAG OFF)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
