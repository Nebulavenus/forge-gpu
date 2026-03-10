add_executable(01-point-particles WIN32 main.c)
target_include_directories(01-point-particles PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(01-point-particles PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(01-point-particles PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL next to executable (Windows)
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 01-point-particles POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:01-point-particles>
        VERBATIM
    )
endif()
