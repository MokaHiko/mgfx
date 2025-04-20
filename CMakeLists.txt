cmake_minimum_required(VERSION 3.0...3.10)
project(mgfx VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(MGFX_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries.")
set(MGFX_BUILD_EXAMPLES OFF CACHE BOOL "Build examples.")

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

if(MGFX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
