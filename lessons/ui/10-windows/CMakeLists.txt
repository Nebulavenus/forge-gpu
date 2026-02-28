add_executable(10-windows main.c)
target_include_directories(10-windows PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(10-windows PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 10-windows POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:10-windows>
    )
endif()
