cmake_minimum_required(VERSION 3.16)

project(cpp_intern_task LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

include(GNUInstallDirs)

# Installed executables locate libprocessing.so relative to themselves, so the
# install tree can be moved anywhere without touching ldconfig or LD_LIBRARY_PATH.
set(CMAKE_INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

find_package(Threads REQUIRED)

include(CTest)

add_subdirectory(common)
add_subdirectory(library)
add_subdirectory(program1)
add_subdirectory(program2)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
