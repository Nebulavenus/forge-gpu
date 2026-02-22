# ── Engine Lesson 08 — Debugging Graphics with RenderDoc ─────────────────────
#
# Unlike engine lessons 01-07 (console programs), this lesson creates a GPU
# window to demonstrate RenderDoc.  It requires the real SDL3 library with
# GPU support — it cannot build with the SDL3 shim.
#
# The WIN32 flag prevents a console window from appearing alongside the
# render window on Windows (SDL_Log still works via OutputDebugString).

if(NOT FORGE_USE_SHIM)
    add_executable(08-renderdoc WIN32 main.c)
    target_include_directories(08-renderdoc PRIVATE ${FORGE_COMMON_DIR})
    target_link_libraries(08-renderdoc PRIVATE SDL3::SDL3)

    if(TARGET SDL3::SDL3-shared)
        add_custom_command(TARGET 08-renderdoc POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:SDL3::SDL3-shared>
                $<TARGET_FILE_DIR:08-renderdoc>
        )
    endif()
endif()
