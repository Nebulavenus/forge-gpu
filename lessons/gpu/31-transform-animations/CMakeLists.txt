add_executable(31-transform-animations WIN32
    main.c
    ${PROJECT_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(31-transform-animations PRIVATE
    ${FORGE_COMMON_DIR}
    ${PROJECT_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(31-transform-animations PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(31-transform-animations PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 31-transform-animations POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:31-transform-animations>
        VERBATIM
    )
endif()

# Copy asset directories next to the executable
foreach(asset_dir IN ITEMS CesiumMilkTruck track skybox)
    add_custom_command(TARGET 31-transform-animations POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/${asset_dir}
            $<TARGET_FILE_DIR:31-transform-animations>/assets/${asset_dir}
        VERBATIM
    )
endforeach()
