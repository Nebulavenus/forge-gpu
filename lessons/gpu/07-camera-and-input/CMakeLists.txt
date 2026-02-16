add_executable(07-camera-and-input WIN32 main.c)
target_include_directories(07-camera-and-input PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(07-camera-and-input PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(07-camera-and-input PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 07-camera-and-input POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:07-camera-and-input>
    )
endif()
