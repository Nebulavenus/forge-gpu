add_executable(test_raster test_raster.c)
target_include_directories(test_raster PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_raster PRIVATE SDL3::SDL3)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_raster POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_raster>
    )
endif()

# Add as a CTest test
add_test(NAME raster COMMAND test_raster)
