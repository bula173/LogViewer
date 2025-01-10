#! /bin/bash

rm -rf ./dist/Debug
rm -rf ./build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -S. -Bbuild-debug
