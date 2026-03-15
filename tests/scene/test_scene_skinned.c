/*
 * Scene Skinned Model Tests (Groups 18–26)
 *
 * Tests for ForgeSceneSkinnedModel: vertex layout, defaults, animation
 * evaluation, world transforms, joint matrix computation, NULL safety,
 * free safety, constants, and animation edge cases.
 *
 * SPDX-License-Identifier: Zlib
 */

/* #include'd from test_scene.c (not compiled separately) — forge_scene.h
 * static functions must share a single translation unit. */
#include "test_scene_common.h"
#include <stddef.h>

/* ── Helpers ───────────────────────────────────────────────────────────── */

/* Initialise a scene node at the origin with identity TRS and transforms. */
static void skinned_test_init_node(ForgePipelineSceneNode *node)
{
    SDL_memset(node, 0, sizeof(*node));
    node->parent = -1;
    node->mesh_index = -1;
    node->skin_index = -1;
    node->has_trs = 1;
    node->scale[0] = 1.0f;
    node->scale[1] = 1.0f;
    node->scale[2] = 1.0f;
    node->rotation[3] = 1.0f; /* identity quaternion w */
    /* Set identity matrices so nodes are usable before anim_apply */
    mat4 ident = mat4_identity();
    SDL_memcpy(node->local_transform, &ident, sizeof(mat4));
    SDL_memcpy(node->world_transform, &ident, sizeof(mat4));
}

/* Build a translation animation: node 0, using caller-owned arrays. */
static void skinned_test_build_translate_anim(
    ForgePipelineAnimSampler *sampler,
    ForgePipelineAnimChannel *channel,
    ForgePipelineAnimation *anim,
    float *timestamps, uint32_t kf_count,
    float *values, float duration)
{
    SDL_memset(sampler, 0, sizeof(*sampler));
    sampler->timestamps = timestamps;
    sampler->values = values;
    sampler->keyframe_count = kf_count;
    sampler->value_components = 3;
    sampler->interpolation = FORGE_PIPELINE_INTERP_LINEAR;

    SDL_memset(channel, 0, sizeof(*channel));
    channel->target_node = 0;
    channel->target_path = FORGE_PIPELINE_ANIM_TRANSLATION;
    channel->sampler_index = 0;

    SDL_memset(anim, 0, sizeof(*anim));
    anim->duration = duration;
    anim->samplers = sampler;
    anim->sampler_count = 1;
    anim->channels = channel;
    anim->channel_count = 1;
}

/* ── Group 18: Skinned vertex layout ── */

static void test_skin_tan_vertex_size_and_stride(void)
{
    TEST("ForgePipelineVertexSkinTan is 72 bytes and stride matches")
    ASSERT_INT_EQ((int)sizeof(ForgePipelineVertexSkinTan), 72);
    ASSERT_INT_EQ(FORGE_PIPELINE_VERTEX_STRIDE_SKIN_TAN,
                  (int)sizeof(ForgePipelineVertexSkinTan));
    END_TEST();
}

static void test_skin_tan_field_offsets(void)
{
    TEST("SkinTanVertex field offsets: pos@0, norm@12, uv@24, tan@32, jnt@48, wt@56")
    ASSERT_INT_EQ((int)offsetof(ForgePipelineVertexSkinTan, position),  0);
    ASSERT_INT_EQ((int)offsetof(ForgePipelineVertexSkinTan, normal),   12);
    ASSERT_INT_EQ((int)offsetof(ForgePipelineVertexSkinTan, uv),       24);
    ASSERT_INT_EQ((int)offsetof(ForgePipelineVertexSkinTan, tangent),  32);
    ASSERT_INT_EQ((int)offsetof(ForgePipelineVertexSkinTan, joints),   48);
    ASSERT_INT_EQ((int)offsetof(ForgePipelineVertexSkinTan, weights),  56);
    END_TEST();
}

/* ── Group 19: ForgeSceneSkinnedModel defaults ── */

