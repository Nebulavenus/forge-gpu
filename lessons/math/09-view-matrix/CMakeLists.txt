add_executable(09-view-matrix main.c)
target_include_directories(09-view-matrix PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(09-view-matrix PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 09-view-matrix POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:09-view-matrix>
    )
endif()
