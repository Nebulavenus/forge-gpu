add_executable(11-color-spaces main.c)
target_include_directories(11-color-spaces PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(11-color-spaces PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

add_custom_command(TARGET 11-color-spaces POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:11-color-spaces>
)
