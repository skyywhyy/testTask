add_library(project_common INTERFACE)

target_include_directories(project_common
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
