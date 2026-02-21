add_executable(02-coordinate-spaces main.c)
target_include_directories(02-coordinate-spaces PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(02-coordinate-spaces PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
add_custom_command(TARGET 02-coordinate-spaces POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:02-coordinate-spaces>
)
endif()
