add_executable(06-depth-and-3d WIN32 main.c)
target_include_directories(06-depth-and-3d PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(06-depth-and-3d PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(06-depth-and-3d PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 06-depth-and-3d POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:06-depth-and-3d>
    )
endif()
