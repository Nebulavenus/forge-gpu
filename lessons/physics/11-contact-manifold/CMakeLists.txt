add_executable(11-contact-manifold WIN32 main.c)
target_include_directories(11-contact-manifold PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(11-contact-manifold PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(11-contact-manifold PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL next to executable (Windows)
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 11-contact-manifold POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:11-contact-manifold>
        VERBATIM
    )
endif()

# Copy font asset next to executable
add_custom_command(TARGET 11-contact-manifold POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:11-contact-manifold>/assets/fonts/liberation_mono
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/liberation_mono/LiberationMono-Regular.ttf
        $<TARGET_FILE_DIR:11-contact-manifold>/assets/fonts/liberation_mono/
    VERBATIM
)
