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

message(STATUS "Downloading glTF Sample Models...")
FetchContent_Declare(
    glTF_Sample_Models
    GIT_REPOSITORY "https://github.com/KhronosGroup/glTF-Sample-Models.git"
    GIT_TAG main
)
FetchContent_MakeAvailable(glTF_Sample_Models)

add_library(ex_common ex_common.c third_party/mikktspace/mikktspace.c)
target_link_libraries(ex_common PUBLIC cglm mgfx glfw)
target_include_directories(ex_common PRIVATE ./third_party/)
target_include_directories(ex_common PUBLIC ./)
target_compile_definitions(ex_common PUBLIC GLTF_MODELS_PATH="${CMAKE_BINARY_DIR}/_deps/gltf_sample_models-src/2.0/")

add_executable(clear_screen clear_screen/clear_screen.c)
target_link_libraries(clear_screen PRIVATE ex_common)

add_executable(cubes cubes/cubes.c)
target_link_libraries(cubes PRIVATE ex_common)

add_executable(text_rendering text_rendering/text_rendering.c)
target_link_libraries(text_rendering PRIVATE ex_common)

add_library(gltf_loader gltf_loading/gltf_loader.c)
target_link_libraries(gltf_loader PUBLIC ex_common)

add_executable(gltf_loading gltf_loading/gltf_loading.c)
target_link_libraries(gltf_loading PRIVATE gltf_loader)

add_executable(shadows shadows/shadows.c)
target_link_libraries(shadows PRIVATE gltf_loader)

set(SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/gradient.comp.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/unlit.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/unlit.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/text.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/text.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/sprites.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/sprites.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/lit.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/lit.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/blit.vert.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/gizmos.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/gizmos.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shadows.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shadows_lit.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shadows_lit.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/pbr.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/skybox.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/skybox.frag.glsl
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

# Copy fonts directory
add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/assets/fonts.copy"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/assets/fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts"
            "${CMAKE_CURRENT_BINARY_DIR}/assets/fonts"
    COMMAND ${CMAKE_COMMAND} -E touch "${CMAKE_CURRENT_BINARY_DIR}/assets/fonts.copy"
    COMMENT "Copying fonts to binary assets directory"
)

# Optional: add a custom target to depend on this
add_custom_target(copy_fonts ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/assets/fonts.copy"
)
