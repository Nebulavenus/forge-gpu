add_executable(05-immediate-mode-basics main.c)
target_include_directories(05-immediate-mode-basics PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(05-immediate-mode-basics PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 05-immediate-mode-basics POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:05-immediate-mode-basics>
    )
endif()
