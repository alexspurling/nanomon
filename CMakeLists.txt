cmake_minimum_required(VERSION 4.2)
project(nanomon)

set(CMAKE_CXX_STANDARD 20)

# Disable extras we don't need
set(NANOGUI_BUILD_EXAMPLE OFF CACHE BOOL " " FORCE)
set(NANOGUI_BUILD_PYTHON  OFF CACHE BOOL " " FORCE)
set(NANOGUI_INSTALL       OFF CACHE BOOL " " FORCE)

# Pull in NanoGUI
add_subdirectory(ext/nanogui)

# For reliable parallel builds, mark NanoGUI targets as dependencies
set_property(TARGET glfw nanogui PROPERTY FOLDER "dependencies")

add_executable(nanomon src/main.cpp)

# Link against nanogui (this also pulls in GLFW, OpenGL, etc.)
target_link_libraries(nanomon PRIVATE nanogui ${NANOGUI_EXTRA_LIBS})

# Include dirs provided by NanoGUI's build system
target_include_directories(nanomon PRIVATE
        ext/nanogui/include
        ${NANOGUI_EXTRA_INCS}
)