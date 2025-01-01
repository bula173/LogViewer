# LogViewer

Simple tool to parse XML logs

## How this works

This template searches for the wxWidgets library using `FindPackage`. If not found, it downloads the library source from GitHub, compiles it, and links it with the main project. 

The super build pattern with `ExternalProject_Add` is used to achieve this.

## Requirements

This works on Windows, Mac, and Linux. You'll need `cmake` and a C++ compiler (tested on `clang`, `gcc`, and MSVC).

Linux builds require the GTK3 library and headers installed in the system.

## Building

To build the project, use:

```bash
cmake -S. -Bbuild-rel -DCMAKE_BUILD_TYPE=Release 
cmake --build build-rel -j
```

To build debug version, use:
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -S. -Bbuild-debug 
cmake --build build-debug -j
```

This will create a directory named either `build-debug` or `build-rel` and create all build artifacts there. The main executable can be found in the `dist` folder.


