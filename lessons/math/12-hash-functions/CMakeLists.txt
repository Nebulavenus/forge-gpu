add_executable(12-hash-functions main.c)
target_include_directories(12-hash-functions PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(12-hash-functions PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
add_custom_command(TARGET 12-hash-functions POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:12-hash-functions>
)
endif()
