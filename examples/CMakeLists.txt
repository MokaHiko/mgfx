cmake_minimum_required(VERSION 3.0.0...3.10)
project(mgfx_examples VERSION 1.0 LANGUAGES C)

include(FetchContent)

message(STATUS "Downloading glTF Sample Models...")
FetchContent_Declare(
    glTF_Sample_Models
    GIT_REPOSITORY "https://github.com/KhronosGroup/glTF-Sample-Models.git"
    GIT_TAG main
)
FetchContent_MakeAvailable(glTF_Sample_Models)

add_library(ex_common ex_common.c third_party/mikktspace/mikktspace.c)
target_link_libraries(ex_common PUBLIC mgfx glfw)
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

add_executable(post_processing post_processing/post_processing.c)
target_link_libraries(post_processing PRIVATE gltf_loader)

add_executable(panoramic_viewer panoramic_viewer/panoramic_viewer.c)
target_link_libraries(panoramic_viewer PRIVATE gltf_loader)
