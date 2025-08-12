# LogViewer

A wxWidgets-based log viewer with virtual list controls and configurable row coloring.

- GUI: wxDataViewCtrl (EventsVirtualListControl, ItemVirtualListControl)
- Model: db::EventsContainer (adapter: gui::EventsContainerAdapter)
- Parsing: parser::XmlParser (std::filesystem::path-based)
- Errors: error::Error (derives from std::runtime_error; shows wxMessageBox; logs via spdlog)

Build:
- macOS (clang), Windows (MSVC), Linux (GCC/Clang)
- Docs: `cmake --build . --target doc` (if Doxygen found)

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

## MSYS config flags
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-toolchain mingw-w64-x86_64-clang

clang --version
          cmake --version
          mkdir build_cmake
          pushd build_cmake
          cmake -G "MinGW Makefiles" \
                -DCMAKE_C_COMPILER=clang.exe \
                -DCMAKE_CXX_COMPILER=clang++.exe \
                -DCMAKE_CXX_FLAGS=-Werror \
                -DCMAKE_BUILD_TYPE=Release \
                -DwxBUILD_SAMPLES=ALL \
                -DwxBUILD_TESTS=ALL \
                -DwxBUILD_DEMOS=ON \
                -DwxUSE_WEBVIEW_EDGE=ON \
                ..
cmake --build . -- -j4
