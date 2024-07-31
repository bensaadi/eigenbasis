cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb -Wall -Wno-cpp -Wno-unused-but-set-variable -Wno-class-memaccess")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_CXX_FLAGS_RELEASE "-O2")
set(CMAKE_CXX_FLAGS_COVERAGE "-g -O0 -Wall -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_OUTPUT_EXTENSION_REPLACE_COVERAGE 1)

project(Eigenbasis VERSION 0.1)

include_directories(${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/include)

add_subdirectory(src/depth)
add_subdirectory(src/utils)
add_subdirectory(src/book)

add_subdirectory(tests/book)
add_subdirectory(tests/depth)