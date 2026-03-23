add_executable(test_pipeline test_pipeline.c
    ${CMAKE_SOURCE_DIR}/third_party/cJSON/cJSON.c)
target_include_directories(test_pipeline PRIVATE
    ${FORGE_COMMON_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/cJSON)
target_link_libraries(test_pipeline PRIVATE SDL3::SDL3
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)
target_compile_definitions(test_pipeline PRIVATE
    FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/")

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_pipeline POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_pipeline>
    )
endif()

add_test(NAME pipeline_library COMMAND test_pipeline)
