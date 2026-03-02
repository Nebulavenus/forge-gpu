add_executable(30-planar-reflections WIN32
    main.c
    ${PROJECT_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(30-planar-reflections PRIVATE
    ${FORGE_COMMON_DIR}
    ${PROJECT_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(30-planar-reflections PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(30-planar-reflections PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 30-planar-reflections POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:30-planar-reflections>
        VERBATIM
    )
endif()

# Copy asset directories (boat model, rock cliffs, skybox cubemap faces)
foreach(asset_dir IN ITEMS boat rocks skybox)
    add_custom_command(TARGET 30-planar-reflections POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/${asset_dir}
            $<TARGET_FILE_DIR:30-planar-reflections>/assets/${asset_dir}
        VERBATIM
    )
endforeach()
