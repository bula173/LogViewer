#! /bin/bash

cmake -DCMAKE_BUILD_TYPE=Debug -S. -Bbuild-debug 
cmake --build build-debug -j