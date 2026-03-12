add_executable(test_scene test_scene.c)
target_include_directories(test_scene PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_scene PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

# Provide the path to a TTF font for UI-integration tests.
# The font must exist at build time — if missing, the font test is skipped.
set(FORGE_TEST_FONT_PATH "${CMAKE_SOURCE_DIR}/assets/fonts/liberation_mono/LiberationMono-Regular.ttf")
if(EXISTS "${FORGE_TEST_FONT_PATH}")
    target_compile_definitions(test_scene PRIVATE
        FONT_PATH="${FORGE_TEST_FONT_PATH}")
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_scene POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_scene>
    )
endif()

# Add as a CTest test
add_test(NAME scene_library COMMAND test_scene)
