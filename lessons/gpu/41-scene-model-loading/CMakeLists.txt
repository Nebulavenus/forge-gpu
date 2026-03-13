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
