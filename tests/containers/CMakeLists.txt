add_executable(test_containers test_containers.c)
target_include_directories(test_containers PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_containers PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_containers POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_containers>
    )
endif()

# Add as a CTest test
add_test(NAME containers COMMAND test_containers)
