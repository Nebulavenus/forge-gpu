add_executable(forge_texture_tool main.c)

target_include_directories(forge_texture_tool PRIVATE
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/third_party/basis_universal
    ${CMAKE_SOURCE_DIR}/tools/common)

target_link_libraries(forge_texture_tool PRIVATE
    SDL3::SDL3
    basisu_transcoder)

# Copy SDL3.dll next to the executable so Windows can find it at runtime.
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET forge_texture_tool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:forge_texture_tool>
    )
endif()
