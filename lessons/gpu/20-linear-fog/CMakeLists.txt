add_executable(20-linear-fog WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(20-linear-fog PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(20-linear-fog PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(20-linear-fog PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL next to executable (Windows)
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 20-linear-fog POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:20-linear-fog>
    )
endif()

# Copy CesiumMilkTruck model files from shared assets
add_custom_command(TARGET 20-linear-fog POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/CesiumMilkTruck
        $<TARGET_FILE_DIR:20-linear-fog>/assets/models/CesiumMilkTruck
)

# Copy BoxTextured model files from shared assets
add_custom_command(TARGET 20-linear-fog POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/BoxTextured
        $<TARGET_FILE_DIR:20-linear-fog>/assets/models/BoxTextured
)
