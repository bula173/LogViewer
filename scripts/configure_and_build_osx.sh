#! /bin/bash

# Configure and build using CMake presets
cmake --preset macos-debug-qt
cmake --build --preset macos-debug-build-qt -j