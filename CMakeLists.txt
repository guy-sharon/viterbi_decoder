cmake_minimum_required(VERSION 3.10)
project(viterbi_decoder C)

file(GLOB SOURCES "main.c")

set(CMAKE_COMPILE_WARNING_AS_ERROR ON)
set(CMAKE_C_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

add_executable(viterbi_decoder ${SOURCES})
target_compile_options(viterbi_decoder PRIVATE -O0 -mavx -march=native)