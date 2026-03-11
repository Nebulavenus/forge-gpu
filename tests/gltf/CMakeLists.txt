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

# ── Overflow & bounds tests (split from test_gltf for focused adversarial input testing)
add_executable(test_gltf_overflow test_gltf_overflow.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c)
target_include_directories(test_gltf_overflow PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON)
target_link_libraries(test_gltf_overflow PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(MSVC)
    target_link_options(test_gltf_overflow PRIVATE /STACK:4194304)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_gltf_overflow POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_gltf_overflow>
    )
endif()

add_test(NAME gltf_overflow COMMAND test_gltf_overflow)

# ── glTF fuzz harness ──────────────────────────────────────────────────────
# Builds as part of the test suite but runs separately (not via ctest) because
# it takes longer than unit tests.  Override iteration count at configure time:
#   cmake -DFORGE_FUZZ_ITERATIONS=1000000 ...
add_executable(fuzz_gltf fuzz_gltf.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c)
target_include_directories(fuzz_gltf PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON)
target_link_libraries(fuzz_gltf PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)
if(NOT DEFINED FORGE_FUZZ_ITERATIONS)
    set(FORGE_FUZZ_ITERATIONS 100000)
endif()
target_compile_definitions(fuzz_gltf PRIVATE
    FORGE_FUZZ_ITERATIONS=${FORGE_FUZZ_ITERATIONS})

if(MSVC)
    target_link_options(fuzz_gltf PRIVATE /STACK:4194304)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET fuzz_gltf POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:fuzz_gltf>
    )
endif()
