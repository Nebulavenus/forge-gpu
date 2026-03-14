add_executable(forge_texture_tool main.c)

target_include_directories(forge_texture_tool PRIVATE
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/third_party/basis_universal
    ${CMAKE_SOURCE_DIR}/tools/common)

target_link_libraries(forge_texture_tool PRIVATE
    SDL3::SDL3
    basisu_transcoder)
