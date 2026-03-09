add_executable(39-pipeline-processed-assets WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(39-pipeline-processed-assets PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(39-pipeline-processed-assets PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(39-pipeline-processed-assets PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 39-pipeline-processed-assets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:39-pipeline-processed-assets>
        VERBATIM
    )
endif()

# Copy raw WaterBottle glTF assets (for the raw/legacy loading path)
add_custom_command(TARGET 39-pipeline-processed-assets POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/WaterBottle
        $<TARGET_FILE_DIR:39-pipeline-processed-assets>/assets/WaterBottle
)

# Copy pipeline-processed assets (.fmesh + textures + .meta.json)
add_custom_command(TARGET 39-pipeline-processed-assets POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/processed
        $<TARGET_FILE_DIR:39-pipeline-processed-assets>/assets/processed
)

# Copy BoxTextured raw assets (for the raw/legacy loading path)
add_custom_command(TARGET 39-pipeline-processed-assets POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/BoxTextured
        $<TARGET_FILE_DIR:39-pipeline-processed-assets>/assets/models/BoxTextured
)
