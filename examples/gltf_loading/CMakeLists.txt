cmake_minimum_required(VERSION 3.0...3.10)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(gltf_loader gltf_loader.c ../third_party/mikktspace/mikktspace.c)
target_link_libraries(gltf_loader PRIVATE mgfx)
target_include_directories(gltf_loader PUBLIC ./ ../third_party/)
