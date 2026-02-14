add_executable(03-uniforms-and-motion WIN32 main.c)
target_include_directories(03-uniforms-and-motion PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(03-uniforms-and-motion PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(03-uniforms-and-motion PRIVATE FORGE_CAPTURE)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 03-uniforms-and-motion POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:03-uniforms-and-motion>
    )
endif()
