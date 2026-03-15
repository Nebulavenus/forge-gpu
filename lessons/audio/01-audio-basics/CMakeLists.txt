add_executable(01-audio-basics WIN32 main.c)
target_include_directories(01-audio-basics PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(01-audio-basics PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(01-audio-basics PRIVATE FORGE_CAPTURE)
endif()

# Set working directory for Visual Studio debugger (needed for asset loading)
set_target_properties(01-audio-basics PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Copy SDL3 DLL next to executable (Windows)
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 01-audio-basics POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:01-audio-basics>
        VERBATIM
    )
endif()