static void test_skinned_zeroed_has_null_buffers(void)
{
    TEST("Zeroed skinned model has NULL GPU buffers")
    ForgeSceneSkinnedModel model;
    SDL_memset(&model, 0, sizeof(model));
    ASSERT_TRUE(model.vertex_buffer == NULL);
    ASSERT_TRUE(model.index_buffer == NULL);
    ASSERT_TRUE(model.joint_buffer == NULL);
    ASSERT_TRUE(model.joint_transfer_buffer == NULL);
    END_TEST();
}

static void test_skinned_zeroed_counts_and_anim(void)
{
    TEST("Zeroed skinned model has zero counts and animation state")
    ForgeSceneSkinnedModel model;
    SDL_memset(&model, 0, sizeof(model));
    ASSERT_INT_EQ((int)model.draw_calls, 0);
    ASSERT_INT_EQ((int)model.mat_texture_count, 0);
    ASSERT_INT_EQ((int)model.active_joint_count, 0);
    ASSERT_NEAR(model.anim_time, 0.0f, EPSILON);
    ASSERT_NEAR(model.anim_speed, 0.0f, EPSILON);
    ASSERT_INT_EQ(model.current_clip, 0);
    ASSERT_TRUE(!model.looping);
    END_TEST();
}

static void test_skinned_zeroed_pipeline_data(void)
{
    TEST("Zeroed skinned model has empty pipeline structs")
    ForgeSceneSkinnedModel model;
    SDL_memset(&model, 0, sizeof(model));
    ASSERT_INT_EQ((int)model.scene_data.node_count, 0);
    ASSERT_INT_EQ((int)model.mesh.vertex_count, 0);
    ASSERT_INT_EQ((int)model.materials.material_count, 0);
    ASSERT_INT_EQ((int)model.skins.skin_count, 0);
    ASSERT_INT_EQ((int)model.animations.clip_count, 0);
    END_TEST();
}

/* ── Group 20: Animation evaluation ── */

#define ANIM_EVAL_DURATION 1.0f
#define ANIM_EVAL_END_X   10.0f
#define ANIM_EVAL_MID_T    0.5f
#define ANIM_EVAL_MID_X    5.0f

static void test_anim_apply_updates_translation(void)
{
    TEST("anim_apply updates node translation at midpoint")
    ForgePipelineSceneNode node;
    skinned_test_init_node(&node);
    float ts[2] = { 0.0f, ANIM_EVAL_DURATION };
    float vals[6] = { 0.0f, 0.0f, 0.0f,  ANIM_EVAL_END_X, 0.0f, 0.0f };
    ForgePipelineAnimSampler samp;
    ForgePipelineAnimChannel ch;
    ForgePipelineAnimation anim;
    skinned_test_build_translate_anim(&samp, &ch, &anim, ts, 2, vals,
                                     ANIM_EVAL_DURATION);
    forge_pipeline_anim_apply(&anim, &node, 1, ANIM_EVAL_MID_T, false);
    ASSERT_NEAR(node.translation[0], ANIM_EVAL_MID_X, EPSILON);
    ASSERT_NEAR(node.translation[1], 0.0f, EPSILON);
    ASSERT_NEAR(node.translation[2], 0.0f, EPSILON);
    END_TEST();
}

static void test_anim_apply_at_start(void)
{
    TEST("anim_apply at t=0 gives start value")
    ForgePipelineSceneNode node;
    skinned_test_init_node(&node);
    float ts[2] = { 0.0f, ANIM_EVAL_DURATION };
    float vals[6] = { 0.0f, 0.0f, 0.0f,  ANIM_EVAL_END_X, 0.0f, 0.0f };
    ForgePipelineAnimSampler samp;
    ForgePipelineAnimChannel ch;
    ForgePipelineAnimation anim;
    skinned_test_build_translate_anim(&samp, &ch, &anim, ts, 2, vals,
                                     ANIM_EVAL_DURATION);
    forge_pipeline_anim_apply(&anim, &node, 1, 0.0f, false);
    ASSERT_NEAR(node.translation[0], 0.0f, EPSILON);
    END_TEST();
}

