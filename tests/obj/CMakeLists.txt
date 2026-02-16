add_executable(test_obj test_obj.c)
target_include_directories(test_obj PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_obj PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_obj POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_obj>
    )
endif()

# Add as a CTest test
add_test(NAME obj_parser COMMAND test_obj)
