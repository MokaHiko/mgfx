cmake_minimum_required(VERSION 3.0...3.10)
project(mgfx VERSION 1.0 LANGUAGES C)

set(MGFX_BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries.")
set(MGFX_BUILD_EXAMPLES ON CACHE BOOL "Build examples.")

find_package(Vulkan REQUIRED)

if(MGFX_BUILD_SHARED_LIBS)
    add_library(mgfx SHARED src/mgfx.c)
    target_compile_definitions(mgfx PRIVATE MGFX_EXPORTS)
else()
    add_library(mgfx STATIC src/mgfx.c)
endif()

target_link_libraries(mgfx PUBLIC mx Vulkan::Vulkan)
target_include_directories(mgfx PUBLIC include)
target_include_directories(mgfx PRIVATE third_party)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
endif()

if(MGFX_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
