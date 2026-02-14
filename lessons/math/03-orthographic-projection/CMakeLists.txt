add_executable(03-orthographic-projection main.c)
target_include_directories(03-orthographic-projection PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(03-orthographic-projection PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

add_custom_command(TARGET 03-orthographic-projection POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:03-orthographic-projection>
)
