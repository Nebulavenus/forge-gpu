add_executable(05-mipmaps WIN32 main.c)
target_include_directories(05-mipmaps PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(05-mipmaps PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(05-mipmaps PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 05-mipmaps POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:05-mipmaps>
    )
endif()
