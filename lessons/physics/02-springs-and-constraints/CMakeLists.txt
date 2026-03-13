add_executable(02-springs-and-constraints WIN32 main.c)
target_include_directories(02-springs-and-constraints PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(02-springs-and-constraints PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(02-springs-and-constraints PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL next to executable (Windows)
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 02-springs-and-constraints POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:02-springs-and-constraints>
        VERBATIM
    )
endif()
