#!/bin/bash

source ../emsdk/emsdk_env.sh || exit 1
emcmake cmake -Bcmake-build-asmjs -DPLATFORM=Web || exit 1
cmake --build cmake-build-asmjs -j 4
