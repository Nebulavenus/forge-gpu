add_executable(test_ui test_ui.c)
target_include_directories(test_ui PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_ui PRIVATE SDL3::SDL3)

# Link math library on platforms that require it (Linux, etc.)
if(UNIX AND NOT APPLE)
    target_link_libraries(test_ui PRIVATE m)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_ui>
    )
endif()

# Copy test font next to executable so relative path resolves
add_custom_command(TARGET test_ui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:test_ui>/assets/fonts/liberation_mono
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FORGE_ASSETS_DIR}/fonts/liberation_mono/LiberationMono-Regular.ttf
        $<TARGET_FILE_DIR:test_ui>/assets/fonts/liberation_mono/LiberationMono-Regular.ttf
)

# Add as a CTest test
add_test(NAME ui_ttf_parser COMMAND test_ui)

# ── UI Context tests (forge_ui_ctx.h) ──────────────────────────────────────
add_executable(test_ui_ctx test_ui_ctx.c)
target_include_directories(test_ui_ctx PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_ui_ctx PRIVATE SDL3::SDL3)

if(UNIX AND NOT APPLE)
    target_link_libraries(test_ui_ctx PRIVATE m)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_ui_ctx POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_ui_ctx>
    )
endif()

# Copy test font next to executable so relative path resolves
add_custom_command(TARGET test_ui_ctx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:test_ui_ctx>/assets/fonts/liberation_mono
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FORGE_ASSETS_DIR}/fonts/liberation_mono/LiberationMono-Regular.ttf
        $<TARGET_FILE_DIR:test_ui_ctx>/assets/fonts/liberation_mono/LiberationMono-Regular.ttf
)

add_test(NAME ui_ctx COMMAND test_ui_ctx)

# ── UI Window tests (forge_ui_window.h) ──────────────────────────────────────
add_executable(test_ui_window test_ui_window.c)
target_include_directories(test_ui_window PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_ui_window PRIVATE SDL3::SDL3)

if(UNIX AND NOT APPLE)
    target_link_libraries(test_ui_window PRIVATE m)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_ui_window POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_ui_window>
    )
endif()

# Copy test font next to executable so relative path resolves
add_custom_command(TARGET test_ui_window POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:test_ui_window>/assets/fonts/liberation_mono
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${FORGE_ASSETS_DIR}/fonts/liberation_mono/LiberationMono-Regular.ttf
        $<TARGET_FILE_DIR:test_ui_window>/assets/fonts/liberation_mono/LiberationMono-Regular.ttf
)

add_test(NAME ui_window COMMAND test_ui_window)

# ── Text input fuzz harness ──────────────────────────────────────────────────
# Builds as part of the test suite but runs separately (not via ctest) because
# it takes longer than unit tests.  Override iteration count at configure time:
#   cmake -DFORGE_FUZZ_ITERATIONS=1000000 ...
add_executable(fuzz_text_input fuzz_text_input.c)
target_include_directories(fuzz_text_input PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(fuzz_text_input PRIVATE SDL3::SDL3)
target_compile_definitions(fuzz_text_input PRIVATE
    FORGE_FUZZ_ITERATIONS=${FORGE_FUZZ_ITERATIONS})

if(UNIX AND NOT APPLE)
    target_link_libraries(fuzz_text_input PRIVATE m)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET fuzz_text_input POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:fuzz_text_input>
    )
endif()
