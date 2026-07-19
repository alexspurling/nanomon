cmake_minimum_required(VERSION 3.13..3.18)
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

function(embed_shader TARGET SHADER_FILE)
    get_filename_component(VAR_NAME ${SHADER_FILE} NAME_WE)
    set(OUT_FILE ${CMAKE_BINARY_DIR}/generated/${VAR_NAME}_shader.h)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)

    # Generate immediately at configure time (so IDEs can find the header)
    execute_process(
            COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE=${CMAKE_SOURCE_DIR}/${SHADER_FILE}
            -DOUTPUT_FILE=${OUT_FILE}
            -DVAR_NAME=${VAR_NAME}
            -P ${CMAKE_SOURCE_DIR}/cmake/EmbedShader.cmake
    )

    # Also regenerate automatically when the shader file changes
    add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE=${CMAKE_SOURCE_DIR}/${SHADER_FILE}
            -DOUTPUT_FILE=${OUT_FILE}
            -DVAR_NAME=${VAR_NAME}
            -P ${CMAKE_SOURCE_DIR}/cmake/EmbedShader.cmake
            DEPENDS ${CMAKE_SOURCE_DIR}/${SHADER_FILE}
            COMMENT "Embedding shader ${SHADER_FILE}"
    )

    target_sources(${TARGET} PRIVATE ${OUT_FILE})
    target_include_directories(${TARGET} PRIVATE ${CMAKE_BINARY_DIR}/generated)
endfunction()

add_executable(nanomon src/main.cpp
        src/CpuTimesSampler.cpp
        src/CpuHistory.cpp
        src/CpuGraph.cpp
        src/LineGraph.cpp
)

embed_shader(nanomon shaders/simple_vert.glsl simple_vert)
embed_shader(nanomon shaders/simple_frag.glsl simple_frag)

# Link against nanogui (this also pulls in GLFW, OpenGL, etc.)
target_link_libraries(nanomon PRIVATE nanogui ${NANOGUI_EXTRA_LIBS})

# Include dirs provided by NanoGUI's build system
target_include_directories(nanomon PRIVATE
        ext/nanogui/include
        ${NANOGUI_EXTRA_INCS}
)

target_compile_options(nanomon PRIVATE -fsanitize=undefined)
target_link_options(nanomon PRIVATE -fsanitize=undefined)