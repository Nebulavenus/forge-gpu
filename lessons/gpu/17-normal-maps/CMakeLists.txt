add_executable(17-normal-maps WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(17-normal-maps PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(17-normal-maps PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(17-normal-maps PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 17-normal-maps POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:17-normal-maps>
    )
endif()

# Copy all model assets to the build output directory.
# The NormalTangentMirrorTest glTF model requires its .gltf, .bin, and
# texture files to be colocated in the same assets/ directory.
add_custom_command(TARGET 17-normal-maps POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:17-normal-maps>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/NormalTangentMirrorTest.gltf
        $<TARGET_FILE_DIR:17-normal-maps>/assets/NormalTangentMirrorTest.gltf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/NormalTangentMirrorTest.bin
        $<TARGET_FILE_DIR:17-normal-maps>/assets/NormalTangentMirrorTest.bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/NormalTangentMirrorTest_BaseColor.png
        $<TARGET_FILE_DIR:17-normal-maps>/assets/NormalTangentMirrorTest_BaseColor.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/NormalTangentMirrorTest_Normal.png
        $<TARGET_FILE_DIR:17-normal-maps>/assets/NormalTangentMirrorTest_Normal.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/NormalTangentMirrorTest_OcclusionRoughnessMetallic.png
        $<TARGET_FILE_DIR:17-normal-maps>/assets/NormalTangentMirrorTest_OcclusionRoughnessMetallic.png
)
