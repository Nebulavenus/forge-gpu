add_executable(15-dev-ui main.c)
target_include_directories(15-dev-ui PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(15-dev-ui PRIVATE SDL3::SDL3)

if(UNIX)
    target_link_libraries(15-dev-ui PRIVATE m)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 15-dev-ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:15-dev-ui>
    )
endif()
