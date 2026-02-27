add_executable(10-cpu-rasterization main.c)
target_include_directories(10-cpu-rasterization PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(10-cpu-rasterization PRIVATE SDL3::SDL3)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 10-cpu-rasterization POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:10-cpu-rasterization>
    )
endif()
