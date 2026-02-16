add_executable(10-anisotropy main.c)
target_include_directories(10-anisotropy PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(10-anisotropy PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

add_custom_command(TARGET 10-anisotropy POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:10-anisotropy>
)
