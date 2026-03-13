add_executable(03-particle-collisions WIN32 main.c)
target_include_directories(03-particle-collisions PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(03-particle-collisions PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(03-particle-collisions PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL next to executable (Windows)
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 03-particle-collisions POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:03-particle-collisions>
        VERBATIM
    )
endif()
