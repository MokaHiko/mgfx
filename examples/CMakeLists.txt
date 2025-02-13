cmake_minimum_required(VERSION 3.0.0...3.10)
project(mgfx_examples VERSION 1.0 LANGUAGES C)

include(FetchContent)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)

FetchContent_MakeAvailable(glfw)

add_library(ex_common ex_common.c)
target_link_libraries(ex_common PUBLIC mgfx)
target_link_libraries(ex_common PRIVATE glfw)
target_include_directories(ex_common PUBLIC ./)

add_executable(clear_screen clear_screen/main.c)
target_link_libraries(clear_screen PRIVATE ex_common)

