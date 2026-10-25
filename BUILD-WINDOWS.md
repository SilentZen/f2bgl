# Windows 11 build

Use the MSYS2 UCRT64 environment. This matches the POSIX filesystem and timing APIs used by the original engine while producing a native Windows executable.

Install MSYS2, open the **UCRT64** terminal, then install the compiler and libraries:

```sh
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-SDL3 mingw-w64-ucrt-x86_64-zlib mingw-w64-ucrt-x86_64-fluidsynth
```

WildMIDI is not available as a UCRT64 package. Build/install its Windows development library separately from the [official WildMIDI releases](https://github.com/Mindwerks/wildmidi/releases), then ensure its headers and library are discoverable by CMake.

From the repository directory in that terminal, configure and build:

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

Run `build/f2bgl.exe` from the UCRT64 terminal and from a directory containing the original PC game data, following the directory structure in `README.txt`. The terminal provides the runtime DLL paths for the linked libraries.

## Portable dependencies already unpacked in this checkout

- CMake 4.4.4: `deps/cmake/cmake-4.4.4-windows-x86_64/bin/cmake.exe`
- SDL 3.4.18 MinGW x64 development package, including headers, import library, and `SDL3.dll`: `deps/SDL3-3.4.18/x86_64-w64-mingw32/`

These files are local to this checkout and do not modify the system-wide MSYS2 installation. The normal MSYS2 instructions above still install the full compiler and audio dependencies needed to build this game.