/* ── Group 21: World transform recomputation ── */

#define WORLD_PARENT_TX  2.0f
#define WORLD_CHILD_TY   3.0f
#define WORLD_GRAND_TZ   4.0f

static void test_world_transforms_parent_child_chain(void)
{
    TEST("World transforms propagate parent x local chain")
    /* root(0) -> child(1) -> grandchild(2) */
    ForgePipelineSceneNode nodes[3];
    SDL_memset(nodes, 0, sizeof(nodes));
    for (int i = 0; i < 3; i++) {
        nodes[i].parent = (i > 0) ? (int32_t)(i - 1) : -1;
        nodes[i].mesh_index = -1;
        nodes[i].skin_index = -1;
        nodes[i].has_trs = 1;
        nodes[i].scale[0] = 1.0f;
        nodes[i].scale[1] = 1.0f;
        nodes[i].scale[2] = 1.0f;
        nodes[i].rotation[3] = 1.0f;
    }
    /* Set up child array: node 0 has child 1, node 1 has child 2 */
    uint32_t children_arr[2] = { 1, 2 };
    nodes[0].first_child = 0;
    nodes[0].child_count = 1;
    nodes[1].first_child = 1;
    nodes[1].child_count = 1;

    nodes[0].translation[0] = WORLD_PARENT_TX;
    nodes[1].translation[1] = WORLD_CHILD_TY;
    nodes[2].translation[2] = WORLD_GRAND_TZ;

    mat4 l0 = mat4_translate(vec3_create(WORLD_PARENT_TX, 0.0f, 0.0f));
    mat4 l1 = mat4_translate(vec3_create(0.0f, WORLD_CHILD_TY, 0.0f));
    mat4 l2 = mat4_translate(vec3_create(0.0f, 0.0f, WORLD_GRAND_TZ));
    SDL_memcpy(nodes[0].local_transform, l0.m, 64);
    SDL_memcpy(nodes[1].local_transform, l1.m, 64);
    SDL_memcpy(nodes[2].local_transform, l2.m, 64);

    uint32_t roots[1] = { 0 };
    forge_pipeline_scene_compute_world_transforms(
        nodes, 3, roots, 1, children_arr, 2);

    /* Root world col-3 = (2, 0, 0) */
    ASSERT_NEAR(nodes[0].world_transform[12], WORLD_PARENT_TX, EPSILON);
    ASSERT_NEAR(nodes[0].world_transform[13], 0.0f, EPSILON);
    /* Child world col-3 = (2, 3, 0) */
    ASSERT_NEAR(nodes[1].world_transform[12], WORLD_PARENT_TX, EPSILON);
    ASSERT_NEAR(nodes[1].world_transform[13], WORLD_CHILD_TY, EPSILON);
    /* Grandchild world col-3 = (2, 3, 4) */
    ASSERT_NEAR(nodes[2].world_transform[12], WORLD_PARENT_TX, EPSILON);
    ASSERT_NEAR(nodes[2].world_transform[13], WORLD_CHILD_TY, EPSILON);
    ASSERT_NEAR(nodes[2].world_transform[14], WORLD_GRAND_TZ, EPSILON);
    END_TEST();
}

#define WORLD_LEAF_TX 5.0f

static void test_world_transforms_leaf_only(void)
{
    TEST("World transforms work for a single root with no children")
    ForgePipelineSceneNode node;
    SDL_memset(&node, 0, sizeof(node));
    node.parent = -1;
    node.mesh_index = -1;
    node.skin_index = -1;
    node.has_trs = 1;
    node.scale[0] = 1.0f;
    node.scale[1] = 1.0f;
    node.scale[2] = 1.0f;
    node.rotation[3] = 1.0f;
    node.translation[0] = WORLD_LEAF_TX;
    /* child_count = 0, no children array needed */

    mat4 l = mat4_translate(vec3_create(WORLD_LEAF_TX, 0.0f, 0.0f));
    SDL_memcpy(node.local_transform, l.m, 64);

    uint32_t roots[1] = { 0 };
    forge_pipeline_scene_compute_world_transforms(
        &node, 1, roots, 1, NULL, 0);

    ASSERT_NEAR(node.world_transform[12], WORLD_LEAF_TX, EPSILON);
    ASSERT_NEAR(node.world_transform[13], 0.0f, EPSILON);
    ASSERT_NEAR(node.world_transform[14], 0.0f, EPSILON);
    END_TEST();
}

