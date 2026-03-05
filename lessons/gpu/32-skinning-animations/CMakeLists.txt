add_executable(32-skinning-animations WIN32
    main.c
    ${PROJECT_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(32-skinning-animations PRIVATE
    ${FORGE_COMMON_DIR}
    ${PROJECT_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(32-skinning-animations PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(32-skinning-animations PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 32-skinning-animations POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:32-skinning-animations>
        VERBATIM
    )
endif()

# Copy CesiumMan assets next to the executable
add_custom_command(TARGET 32-skinning-animations POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/CesiumMan
        $<TARGET_FILE_DIR:32-skinning-animations>/assets/CesiumMan
    VERBATIM
)
