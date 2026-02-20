add_executable(01-intro-to-c main.c)
target_include_directories(01-intro-to-c PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(01-intro-to-c PRIVATE SDL3::SDL3)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 01-intro-to-c POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:01-intro-to-c>
    )
endif()
