add_executable(04-textures-and-samplers WIN32 main.c)
target_include_directories(04-textures-and-samplers PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(04-textures-and-samplers PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(04-textures-and-samplers PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 04-textures-and-samplers POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:04-textures-and-samplers>
    )
endif()

# Copy texture files next to the executable so they can be loaded at runtime
add_custom_command(TARGET 04-textures-and-samplers POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:04-textures-and-samplers>/textures
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/textures/brick_wall.png
        $<TARGET_FILE_DIR:04-textures-and-samplers>/textures/brick_wall.png
)
