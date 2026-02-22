add_executable(25-shader-noise WIN32 main.c)
target_include_directories(25-shader-noise PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(25-shader-noise PRIVATE SDL3::SDL3)

if(FORGE_CAPTURE)
    target_compile_definitions(25-shader-noise PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 25-shader-noise POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:25-shader-noise>
    )
endif()
