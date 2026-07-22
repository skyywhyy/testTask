cmake_minimum_required(VERSION 3.16)

project(cpp_intern_task LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Threads REQUIRED)

include(CTest)

add_subdirectory(common)
add_subdirectory(library)
add_subdirectory(program1)
add_subdirectory(program2)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
