add_executable(processing_tests
    processing_tests.cpp
)

target_link_libraries(processing_tests
    PRIVATE
        processing
)

target_compile_options(processing_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_test(
    NAME processing_tests
    COMMAND processing_tests
)
