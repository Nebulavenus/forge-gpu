add_executable(forge_scene_tool main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c)
target_include_directories(forge_scene_tool PRIVATE
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/cJSON)
target_link_libraries(forge_scene_tool PRIVATE SDL3::SDL3)
if(NOT MSVC)
    target_link_libraries(forge_scene_tool PRIVATE m)
endif()

# Copy SDL3.dll next to the executable so Windows can find it at runtime.
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET forge_scene_tool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:forge_scene_tool>
    )
endif()
