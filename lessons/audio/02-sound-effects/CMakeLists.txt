add_executable(02-sound-effects WIN32 main.c)
target_include_directories(02-sound-effects PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(02-sound-effects PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(02-sound-effects PRIVATE FORGE_CAPTURE)
endif()

# Set working directory for Visual Studio debugger (needed for asset loading)
set_target_properties(02-sound-effects PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Copy SDL3 DLL next to executable (Windows)
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 02-sound-effects POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:02-sound-effects>
        VERBATIM
    )
endif()
