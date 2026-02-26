# UI Lesson 02 -- Glyph Rasterization
# A console program that rasterizes TrueType glyph outlines into bitmaps.
# No GPU or window required.

add_executable(ui-02-glyph-rasterization main.c)
target_include_directories(ui-02-glyph-rasterization PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(ui-02-glyph-rasterization PRIVATE SDL3::SDL3)

# Link math library on platforms that require it (Linux, etc.)
# The rasterizer uses sqrt, ceil, floor via SDL wrapper functions.
if(UNIX AND NOT APPLE)
    target_link_libraries(ui-02-glyph-rasterization PRIVATE m)
endif()

# Copy SDL3 shared library next to the executable (Windows needs this).
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET ui-02-glyph-rasterization POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:ui-02-glyph-rasterization>
    )
endif()
