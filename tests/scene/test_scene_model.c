/*
 * Scene Model Tests (Groups 10–17)
 *
 * Tests for ForgeSceneModel: struct layout, defaults, transform composition,
 * NULL safety, free safety, file error handling, and constants.
 *
 * Split from test_scene.c to keep individual files manageable.
 *
 * SPDX-License-Identifier: Zlib
 */

/* This file is #include'd from test_scene.c (not compiled separately)
 * because forge_scene.h uses static functions that must share a single
 * translation unit.  The split is purely for readability — keeping the
 * model tests in their own file. */
#include "test_scene_common.h"

/* ══════════════════════════════════════════════════════════════════════════
 * Group 10: ModelVertex struct layout
 * ══════════════════════════════════════════════════════════════════════════ */

/* Expected layout constants — update here when the structs change */
#define EXPECTED_MODEL_VERTEX_SIZE        48
#define EXPECTED_MODEL_VERTEX_POS_OFF      0
#define EXPECTED_MODEL_VERTEX_NORMAL_OFF  12
#define EXPECTED_MODEL_VERTEX_UV_OFF      24
#define EXPECTED_MODEL_VERTEX_TANGENT_OFF 32

#define EXPECTED_MODEL_FRAG_SIZE              96
#define EXPECTED_MODEL_FRAG_LIGHT_DIR_OFF      0
#define EXPECTED_MODEL_FRAG_BASE_COLOR_OFF    32
#define EXPECTED_MODEL_FRAG_SHININESS_OFF     80
#define EXPECTED_MODEL_FRAG_AMBIENT_OFF       92

static void test_model_vertex_size(void)
{
    TEST("ForgeSceneModelVertex size is 48 bytes")
    ASSERT_INT_EQ((int)sizeof(ForgeSceneModelVertex), EXPECTED_MODEL_VERTEX_SIZE);
    END_TEST();
}

static void test_model_vertex_position_offset(void)
{
    TEST("ModelVertex position at offset 0")
    ASSERT_INT_EQ((int)offsetof(ForgeSceneModelVertex, position), EXPECTED_MODEL_VERTEX_POS_OFF);
    END_TEST();
}

static void test_model_vertex_normal_offset(void)
{
    TEST("ModelVertex normal at offset 12")
    ASSERT_INT_EQ((int)offsetof(ForgeSceneModelVertex, normal), EXPECTED_MODEL_VERTEX_NORMAL_OFF);
    END_TEST();
}

static void test_model_vertex_uv_offset(void)
{
    TEST("ModelVertex uv at offset 24")
    ASSERT_INT_EQ((int)offsetof(ForgeSceneModelVertex, uv), EXPECTED_MODEL_VERTEX_UV_OFF);
    END_TEST();
}

