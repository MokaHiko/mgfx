cmake_minimum_required(VERSION 3.0...3.10)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(mxgui main.c mxgui.c asset.c ecs.c mx_material.c renderer.c)
target_link_libraries(mxgui PRIVATE ex_common gltf_loader)

find_program(SLANGC slangc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin)

set(SPV_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}")

set(SHADOW_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/shadow.slang")
add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/shadow.vert.spv"
    COMMAND ${SLANGC} ${SHADOW_SHADER_SOURCE} -entry vertexMain -target spirv -profile spirv_1_4 -o "${SPV_OUTPUT_DIR}/shadow.vert.spv"
    DEPENDS "${SHADOW_SHADER_SOURCE}"
    COMMENT "Compiling vertex shader shadow.vert.spv"
    VERBATIM
)

set(LIT_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/lit.slang")
add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/lit.vert.spv"
    COMMAND ${SLANGC} ${LIT_SHADER_SOURCE} -entry vertexMain -target spirv -profile spirv_1_4 -g -o "${SPV_OUTPUT_DIR}/lit.vert.spv"
    DEPENDS "${LIT_SHADER_SOURCE}"
    COMMENT "Compiling vertex shader lit.vert.spv"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/lit.frag.spv"
    COMMAND ${SLANGC} ${LIT_SHADER_SOURCE} -entry fragmentMain -target spirv -profile spirv_1_4 -g -o "${SPV_OUTPUT_DIR}/lit.frag.spv"
    DEPENDS "${LIT_SHADER_SOURCE}"
    COMMENT "Compiling fragment shader lit.frag.spv"
    VERBATIM
)

set(BLIT_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/blit.slang")
add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/blit.vert.spv"
    COMMAND ${SLANGC} ${BLIT_SHADER_SOURCE} -entry vertexMain -target spirv -profile spirv_1_4 -o "${SPV_OUTPUT_DIR}/blit.vert.spv"
    DEPENDS "${BLIT_SHADER_SOURCE}"
    COMMENT "Compiling vertex shader blit.vert.spv"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/blit.frag.spv"
    COMMAND ${SLANGC} ${BLIT_SHADER_SOURCE} -entry fragmentMain -target spirv -profile spirv_1_4 -o "${SPV_OUTPUT_DIR}/blit.frag.spv"
    DEPENDS "${BLIT_SHADER_SOURCE}"
    COMMENT "Compiling fragment shader blit.frag.spv"
    VERBATIM
)

set(BLUR_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/blur.slang")
add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/blur.frag.spv"
    COMMAND ${SLANGC} ${BLUR_SHADER_SOURCE} -entry fragmentMain -target spirv -profile spirv_1_4 -o "${SPV_OUTPUT_DIR}/blur.frag.spv"
    DEPENDS "${BLUR_SHADER_SOURCE}"
    COMMENT "Compiling fragment shader blur.frag.spv"
    VERBATIM
)

set(MXGUI_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/mxgui.slang")
add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/mxgui.vert.spv"
    COMMAND ${SLANGC} ${MXGUI_SHADER_SOURCE} -entry vertexMain -target spirv -profile spirv_1_4 -o "${SPV_OUTPUT_DIR}/mxgui.vert.spv"
    DEPENDS "${MXGUI_SHADER_SOURCE}"
    COMMENT "Compiling vertment shader mxgui.vert.spv"
    VERBATIM
)

add_custom_command(
    OUTPUT "${SPV_OUTPUT_DIR}/mxgui.frag.spv"
    COMMAND ${SLANGC} ${MXGUI_SHADER_SOURCE} -entry fragmentMain -target spirv -profile spirv_1_4 -o "${SPV_OUTPUT_DIR}/mxgui.frag.spv"
    DEPENDS "${MXGUI_SHADER_SOURCE}"
    COMMENT "Compiling fragment shader mxgui.frag.spv"
    VERBATIM
)

add_custom_target(mxgui_compile_shaders ALL
    DEPENDS
        "${SPV_OUTPUT_DIR}/shadow.vert.spv"
        "${SPV_OUTPUT_DIR}/lit.vert.spv"
        "${SPV_OUTPUT_DIR}/lit.frag.spv"
        "${SPV_OUTPUT_DIR}/blit.vert.spv"
        "${SPV_OUTPUT_DIR}/blit.frag.spv"
        "${SPV_OUTPUT_DIR}/blur.frag.spv"
        "${SPV_OUTPUT_DIR}/mxgui.vert.spv"
        "${SPV_OUTPUT_DIR}/mxgui.frag.spv"
)
