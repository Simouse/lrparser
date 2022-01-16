cmake_minimum_required(VERSION 3.10)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Somehow my exe doesn't have debug info
# Doesn't work
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ADDITIONAL_CFLAGS} -g")

project(lrparser)

file(GLOB AUTOMATA_SRC "src/automata/*.cpp")
file(GLOB GRAMMAR_SRC  "src/grammar/*.cpp")
file(GLOB PORT_SRC     "src/display/*.cpp")
file(GLOB UTIL_SRC     "src/util/*.cpp")

include_directories( ${PROJECT_SOURCE_DIR} )
set ( SOURCES src/main.cpp ${AUTOMATA_SRC} ${GRAMMAR_SRC} ${PORT_SRC} ${UTIL_SRC} src/util/bitset.h src/util/bitset.cpp)
add_executable (lrparser ${SOURCES})

# install (TARGETS lrparser DESTINATION ${PROJECT_SOURCE_DIR}/bin)