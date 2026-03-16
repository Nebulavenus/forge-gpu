/*
 * Scene Transparency Sorting Tests (Groups 27–30)
 *
 * Tests for the transparency sorting additions in forge_scene.h:
 * - ForgeSceneTransparentDraw struct layout
 * - FORGE_SCENE_MAX_TRANSPARENT_DRAWS constant
 * - View-depth sort key computation (dot product vs radial distance)
 * - Centroid precompute bounds checking (calls real production code)
 *
 * Split from test_scene.c following the existing pattern.
 *
 * SPDX-License-Identifier: Zlib
 */

/* This file is #include'd from test_scene.c (not compiled separately)
 * because forge_scene.h uses static functions that must share a single
 * translation unit.  The split is purely for readability. */
#include "test_scene_common.h"

/* ══════════════════════════════════════════════════════════════════════════
 * Group 27: ForgeSceneTransparentDraw struct layout
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_transparent_draw_struct_has_sort_depth(void)
{
    TEST("ForgeSceneTransparentDraw has sort_depth field")
    ForgeSceneTransparentDraw td;
    SDL_memset(&td, 0, sizeof(td));
    td.sort_depth = 1.5f;
    ASSERT_NEAR(td.sort_depth, 1.5f, EPSILON);
    END_TEST();
}

static void test_transparent_draw_struct_has_final_world(void)
{
    TEST("ForgeSceneTransparentDraw has final_world field")
    ForgeSceneTransparentDraw td;
    SDL_memset(&td, 0, sizeof(td));
    td.final_world = mat4_identity();
    /* Diagonal should be 1 */
    ASSERT_NEAR(td.final_world.m[0], 1.0f, EPSILON);
    ASSERT_NEAR(td.final_world.m[5], 1.0f, EPSILON);
    ASSERT_NEAR(td.final_world.m[10], 1.0f, EPSILON);
    ASSERT_NEAR(td.final_world.m[15], 1.0f, EPSILON);
    END_TEST();
}