static void test_world_transforms_child_array_bounds(void)
{
    TEST("World transforms stop at child_array_count boundary")
    /* 3 nodes: root(0) claims 2 children but child array only has 1 entry.
     * The second child (node 2) should NOT be visited. */
    ForgePipelineSceneNode nodes[3];
    SDL_memset(nodes, 0, sizeof(nodes));
    for (int i = 0; i < 3; i++) {
        nodes[i].parent = (i > 0) ? 0 : -1;
        nodes[i].mesh_index = -1;
        nodes[i].skin_index = -1;
        nodes[i].has_trs = 1;
        nodes[i].scale[0] = 1.0f;
        nodes[i].scale[1] = 1.0f;
        nodes[i].scale[2] = 1.0f;
        nodes[i].rotation[3] = 1.0f;
    }
    nodes[0].first_child = 0;
    nodes[0].child_count = 2; /* claims 2 children */
    nodes[1].translation[0] = WORLD_PARENT_TX;
    nodes[2].translation[1] = WORLD_CHILD_TY;

    mat4 id = mat4_identity();
    mat4 l1 = mat4_translate(vec3_create(WORLD_PARENT_TX, 0.0f, 0.0f));
    mat4 l2 = mat4_translate(vec3_create(0.0f, WORLD_CHILD_TY, 0.0f));
    SDL_memcpy(nodes[0].local_transform, id.m, 64);
    SDL_memcpy(nodes[1].local_transform, l1.m, 64);
    SDL_memcpy(nodes[2].local_transform, l2.m, 64);

    /* Only 1 entry in children array — bounds check should stop before
     * reaching the second child. */
    uint32_t children_arr[1] = { 1 };
    uint32_t roots[1] = { 0 };
    forge_pipeline_scene_compute_world_transforms(
        nodes, 3, roots, 1, children_arr, 1);

    /* Root gets identity world transform */
    ASSERT_NEAR(nodes[0].world_transform[12], 0.0f, EPSILON);
    /* Child 1 was visited — gets its translation */
    ASSERT_NEAR(nodes[1].world_transform[12], WORLD_PARENT_TX, EPSILON);
    /* Child 2 was NOT visited — world_transform stays zeroed */
    ASSERT_NEAR(nodes[2].world_transform[12], 0.0f, EPSILON);
    ASSERT_NEAR(nodes[2].world_transform[13], 0.0f, EPSILON);
    END_TEST();
}

/* ── Group 22: Joint matrix computation ── */

#define JOINT_TEST_TX  1.0f
#define JOINT_TEST_TY  2.0f

