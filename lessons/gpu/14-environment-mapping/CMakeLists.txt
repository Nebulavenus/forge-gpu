add_executable(14-environment-mapping WIN32 main.c)
target_include_directories(14-environment-mapping PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(14-environment-mapping PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(14-environment-mapping PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 14-environment-mapping POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:14-environment-mapping>
    )
endif()

# Copy space shuttle model files (OBJ + texture) from shared assets
add_custom_command(TARGET 14-environment-mapping POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/models/space-shuttle
        $<TARGET_FILE_DIR:14-environment-mapping>/assets/models/space-shuttle
)

# Copy skybox cube map face PNGs from shared assets
add_custom_command(TARGET 14-environment-mapping POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/skyboxes/milkyway
        $<TARGET_FILE_DIR:14-environment-mapping>/assets/skyboxes/milkyway
)
