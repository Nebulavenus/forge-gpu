add_executable(34-stencil-testing WIN32 main.c)
target_include_directories(34-stencil-testing PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(34-stencil-testing PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(34-stencil-testing PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 34-stencil-testing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:34-stencil-testing>
        VERBATIM
    )
endif()
