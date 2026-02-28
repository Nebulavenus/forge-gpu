add_executable(09-panels-and-scrolling main.c)
target_include_directories(09-panels-and-scrolling PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(09-panels-and-scrolling PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 09-panels-and-scrolling POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:09-panels-and-scrolling>
    )
endif()
