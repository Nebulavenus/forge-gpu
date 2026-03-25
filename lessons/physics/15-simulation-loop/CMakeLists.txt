add_executable(15-simulation-loop WIN32 main.c)
target_include_directories(15-simulation-loop PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(15-simulation-loop PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(15-simulation-loop PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL and font asset next to executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 15-simulation-loop POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:15-simulation-loop>
    )
endif()

add_custom_command(TARGET 15-simulation-loop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:15-simulation-loop>/assets/fonts/liberation_mono
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/liberation_mono/LiberationMono-Regular.ttf
        $<TARGET_FILE_DIR:15-simulation-loop>/assets/fonts/liberation_mono/
)
