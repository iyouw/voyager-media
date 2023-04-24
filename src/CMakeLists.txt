find_library(Math m)

add_executable(main main.c memory_stream.c)
target_include_directories(main PUBLIC ${PROJECT_BINARY_DIR})
target_link_libraries(main PUBLIC ${Math})

