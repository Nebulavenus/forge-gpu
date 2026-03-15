# Lesson 43 — Pipeline Skinned Animations
#
# Demonstrates skeletal animation using pre-processed pipeline assets:
# .fmesh v3 (skinned vertices), .fskin (joint hierarchy), .fanim (keyframes).
#
# SPDX-License-Identifier: Zlib

add_executable(43-pipeline-skinned-animations WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(43-pipeline-skinned-animations PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(43-pipeline-skinned-animations PRIVATE
    SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
)

if(FORGE_CAPTURE)
    target_compile_definitions(43-pipeline-skinned-animations PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 43-pipeline-skinned-animations POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:43-pipeline-skinned-animations>
        VERBATIM
    )
endif()

# ── Copy assets + fonts to the exe directory ─────────────────────────────

# Copy fonts for UI
add_custom_command(TARGET 43-pipeline-skinned-animations POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/fonts
        $<TARGET_FILE_DIR:43-pipeline-skinned-animations>/assets/fonts
)

# Copy model assets (.fscene, .fmesh, .fmat, .fskin, .fanim, textures)
add_custom_command(TARGET 43-pipeline-skinned-animations POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:43-pipeline-skinned-animations>/assets
)
