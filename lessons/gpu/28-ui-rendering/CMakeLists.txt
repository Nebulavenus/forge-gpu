add_executable(28-ui-rendering WIN32
    main.c
)
target_include_directories(28-ui-rendering PRIVATE
    ${FORGE_COMMON_DIR}
)
target_link_libraries(28-ui-rendering PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(28-ui-rendering PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 28-ui-rendering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:28-ui-rendering>
    )
endif()

# Copy font asset to build output directory
add_custom_command(TARGET 28-ui-rendering POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:28-ui-rendering>/assets/fonts/liberation_mono
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FORGE_ASSETS_DIR}/fonts/liberation_mono/LiberationMono-Regular.ttf
        $<TARGET_FILE_DIR:28-ui-rendering>/assets/fonts/liberation_mono/LiberationMono-Regular.ttf
)
