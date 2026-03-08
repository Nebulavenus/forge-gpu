add_executable(36-edge-detection WIN32
    main.c
)
target_include_directories(36-edge-detection PRIVATE
    ${FORGE_COMMON_DIR}
)
target_link_libraries(36-edge-detection PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(36-edge-detection PRIVATE FORGE_CAPTURE)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 36-edge-detection POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:36-edge-detection>
        VERBATIM
    )
endif()
