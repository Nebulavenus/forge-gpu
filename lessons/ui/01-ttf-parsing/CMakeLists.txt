# UI Lesson 01 -- TTF Parsing
# A console program that parses a TrueType font file and prints its internal
# structure.  No GPU or window required.

add_executable(ui-01-ttf-parsing main.c)
target_include_directories(ui-01-ttf-parsing PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(ui-01-ttf-parsing PRIVATE SDL3::SDL3)

# Copy SDL3 shared library next to the executable (Windows needs this).
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET ui-01-ttf-parsing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:ui-01-ttf-parsing>
    )
endif()
