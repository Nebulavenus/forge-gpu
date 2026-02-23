# ── Engine Lesson 09 — HLSL Shared Headers ─────────────────────────────────
#
# This lesson has TWO source files (main.c and sky_pass.c) to demonstrate
# that both files can include shared_params.h — the same pattern as two
# HLSL shaders including the same .hlsli file.

add_executable(09-hlsl-shared-headers main.c sky_pass.c)
target_include_directories(09-hlsl-shared-headers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}   # shared_params.h, sky_pass.h
)
target_link_libraries(09-hlsl-shared-headers PRIVATE
    SDL3::SDL3
)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 09-hlsl-shared-headers POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:09-hlsl-shared-headers>
    )
endif()
