cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/D_CRT_SECURE_NO_DEPRECATE>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/D_CRT_SECURE_NO_DEPRECATE>")

project(lrparser)

include_directories(${PROJECT_SOURCE_DIR})

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(lrparser ${SOURCES})

if (UNIX)
    target_link_libraries(lrparser libpthread.so)
endif()