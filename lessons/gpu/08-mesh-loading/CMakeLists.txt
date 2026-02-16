add_executable(08-mesh-loading WIN32 main.c)
target_include_directories(08-mesh-loading PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(08-mesh-loading PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(08-mesh-loading PRIVATE FORGE_CAPTURE)
endif()

# Copy SDL3 shared library next to the executable
if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 08-mesh-loading POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:08-mesh-loading>
    )
endif()

# Copy model files (OBJ + texture) next to the executable so they can be
# loaded at runtime via SDL_GetBasePath() + relative path.
add_custom_command(TARGET 08-mesh-loading POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:08-mesh-loading>/models/space-shuttle
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/space-shuttle/space-shuttle.obj
        $<TARGET_FILE_DIR:08-mesh-loading>/models/space-shuttle/space-shuttle.obj
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/models/space-shuttle/ShuttleDiffuseMap.png
        $<TARGET_FILE_DIR:08-mesh-loading>/models/space-shuttle/ShuttleDiffuseMap.png
)