static void test_joint_matrices_two_joints(void)
{
    TEST("Joint matrices for 2-joint skin match skinning equation")
    /* 3 nodes: mesh(0), joint0(1), joint1(2) */
    ForgePipelineSceneNode nodes[3];
    SDL_memset(nodes, 0, sizeof(nodes));
    mat4 id = mat4_identity();
    for (int i = 0; i < 3; i++) {
        nodes[i].parent = -1;
        nodes[i].mesh_index = -1;
        nodes[i].skin_index = -1;
        SDL_memcpy(nodes[i].local_transform, id.m, 64);
        SDL_memcpy(nodes[i].world_transform, id.m, 64);
    }

    mat4 j0w = mat4_translate(vec3_create(JOINT_TEST_TX, 0.0f, 0.0f));
    SDL_memcpy(nodes[1].world_transform, j0w.m, 64);
    mat4 j1w = mat4_translate(vec3_create(0.0f, JOINT_TEST_TY, 0.0f));
    SDL_memcpy(nodes[2].world_transform, j1w.m, 64);

    int32_t joint_indices[2] = { 1, 2 };
    float ibms[32];
    SDL_memcpy(&ibms[0],  id.m, 64);
    SDL_memcpy(&ibms[16], id.m, 64);

    ForgePipelineSkin skin;
    SDL_memset(&skin, 0, sizeof(skin));
    skin.joints = joint_indices;
    skin.inverse_bind_matrices = ibms;
    skin.joint_count = 2;
    skin.skeleton = -1;

    mat4 out[2];
    uint32_t count = forge_pipeline_compute_joint_matrices(
        &skin, nodes, 3, 0, out, 2);

    ASSERT_INT_EQ((int)count, 2);
    /* inv(mesh_world)=I, IBM=I -> joint_matrix = joint_world */
    ASSERT_NEAR(out[0].m[12], JOINT_TEST_TX, EPSILON);
    ASSERT_NEAR(out[0].m[13], 0.0f, EPSILON);
    ASSERT_NEAR(out[1].m[12], 0.0f, EPSILON);
    ASSERT_NEAR(out[1].m[13], JOINT_TEST_TY, EPSILON);
    END_TEST();
}

/* ── Group 23: NULL safety ── */

static void test_anim_apply_null_args(void)
{
    TEST("anim_apply with NULL anim or NULL nodes is safe")
    ForgePipelineSceneNode node;
    SDL_memset(&node, 0, sizeof(node));
    forge_pipeline_anim_apply(NULL, &node, 1, 0.0f, false);
    ForgePipelineAnimation anim;
    SDL_memset(&anim, 0, sizeof(anim));
    forge_pipeline_anim_apply(&anim, NULL, 1, 0.0f, false);
    ASSERT_TRUE(1);
    END_TEST();
}

static void test_world_transforms_null_nodes(void)
{
    TEST("compute_world_transforms with NULL nodes is safe")
    forge_pipeline_scene_compute_world_transforms(
        NULL, 0, NULL, 0, NULL, 0);
    ASSERT_TRUE(1);
    END_TEST();
}

static void test_joint_matrices_null_args(void)
{
    TEST("compute_joint_matrices with NULL skin or output returns 0")
    mat4 out;
    uint32_t c1 = forge_pipeline_compute_joint_matrices(
        NULL, NULL, 0, 0, &out, 1);
    ASSERT_INT_EQ((int)c1, 0);

    ForgePipelineSkin skin;
    SDL_memset(&skin, 0, sizeof(skin));
    ForgePipelineSceneNode node;
    SDL_memset(&node, 0, sizeof(node));
    uint32_t c2 = forge_pipeline_compute_joint_matrices(
        &skin, &node, 1, 0, NULL, 1);
    ASSERT_INT_EQ((int)c2, 0);
    END_TEST();
}

static void test_draw_skinned_null_pass_safe(void)
{
    TEST("draw_skinned_model and shadows with NULL pass is safe")
    ForgeScene scene;
    ForgeSceneSkinnedModel model;
    SDL_memset(&scene, 0, sizeof(scene));
    SDL_memset(&model, 0, sizeof(model));
    forge_scene_draw_skinned_model(&scene, &model, mat4_identity());
    forge_scene_draw_skinned_model_shadows(&scene, &model, mat4_identity());
    ASSERT_TRUE(1);
    END_TEST();
}

/* ── Group 24: Free safety ── */

static void test_free_skinned_null_safe(void)
{
    TEST("free_skinned_model with NULL model is safe")
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    forge_scene_free_skinned_model(&scene, NULL);
    ASSERT_TRUE(1);
    END_TEST();
}

