# Lesson 44 — Pipeline Morph Target Animations
#
# Demonstrates morph target (blend shape) animation using pre-processed
# pipeline assets: .fmesh with morph deltas, .fanim with morph weight channels.
#
# SPDX-License-Identifier: Zlib

add_executable(44-pipeline-morph-animations WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(44-pipeline-morph-animations PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(44-pipeline-morph-animations PRIVATE
    SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
)

if(FORGE_CAPTURE)
    target_compile_definitions(44-pipeline-morph-animations PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 44-pipeline-morph-animations POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:44-pipeline-morph-animations>
        VERBATIM
    )
endif()

# ── Copy assets + fonts to the exe directory ─────────────────────────────

# Copy fonts for UI
add_custom_command(TARGET 44-pipeline-morph-animations POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/fonts
        $<TARGET_FILE_DIR:44-pipeline-morph-animations>/assets/fonts
)

# Copy model assets (.fscene, .fmesh, .fmat, .fanim, textures)
add_custom_command(TARGET 44-pipeline-morph-animations POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:44-pipeline-morph-animations>/assets
)
