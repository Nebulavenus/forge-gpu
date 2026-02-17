add_executable(10-basic-lighting WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(10-basic-lighting PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(10-basic-lighting PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(10-basic-lighting PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 10-basic-lighting POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:10-basic-lighting>
    )
endif()

# Copy Suzanne model from shared assets directory next to the executable
# so it can be loaded at runtime via SDL_GetBasePath() + relative path.
add_custom_command(TARGET 10-basic-lighting POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/Suzanne
        $<TARGET_FILE_DIR:10-basic-lighting>/assets/models/Suzanne
)
