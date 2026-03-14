add_executable(17-implicit-curves main.c)
target_include_directories(17-implicit-curves PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(17-implicit-curves PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
add_custom_command(TARGET 17-implicit-curves POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:17-implicit-curves>
)
endif()