static void test_free_skinned_zeroed_and_double_free(void)
{
    TEST("free_skinned_model on zeroed struct is safe and idempotent")
    ForgeScene scene;
    ForgeSceneSkinnedModel model;
    SDL_memset(&scene, 0, sizeof(scene));
    SDL_memset(&model, 0, sizeof(model));
    forge_scene_free_skinned_model(&scene, &model);
    ASSERT_TRUE(model.vertex_buffer == NULL);
    ASSERT_TRUE(model.joint_buffer == NULL);
    ASSERT_INT_EQ((int)model.draw_calls, 0);
    /* Double free must also be safe */
    forge_scene_free_skinned_model(&scene, &model);
    ASSERT_TRUE(1);
    END_TEST();
}

/* ── Group 25: Constants ── */

#define EXPECTED_MAX_SKIN_JOINTS    256
#define EXPECTED_JOINT_BUFFER_SIZE  16384  /* 256 * 64 */

static void test_skinned_constants(void)
{
    TEST("MAX_SKIN_JOINTS=256, JOINT_BUFFER_SIZE=16384, strides match")
    ASSERT_INT_EQ(FORGE_PIPELINE_MAX_SKIN_JOINTS, EXPECTED_MAX_SKIN_JOINTS);
    ASSERT_INT_EQ((int)FORGE_SCENE_JOINT_BUFFER_SIZE,
                  EXPECTED_JOINT_BUFFER_SIZE);
    ASSERT_INT_EQ(FORGE_PIPELINE_VERTEX_STRIDE_SKIN_TAN,
                  (int)sizeof(ForgePipelineVertexSkinTan));
    ASSERT_INT_EQ(FORGE_PIPELINE_VERTEX_STRIDE_SKIN,
                  (int)sizeof(ForgePipelineVertexSkin));
    END_TEST();
}

static void test_joint_matrices_array_size(void)
{
    TEST("joint_matrices array holds MAX_SKIN_JOINTS entries")
    ForgeSceneSkinnedModel model;
    int arr_count = (int)(sizeof(model.joint_matrices) /
                          sizeof(model.joint_matrices[0]));
    ASSERT_INT_EQ(arr_count, FORGE_PIPELINE_MAX_SKIN_JOINTS);
    END_TEST();
}

/* ── Group 26: Animation evaluation edge cases ── */

static void test_anim_zero_duration(void)
{
    TEST("anim_apply with zero-duration clip does not crash")
    ForgePipelineSceneNode node;
    skinned_test_init_node(&node);
    float ts[1] = { 0.0f };
    float vals[3] = { 7.0f, 8.0f, 9.0f };
    ForgePipelineAnimSampler samp;
    ForgePipelineAnimChannel ch;
    ForgePipelineAnimation anim;
    skinned_test_build_translate_anim(&samp, &ch, &anim, ts, 1, vals, 0.0f);
    forge_pipeline_anim_apply(&anim, &node, 1, 0.5f, false);
    ASSERT_TRUE(1);
    END_TEST();
}

#define ANIM_SINGLE_KF_X 42.0f

static void test_anim_single_keyframe(void)
{
    TEST("anim_apply with single keyframe sets constant value")
    ForgePipelineSceneNode node;
    skinned_test_init_node(&node);
    float ts[1] = { 0.0f };
    float vals[3] = { ANIM_SINGLE_KF_X, 0.0f, 0.0f };
    ForgePipelineAnimSampler samp;
    ForgePipelineAnimChannel ch;
    ForgePipelineAnimation anim;
    skinned_test_build_translate_anim(&samp, &ch, &anim, ts, 1, vals, 1.0f);
    forge_pipeline_anim_apply(&anim, &node, 1, 0.5f, false);
    ASSERT_NEAR(node.translation[0], ANIM_SINGLE_KF_X, EPSILON);
    END_TEST();
}

