add_executable(07-text-input main.c)
target_include_directories(07-text-input PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(07-text-input PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 07-text-input POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:07-text-input>
    )
endif()
