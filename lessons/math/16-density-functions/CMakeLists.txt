add_executable(16-density-functions main.c)
target_include_directories(16-density-functions PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(16-density-functions PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
add_custom_command(TARGET 16-density-functions POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:16-density-functions>
)
endif()
