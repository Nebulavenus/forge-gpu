add_executable(04-pointers-and-memory main.c)
target_include_directories(04-pointers-and-memory PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(04-pointers-and-memory PRIVATE SDL3::SDL3)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 04-pointers-and-memory POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:04-pointers-and-memory>
    )
endif()
