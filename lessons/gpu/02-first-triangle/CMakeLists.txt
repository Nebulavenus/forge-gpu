add_executable(02-first-triangle WIN32 main.c)
target_include_directories(02-first-triangle PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(02-first-triangle PRIVATE SDL3::SDL3)

if(FORGE_CAPTURE)
    target_compile_definitions(02-first-triangle PRIVATE FORGE_CAPTURE)
endif()

add_custom_command(TARGET 02-first-triangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:02-first-triangle>
)
