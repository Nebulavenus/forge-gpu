add_executable(18-blinn-phong-materials WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(18-blinn-phong-materials PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(18-blinn-phong-materials PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(18-blinn-phong-materials PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 DLL next to executable (Windows)
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 18-blinn-phong-materials POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:18-blinn-phong-materials>
    )
endif()

# Copy Suzanne model from shared assets
add_custom_command(TARGET 18-blinn-phong-materials POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:18-blinn-phong-materials>/assets/models/Suzanne
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FORGE_ASSETS_DIR}/models/Suzanne/Suzanne.gltf
        $<TARGET_FILE_DIR:18-blinn-phong-materials>/assets/models/Suzanne/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FORGE_ASSETS_DIR}/models/Suzanne/Suzanne.bin
        $<TARGET_FILE_DIR:18-blinn-phong-materials>/assets/models/Suzanne/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FORGE_ASSETS_DIR}/models/Suzanne/Suzanne_BaseColor.png
        $<TARGET_FILE_DIR:18-blinn-phong-materials>/assets/models/Suzanne/
)
