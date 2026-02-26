add_executable(15-bezier-curves main.c)
target_include_directories(15-bezier-curves PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(15-bezier-curves PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
add_custom_command(TARGET 15-bezier-curves POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:15-bezier-curves>
)
endif()
