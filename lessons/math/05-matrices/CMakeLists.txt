add_executable(05-matrices main.c)
target_include_directories(05-matrices PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(05-matrices PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 05-matrices POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:05-matrices>
    )
endif()
