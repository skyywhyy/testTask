add_library(processing_api INTERFACE)

target_include_directories(processing_api
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
