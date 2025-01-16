#! /bin/bash

clang --version
cmake --version
mkdir build_cmake
pushd build_cmake
cmake -G "MinGW Makefiles" \
    -DCMAKE_C_COMPILER=clang.exe \
    -DCMAKE_CXX_COMPILER=clang++.exe \
    -DCMAKE_BUILD_TYPE=Release \
    ..
cmake --build . -- -j4