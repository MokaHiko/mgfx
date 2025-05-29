cmake_minimum_required(VERSION 3.0...3.10)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(ex_cubes cubes.c)
target_link_libraries(ex_cubes PUBLIC ex_common)

find_program(SLANGC slangc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin)

set(SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/cubes.slang")
set(SPV_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/cubes.vert.spv"
    COMMAND ${SLANGC} ${SHADER_SOURCE} -entry vertexMain -target spirv -o "${SPV_OUTPUT_DIR}/cubes.vert.spv"
    DEPENDS "${SHADER_SOURCE}"
    COMMENT "Compiling vertex shader cubes.vert.spv"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/cubes.frag.spv"
    COMMAND ${SLANGC} ${SHADER_SOURCE} -entry fragmentMain -target spirv -o "${SPV_OUTPUT_DIR}/cubes.frag.spv"
    DEPENDS "${SHADER_SOURCE}"
    COMMENT "Compiling fragment shader cubes.frag.spv"
    VERBATIM
)

add_custom_target(ex_cubes_compile_shaders ALL
    DEPENDS
        "${SPV_OUTPUT_DIR}/cubes.vert.spv"
        "${SPV_OUTPUT_DIR}/cubes.frag.spv"
)