static void test_transparent_draw_zeroed_defaults(void)
{
    TEST("Zeroed ForgeSceneTransparentDraw has zero fields")
    ForgeSceneTransparentDraw td;
    SDL_memset(&td, 0, sizeof(td));
    ASSERT_INT_EQ((int)td.node_index, 0);
    ASSERT_INT_EQ((int)td.submesh_index, 0);
    ASSERT_NEAR(td.sort_depth, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Group 28: Transparency sorting constants
 * ══════════════════════════════════════════════════════════════════════════ */

#define EXPECTED_MAX_TRANSPARENT_DRAWS 256
#define EXPECTED_MAX_SUBMESHES         512

static void test_max_transparent_draws_constant(void)
{
    TEST("FORGE_SCENE_MAX_TRANSPARENT_DRAWS is 256")
    ASSERT_INT_EQ(FORGE_SCENE_MAX_TRANSPARENT_DRAWS,
                  EXPECTED_MAX_TRANSPARENT_DRAWS);
    END_TEST();
}

static void test_max_transparent_draws_fits_max_submeshes(void)
{
    TEST("MAX_TRANSPARENT_DRAWS <= MAX_SUBMESHES")
    ASSERT_TRUE(FORGE_SCENE_MAX_TRANSPARENT_DRAWS <=
                FORGE_SCENE_MODEL_MAX_SUBMESHES);
    END_TEST();
}

static void test_max_submeshes_constant(void)
{
    TEST("FORGE_SCENE_MODEL_MAX_SUBMESHES is 512")
    ASSERT_INT_EQ(FORGE_SCENE_MODEL_MAX_SUBMESHES,
                  EXPECTED_MAX_SUBMESHES);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Group 29: View-depth sort key math
 *
 * The sort key must be the projected depth along the camera forward axis
 * (vec3_dot), NOT radial Euclidean distance (vec3_length). This ensures
 * correct back-to-front ordering for off-axis transparent geometry.
 * ══════════════════════════════════════════════════════════════════════════ */

/* Named constants for sort-depth test geometry */
#define SORT_ON_AXIS_DEPTH       5.0f   /* distance along -Z for on-axis test  */
#define SORT_OFF_AXIS_LATERAL    5.0f   /* lateral offset for off-axis test     */
#define SORT_OFF_AXIS_FORWARD    5.0f   /* forward offset for off-axis test     */
#define SORT_RADIAL_MIN          7.0f   /* expected minimum radial for off-axis */
#define SORT_OBJ_A_DEPTH        10.0f   /* on-axis object A depth              */
#define SORT_OBJ_B_LATERAL      10.8f   /* off-axis object B lateral offset    */
#define SORT_OBJ_B_DEPTH         3.0f   /* off-axis object B forward offset    */

static void test_sort_depth_is_view_depth_not_radial(void)
{
    TEST("View-depth sort: on-axis objects match radial distance")
    /* Object directly in front of camera: both methods agree */
    vec3 cam_pos = vec3_create(0, 0, 0);
    quat cam_q = quat_from_euler(0.0f, 0.0f, 0.0f);
    vec3 cam_fwd = quat_forward(cam_q);
    vec3 obj_pos = vec3_create(0, 0, -SORT_ON_AXIS_DEPTH);

    float view_depth = vec3_dot(vec3_sub(obj_pos, cam_pos), cam_fwd);
    float radial = vec3_length(vec3_sub(obj_pos, cam_pos));

    /* For on-axis objects, view_depth magnitude equals radial distance */
    ASSERT_NEAR(SDL_fabsf(view_depth), radial, EPSILON);
    END_TEST();
}

static void test_sort_depth_off_axis_differs_from_radial(void)
{
    TEST("View-depth sort: off-axis objects differ from radial distance")
    /* Object to the side at same forward distance: view-depth < radial */
    vec3 cam_pos = vec3_create(0, 0, 0);
    quat cam_q = quat_from_euler(0.0f, 0.0f, 0.0f);
    vec3 cam_fwd = quat_forward(cam_q);
    vec3 obj_pos = vec3_create(SORT_OFF_AXIS_LATERAL, 0,
                                -SORT_OFF_AXIS_FORWARD);

    float view_depth = vec3_dot(vec3_sub(obj_pos, cam_pos), cam_fwd);
    float radial = vec3_length(vec3_sub(obj_pos, cam_pos));

    /* View depth equals forward component, radial is longer */
    ASSERT_NEAR(SDL_fabsf(view_depth), SORT_OFF_AXIS_FORWARD, EPSILON);
    ASSERT_TRUE(radial > SORT_RADIAL_MIN);
    ASSERT_TRUE(SDL_fabsf(view_depth) < radial);
    END_TEST();
}

static void test_sort_depth_ordering_off_axis(void)
{
    TEST("View-depth sort: off-axis ordering is correct")
    /* Two objects: A is farther on view-depth, B is farther radially.
     * Correct transparency sort should order by view-depth. */
    vec3 cam_pos = vec3_create(0, 0, 0);
    quat cam_q = quat_from_euler(0.0f, 0.0f, 0.0f);
    vec3 cam_fwd = quat_forward(cam_q);

    /* A: far ahead, on-axis */
    vec3 obj_a = vec3_create(0, 0, -SORT_OBJ_A_DEPTH);
    /* B: less far ahead, far off-axis */
    vec3 obj_b = vec3_create(SORT_OBJ_B_LATERAL, 0, -SORT_OBJ_B_DEPTH);

    float depth_a = vec3_dot(vec3_sub(obj_a, cam_pos), cam_fwd);
    float depth_b = vec3_dot(vec3_sub(obj_b, cam_pos), cam_fwd);
    float radial_a = vec3_length(vec3_sub(obj_a, cam_pos));
    float radial_b = vec3_length(vec3_sub(obj_b, cam_pos));

    /* By view-depth: A is farther (drawn first in back-to-front) */
    ASSERT_TRUE(SDL_fabsf(depth_a) > SDL_fabsf(depth_b));
    /* By radial: B is farther — wrong order for transparency! */
    ASSERT_TRUE(radial_b > radial_a);
    END_TEST();
}

static void test_sort_depth_camera_yaw_changes_forward(void)
{
    TEST("View-depth sort: camera yaw rotates the forward vector")
    /* Default forward is -Z. Yaw of -PI/2 should point forward to +X */
    float yaw = -FORGE_PI * 0.5f;
    quat cam_q = quat_from_euler(yaw, 0.0f, 0.0f);
    vec3 cam_fwd = quat_forward(cam_q);

    /* Forward should now be +X direction (positive, not just abs > 0.99) */
    ASSERT_TRUE(cam_fwd.x > 0.99f);
    ASSERT_NEAR(cam_fwd.y, 0.0f, EPSILON);
    ASSERT_NEAR(cam_fwd.z, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Group 30: Centroid precompute — calls the real production function
 *
 * All tests call forge_scene_compute_centroids() directly with synthetic
 * mesh data.  This exercises the exact same code path used by
 * forge_scene_load_model and forge_scene_load_skinned_model.
 * ══════════════════════════════════════════════════════════════════════════ */

/* Test mesh constants */
#define TEST_CENTROID_VERTEX_COUNT 3
#define TEST_CENTROID_STRIDE       48  /* FORGE_PIPELINE_VERTEX_STRIDE_TAN */
#define TEST_CENTROID_VTX1_X       3.0f
#define TEST_CENTROID_VTX2_Y       6.0f
#define TEST_EXPECTED_CX           1.0f  /* (0 + 3 + 0) / 3 */
#define TEST_EXPECTED_CY           2.0f  /* (0 + 0 + 6) / 3 */
#define TEST_CENTROID_UNDERSIZED_STRIDE  8   /* below 3 * sizeof(float) = 12 */
#define TEST_CENTROID_MISALIGNED_OFF     3   /* not aligned to sizeof(uint32_t) */

/* Vertex positions: (0,0,0), (3,0,0), (0,6,0) → centroid = (1,2,0) */
static float test_centroid_vertices[TEST_CENTROID_VERTEX_COUNT *
                                    (TEST_CENTROID_STRIDE / sizeof(float))];

static uint32_t test_centroid_indices[] = { 0, 1, 2 };

static void test_centroid_init_vertices(void)
{
    SDL_memset(test_centroid_vertices, 0, sizeof(test_centroid_vertices));
    uint32_t floats_per_vert = TEST_CENTROID_STRIDE / (uint32_t)sizeof(float);
    /* Vertex 0: position (0, 0, 0) — already zeroed */
    /* Vertex 1: position (3, 0, 0) */
    test_centroid_vertices[1 * floats_per_vert + 0] = TEST_CENTROID_VTX1_X;
    /* Vertex 2: position (0, 6, 0) */
    test_centroid_vertices[2 * floats_per_vert + 1] = TEST_CENTROID_VTX2_Y;
}

/* Helper: build a minimal mesh for centroid testing (no GPU needed). */
static void build_centroid_test_mesh(ForgePipelineMesh *mesh,
                                      ForgePipelineLod *lod,
                                      ForgePipelineSubmesh *sub)
{
    SDL_memset(mesh, 0, sizeof(*mesh));
    SDL_memset(lod, 0, sizeof(*lod));
    SDL_memset(sub, 0, sizeof(*sub));

    mesh->vertices = test_centroid_vertices;
    mesh->indices = test_centroid_indices;
    mesh->vertex_count = TEST_CENTROID_VERTEX_COUNT;
    mesh->vertex_stride = TEST_CENTROID_STRIDE;
    mesh->lod_count = 1;
    mesh->lods = lod;
    mesh->submesh_count = 1;
    mesh->submeshes = sub;

    lod->index_offset = 0;
    lod->index_count = 3;
    lod->target_error = 0.0f;

    sub->index_count = 3;
    sub->index_offset = 0;
    sub->material_index = 0;
}

static void test_centroid_valid_triangle(void)
{
    TEST("Centroid: valid triangle averages correctly")
    test_centroid_init_vertices();
    ForgePipelineMesh mesh;
    ForgePipelineLod lod;
    ForgePipelineSubmesh sub;
    build_centroid_test_mesh(&mesh, &lod, &sub);

    vec3 centroid;
    forge_scene_compute_centroids(&mesh, &centroid, 1);

    ASSERT_NEAR(centroid.x, TEST_EXPECTED_CX, EPSILON);
    ASSERT_NEAR(centroid.y, TEST_EXPECTED_CY, EPSILON);
    ASSERT_NEAR(centroid.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_centroid_oob_vertex_index_skipped(void)
{
    TEST("Centroid: OOB vertex index is skipped, valid ones averaged")
    test_centroid_init_vertices();

    /* Index buffer: vertex 1 at (3,0,0) is valid, 999 is OOB.
     * Use a non-zero vertex so the assertion distinguishes "skip OOB and
     * average valid" from "incorrectly fall back to zero". */
    uint32_t bad_indices[] = { 1, 999 };
    ForgePipelineMesh mesh;
    ForgePipelineLod lod;
    ForgePipelineSubmesh sub;
    build_centroid_test_mesh(&mesh, &lod, &sub);

    mesh.indices = bad_indices;
    sub.index_count = 2;
    lod.index_count = 2;

    vec3 centroid;
    forge_scene_compute_centroids(&mesh, &centroid, 1);

    /* Only vertex 1 (3,0,0) is valid — centroid should be (3,0,0) */
    ASSERT_NEAR(centroid.x, TEST_CENTROID_VTX1_X, EPSILON);
    ASSERT_NEAR(centroid.y, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_centroid_misaligned_offset_falls_back(void)
{
    TEST("Centroid: misaligned index_offset produces zero centroid")
    test_centroid_init_vertices();
    ForgePipelineMesh mesh;
    ForgePipelineLod lod;
    ForgePipelineSubmesh sub;
    build_centroid_test_mesh(&mesh, &lod, &sub);

    /* Set submesh offset to 3 (not aligned to sizeof(uint32_t)) */
    sub.index_offset = TEST_CENTROID_MISALIGNED_OFF;

    vec3 centroid;
    SDL_memset(&centroid, 0xFF, sizeof(centroid)); /* poison */
    forge_scene_compute_centroids(&mesh, &centroid, 1);

    ASSERT_NEAR(centroid.x, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.y, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.z, 0.0f, EPSILON);
    END_TEST();
}

#define TEST_SPAN_LOD0_COUNT 6

static void test_centroid_span_overflow_falls_back(void)
{
    TEST("Centroid: index span exceeding LOD count produces zero centroid")
    test_centroid_init_vertices();

    /* 6-index buffer, but submesh claims to start at index 4 with count 5
     * (4 + 5 = 9 > 6), which overflows the LOD 0 range. */
    uint32_t six_indices[] = { 0, 1, 2, 0, 1, 2 };
    ForgePipelineMesh mesh;
    ForgePipelineLod lod;
    ForgePipelineSubmesh sub;
    build_centroid_test_mesh(&mesh, &lod, &sub);

    mesh.indices = six_indices;
    lod.index_count = TEST_SPAN_LOD0_COUNT;
    sub.index_count = 5;
    sub.index_offset = 4 * (uint32_t)sizeof(uint32_t); /* byte offset 16 */

    vec3 centroid;
    SDL_memset(&centroid, 0xFF, sizeof(centroid)); /* poison */
    forge_scene_compute_centroids(&mesh, &centroid, 1);

    ASSERT_NEAR(centroid.x, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.y, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_centroid_all_oob_indices_produces_zero(void)
{
    TEST("Centroid: all OOB indices produce zero centroid")
    test_centroid_init_vertices();

    uint32_t all_bad[] = { 999, 1000, 2000 };
    ForgePipelineMesh mesh;
    ForgePipelineLod lod;
    ForgePipelineSubmesh sub;
    build_centroid_test_mesh(&mesh, &lod, &sub);

    mesh.indices = all_bad;

    vec3 centroid;
    SDL_memset(&centroid, 0xFF, sizeof(centroid)); /* poison */
    forge_scene_compute_centroids(&mesh, &centroid, 1);

    ASSERT_NEAR(centroid.x, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.y, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_centroid_nonzero_lod0_offset(void)
{
    TEST("Centroid: non-zero lod0_off rebases indices correctly")
    test_centroid_init_vertices();

    /* Prepend 2 dummy indices before the real data to simulate a
     * non-zero LOD 0 offset.  The function must rebase the indices
     * pointer by lod0_off so it reads the correct triangle. */
    uint32_t padded_indices[] = { 99, 99, 0, 1, 2 };
    uint32_t lod0_off = 2 * (uint32_t)sizeof(uint32_t); /* 8 bytes */

    ForgePipelineMesh mesh;
    ForgePipelineLod lod;
    ForgePipelineSubmesh sub;
    build_centroid_test_mesh(&mesh, &lod, &sub);

    mesh.indices = padded_indices;
    lod.index_offset = lod0_off;
    sub.index_offset = lod0_off; /* submesh starts at LOD 0 start */

    vec3 centroid;
    forge_scene_compute_centroids(&mesh, &centroid, 1);

    /* Same expected centroid as the valid triangle test: (1, 2, 0) */
    ASSERT_NEAR(centroid.x, TEST_EXPECTED_CX, EPSILON);
    ASSERT_NEAR(centroid.y, TEST_EXPECTED_CY, EPSILON);
    ASSERT_NEAR(centroid.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_centroid_null_mesh_is_safe(void)
{
    TEST("Centroid: NULL mesh does not crash or write output")
    vec3 centroid;
    SDL_memset(&centroid, 0xFF, sizeof(centroid));
    forge_scene_compute_centroids(NULL, &centroid, 1);
    /* Function should return without writing — sentinel bytes unchanged */
    unsigned char *bytes = (unsigned char *)&centroid;
    for (size_t i = 0; i < sizeof(centroid); i++) {
        ASSERT_TRUE(bytes[i] == 0xFF);
    }
    END_TEST();
}

static void test_centroid_null_output_is_safe(void)
{
    TEST("Centroid: NULL output does not crash")
    test_centroid_init_vertices();
    ForgePipelineMesh mesh;
    ForgePipelineLod lod;
    ForgePipelineSubmesh sub;
    build_centroid_test_mesh(&mesh, &lod, &sub);

    forge_scene_compute_centroids(&mesh, NULL, 1);
    ASSERT_TRUE(1);
    END_TEST();
}

static void test_centroid_undersized_stride_falls_back(void)
{
    TEST("Centroid: stride < 3 floats produces zero centroids")
    test_centroid_init_vertices();
    ForgePipelineMesh mesh;
    ForgePipelineLod lod;
    ForgePipelineSubmesh sub;
    build_centroid_test_mesh(&mesh, &lod, &sub);

    /* Set stride below minimum (3 * sizeof(float) = 12 bytes) */
    mesh.vertex_stride = TEST_CENTROID_UNDERSIZED_STRIDE;

    vec3 centroid;
    SDL_memset(&centroid, 0xFF, sizeof(centroid));
    forge_scene_compute_centroids(&mesh, &centroid, 1);

    ASSERT_NEAR(centroid.x, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.y, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_centroid_zero_stride_falls_back(void)
{
    TEST("Centroid: stride == 0 produces zero centroids")
    test_centroid_init_vertices();
    ForgePipelineMesh mesh;
    ForgePipelineLod lod;
    ForgePipelineSubmesh sub;
    build_centroid_test_mesh(&mesh, &lod, &sub);

    mesh.vertex_stride = 0;

    vec3 centroid;
    SDL_memset(&centroid, 0xFF, sizeof(centroid));
    forge_scene_compute_centroids(&mesh, &centroid, 1);

    ASSERT_NEAR(centroid.x, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.y, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_centroid_misaligned_lod0_offset_falls_back(void)
{
    TEST("Centroid: misaligned LOD 0 index_offset produces zero centroids")
    test_centroid_init_vertices();
    ForgePipelineMesh mesh;
    ForgePipelineLod lod;
    ForgePipelineSubmesh sub;
    build_centroid_test_mesh(&mesh, &lod, &sub);

    /* Set LOD 0 offset to 3 (not aligned to sizeof(uint32_t)) */
    lod.index_offset = TEST_CENTROID_MISALIGNED_OFF;

    vec3 centroid;
    SDL_memset(&centroid, 0xFF, sizeof(centroid));
    forge_scene_compute_centroids(&mesh, &centroid, 1);

    ASSERT_NEAR(centroid.x, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.y, 0.0f, EPSILON);
    ASSERT_NEAR(centroid.z, 0.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Runner (called from test_scene.c main)
 * ══════════════════════════════════════════════════════════════════════════ */

static void run_transparency_tests(void)
{
    /* ── Group 27: TransparentDraw struct ──────────────────────── */
    SDL_Log("--- 27. TransparentDraw Struct ---");
    test_transparent_draw_struct_has_sort_depth();
    test_transparent_draw_struct_has_final_world();
    test_transparent_draw_zeroed_defaults();

    /* ── Group 28: Transparency constants ─────────────────────── */
    SDL_Log("--- 28. Transparency Constants ---");
    test_max_transparent_draws_constant();
    test_max_transparent_draws_fits_max_submeshes();
    test_max_submeshes_constant();

    /* ── Group 29: View-depth sort key math ───────────────────── */
    SDL_Log("--- 29. View-Depth Sort Key Math ---");
    test_sort_depth_is_view_depth_not_radial();
    test_sort_depth_off_axis_differs_from_radial();
    test_sort_depth_ordering_off_axis();
    test_sort_depth_camera_yaw_changes_forward();

    /* ── Group 30: Centroid bounds checking (production code) ── */
    SDL_Log("--- 30. Centroid Bounds Checking ---");
    test_centroid_valid_triangle();
    test_centroid_oob_vertex_index_skipped();
    test_centroid_misaligned_offset_falls_back();
    test_centroid_span_overflow_falls_back();
    test_centroid_all_oob_indices_produces_zero();
    test_centroid_nonzero_lod0_offset();
    test_centroid_null_mesh_is_safe();
    test_centroid_null_output_is_safe();
    test_centroid_undersized_stride_falls_back();
    test_centroid_zero_stride_falls_back();
    test_centroid_misaligned_lod0_offset_falls_back();
}
