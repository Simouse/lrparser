cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
# add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/std:c++latest>") # format
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/D_CRT_SECURE_NO_DEPRECATE>")

project(lrparser)
 
include_directories(${PROJECT_SOURCE_DIR})

# include_directories(SYSTEM "C:\\Program Files\\Graphviz\\include")
# link_directories(SYSTEM "C:\\Program Files\\Graphviz\\lib")

# add_custom_command(
#     OUTPUT            src/help.cpp
#     DEPENDS           src/help.txt
#     COMMAND           sh help.sh
#     WORKING_DIRECTORY src
# )

file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(lrparser ${SOURCES})
add_custom_target(help_txt COMMAND sh help.sh WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/src)
add_dependencies(lrparser help_txt)

if (UNIX)
    target_link_libraries(lrparser libpthread.so)
endif() 