cmake_minimum_required(VERSION 3.10)
project(viterbi_decoder C)

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_C_STANDARD 99)
set(CMAKE_BUILD_TYPE Debug)

file(GLOB SOURCES "*.c")

add_executable(viterbi_decoder ${SOURCES})