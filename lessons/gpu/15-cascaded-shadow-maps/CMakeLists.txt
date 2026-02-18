add_executable(15-cascaded-shadow-maps WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(15-cascaded-shadow-maps PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(15-cascaded-shadow-maps PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(15-cascaded-shadow-maps PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 15-cascaded-shadow-maps POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:15-cascaded-shadow-maps>
    )
endif()

# Copy CesiumMilkTruck model files from shared assets
add_custom_command(TARGET 15-cascaded-shadow-maps POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/CesiumMilkTruck
        $<TARGET_FILE_DIR:15-cascaded-shadow-maps>/assets/models/CesiumMilkTruck
)

# Copy BoxTextured model files from shared assets
add_custom_command(TARGET 15-cascaded-shadow-maps POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/BoxTextured
        $<TARGET_FILE_DIR:15-cascaded-shadow-maps>/assets/models/BoxTextured
)
