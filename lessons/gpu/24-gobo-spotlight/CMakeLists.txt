add_executable(24-gobo-spotlight WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(24-gobo-spotlight PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(24-gobo-spotlight PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(24-gobo-spotlight PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 24-gobo-spotlight POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:24-gobo-spotlight>
    )
endif()

# Copy CesiumMilkTruck model files from shared assets
add_custom_command(TARGET 24-gobo-spotlight POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/CesiumMilkTruck
        $<TARGET_FILE_DIR:24-gobo-spotlight>/assets/models/CesiumMilkTruck
)

# Copy BoxTextured model files from shared assets
add_custom_command(TARGET 24-gobo-spotlight POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/BoxTextured
        $<TARGET_FILE_DIR:24-gobo-spotlight>/assets/models/BoxTextured
)

# Copy Searchlight model from lesson-local assets
add_custom_command(TARGET 24-gobo-spotlight POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Searchlight
        $<TARGET_FILE_DIR:24-gobo-spotlight>/assets/models/Searchlight
)

# Copy gobo texture from lesson-local assets
add_custom_command(TARGET 24-gobo-spotlight POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/gobo_window.png
        $<TARGET_FILE_DIR:24-gobo-spotlight>/assets/gobo_window.png
)
