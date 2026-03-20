add_executable(46-particle-animations WIN32 main.c)
target_include_directories(46-particle-animations PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(46-particle-animations PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(FORGE_CAPTURE)
    target_compile_definitions(46-particle-animations PRIVATE FORGE_CAPTURE)
endif()

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 46-particle-animations POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:46-particle-animations>
        VERBATIM
    )
endif()

add_custom_command(TARGET 46-particle-animations POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${FORGE_ASSETS_DIR}/fonts
        $<TARGET_FILE_DIR:46-particle-animations>/assets/fonts
)
