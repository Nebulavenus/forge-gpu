add_executable(41-scene-model-loading WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(41-scene-model-loading PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(41-scene-model-loading PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(41-scene-model-loading PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 41-scene-model-loading POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:41-scene-model-loading>
        VERBATIM
    )
endif()

# ── Asset pipeline: generate .fmesh/.fmat/.fscene from raw .gltf ─────────
#
# forge_mesh_tool produces .fmesh + .fmat; forge_scene_tool produces .fscene.
# Outputs go into the lesson's source assets/ directory for committing.
#
# Pre-built assets are committed so CI and users without Git LFS can build
# without running the pipeline.  The if(NOT EXISTS) guards are evaluated at
# CMake *configure* time — the pipeline only runs when outputs are missing.
# A reconfigure (cmake ..) is needed if pre-built assets are deleted.

set(_LESSON_ASSETS ${CMAKE_CURRENT_SOURCE_DIR}/assets)
set(_RAW_MODELS    ${FORGE_ASSETS_DIR}/models)

# List of models to process: <name> is both the subdirectory under
# assets/models/ and the glTF stem name.
set(_MODELS CesiumMilkTruck Suzanne Duck)

set(_ALL_OUTPUTS "")

foreach(_MODEL ${_MODELS})
    set(_GLTF   ${_RAW_MODELS}/${_MODEL}/${_MODEL}.gltf)
    set(_OUTDIR ${_LESSON_ASSETS}/${_MODEL})
    set(_FMESH  ${_OUTDIR}/${_MODEL}.fmesh)
    set(_FMAT   ${_OUTDIR}/${_MODEL}.fmat)
    set(_FSCENE ${_OUTDIR}/${_MODEL}.fscene)

    # Only add pipeline commands if outputs do not already exist.
    # This avoids failures on CI where LFS .bin files are pointers.
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

    # Copy textures from raw model directory to lesson assets
    # (only .png/.jpg — skip .gltf, .bin, .meta.json)
    file(GLOB _TEXTURES
        ${_RAW_MODELS}/${_MODEL}/*.png
        ${_RAW_MODELS}/${_MODEL}/*.jpg
    )
    foreach(_TEX ${_TEXTURES})
        get_filename_component(_TEX_NAME ${_TEX} NAME)
        set(_TEX_DST ${_OUTDIR}/${_TEX_NAME})
        if(NOT EXISTS ${_TEX_DST})
            add_custom_command(
                OUTPUT  ${_TEX_DST}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${_OUTDIR}
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_TEX} ${_TEX_DST}
                DEPENDS ${_TEX}
                COMMENT "Copying texture ${_TEX_NAME} for ${_MODEL}"
                VERBATIM
            )
            list(APPEND _ALL_OUTPUTS ${_TEX_DST})
        endif()
    endforeach()
endforeach()

# Custom target that triggers all asset processing
add_custom_target(41-scene-model-loading-assets DEPENDS ${_ALL_OUTPUTS})
add_dependencies(41-scene-model-loading 41-scene-model-loading-assets)

# ── Copy processed assets + fonts to the exe directory ───────────────────

# Copy fonts for UI
add_custom_command(TARGET 41-scene-model-loading POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/fonts
        $<TARGET_FILE_DIR:41-scene-model-loading>/assets/fonts
)

# Copy model assets (.fscene, .fmesh, .fmat, textures)
add_custom_command(TARGET 41-scene-model-loading POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:41-scene-model-loading>/assets
)
