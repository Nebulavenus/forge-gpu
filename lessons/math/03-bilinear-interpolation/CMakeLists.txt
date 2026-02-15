add_executable(03-bilinear-interpolation main.c)
target_include_directories(03-bilinear-interpolation PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(03-bilinear-interpolation PRIVATE SDL3::SDL3)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 03-bilinear-interpolation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:03-bilinear-interpolation>
    )
endif()
