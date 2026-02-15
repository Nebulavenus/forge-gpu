add_executable(04-mipmaps-and-lod main.c)
target_include_directories(04-mipmaps-and-lod PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(04-mipmaps-and-lod PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

add_custom_command(TARGET 04-mipmaps-and-lod POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:04-mipmaps-and-lod>
)
