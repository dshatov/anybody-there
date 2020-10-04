#!/bin/bash

cmake -Bcmake-build-native -DCMAKE_BUILD_TYPE=Release || exit 1
cmake --build cmake-build-native -j 4
