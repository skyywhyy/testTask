if(NOT UNIX)
    return()
endif()

add_executable(program2
    src/application.cpp
    src/main.cpp
    src/message_processor.cpp
    src/tcp_server.cpp
)

target_include_directories(program2
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(program2
    PRIVATE
        processing
        project_common
)

target_compile_options(program2
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)
