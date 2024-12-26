#! /bin/bash

rm -rf ./build-debug
rm -rf ./build
rm -rf ./dist/Debug
cmake -DCMAKE_BUILD_TYPE=Debug -S. -Bbuild-debug
cmake --build build-debug -j