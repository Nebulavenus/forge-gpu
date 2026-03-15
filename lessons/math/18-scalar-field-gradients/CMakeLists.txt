add_executable(18-scalar-field-gradients main.c)
target_include_directories(18-scalar-field-gradients PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(18-scalar-field-gradients PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
add_custom_command(TARGET 18-scalar-field-gradients POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:18-scalar-field-gradients>
)
endif()
