add_executable(test_audio test_audio.c)
target_include_directories(test_audio PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_audio PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_audio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_audio>
    )
endif()

# Add as a CTest test
add_test(NAME audio_library COMMAND test_audio)