static void test_anim_looping_wraps_time(void)
{
    TEST("anim_apply with looping wraps time past duration")
    ForgePipelineSceneNode node;
    skinned_test_init_node(&node);
    float ts[2] = { 0.0f, 1.0f };
    float vals[6] = { 0.0f, 0.0f, 0.0f,  ANIM_EVAL_END_X, 0.0f, 0.0f };
    ForgePipelineAnimSampler samp;
    ForgePipelineAnimChannel ch;
    ForgePipelineAnimation anim;
    skinned_test_build_translate_anim(&samp, &ch, &anim, ts, 2, vals, 1.0f);
    /* t=1.5 with looping wraps to 0.5 -> expect 5.0 */
    forge_pipeline_anim_apply(&anim, &node, 1, 1.5f, true);
    ASSERT_NEAR(node.translation[0], ANIM_EVAL_MID_X, EPSILON);
    END_TEST();
}

#define ANIM_CLAMP_END_X 10.0f

static void test_anim_clamping_past_duration(void)
{
    TEST("anim_apply without looping clamps at duration")
    ForgePipelineSceneNode node;
    skinned_test_init_node(&node);
    float ts[2] = { 0.0f, 1.0f };
    float vals[6] = { 0.0f, 0.0f, 0.0f,  ANIM_CLAMP_END_X, 0.0f, 0.0f };
    ForgePipelineAnimSampler samp;
    ForgePipelineAnimChannel ch;
    ForgePipelineAnimation anim;
    skinned_test_build_translate_anim(&samp, &ch, &anim, ts, 2, vals, 1.0f);
    /* t=5.0 without looping clamps to 1.0 -> expect 10.0 */
    forge_pipeline_anim_apply(&anim, &node, 1, 5.0f, false);
    ASSERT_NEAR(node.translation[0], ANIM_CLAMP_END_X, EPSILON);
    END_TEST();
}

/* ── Runner (called from test_scene.c main) ───────────────────────────── */

static void run_skinned_tests(void)
{
    /* ── Group 18: Skinned vertex layout ──────────────────────── */
    SDL_Log("--- 18. Skinned Vertex Layout ---");
    test_skin_tan_vertex_size_and_stride();
    test_skin_tan_field_offsets();

    /* ── Group 19: ForgeSceneSkinnedModel defaults ────────────── */
    SDL_Log("--- 19. ForgeSceneSkinnedModel Defaults ---");
    test_skinned_zeroed_has_null_buffers();
    test_skinned_zeroed_counts_and_anim();
    test_skinned_zeroed_pipeline_data();

    /* ── Group 20: Animation evaluation ───────────────────────── */
    SDL_Log("--- 20. Animation Evaluation ---");
    test_anim_apply_updates_translation();
    test_anim_apply_at_start();

    /* ── Group 21: World transform recomputation ──────────────── */
    SDL_Log("--- 21. World Transform Recomputation ---");
    test_world_transforms_parent_child_chain();
    test_world_transforms_leaf_only();
    test_world_transforms_child_array_bounds();

    /* ── Group 22: Joint matrix computation ───────────────────── */
    SDL_Log("--- 22. Joint Matrix Computation ---");
    test_joint_matrices_two_joints();

    /* ── Group 23: NULL safety ────────────────────────────────── */
    SDL_Log("--- 23. Skinned NULL Safety ---");
    test_anim_apply_null_args();
    test_world_transforms_null_nodes();
    test_joint_matrices_null_args();
    test_draw_skinned_null_pass_safe();

    /* ── Group 24: Free safety ────────────────────────────────── */
    SDL_Log("--- 24. Skinned Free Safety ---");
    test_free_skinned_null_safe();
    test_free_skinned_zeroed_and_double_free();

    /* ── Group 25: Constants ──────────────────────────────────── */
    SDL_Log("--- 25. Skinned Constants ---");
    test_skinned_constants();
    test_joint_matrices_array_size();

    /* ── Group 26: Animation edge cases ───────────────────────── */
    SDL_Log("--- 26. Animation Edge Cases ---");
    test_anim_zero_duration();
    test_anim_single_keyframe();
    test_anim_looping_wraps_time();
    test_anim_clamping_past_duration();
}
