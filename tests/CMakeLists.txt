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

add_executable(shared_buffer_tests
    shared_buffer_tests.cpp
    ${CMAKE_SOURCE_DIR}/program1/src/shared_buffer.cpp
)

target_include_directories(shared_buffer_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/program1/include
)

target_link_libraries(shared_buffer_tests
    PRIVATE
        Threads::Threads
)

target_compile_options(shared_buffer_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_test(
    NAME shared_buffer_tests
    COMMAND shared_buffer_tests
)

set_tests_properties(shared_buffer_tests PROPERTIES TIMEOUT 10)

add_executable(application_tests
    application_tests.cpp
    ${CMAKE_SOURCE_DIR}/program1/src/application.cpp
    ${CMAKE_SOURCE_DIR}/program1/src/shared_buffer.cpp
)

target_include_directories(application_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/program1/include
)

target_link_libraries(application_tests
    PRIVATE
        processing
        Threads::Threads
)

target_compile_options(application_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_test(
    NAME application_tests
    COMMAND application_tests
)

set_tests_properties(application_tests PROPERTIES TIMEOUT 10)

add_executable(program2_message_tests
    program2_message_tests.cpp
    ${CMAKE_SOURCE_DIR}/program2/src/message_processor.cpp
)

target_include_directories(program2_message_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/program2/include
)

target_link_libraries(program2_message_tests
    PRIVATE
        processing
)

target_compile_options(program2_message_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_test(
    NAME program2_message_tests
    COMMAND program2_message_tests
)

if(NOT UNIX)
    return()
endif()

add_executable(tcp_client_tests
    tcp_client_tests.cpp
    ${CMAKE_SOURCE_DIR}/program1/src/tcp_client.cpp
)

target_include_directories(tcp_client_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/program1/include
)

target_link_libraries(tcp_client_tests
    PRIVATE
        Threads::Threads
)

target_compile_options(tcp_client_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_test(
    NAME tcp_client_tests
    COMMAND tcp_client_tests
)

set_tests_properties(tcp_client_tests PROPERTIES TIMEOUT 10)

add_executable(tcp_server_tests
    tcp_server_tests.cpp
    ${CMAKE_SOURCE_DIR}/program2/src/tcp_server.cpp
)

target_include_directories(tcp_server_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/program2/include
)

target_link_libraries(tcp_server_tests
    PRIVATE
        Threads::Threads
)

target_compile_options(tcp_server_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_test(
    NAME tcp_server_tests
    COMMAND tcp_server_tests
)

set_tests_properties(tcp_server_tests PROPERTIES TIMEOUT 10)

add_executable(program2_application_tests
    program2_application_tests.cpp
    ${CMAKE_SOURCE_DIR}/program2/src/application.cpp
    ${CMAKE_SOURCE_DIR}/program2/src/message_processor.cpp
    ${CMAKE_SOURCE_DIR}/program2/src/tcp_server.cpp
)

target_include_directories(program2_application_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/program2/include
)

target_link_libraries(program2_application_tests
    PRIVATE
        processing
        Threads::Threads
)

target_compile_options(program2_application_tests
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
)

add_test(
    NAME program2_application_tests
    COMMAND program2_application_tests
)

set_tests_properties(program2_application_tests PROPERTIES TIMEOUT 10)
