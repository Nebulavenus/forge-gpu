add_executable(11-git-version-control main.c)
target_include_directories(11-git-version-control PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(11-git-version-control PRIVATE SDL3::SDL3)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 11-git-version-control POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:11-git-version-control>
    )
endif()
