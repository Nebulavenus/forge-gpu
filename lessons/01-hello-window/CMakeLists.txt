# Lesson 01 — Hello Window
# A standalone executable that opens a window and clears it using the GPU.

# WIN32 sets the subsystem to WINDOWS so no console window is created.
# SDL_main.h provides the WinMain entry point that calls our SDL_main().
# On non-Windows platforms, WIN32 is silently ignored.
add_executable(01-hello-window WIN32 main.c)

target_include_directories(01-hello-window PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(01-hello-window PRIVATE SDL3::SDL3)

# Copy SDL3.dll next to the executable so Windows can find it at runtime.
add_custom_command(TARGET 01-hello-window POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:01-hello-window>
)
