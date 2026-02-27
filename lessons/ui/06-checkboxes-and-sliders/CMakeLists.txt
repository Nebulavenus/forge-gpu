add_executable(06-checkboxes-and-sliders main.c)
target_include_directories(06-checkboxes-and-sliders PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(06-checkboxes-and-sliders PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 06-checkboxes-and-sliders POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:06-checkboxes-and-sliders>
    )
endif()
