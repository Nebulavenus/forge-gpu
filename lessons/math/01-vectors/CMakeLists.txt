add_executable(01-vectors main.c)
target_include_directories(01-vectors PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(01-vectors PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 01-vectors POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:01-vectors>
    )
endif()
