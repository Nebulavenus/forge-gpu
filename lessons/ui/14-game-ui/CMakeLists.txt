add_executable(14-game-ui main.c)
target_include_directories(14-game-ui PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(14-game-ui PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 14-game-ui POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:14-game-ui>
    )
endif()
