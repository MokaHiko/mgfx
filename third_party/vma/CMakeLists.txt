cmake_minimum_required(VERSION 3.0...3.10)
project(vma VERSION 1.0 LANGUAGES CXX)

find_package(Vulkan REQUIRED)

add_library(vma vma_impl.cpp)
set_target_properties(vma PROPERTIES LINKER_LANGUAGE CXX)
set_target_properties(vma PROPERTIES CXX_STANDARD 14)
target_link_libraries(vma PUBLIC Vulkan::Vulkan)
