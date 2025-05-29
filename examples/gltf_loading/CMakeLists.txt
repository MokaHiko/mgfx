cmake_minimum_required(VERSION 3.0...3.10)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(gltf_loader gltf_loader.c ../third_party/mikktspace/mikktspace.c)

target_link_libraries(gltf_loader PRIVATE mgfx)
target_include_directories(gltf_loader PUBLIC ./ ../third_party/)

add_executable(ex_gltf_loading gltf_loading.c)
target_link_libraries(ex_gltf_loading PRIVATE ex_common gltf_loader)

find_program(SLANGC slangc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin)

set(SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/lit.slang")
set(SPV_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/lit.vert.spv"
    COMMAND ${SLANGC} ${SHADER_SOURCE} -entry vertexMain -target spirv -o "${SPV_OUTPUT_DIR}/lit.vert.spv"
    DEPENDS "${SHADER_SOURCE}"
    COMMENT "Compiling vertex shader lit.vert.spv"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/lit.frag.spv"
    COMMAND ${SLANGC} ${SHADER_SOURCE} -entry fragmentMain -target spirv -o "${SPV_OUTPUT_DIR}/lit.frag.spv"
    DEPENDS "${SHADER_SOURCE}"
    COMMENT "Compiling fragment shader lit.frag.spv"
    VERBATIM
)

add_custom_target(ex_gltf_loading_compile_shaders ALL DEPENDS
        "${SPV_OUTPUT_DIR}/lit.vert.spv"
        "${SPV_OUTPUT_DIR}/lit.frag.spv"
)
