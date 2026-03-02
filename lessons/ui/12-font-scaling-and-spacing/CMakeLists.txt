add_executable(12-font-scaling-and-spacing main.c)
target_include_directories(12-font-scaling-and-spacing PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(12-font-scaling-and-spacing PRIVATE SDL3::SDL3)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 12-font-scaling-and-spacing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:12-font-scaling-and-spacing>
    )
endif()
