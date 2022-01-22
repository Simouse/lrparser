cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

project(lrparser)

file(GLOB AUTOMATA_SRC "src/automata/*.cpp")
file(GLOB GRAMMAR_SRC "src/grammar/*.cpp")
file(GLOB PORT_SRC "src/display/*.cpp")
file(GLOB UTIL_SRC "src/util/*.cpp")

include_directories(${PROJECT_SOURCE_DIR})
set(SOURCES src/main.cpp ${AUTOMATA_SRC} ${GRAMMAR_SRC} ${PORT_SRC} ${UTIL_SRC})
add_executable(lrparser ${SOURCES})

if (UNIX)
    target_link_libraries(lrparser libpthread.so)
endif()

# install (TARGETS lrparser DESTINATION ${PROJECT_SOURCE_DIR}/bin)