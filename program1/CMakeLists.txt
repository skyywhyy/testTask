if(NOT UNIX)
    return()
endif()

add_executable(program1
    src/application.cpp
    src/main.cpp
    src/shared_buffer.cpp
    src/tcp_client.cpp
)

target_include_directories(program1
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(program1
    PRIVATE
        processing
        project_common
        Threads::Threads
)

target_compile_options(program1
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
