#!/bin/bash

source ../emsdk/emsdk_env.sh || exit 1
emcmake cmake -Bcmake-build-wasm -DPLATFORM=Web -DWASM=1 || exit 1
cmake --build cmake-build-wasm -j 4
