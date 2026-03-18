add_executable(13-stretchy-containers main.c)
target_include_directories(13-stretchy-containers PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(13-stretchy-containers PRIVATE SDL3::SDL3)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 13-stretchy-containers POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:13-stretchy-containers>
    )
endif()
