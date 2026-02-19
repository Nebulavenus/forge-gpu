add_executable(16-blending WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(16-blending PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(16-blending PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(16-blending PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 16-blending POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:16-blending>
    )
endif()

# Copy all model assets to the build output directory.
# The TransmissionOrderTest glTF model requires its .gltf, .bin, and
# texture files to be colocated in the same assets/ directory.
add_custom_command(TARGET 16-blending POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:16-blending>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/TransmissionOrderTest.gltf
        $<TARGET_FILE_DIR:16-blending>/assets/TransmissionOrderTest.gltf
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/TransmissionOrderTest.bin
        $<TARGET_FILE_DIR:16-blending>/assets/TransmissionOrderTest.bin
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/alphaInACircle.png
        $<TARGET_FILE_DIR:16-blending>/assets/alphaInACircle.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/checkerboard.png
        $<TARGET_FILE_DIR:16-blending>/assets/checkerboard.png
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/BlendMaskOpaqueLabels.png
        $<TARGET_FILE_DIR:16-blending>/assets/BlendMaskOpaqueLabels.png
)
