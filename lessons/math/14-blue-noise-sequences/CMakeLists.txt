add_executable(14-blue-noise-sequences main.c)
target_include_directories(14-blue-noise-sequences PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(14-blue-noise-sequences PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
add_custom_command(TARGET 14-blue-noise-sequences POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:14-blue-noise-sequences>
)
endif()
