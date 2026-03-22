add_executable(47-texture-atlas-rendering WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(47-texture-atlas-rendering PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(47-texture-atlas-rendering PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(47-texture-atlas-rendering PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 47-texture-atlas-rendering POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:47-texture-atlas-rendering>
        VERBATIM
    )
endif()

# Copy font
add_custom_command(TARGET 47-texture-atlas-rendering POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/fonts/liberation_mono
        $<TARGET_FILE_DIR:47-texture-atlas-rendering>/assets/fonts/liberation_mono
    VERBATIM
)

# Copy atlas and individual material textures
add_custom_command(TARGET 47-texture-atlas-rendering POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/atlas.png
        $<TARGET_FILE_DIR:47-texture-atlas-rendering>/assets/atlas.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/atlas.json
        $<TARGET_FILE_DIR:47-texture-atlas-rendering>/assets/atlas.json
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/materials
        $<TARGET_FILE_DIR:47-texture-atlas-rendering>/assets/materials
    VERBATIM
)
