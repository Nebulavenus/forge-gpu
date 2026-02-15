add_executable(08-orientation main.c)
target_include_directories(08-orientation PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(08-orientation PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 08-orientation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:08-orientation>
    )
endif()
