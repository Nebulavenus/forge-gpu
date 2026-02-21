add_executable(13-gradient-noise main.c)
target_include_directories(13-gradient-noise PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(13-gradient-noise PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
add_custom_command(TARGET 13-gradient-noise POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:13-gradient-noise>
)
endif()
