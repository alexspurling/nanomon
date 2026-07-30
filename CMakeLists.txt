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

add_executable(nanomon
        src/main.cpp
        src/CpuTimesSampler.cpp
        src/CpuDataSource.cpp
        src/Graph.cpp
        src/ViewWindow.cpp
        src/VertexGenerator.cpp
        src/MemoryDataSource.cpp
        src/DiskDataSource.cpp
        src/StatsWidget.cpp
        src/ViewWindowRecorder.cpp
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

# Replays a recording written by ViewWindow's bounds trip-wire. Needs no GUI, so
# it builds against ViewWindow alone.
add_executable(viewwindow_replay
        src/replay_main.cpp
        src/ViewWindow.cpp
        src/ViewWindowRecorder.cpp
)
target_compile_options(viewwindow_replay PRIVATE -fsanitize=undefined)
target_link_options(viewwindow_replay PRIVATE -fsanitize=undefined)

target_include_directories(viewwindow_replay PRIVATE src)

# ---- tests ----
enable_testing()

# Self-contained property test for the bounds invariant. No GUI, no fixtures.
add_executable(viewwindow_invariant_test
        test/viewwindow_invariant_test.cpp
        src/ViewWindow.cpp
        src/ViewWindowRecorder.cpp
)
target_include_directories(viewwindow_invariant_test PRIVATE src)
target_compile_options(viewwindow_invariant_test PRIVATE -fsanitize=undefined)
target_link_options(viewwindow_invariant_test PRIVATE -fsanitize=undefined)
add_test(NAME viewwindow_invariant COMMAND viewwindow_invariant_test)

# Turns a recorded session into a regression test: drop a recording written by
# the app's bounds trip-wire into test/fixtures/ and it must replay without ever
# running the window past the newest sample. Each fixture is one test case.
file(GLOB VIEWWINDOW_RECORDINGS ${CMAKE_SOURCE_DIR}/test/fixtures/*.txt)
foreach(recording ${VIEWWINDOW_RECORDINGS})
    get_filename_component(recording_name ${recording} NAME_WE)
    add_test(NAME replay_${recording_name}
             COMMAND viewwindow_replay ${recording})
endforeach()