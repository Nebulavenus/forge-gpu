add_executable(08-layout main.c)
target_include_directories(08-layout PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(08-layout PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET 08-layout POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:08-layout>
    )
endif()
