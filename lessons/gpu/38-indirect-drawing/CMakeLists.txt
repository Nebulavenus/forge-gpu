add_executable(38-indirect-drawing WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(38-indirect-drawing PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(38-indirect-drawing PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(38-indirect-drawing PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 38-indirect-drawing POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:38-indirect-drawing>
        VERBATIM
    )
endif()

# Copy glTF models from shared assets directory next to the
# executable so they can be loaded at runtime via SDL_GetBasePath().
add_custom_command(TARGET 38-indirect-drawing POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/BoxTextured
        $<TARGET_FILE_DIR:38-indirect-drawing>/assets/models/BoxTextured
)
add_custom_command(TARGET 38-indirect-drawing POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/CesiumMilkTruck
        $<TARGET_FILE_DIR:38-indirect-drawing>/assets/models/CesiumMilkTruck
)
