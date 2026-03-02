add_executable(13-theming-and-color-system main.c)
target_include_directories(13-theming-and-color-system PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(13-theming-and-color-system PRIVATE SDL3::SDL3)

# Link math library on platforms that require it (Linux, etc.)
if(UNIX AND NOT APPLE)
    target_link_libraries(13-theming-and-color-system PRIVATE m)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 13-theming-and-color-system POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:13-theming-and-color-system>
    )
endif()
