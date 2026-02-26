# UI Lesson 04 -- Text Layout
# A console program that lays out text into positioned quads.
# No GPU or window required.

add_executable(ui-04-text-layout main.c)
target_include_directories(ui-04-text-layout PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(ui-04-text-layout PRIVATE SDL3::SDL3)

# Link math library on platforms that require it (Linux, etc.)
if(UNIX AND NOT APPLE)
    target_link_libraries(ui-04-text-layout PRIVATE m)
endif()

# Copy SDL3 shared library next to the executable (Windows needs this).
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET ui-04-text-layout POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:ui-04-text-layout>
    )
endif()
