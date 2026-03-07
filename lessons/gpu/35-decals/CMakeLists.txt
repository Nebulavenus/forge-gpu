add_executable(35-decals WIN32
    main.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(35-decals PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON
)
target_link_libraries(35-decals PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(35-decals PRIVATE FORGE_CAPTURE)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 35-decals POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:35-decals>
        VERBATIM
    )
endif()

# Copy Suzanne model files next to executable
add_custom_command(TARGET 35-decals POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/Suzanne
        $<TARGET_FILE_DIR:35-decals>/assets/models/Suzanne
)
