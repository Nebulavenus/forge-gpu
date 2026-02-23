add_executable(26-procedural-sky WIN32 main.c)
target_include_directories(26-procedural-sky PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(26-procedural-sky PRIVATE SDL3::SDL3)

# Link libm on POSIX systems (sincosf, sinf, etc. from forge_math.h).
if(UNIX)
    target_link_libraries(26-procedural-sky PRIVATE m)
endif()

if(FORGE_CAPTURE)
    target_compile_definitions(26-procedural-sky PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 26-procedural-sky POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:26-procedural-sky>
    )
endif()
