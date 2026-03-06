add_executable(33-vertex-pulling WIN32
    main.c
    ${PROJECT_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(33-vertex-pulling PRIVATE
    ${FORGE_COMMON_DIR}
    ${PROJECT_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(33-vertex-pulling PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(33-vertex-pulling PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 33-vertex-pulling POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:33-vertex-pulling>
        VERBATIM
    )
endif()

# Copy CesiumMilkTruck assets next to the executable
add_custom_command(TARGET 33-vertex-pulling POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/CesiumMilkTruck
        $<TARGET_FILE_DIR:33-vertex-pulling>/assets/CesiumMilkTruck
    VERBATIM
)

# Copy BoxTextured assets next to the executable
add_custom_command(TARGET 33-vertex-pulling POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/BoxTextured
        $<TARGET_FILE_DIR:33-vertex-pulling>/assets/BoxTextured
    VERBATIM
)
