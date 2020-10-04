#!/bin/bash

source ../emsdk/emsdk_env.sh || exit 1
emcmake cmake -DCMAKE_BUILD_TYPE=Release -Bcmake-build-wasm -DPLATFORM=Web -DWASM=1 || exit 1
cmake -DCMAKE_BUILD_TYPE=Release --build cmake-build-wasm -j 4
