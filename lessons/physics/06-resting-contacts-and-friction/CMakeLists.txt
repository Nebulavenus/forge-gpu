add_executable(06-resting-contacts-and-friction WIN32 main.c)
target_include_directories(06-resting-contacts-and-friction PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(06-resting-contacts-and-friction PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(06-resting-contacts-and-friction PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL next to executable (Windows)
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 06-resting-contacts-and-friction POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:06-resting-contacts-and-friction>
        VERBATIM
    )
endif()

# Copy font asset next to executable
add_custom_command(TARGET 06-resting-contacts-and-friction POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:06-resting-contacts-and-friction>/assets/fonts/liberation_mono
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/liberation_mono/LiberationMono-Regular.ttf
        $<TARGET_FILE_DIR:06-resting-contacts-and-friction>/assets/fonts/liberation_mono/
    VERBATIM
)
