add_executable(forge_anim_tool main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c)
target_include_directories(forge_anim_tool PRIVATE
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/third_party/cJSON)
target_link_libraries(forge_anim_tool PRIVATE SDL3::SDL3)
if(NOT MSVC)
    target_link_libraries(forge_anim_tool PRIVATE m)
endif()
