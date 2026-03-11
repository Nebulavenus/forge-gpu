add_executable(12-memory-arenas main.c)
target_include_directories(12-memory-arenas PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(12-memory-arenas PRIVATE SDL3::SDL3)

add_custom_command(TARGET 12-memory-arenas POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:12-memory-arenas>
)
