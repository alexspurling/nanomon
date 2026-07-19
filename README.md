# nanomon

Lightweight but powerful linux system usage monitor written in C++ using the
[nanogui](https://github.com/mitsuba-renderer/nanogui.git) framework. Shows CPU usage graphs with breakdown by process
as well as memory, disk and network usage graphs.

## Build

Requirements:
- **CMake** ≥ 3.16
- **C++20** compiler (GCC, Clang)
- **X11 / Wayland** development headers (for GLFW)

On Debian/Ubuntu:

    sudo apt install cmake g++ libwayland-dev libxkbcommon-dev xorg-dev

## Build

    git clone --recursive https://github.com/user/nanomon.git
    cd nanomon
    cmake -B build
    cmake --build build

## Run

    ./build/nanomon
