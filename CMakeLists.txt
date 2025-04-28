cmake_minimum_required(VERSION 3.0...3.10)
project(mgfx VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MGFX_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries.")
set(MGFX_BUILD_EXAMPLES ON CACHE BOOL "Build examples.")

find_package(Vulkan REQUIRED)
add_subdirectory(third_party/vma)

if(MGFX_BUILD_SHARED_LIBS)
    add_library(mgfx SHARED src/mgfx.c src/renderer_vk.c third_party/spirv_reflect/spirv_reflect.c)
    target_compile_definitions(mgfx PRIVATE MGFX_EXPORTS)
else()
    add_library(mgfx STATIC src/mgfx.c src/renderer_vk.c third_party/spirv_reflect/spirv_reflect.c)
endif()

target_link_libraries(mgfx PUBLIC mx vma Vulkan::Vulkan)

target_include_directories(mgfx PUBLIC include)
target_include_directories(mgfx PUBLIC third_party)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message("-- @warn mgfx uses glfw to get window surfaces on mac!")
    target_link_libraries(mgfx PRIVATE glfw)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message("-- @warn mgfx uses glfw to get window surfaces on windows!")
    target_link_libraries(mgfx PRIVATE glfw)
endif()

set(SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/text.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/text.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/gizmos.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/gizmos.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/blit.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/blit.frag.glsl
)

if(MGFX_BUILD_EXAMPLES)
    add_subdirectory(examples)
    list(APPEND SHADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/unlit.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/unlit.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/sprites.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/sprites.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/lit.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/lit.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shadows.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shadows_lit.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shadows_lit.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/pbr.frag.glsl

    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/skybox.vert.glsl
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/skybox.frag.glsl
    )
endif()

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

# Built in assets
target_compile_definitions(mgfx PUBLIC MGFX_ASSET_PATH="${CMAKE_CURRENT_BINARY_DIR}/assets/")
add_custom_target(compile_built_in_shaders ALL DEPENDS ${SPIRV_OUTPUTS})

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
add_custom_target(copy_built_in_fonts ALL
    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/assets/fonts.copy"
)
