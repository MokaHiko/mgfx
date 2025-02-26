cmake_minimum_required(VERSION 3.0.0...3.10)
project(mgfx_examples VERSION 1.0 LANGUAGES C)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
    cglm
    GIT_REPOSITORY https://github.com/recp/cglm.git
    GIT_TAG        v0.9.6
)
FetchContent_MakeAvailable(cglm)

add_library(ex_common ex_common.c)
target_link_libraries(ex_common PUBLIC mgfx)
target_link_libraries(ex_common PRIVATE glfw)
target_include_directories(ex_common PUBLIC ./)

add_executable(clear_screen clear_screen/clear_screen.c)
target_link_libraries(clear_screen PRIVATE ex_common)

add_executable(cubes cubes/cubes.c)
target_link_libraries(cubes PRIVATE ex_common cglm)

set(SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/triangle.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/triangle.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/gradient.comp.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/unlit.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/unlit.frag.glsl
)

find_program(GLSLANG_VALIDATOR glslangValidator)
foreach(SHADER ${SHADERS})
    # Get the file name without extension
    get_filename_component(FILE_NAME ${SHADER} NAME)

    # Specify output file path
    set(SPIRV "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders/${FILE_NAME}.spv")

    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders"
        COMMAND ${GLSLANG_VALIDATOR} -V ${SHADER} -o ${SPIRV}
        DEPENDS ${SHADER}
        COMMENT "Compiling ${SHADER} to SPIR-V"
        VERBATIM
    )

    # Collect SPIR-V files into a list for dependencies
    list(APPEND SPIRV_OUTPUTS ${SPIRV})
endforeach()

add_custom_target(compile_shaders ALL DEPENDS ${SPIRV_OUTPUTS})
