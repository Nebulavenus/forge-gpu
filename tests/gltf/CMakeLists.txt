add_executable(test_gltf test_gltf.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c)
target_include_directories(test_gltf PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON)
target_link_libraries(test_gltf PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

# ForgeGltfScene is a large struct (~1.5MB) allocated on the stack in tests.
# MSVC default stack is 1MB — increase to 4MB to prevent stack overflow.
if(MSVC)
    target_link_options(test_gltf PRIVATE /STACK:4194304)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_gltf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_gltf>
    )
endif()

# Copy model assets next to test executable for integration tests
foreach(model_dir IN ITEMS CesiumMilkTruck CesiumMan)
    add_custom_command(TARGET test_gltf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${FORGE_ASSETS_DIR}/models/${model_dir}
            $<TARGET_FILE_DIR:test_gltf>/assets/${model_dir}
    )
endforeach()

# Add as a CTest test
add_test(NAME gltf_parser COMMAND test_gltf)
