cmake_minimum_required(VERSION 3.10)
project(lrparser)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")

file(GLOB AUTOMATA_SRC "automata/*.cpp")
file(GLOB GRAMMAR_SRC  "grammar/*.cpp")
file(GLOB PORT_SRC     "port/*.cpp")
file(GLOB UTIL_SRC     "util/*.cpp")

include_directories( ${PROJECT_SOURCE_DIR} )
set ( SOURCES main.cpp ${AUTOMATA_SRC} ${GRAMMAR_SRC} ${PORT_SRC} ${UTIL_SRC} )
add_executable (lrparser ${SOURCES})

# target_link_libraries( lrparser ${LIBS})
install (TARGETS lrparser DESTINATION ${PROJECT_SOURCE_DIR}/bin)