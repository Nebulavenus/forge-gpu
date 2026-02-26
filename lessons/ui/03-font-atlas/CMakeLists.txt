# UI Lesson 03 -- Font Atlas Packing
# A console program that builds a font atlas from rasterized glyphs.
# No GPU or window required.

add_executable(ui-03-font-atlas main.c)
target_include_directories(ui-03-font-atlas PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(ui-03-font-atlas PRIVATE SDL3::SDL3)

# Link math library on platforms that require it (Linux, etc.)
# The rasterizer uses sqrt, ceil, floor via SDL wrapper functions.
if(UNIX AND NOT APPLE)
    target_link_libraries(ui-03-font-atlas PRIVATE m)
endif()

# Copy SDL3 shared library next to the executable (Windows needs this).
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET ui-03-font-atlas POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:ui-03-font-atlas>
    )
endif()
