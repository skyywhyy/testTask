add_library(processing SHARED
    src/processing.cpp
)

target_include_directories(processing
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(processing
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
