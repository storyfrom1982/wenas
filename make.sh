#!/bin/sh

rm -rf build
cmake -B build -D CMAKE_BUILD_TYPE=$1 && cmake --build build
# ./build/kang
# ./build/server
./build/tree