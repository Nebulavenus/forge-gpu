add_executable(42-pipeline-texture-compression WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(42-pipeline-texture-compression PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(42-pipeline-texture-compression PRIVATE
    SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>
)

if(FORGE_CAPTURE)
    target_compile_definitions(42-pipeline-texture-compression PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 42-pipeline-texture-compression POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:42-pipeline-texture-compression>
        VERBATIM
    )
endif()

# ── Asset pipeline: generate .fmesh/.fmat/.fscene from raw .gltf ─────────

set(_LESSON_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
set(_MODEL ABeautifulGame)
set(_GLTF  ${_LESSON_ASSETS}/${_MODEL}/${_MODEL}.gltf)
set(_OUTDIR ${_LESSON_ASSETS}/${_MODEL})
set(_FMESH  ${_OUTDIR}/${_MODEL}.fmesh)
set(_FMAT   ${_OUTDIR}/${_MODEL}.fmat)
set(_FSCENE ${_OUTDIR}/${_MODEL}.fscene)

# Only add pipeline commands if outputs do not already exist.
# This avoids failures on CI where LFS .bin files are pointers.
# Pre-built assets are committed so CI and users without Git LFS can build
# without running the pipeline.  The if(NOT EXISTS) guards are evaluated at
# CMake *configure* time — the pipeline only runs when outputs are missing.
# A reconfigure (cmake ..) is needed if pre-built assets are deleted.

set(_ALL_OUTPUTS "")

if(NOT EXISTS ${_FMESH} OR NOT EXISTS ${_FMAT})
    add_custom_command(
        OUTPUT  ${_FMESH} ${_FMAT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_OUTDIR}
        COMMAND $<TARGET_FILE:forge_mesh_tool> ${_GLTF} ${_FMESH} --verbose
        DEPENDS ${_GLTF} forge_mesh_tool
        COMMENT "Processing ${_MODEL}: .gltf -> .fmesh + .fmat"
        VERBATIM
    )
    list(APPEND _ALL_OUTPUTS ${_FMESH} ${_FMAT})
endif()

if(NOT EXISTS ${_FSCENE})
    add_custom_command(
        OUTPUT  ${_FSCENE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_OUTDIR}
        COMMAND $<TARGET_FILE:forge_scene_tool> ${_GLTF} ${_FSCENE} --verbose
        DEPENDS ${_GLTF} forge_scene_tool
        COMMENT "Processing ${_MODEL}: .gltf -> .fscene"
        VERBATIM
    )
    list(APPEND _ALL_OUTPUTS ${_FSCENE})
endif()

# Custom target that triggers all asset processing
if(_ALL_OUTPUTS)
    add_custom_target(42-pipeline-texture-compression-assets DEPENDS ${_ALL_OUTPUTS})
    add_dependencies(42-pipeline-texture-compression 42-pipeline-texture-compression-assets)
endif()

# ── Copy processed assets + fonts to the exe directory ───────────────────

# Copy fonts for UI
add_custom_command(TARGET 42-pipeline-texture-compression POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/fonts
        $<TARGET_FILE_DIR:42-pipeline-texture-compression>/assets/fonts
)

# Copy model assets (.fscene, .fmesh, .fmat, textures, .ftex, .meta.json)
add_custom_command(TARGET 42-pipeline-texture-compression POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:42-pipeline-texture-compression>/assets
)