static void test_model_vertex_tangent_offset(void)
{
    TEST("ModelVertex tangent at offset 32")
    ASSERT_INT_EQ((int)offsetof(ForgeSceneModelVertex, tangent), EXPECTED_MODEL_VERTEX_TANGENT_OFF);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Group 11: ModelFragUniforms layout
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_model_frag_uniforms_size(void)
{
    TEST("ForgeSceneModelFragUniforms size is 96 bytes")
    ASSERT_INT_EQ((int)sizeof(ForgeSceneModelFragUniforms), EXPECTED_MODEL_FRAG_SIZE);
    END_TEST();
}

static void test_model_frag_uniforms_light_dir_offset(void)
{
    TEST("ModelFragUniforms light_dir at offset 0")
    ASSERT_INT_EQ((int)offsetof(ForgeSceneModelFragUniforms, light_dir), EXPECTED_MODEL_FRAG_LIGHT_DIR_OFF);
    END_TEST();
}

static void test_model_frag_uniforms_base_color_offset(void)
{
    TEST("ModelFragUniforms base_color_factor at offset 32")
    ASSERT_INT_EQ((int)offsetof(ForgeSceneModelFragUniforms, base_color_factor), EXPECTED_MODEL_FRAG_BASE_COLOR_OFF);
    END_TEST();
}

static void test_model_frag_uniforms_shininess_offset(void)
{
    TEST("ModelFragUniforms shininess at offset 80")
    ASSERT_INT_EQ((int)offsetof(ForgeSceneModelFragUniforms, shininess), EXPECTED_MODEL_FRAG_SHININESS_OFF);
    END_TEST();
}

static void test_model_frag_uniforms_ambient_offset(void)
{
    TEST("ModelFragUniforms ambient at offset 92")
    ASSERT_INT_EQ((int)offsetof(ForgeSceneModelFragUniforms, ambient), EXPECTED_MODEL_FRAG_AMBIENT_OFF);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Group 12: ForgeSceneModel defaults
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_model_zeroed_has_null_buffers(void)
{
    TEST("Zeroed model has NULL buffers")
    ForgeSceneModel model;
    SDL_memset(&model, 0, sizeof(model));
    ASSERT_TRUE(model.vertex_buffer == NULL);
    ASSERT_TRUE(model.index_buffer == NULL);
    ASSERT_INT_EQ((int)model.draw_calls, 0);
    END_TEST();
}

static void test_model_zeroed_has_empty_pipeline(void)
{
    TEST("Zeroed model has empty pipeline structs")
    ForgeSceneModel model;
    SDL_memset(&model, 0, sizeof(model));
    ASSERT_INT_EQ((int)model.scene_data.node_count, 0);
    ASSERT_INT_EQ((int)model.mesh.vertex_count, 0);
    ASSERT_INT_EQ((int)model.materials.material_count, 0);
    END_TEST();
}

static void test_model_zeroed_has_zero_mat_count(void)
{
    TEST("Zeroed model has zero mat_texture_count")
    ForgeSceneModel model;
    SDL_memset(&model, 0, sizeof(model));
    ASSERT_INT_EQ((int)model.mat_texture_count, 0);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Group 13: Transform composition
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_transform_identity_times_identity(void)
{
    TEST("Identity × identity = identity")
    mat4 a = mat4_identity();
    mat4 b = mat4_identity();
    mat4 c = mat4_multiply(a, b);
    /* Column-major flat: m[col*4+row], diagonal at 0,5,10,15 */
    for (int i = 0; i < 16; i++) {
        float expected = (i == 0 || i == 5 || i == 10 || i == 15) ? 1.0f : 0.0f;
        ASSERT_NEAR(c.m[i], expected, EPSILON);
    }
    END_TEST();
}

/* Transform test constants */
#define PLACEMENT_TX  5.0f   /* placement translation X */
#define NODE_TY       3.0f   /* node translation Y      */
#define SCALE_A       2.0f   /* first scale factor       */
#define SCALE_B       3.0f   /* second scale factor      */
#define SCALE_AB      6.0f   /* SCALE_A * SCALE_B        */
#define COLMAJ_TX     1.0f   /* column-major test X      */
#define COLMAJ_TY     2.0f   /* column-major test Y      */
#define COLMAJ_TZ     3.0f   /* column-major test Z      */

static void test_transform_placement_translate(void)
{
    TEST("Placement translate × node world = translated result")
    mat4 placement = mat4_translate(vec3_create(PLACEMENT_TX, 0.0f, 0.0f));
    mat4 node_world = mat4_translate(vec3_create(0.0f, NODE_TY, 0.0f));
    mat4 result = mat4_multiply(placement, node_world);
    /* Column-major: translation in col 3 at indices 12,13,14 */
    ASSERT_NEAR(result.m[12], PLACEMENT_TX, EPSILON);
    ASSERT_NEAR(result.m[13], NODE_TY, EPSILON);
    ASSERT_NEAR(result.m[14], 0.0f, EPSILON);
    END_TEST();
}

static void test_transform_scale_composes(void)
{
    TEST("Scale composes correctly")
    mat4 a = mat4_scale(vec3_create(SCALE_A, SCALE_A, SCALE_A));
    mat4 b = mat4_scale(vec3_create(SCALE_B, SCALE_B, SCALE_B));
    mat4 c = mat4_multiply(a, b);
    /* Diagonal: indices 0, 5, 10 */
    ASSERT_NEAR(c.m[0], SCALE_AB, EPSILON);
    ASSERT_NEAR(c.m[5], SCALE_AB, EPSILON);
    ASSERT_NEAR(c.m[10], SCALE_AB, EPSILON);
    END_TEST();
}

static void test_transform_column_major_order(void)
{
    TEST("Column-major: translation in column 3")
    mat4 t = mat4_translate(vec3_create(COLMAJ_TX, COLMAJ_TY, COLMAJ_TZ));
    /* Column-major flat: translation at indices 12, 13, 14 */
    ASSERT_NEAR(t.m[12], COLMAJ_TX, EPSILON);
    ASSERT_NEAR(t.m[13], COLMAJ_TY, EPSILON);
    ASSERT_NEAR(t.m[14], COLMAJ_TZ, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Group 14: NULL safety for model functions
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_load_model_null_scene(void)
{
    TEST("load_model NULL scene returns false")
    ForgeSceneModel model;
    bool ok = forge_scene_load_model(NULL, &model, "a", "b", "c", "d");
    ASSERT_TRUE(!ok);
    END_TEST();
}

static void test_load_model_null_model(void)
{
    TEST("load_model NULL model returns false")
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    bool ok = forge_scene_load_model(&scene, NULL, "a", "b", "c", "d");
    ASSERT_TRUE(!ok);
    END_TEST();
}

static void test_load_model_null_paths(void)
{
    TEST("load_model NULL path returns false")
    ForgeScene scene;
    ForgeSceneModel model;
    SDL_memset(&scene, 0, sizeof(scene));
    bool ok = forge_scene_load_model(&scene, &model, NULL, "b", "c", "d");
    ASSERT_TRUE(!ok);
    END_TEST();
}

static void test_draw_model_null_pass_safe(void)
{
    TEST("draw_model with NULL pass is safe")
    ForgeScene scene;
    ForgeSceneModel model;
    SDL_memset(&scene, 0, sizeof(scene));
    SDL_memset(&model, 0, sizeof(model));
    forge_scene_draw_model(&scene, &model, mat4_identity());
    ASSERT_TRUE(1);
    END_TEST();
}

static void test_draw_model_shadows_null_pass_safe(void)
{
    TEST("draw_model_shadows with NULL pass is safe")
    ForgeScene scene;
    ForgeSceneModel model;
    SDL_memset(&scene, 0, sizeof(scene));
    SDL_memset(&model, 0, sizeof(model));
    forge_scene_draw_model_shadows(&scene, &model, mat4_identity());
    ASSERT_TRUE(1);
    END_TEST();
}

static void test_draw_model_null_buffers_safe(void)
{
    TEST("draw_model with NULL vertex_buffer is safe")
    ForgeScene scene;
    ForgeSceneModel model;
    SDL_memset(&scene, 0, sizeof(scene));
    SDL_memset(&model, 0, sizeof(model));
    /* Set non-NULL pass so draw_model reaches the vb/ib null-safety guard
     * instead of returning early on the pass check. */
    scene.pass = (SDL_GPURenderPass *)(uintptr_t)1;
    forge_scene_draw_model(&scene, &model, mat4_identity());
    ASSERT_TRUE(1);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Group 15: Free safety
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_free_model_null_safe(void)
{
    TEST("free_model with NULL model is safe")
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    forge_scene_free_model(&scene, NULL);
    ASSERT_TRUE(1);
    END_TEST();
}

static void test_free_model_zeroed_safe(void)
{
    TEST("free_model on zeroed model is safe")
    ForgeScene scene;
    ForgeSceneModel model;
    SDL_memset(&scene, 0, sizeof(scene));
    SDL_memset(&model, 0, sizeof(model));
    forge_scene_free_model(&scene, &model);
    ASSERT_TRUE(model.vertex_buffer == NULL);
    ASSERT_INT_EQ((int)model.draw_calls, 0);
    END_TEST();
}

#define TEST_MODEL_NONZERO_DRAW_CALLS    42
#define TEST_MODEL_NONZERO_MAT_TEX_COUNT 5

static void test_free_model_zeroes_struct(void)
{
    TEST("free_model zeroes the struct")
    ForgeSceneModel model;
    SDL_memset(&model, 0, sizeof(model));
    model.draw_calls = TEST_MODEL_NONZERO_DRAW_CALLS;
    model.mat_texture_count = TEST_MODEL_NONZERO_MAT_TEX_COUNT;
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    forge_scene_free_model(&scene, &model);
    ASSERT_INT_EQ((int)model.draw_calls, 0);
    ASSERT_INT_EQ((int)model.mat_texture_count, 0);
    END_TEST();
}

static void test_free_model_double_free_safe(void)
{
    TEST("free_model is idempotent (double free safe)")
    ForgeSceneModel model;
    SDL_memset(&model, 0, sizeof(model));
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    forge_scene_free_model(&scene, &model);
    forge_scene_free_model(&scene, &model);
    ASSERT_TRUE(1);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Group 16: File error handling
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_load_model_missing_fscene(void)
{
    TEST("load_model with missing .fscene returns false (no device)")
    ForgeScene scene;
    ForgeSceneModel model;
    SDL_memset(&scene, 0, sizeof(scene));
    /* No GPU device — tests the precondition path (no crash, returns false).
     * The GPU-path test below exercises actual file I/O. */
    bool ok = forge_scene_load_model(&scene, &model,
        "/nonexistent.fscene", "/nonexistent.fmesh",
        "/nonexistent.fmat", "/tmp");
    ASSERT_TRUE(!ok);
    END_TEST();
}

static void test_load_model_missing_fscene_gpu(void)
{
    if (!gpu_available) {
        TEST("load_model with missing .fscene (GPU) (SKIPPED)")
        SKIP_TEST();
        return;
    }
    TEST_C("load_model with missing .fscene returns false (GPU)")
    ForgeScene scene;
    ForgeSceneModel model;
    SDL_memset(&scene, 0, sizeof(scene));
    SDL_memset(&model, 0, sizeof(model));
    ForgeSceneConfig cfg = forge_scene_default_config("File Error Test");
    char *argv[] = { "test_scene", NULL };
    ASSERT_TRUE_C(forge_scene_init(&scene, &cfg, 1, argv));
    ASSERT_TRUE_C(!forge_scene_load_model(
        &scene, &model,
        "/nonexistent.fscene", "/nonexistent.fmesh",
        "/nonexistent.fmat", "/tmp"));
cleanup:
    forge_scene_free_model(&scene, &model);
    forge_scene_destroy(&scene);
    END_TEST_C();
}

static void test_load_model_empty_base_dir(void)
{
    if (!gpu_available) {
        TEST("load_model with empty base_dir returns false (GPU) (SKIPPED)")
        SKIP_TEST();
        return;
    }
    TEST_C("load_model with empty base_dir returns false (GPU)")
    ForgeScene scene;
    ForgeSceneModel model;
    SDL_memset(&scene, 0, sizeof(scene));
    SDL_memset(&model, 0, sizeof(model));
    ForgeSceneConfig cfg = forge_scene_default_config("Empty Base Dir Test");
    char *argv[] = { "test_scene", NULL };
    ASSERT_TRUE_C(forge_scene_init(&scene, &cfg, 1, argv));
    ASSERT_TRUE_C(!forge_scene_load_model(
        &scene, &model,
        "/nonexistent.fscene", "/nonexistent.fmesh",
        "/nonexistent.fmat", ""));
cleanup:
    forge_scene_free_model(&scene, &model);
    forge_scene_destroy(&scene);
    END_TEST_C();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Group 17: Max materials constant
 * ══════════════════════════════════════════════════════════════════════════ */

#define EXPECTED_MODEL_MAX_MATERIALS 32

static void test_max_materials_constant(void)
{
    TEST("FORGE_SCENE_MODEL_MAX_MATERIALS is 32")
    ASSERT_INT_EQ(FORGE_SCENE_MODEL_MAX_MATERIALS, EXPECTED_MODEL_MAX_MATERIALS);
    END_TEST();
}

static void test_model_textures_array_size(void)
{
    TEST("mat_textures array matches MAX_MATERIALS")
    ForgeSceneModel model;
    SDL_memset(&model, 0, sizeof(model));
    int arr_count = (int)(sizeof(model.mat_textures) /
                          sizeof(model.mat_textures[0]));
    ASSERT_INT_EQ(arr_count, FORGE_SCENE_MODEL_MAX_MATERIALS);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Runner functions (called from test_scene.c main)
 * ══════════════════════════════════════════════════════════════════════════ */

static void run_model_tests(void)
{
    /* ── Group 10: ModelVertex struct layout ─────────────────── */
    SDL_Log("--- 10. ModelVertex Struct Layout ---");
    test_model_vertex_size();
    test_model_vertex_position_offset();
    test_model_vertex_normal_offset();
    test_model_vertex_uv_offset();
    test_model_vertex_tangent_offset();

    /* ── Group 11: ModelFragUniforms layout ───────────────────── */
    SDL_Log("--- 11. ModelFragUniforms Layout ---");
    test_model_frag_uniforms_size();
    test_model_frag_uniforms_light_dir_offset();
    test_model_frag_uniforms_base_color_offset();
    test_model_frag_uniforms_shininess_offset();
    test_model_frag_uniforms_ambient_offset();

    /* ── Group 12: ForgeSceneModel defaults ───────────────────── */
    SDL_Log("--- 12. ForgeSceneModel Defaults ---");
    test_model_zeroed_has_null_buffers();
    test_model_zeroed_has_empty_pipeline();
    test_model_zeroed_has_zero_mat_count();

    /* ── Group 13: Transform composition ─────────────────────── */
    SDL_Log("--- 13. Transform Composition ---");
    test_transform_identity_times_identity();
    test_transform_placement_translate();
    test_transform_scale_composes();
    test_transform_column_major_order();

    /* ── Group 14: NULL safety ───────────────────────────────── */
    SDL_Log("--- 14. Model NULL Safety ---");
    test_load_model_null_scene();
    test_load_model_null_model();
    test_load_model_null_paths();
    test_draw_model_null_pass_safe();
    test_draw_model_shadows_null_pass_safe();
    test_draw_model_null_buffers_safe();

    /* ── Group 15: Free safety ───────────────────────────────── */
    SDL_Log("--- 15. Model Free Safety ---");
    test_free_model_null_safe();
    test_free_model_zeroed_safe();
    test_free_model_zeroes_struct();
    test_free_model_double_free_safe();

    /* ── Group 16: File error handling ────────────────────────── */
    SDL_Log("--- 16. File Error Handling ---");
    test_load_model_missing_fscene();

    /* ── Group 17: Constants ─────────────────────────────────── */
    SDL_Log("--- 17. Model Constants ---");
    test_max_materials_constant();
    test_model_textures_array_size();
}

static void run_model_gpu_tests(void)
{
    test_load_model_missing_fscene_gpu();
    test_load_model_empty_base_dir();
}
