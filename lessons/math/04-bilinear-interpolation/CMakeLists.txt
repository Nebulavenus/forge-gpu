add_executable(04-bilinear-interpolation main.c)
target_include_directories(04-bilinear-interpolation PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(04-bilinear-interpolation PRIVATE SDL3::SDL3)

add_custom_command(TARGET 04-bilinear-interpolation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:04-bilinear-interpolation>
)
