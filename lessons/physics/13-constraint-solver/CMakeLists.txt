add_executable(13-constraint-solver WIN32 main.c)
target_include_directories(13-constraint-solver PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(13-constraint-solver PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(13-constraint-solver PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL next to executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 13-constraint-solver POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:13-constraint-solver>
    )
endif()

# Copy font asset
add_custom_command(TARGET 13-constraint-solver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:13-constraint-solver>/assets/fonts/liberation_mono
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/liberation_mono/LiberationMono-Regular.ttf
        $<TARGET_FILE_DIR:13-constraint-solver>/assets/fonts/liberation_mono/
)
