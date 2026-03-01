add_executable(11-widget-id-system main.c)
target_include_directories(11-widget-id-system PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(11-widget-id-system PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 11-widget-id-system POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:11-widget-id-system>
    )
endif()
