add_executable(test_physics test_physics.c test_physics_rbc.c test_physics_shapes.c test_physics_sap.c test_physics_gjk.c test_physics_epa.c test_physics_production.c test_physics_manifold.c test_physics_si.c test_physics_joints.c test_physics_world.c)
target_include_directories(test_physics PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_physics PRIVATE SDL3::SDL3 $<$<NOT:$<C_COMPILER_ID:MSVC>>:m>)

if(TARGET SDL3::SDL3-shared)
    add_custom_command(TARGET test_physics POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:test_physics>
    )
endif()

# Add as a CTest test
add_test(NAME physics_library COMMAND test_physics)
