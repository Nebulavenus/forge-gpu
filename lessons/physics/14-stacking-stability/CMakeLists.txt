add_executable(14-stacking-stability WIN32 main.c)
target_include_directories(14-stacking-stability PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(14-stacking-stability PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(14-stacking-stability PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL and font asset next to executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 14-stacking-stability POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:14-stacking-stability>
    )
endif()

add_custom_command(TARGET 14-stacking-stability POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:14-stacking-stability>/assets/fonts/liberation_mono
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/assets/fonts/liberation_mono/LiberationMono-Regular.ttf
        $<TARGET_FILE_DIR:14-stacking-stability>/assets/fonts/liberation_mono/
)
