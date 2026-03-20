add_executable(10-epa-penetration-depth WIN32 main.c)
target_include_directories(10-epa-penetration-depth PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(10-epa-penetration-depth PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(10-epa-penetration-depth PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL next to executable (Windows)
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 10-epa-penetration-depth POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:10-epa-penetration-depth>
        VERBATIM
    )
endif()

# Copy font asset next to executable
add_custom_command(TARGET 10-epa-penetration-depth POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:10-epa-penetration-depth>/assets/fonts/liberation_mono
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/liberation_mono/LiberationMono-Regular.ttf
        $<TARGET_FILE_DIR:10-epa-penetration-depth>/assets/fonts/liberation_mono/
    VERBATIM
)
