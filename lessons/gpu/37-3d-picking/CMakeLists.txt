add_executable(37-3d-picking WIN32
    main.c
)
target_include_directories(37-3d-picking PRIVATE
    ${FORGE_COMMON_DIR}
)
target_link_libraries(37-3d-picking PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(37-3d-picking PRIVATE FORGE_CAPTURE)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 37-3d-picking POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:37-3d-picking>
        VERBATIM
    )
endif()
