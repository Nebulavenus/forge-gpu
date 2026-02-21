add_executable(21-hdr-tone-mapping WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(21-hdr-tone-mapping PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(21-hdr-tone-mapping PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(21-hdr-tone-mapping PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 21-hdr-tone-mapping POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:21-hdr-tone-mapping>
    )
endif()

# Copy CesiumMilkTruck model files from shared assets
add_custom_command(TARGET 21-hdr-tone-mapping POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/CesiumMilkTruck
        $<TARGET_FILE_DIR:21-hdr-tone-mapping>/assets/models/CesiumMilkTruck
)

# Copy BoxTextured model files from shared assets
add_custom_command(TARGET 21-hdr-tone-mapping POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/BoxTextured
        $<TARGET_FILE_DIR:21-hdr-tone-mapping>/assets/models/BoxTextured
)
