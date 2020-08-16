cmake_minimum_required(VERSION 3.16)
project(tgswebp)

set(CMAKE_CXX_STANDARD 14)
add_subdirectory (lib/rlottie)
add_subdirectory (lib/libwebp)
add_subdirectory (lib/zlib)

add_executable(tgswebp main.cpp)

target_link_libraries (tgswebp rlottie webpdecoder exampleutil libwebpmux zlibstatic)

