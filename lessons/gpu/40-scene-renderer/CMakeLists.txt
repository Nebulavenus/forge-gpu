add_executable(40-scene-renderer WIN32 main.c)
target_include_directories(40-scene-renderer PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(40-scene-renderer PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(40-scene-renderer PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 40-scene-renderer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:40-scene-renderer>
        VERBATIM
    )
endif()

# Copy font assets for UI rendering
add_custom_command(TARGET 40-scene-renderer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/fonts
        $<TARGET_FILE_DIR:40-scene-renderer>/assets/fonts
)
