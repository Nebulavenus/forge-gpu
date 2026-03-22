/*
 * Scene Renderer Library Tests (Groups 1–9)
 *
 * Automated tests for common/scene/forge_scene.h
 * Verifies correctness of configuration defaults, light view-projection
 * computation, camera math, inline accessors, struct layout sizes, and
 * error handling for invalid configurations.
 *
 * Model-specific tests (Groups 10–17) are in test_scene_model.c.
 * Transparency sorting tests (Groups 27–30) are in test_scene_transparency.c.
 *
 * Most tests exercise the pure-math / configuration layer that does NOT
 * require a GPU device.  GPU-integration tests (group 9) require a
 * Vulkan-capable device (hardware or Lavapipe) and are skipped gracefully
 * when unavailable.
 *
 * Exit code: 0 if all tests pass, 1 if any test fails
 *
 * SPDX-License-Identifier: Zlib
 */

/* Define implementation macros before including the common header so that
 * forge_scene.h and forge_pipeline.h emit their function bodies in this
 * translation unit only. test_scene_model.c gets declarations only. */
#define FORGE_PIPELINE_IMPLEMENTATION
#define FORGE_SCENE_IMPLEMENTATION
#include "test_scene_common.h"

/* ── Shared state (extern'd in test_scene_common.h) ─────────────────────── */

int test_count = 0;
int pass_count = 0;
int fail_count = 0;
int skip_count = 0;
bool gpu_available = false;

/* ── Model tests (Groups 10–17) ─────────────────────────────────────────── */
#include "test_scene_model.c"

/* ── Skinned model tests (Groups 18–26) ────────────────────────────────── */
#include "test_scene_skinned.c"

/* ── Transparency sorting tests (Groups 27–30) ────────────────────────── */
#include "test_scene_transparency.c"

/* ── Shared constants ────────────────────────────────────────────────────── */

/* Default config expected values (from forge_scene.h constants) */
#define DEF_FOV_DEG            60.0f
#define DEF_NEAR               0.1f
#define DEF_FAR                200.0f
#define DEF_MOVE_SPEED         5.0f
#define DEF_MOUSE_SENS         0.003f
#define DEF_CAM_POS_X          0.0f
#define DEF_CAM_POS_Y          4.0f
#define DEF_CAM_POS_Z          12.0f
#define DEF_CAM_YAW            0.0f
#define DEF_CAM_PITCH          (-0.2f)
#define DEF_LIGHT_INTENSITY    1.2f
#define DEF_AMBIENT            0.15f
#define DEF_SHININESS          32.0f
#define DEF_SPECULAR_STR       0.5f
#define DEF_SHADOW_MAP_SIZE    2048
#define DEF_SHADOW_ORTHO_SIZE  15.0f
#define DEF_SHADOW_HEIGHT      20.0f
#define DEF_SHADOW_NEAR        0.1f
#define DEF_SHADOW_FAR         50.0f
#define DEF_GRID_HALF_SIZE     20.0f
#define DEF_GRID_SPACING       1.0f
#define DEF_GRID_LINE_WIDTH    0.02f
#define DEF_GRID_FADE_DIST     30.0f
#define DEF_FONT_SIZE          24.0f

/* Custom config values for override tests */
#define CUSTOM_FOV             90.0f
#define CUSTOM_NEAR            0.5f
#define CUSTOM_FAR             500.0f
#define CUSTOM_SHADOW_SIZE     4096
#define CUSTOM_CAM_X           10.0f
#define CUSTOM_CAM_Y           5.0f
#define CUSTOM_CAM_Z           (-3.0f)
#define CUSTOM_YAW             1.57f
#define CUSTOM_PITCH           (-0.5f)

/* Light direction test values */
#define LIGHT_DIR_X            0.4f
#define LIGHT_DIR_Y            0.8f
#define LIGHT_DIR_Z            0.6f

/* Helpers for generating non-finite values in a way that avoids
 * MSVC constant-division warnings. */
static float forge_test_nan(void) { volatile float z = 0.0f; return z / z; }
static float forge_test_inf(void) { volatile float z = 0.0f; return 1.0f / z; }

/* ══════════════════════════════════════════════════════════════════════════
 * 1. forge_scene_default_config — Configuration Defaults
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_config_default_title(void)
{
    TEST("default_config — title passed through");
    ForgeSceneConfig cfg = forge_scene_default_config("Test Window");
    ASSERT_TRUE(SDL_strcmp(cfg.window_title, "Test Window") == 0);
    END_TEST();
}

static void test_config_default_title_null(void)
{
    TEST("default_config — NULL title becomes 'Forge GPU'");
    ForgeSceneConfig cfg = forge_scene_default_config(NULL);
    ASSERT_TRUE(SDL_strcmp(cfg.window_title, "Forge GPU") == 0);
    END_TEST();
}

static void test_config_default_camera(void)
{
    TEST("default_config — camera start position and angles");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    ASSERT_NEAR(cfg.cam_start_pos.x, DEF_CAM_POS_X, EPSILON);
    ASSERT_NEAR(cfg.cam_start_pos.y, DEF_CAM_POS_Y, EPSILON);
    ASSERT_NEAR(cfg.cam_start_pos.z, DEF_CAM_POS_Z, EPSILON);
    ASSERT_NEAR(cfg.cam_start_yaw, DEF_CAM_YAW, EPSILON);
    ASSERT_NEAR(cfg.cam_start_pitch, DEF_CAM_PITCH, EPSILON);
    ASSERT_NEAR(cfg.move_speed, DEF_MOVE_SPEED, EPSILON);
    ASSERT_NEAR(cfg.mouse_sensitivity, DEF_MOUSE_SENS, EPSILON);
    END_TEST();
}

static void test_config_default_projection(void)
{
    TEST("default_config — projection parameters");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    ASSERT_NEAR(cfg.fov_deg, DEF_FOV_DEG, EPSILON);
    ASSERT_NEAR(cfg.near_plane, DEF_NEAR, EPSILON);
    ASSERT_NEAR(cfg.far_plane, DEF_FAR, EPSILON);
    END_TEST();
}

static void test_config_default_lighting(void)
{
    TEST("default_config — lighting parameters");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    ASSERT_NEAR(cfg.light_intensity, DEF_LIGHT_INTENSITY, EPSILON);
    ASSERT_NEAR(cfg.ambient, DEF_AMBIENT, EPSILON);
    ASSERT_NEAR(cfg.shininess, DEF_SHININESS, EPSILON);
    ASSERT_NEAR(cfg.specular_str, DEF_SPECULAR_STR, EPSILON);
    /* Light color near white-warm */
    ASSERT_NEAR(cfg.light_color[0], 1.0f, EPSILON);
    ASSERT_NEAR(cfg.light_color[1], 0.95f, EPSILON);
    ASSERT_NEAR(cfg.light_color[2], 0.9f, EPSILON);
    END_TEST();
}

static void test_config_default_light_dir_normalized(void)
{
    TEST("default_config — light_dir is normalized");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    float len = vec3_length(cfg.light_dir);
    ASSERT_NEAR(len, 1.0f, EPSILON);
    /* Positive Y component — light comes from above */
    ASSERT_TRUE(cfg.light_dir.y > 0.0f);
    END_TEST();
}

static void test_config_default_shadow(void)
{
    TEST("default_config — shadow map parameters");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    ASSERT_INT_EQ(cfg.shadow_map_size, DEF_SHADOW_MAP_SIZE);
    ASSERT_NEAR(cfg.shadow_ortho_size, DEF_SHADOW_ORTHO_SIZE, EPSILON);
    ASSERT_NEAR(cfg.shadow_height, DEF_SHADOW_HEIGHT, EPSILON);
    ASSERT_NEAR(cfg.shadow_near, DEF_SHADOW_NEAR, EPSILON);
    ASSERT_NEAR(cfg.shadow_far, DEF_SHADOW_FAR, EPSILON);
    END_TEST();
}

static void test_config_default_grid(void)
{
    TEST("default_config — grid parameters");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    ASSERT_NEAR(cfg.grid_half_size, DEF_GRID_HALF_SIZE, EPSILON);
    ASSERT_NEAR(cfg.grid_spacing, DEF_GRID_SPACING, EPSILON);
    ASSERT_NEAR(cfg.grid_line_width, DEF_GRID_LINE_WIDTH, EPSILON);
    ASSERT_NEAR(cfg.grid_fade_dist, DEF_GRID_FADE_DIST, EPSILON);
    END_TEST();
}

static void test_config_default_font_disabled(void)
{
    TEST("default_config — font_path is NULL (UI disabled)");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    ASSERT_TRUE(cfg.font_path == NULL);
    ASSERT_NEAR(cfg.font_size, DEF_FONT_SIZE, EPSILON);
    END_TEST();
}

static void test_config_default_clear_color(void)
{
    TEST("default_config — clear_color is dark but not black");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    /* Dark background — all channels > 0 but < 0.5 */
    ASSERT_TRUE(cfg.clear_color[0] > 0.0f && cfg.clear_color[0] < 0.5f);
    ASSERT_TRUE(cfg.clear_color[1] > 0.0f && cfg.clear_color[1] < 0.5f);
    ASSERT_TRUE(cfg.clear_color[2] > 0.0f && cfg.clear_color[2] < 0.5f);
    ASSERT_NEAR(cfg.clear_color[3], 1.0f, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 2. Config Override — user-specified values override defaults
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_config_override_camera(void)
{
    TEST("config_override — camera position and angles");
    ForgeSceneConfig cfg = forge_scene_default_config("Override");
    cfg.cam_start_pos = vec3_create(CUSTOM_CAM_X, CUSTOM_CAM_Y, CUSTOM_CAM_Z);
    cfg.cam_start_yaw = CUSTOM_YAW;
    cfg.cam_start_pitch = CUSTOM_PITCH;

    /* Verify the values stuck */
    ASSERT_NEAR(cfg.cam_start_pos.x, CUSTOM_CAM_X, EPSILON);
    ASSERT_NEAR(cfg.cam_start_pos.y, CUSTOM_CAM_Y, EPSILON);
    ASSERT_NEAR(cfg.cam_start_pos.z, CUSTOM_CAM_Z, EPSILON);
    ASSERT_NEAR(cfg.cam_start_yaw, CUSTOM_YAW, EPSILON);
    ASSERT_NEAR(cfg.cam_start_pitch, CUSTOM_PITCH, EPSILON);
    END_TEST();
}

static void test_config_override_projection(void)
{
    TEST("config_override — projection parameters");
    ForgeSceneConfig cfg = forge_scene_default_config("Override");
    cfg.fov_deg = CUSTOM_FOV;
    cfg.near_plane = CUSTOM_NEAR;
    cfg.far_plane = CUSTOM_FAR;

    ASSERT_NEAR(cfg.fov_deg, CUSTOM_FOV, EPSILON);
    ASSERT_NEAR(cfg.near_plane, CUSTOM_NEAR, EPSILON);
    ASSERT_NEAR(cfg.far_plane, CUSTOM_FAR, EPSILON);
    END_TEST();
}

static void test_config_override_shadow_size(void)
{
    TEST("config_override — shadow map size");
    ForgeSceneConfig cfg = forge_scene_default_config("Override");
    cfg.shadow_map_size = CUSTOM_SHADOW_SIZE;
    ASSERT_INT_EQ(cfg.shadow_map_size, CUSTOM_SHADOW_SIZE);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 3. Light View-Projection Math
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_light_vp_camera_position(void)
{
    TEST("light_vp — shadow camera placed along light_dir");
    /* Reproduce the light VP calculation from forge_scene_init.
     * light_dir points TOWARD the light; the shadow camera is placed
     * at light_dir * shadow_height looking at the origin. */
    vec3 light_dir = vec3_normalize(
        vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));
    float shadow_height = DEF_SHADOW_HEIGHT;

    vec3 light_pos = vec3_scale(light_dir, shadow_height);

    /* Light position should be above origin (positive Y) when light_dir
     * has a positive Y component */
    ASSERT_TRUE(light_pos.y > 0.0f);

    /* Distance from origin should equal shadow_height */
    float dist = vec3_length(light_pos);
    ASSERT_NEAR(dist, shadow_height, EPSILON);
    END_TEST();
}

static void test_light_vp_matrix_not_identity(void)
{
    TEST("light_vp — matrix is non-trivial");
    vec3 light_dir = vec3_normalize(
        vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));
    float shadow_height = DEF_SHADOW_HEIGHT;

    vec3 light_pos = vec3_scale(light_dir, shadow_height);
    mat4 light_view = mat4_look_at(
        light_pos,
        vec3_create(0.0f, 0.0f, 0.0f),
        vec3_create(0.0f, 1.0f, 0.0f));
    mat4 light_proj = mat4_orthographic(
        -DEF_SHADOW_ORTHO_SIZE, DEF_SHADOW_ORTHO_SIZE,
        -DEF_SHADOW_ORTHO_SIZE, DEF_SHADOW_ORTHO_SIZE,
        DEF_SHADOW_NEAR, DEF_SHADOW_FAR);
    mat4 light_vp = mat4_multiply(light_proj, light_view);

    /* The result should not be the identity matrix */
    ASSERT_TRUE(SDL_fabsf(light_vp.m[0] - 1.0f) > EPSILON ||
                SDL_fabsf(light_vp.m[5] - 1.0f) > EPSILON ||
                SDL_fabsf(light_vp.m[10] - 1.0f) > EPSILON);
    END_TEST();
}

static void test_light_vp_origin_projects_near_center(void)
{
    TEST("light_vp — origin projects near NDC center");
    vec3 light_dir = vec3_normalize(
        vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));
    float shadow_height = DEF_SHADOW_HEIGHT;

    vec3 light_pos = vec3_scale(light_dir, shadow_height);
    mat4 light_view = mat4_look_at(
        light_pos,
        vec3_create(0.0f, 0.0f, 0.0f),
        vec3_create(0.0f, 1.0f, 0.0f));
    mat4 light_proj = mat4_orthographic(
        -DEF_SHADOW_ORTHO_SIZE, DEF_SHADOW_ORTHO_SIZE,
        -DEF_SHADOW_ORTHO_SIZE, DEF_SHADOW_ORTHO_SIZE,
        DEF_SHADOW_NEAR, DEF_SHADOW_FAR);
    mat4 light_vp = mat4_multiply(light_proj, light_view);

    /* Transform the world origin — should land near NDC (0, 0) in XY */
    vec4 origin;
    origin = vec4_create(0.0f, 0.0f, 0.0f, 1.0f);
    vec4 clip;
    clip = mat4_multiply_vec4(light_vp, origin);
    float ndc_x = clip.x / clip.w;
    float ndc_y = clip.y / clip.w;

    /* Should be close to center (within ortho bounds) */
    ASSERT_NEAR(ndc_x, 0.0f, 0.1f);
    ASSERT_NEAR(ndc_y, 0.0f, 0.1f);
    END_TEST();
}

static void test_light_vp_direction_consistency(void)
{
    TEST("light_vp — light camera looks from above toward origin");
    /* A point directly below the light (at origin) should be inside
     * the shadow map frustum. A point far behind should be outside
     * or have a large NDC Z. */
    vec3 light_dir = vec3_normalize(
        vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z));
    vec3 light_pos = vec3_scale(light_dir, DEF_SHADOW_HEIGHT);
    mat4 light_view = mat4_look_at(
        light_pos,
        vec3_create(0.0f, 0.0f, 0.0f),
        vec3_create(0.0f, 1.0f, 0.0f));
    mat4 light_proj = mat4_orthographic(
        -DEF_SHADOW_ORTHO_SIZE, DEF_SHADOW_ORTHO_SIZE,
        -DEF_SHADOW_ORTHO_SIZE, DEF_SHADOW_ORTHO_SIZE,
        DEF_SHADOW_NEAR, DEF_SHADOW_FAR);
    mat4 light_vp = mat4_multiply(light_proj, light_view);

    /* Point at origin: should have small NDC Z (near the light) */
    vec4 at_origin;
    at_origin = vec4_create(0.0f, 0.0f, 0.0f, 1.0f);
    vec4 clip0;
    clip0 = mat4_multiply_vec4(light_vp, at_origin);
    float z_origin = clip0.z / clip0.w;

    /* Point behind the light: should have even smaller NDC Z
     * or be outside the frustum entirely */
    vec4 behind;
    behind = vec4_create(light_pos.x * 2.0f, light_pos.y * 2.0f,
                         light_pos.z * 2.0f, 1.0f);
    vec4 clip1;
    clip1 = mat4_multiply_vec4(light_vp, behind);
    float z_behind = clip1.z / clip1.w;

    /* Origin should be at a valid depth; the point behind the light
     * should be at a different (smaller) depth since it's closer to
     * the camera or behind it */
    ASSERT_TRUE(z_origin != z_behind);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 4. Camera Math
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_camera_default_orientation(void)
{
    TEST("camera — default yaw=0 produces forward along -Z");
    /* The canonical camera: yaw=0, pitch=0 looks along -Z */
    quat orient = quat_from_euler(0.0f, 0.0f, 0.0f);
    vec3 forward = quat_forward(orient);

    /* Forward should be approximately (0, 0, -1) */
    ASSERT_NEAR(forward.x, 0.0f, EPSILON);
    ASSERT_NEAR(forward.y, 0.0f, EPSILON);
    ASSERT_NEAR(forward.z, -1.0f, EPSILON);
    END_TEST();
}

static void test_camera_right_vector(void)
{
    TEST("camera — default yaw=0 produces right along +X");
    quat orient = quat_from_euler(0.0f, 0.0f, 0.0f);
    vec3 right = quat_right(orient);

    ASSERT_NEAR(right.x, 1.0f, EPSILON);
    ASSERT_NEAR(right.y, 0.0f, EPSILON);
    ASSERT_NEAR(right.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_camera_pitch_looks_down(void)
{
    TEST("camera — negative pitch tilts forward downward");
    quat orient = quat_from_euler(0.0f, -0.3f, 0.0f);
    vec3 forward = quat_forward(orient);

    /* With negative pitch, forward.y should be negative (looking down) */
    ASSERT_TRUE(forward.y < 0.0f);
    /* Still mostly along -Z */
    ASSERT_TRUE(forward.z < -0.5f);
    END_TEST();
}

static void test_camera_yaw_turns_right(void)
{
    TEST("camera — negative yaw (from mouse right) turns camera right");
    /* Yaw decrements on positive xrel (mouse right), so negative yaw
     * should rotate the view rightward (forward gets +X component) */
    float yaw = -1.57f; /* approximately -pi/2 */
    quat orient = quat_from_euler(yaw, 0.0f, 0.0f);
    vec3 forward = quat_forward(orient);

    /* After ~90 deg right turn, forward should be along +X */
    ASSERT_NEAR(forward.x, 1.0f, 0.05f);
    ASSERT_NEAR(forward.z, 0.0f, 0.05f);
    END_TEST();
}

static void test_camera_view_matrix(void)
{
    TEST("camera — view matrix transforms cam_pos to near origin");
    vec3 cam_pos = vec3_create(DEF_CAM_POS_X, DEF_CAM_POS_Y, DEF_CAM_POS_Z);
    quat orient = quat_from_euler(DEF_CAM_YAW, DEF_CAM_PITCH, 0.0f);
    mat4 view = mat4_view_from_quat(cam_pos, orient);

    /* The camera position in view space should be at origin */
    vec4 cam_view;
    cam_view = mat4_multiply_vec4(view,
        vec4_create(cam_pos.x, cam_pos.y, cam_pos.z, 1.0f));
    ASSERT_NEAR(cam_view.x, 0.0f, EPSILON);
    ASSERT_NEAR(cam_view.y, 0.0f, EPSILON);
    ASSERT_NEAR(cam_view.z, 0.0f, EPSILON);
    END_TEST();
}

static void test_camera_perspective_aspect(void)
{
    TEST("camera — perspective matrix has correct aspect ratio");
    float aspect = 16.0f / 9.0f;
    mat4 proj = mat4_perspective(
        DEF_FOV_DEG * FORGE_DEG2RAD, aspect, DEF_NEAR, DEF_FAR);

    /* proj.m[0] = 1 / (aspect * tan(fov/2))
     * proj.m[5] = 1 / tan(fov/2)
     * ratio m[5]/m[0] should equal aspect */
    float ratio = proj.m[5] / proj.m[0];
    ASSERT_NEAR(ratio, aspect, EPSILON);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 5. Struct Sizes and Layout
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_struct_scene_vertex_size(void)
{
    TEST("struct — ForgeSceneVertex is 24 bytes (2 x vec3)");
    ASSERT_INT_EQ((int)sizeof(ForgeSceneVertex), 24);
    END_TEST();
}

static void test_struct_vert_uniforms_size(void)
{
    TEST("struct — ForgeSceneVertUniforms is 192 bytes (3 x mat4)");
    ASSERT_INT_EQ((int)sizeof(ForgeSceneVertUniforms), 192);
    END_TEST();
}

static void test_struct_frag_uniforms_size(void)
{
    TEST("struct — ForgeSceneFragUniforms is 80 bytes");
    ASSERT_INT_EQ((int)sizeof(ForgeSceneFragUniforms), 80);
    END_TEST();
}

/* Expected ABI sizes for textured pipeline structs — must match the HLSL
 * cbuffer layouts.  Named constants prevent magic-number lint warnings and
 * document the contract between C and shader code. */
#define TEXTURED_VERTEX_SIZE             32  /* vec3 pos + vec3 normal + vec2 uv */
#define TEXTURED_VERTEX_POSITION_OFFSET   0
#define TEXTURED_VERTEX_NORMAL_OFFSET    12  /* sizeof(vec3) */
#define TEXTURED_VERTEX_UV_OFFSET        24  /* 2 * sizeof(vec3) */
#define TEXTURED_FRAG_UNIFORMS_SIZE      80  /* matches scene_textured.frag.hlsl cbuffer */

static void test_struct_textured_vertex_size(void)
{
    TEST("struct — ForgeSceneTexturedVertex is 32 bytes (pos+normal+uv)");
    ASSERT_INT_EQ((int)sizeof(ForgeSceneTexturedVertex),
                  TEXTURED_VERTEX_SIZE);
    ASSERT_INT_EQ((int)offsetof(ForgeSceneTexturedVertex, position),
                  TEXTURED_VERTEX_POSITION_OFFSET);
    ASSERT_INT_EQ((int)offsetof(ForgeSceneTexturedVertex, normal),
                  TEXTURED_VERTEX_NORMAL_OFFSET);
    ASSERT_INT_EQ((int)offsetof(ForgeSceneTexturedVertex, uv),
                  TEXTURED_VERTEX_UV_OFFSET);
    END_TEST();
}

static void test_struct_textured_frag_uniforms_size(void)
{
    TEST("struct — ForgeSceneTexturedFragUniforms is 80 bytes");
    ASSERT_INT_EQ((int)sizeof(ForgeSceneTexturedFragUniforms),
                  TEXTURED_FRAG_UNIFORMS_SIZE);
    END_TEST();
}

static void test_struct_grid_vert_uniforms_size(void)
{
    TEST("struct — ForgeSceneGridVertUniforms is 128 bytes (2 x mat4)");
    ASSERT_INT_EQ((int)sizeof(ForgeSceneGridVertUniforms), 128);
    END_TEST();
}

static void test_struct_grid_frag_uniforms_size(void)
{
    TEST("struct — ForgeSceneGridFragUniforms is 80 bytes");
    ASSERT_INT_EQ((int)sizeof(ForgeSceneGridFragUniforms), 80);
    END_TEST();
}

static void test_struct_shadow_uniforms_size(void)
{
    TEST("struct — ForgeSceneShadowVertUniforms is 64 bytes (1 x mat4)");
    ASSERT_INT_EQ((int)sizeof(ForgeSceneShadowVertUniforms), 64);
    END_TEST();
}

static void test_struct_ui_uniforms_size(void)
{
    TEST("struct — ForgeSceneUiUniforms is 64 bytes (1 x mat4)");
    ASSERT_INT_EQ((int)sizeof(ForgeSceneUiUniforms), 64);
    END_TEST();
}

static void test_struct_vertex_offsets(void)
{
    TEST("struct — ForgeSceneVertex position at offset 0, normal at 12");
    ASSERT_INT_EQ((int)offsetof(ForgeSceneVertex, position), 0);
    ASSERT_INT_EQ((int)offsetof(ForgeSceneVertex, normal), 12);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 9. GPU Integration Tests (require a Vulkan device)
 * ══════════════════════════════════════════════════════════════════════════ */

/* Check if a GPU device can be created — sets gpu_available flag */
static void check_gpu_availability(void)
{
    SDL_GPUDevice *dev = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
        true, NULL);
    if (dev) {
        gpu_available = true;
        SDL_DestroyGPUDevice(dev);
    } else {
        SDL_Log("  GPU not available — skipping GPU integration tests: %s",
                SDL_GetError());
    }
}

static void test_gpu_init_destroy_no_font(void)
{
    if (!gpu_available) {
        TEST("gpu — init/destroy without font (SKIPPED)");
        SKIP_TEST();
        return;
    }
    TEST_C("gpu — init/destroy without font");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("Test Init");
    /* No font_path — UI stays disabled */
    char *argv[] = { "test_scene", NULL };
    bool ok = forge_scene_init(&scene, &cfg, 1, argv);
    ASSERT_TRUE_C(ok);
    ASSERT_TRUE_C(scene.device != NULL);
    ASSERT_TRUE_C(scene.window != NULL);
    ASSERT_TRUE_C(scene.scene_pipeline != NULL);
    ASSERT_TRUE_C(scene.shadow_pipeline != NULL);
    ASSERT_TRUE_C(scene.shadow_pipeline_pos != NULL);
    ASSERT_TRUE_C(scene.shadow_pipeline_tex != NULL);
    ASSERT_TRUE_C(scene.textured_pipeline != NULL);
    ASSERT_TRUE_C(scene.grid_pipeline != NULL);
    ASSERT_TRUE_C(scene.sky_pipeline != NULL);
    ASSERT_TRUE_C(!scene.ui_enabled);
    /* Accessors should work */
    ASSERT_TRUE_C(forge_scene_device(&scene) == scene.device);
    ASSERT_NEAR_C(forge_scene_dt(&scene), 0.0f, EPSILON);
    /* Camera should be at config start position */
    vec3 pos = forge_scene_cam_pos(&scene);
    ASSERT_NEAR_C(pos.x, DEF_CAM_POS_X, EPSILON);
    ASSERT_NEAR_C(pos.y, DEF_CAM_POS_Y, EPSILON);
    ASSERT_NEAR_C(pos.z, DEF_CAM_POS_Z, EPSILON);
cleanup:
    forge_scene_destroy(&scene);
    END_TEST_C();
}

static void test_gpu_init_with_font(void)
{
    if (!gpu_available) {
        TEST("gpu — init with font enables UI (SKIPPED)");
        SKIP_TEST();
        return;
    }

#ifdef FONT_PATH
    TEST_C("gpu — init with font enables UI");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("Test Font");
    cfg.font_path = FONT_PATH;
    char *argv[] = { "test_scene", NULL };
    bool ok = forge_scene_init(&scene, &cfg, 1, argv);
    ASSERT_TRUE_C(ok);
    ASSERT_TRUE_C(scene.ui_enabled);
    ASSERT_TRUE_C(scene.ui_atlas_texture != NULL);
    ASSERT_TRUE_C(scene.ui_pipeline != NULL);
cleanup:
    forge_scene_destroy(&scene);
    END_TEST_C();
#else
    TEST("gpu — init with font enables UI (SKIPPED — no FONT_PATH)");
    SKIP_TEST();
#endif
}

static void test_gpu_init_custom_config(void)
{
    if (!gpu_available) {
        TEST("gpu — init with custom config (SKIPPED)");
        SKIP_TEST();
        return;
    }
    TEST_C("gpu — init with custom camera position");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("Custom");
    cfg.cam_start_pos = vec3_create(CUSTOM_CAM_X, CUSTOM_CAM_Y, CUSTOM_CAM_Z);
    cfg.cam_start_yaw = CUSTOM_YAW;
    cfg.cam_start_pitch = CUSTOM_PITCH;
    char *argv[] = { "test_scene", NULL };
    bool ok = forge_scene_init(&scene, &cfg, 1, argv);
    ASSERT_TRUE_C(ok);
    vec3 pos = forge_scene_cam_pos(&scene);
    ASSERT_NEAR_C(pos.x, CUSTOM_CAM_X, EPSILON);
    ASSERT_NEAR_C(pos.y, CUSTOM_CAM_Y, EPSILON);
    ASSERT_NEAR_C(pos.z, CUSTOM_CAM_Z, EPSILON);
    /* Light VP should be valid (not zero matrix) */
    mat4 lvp = forge_scene_light_vp_mat(&scene);
    ASSERT_TRUE_C(SDL_fabsf(lvp.m[0]) > EPSILON ||
                  SDL_fabsf(lvp.m[5]) > EPSILON);
cleanup:
    forge_scene_destroy(&scene);
    END_TEST_C();
}

static void test_gpu_accessors(void)
{
    if (!gpu_available) {
        TEST("gpu — accessor functions (SKIPPED)");
        SKIP_TEST();
        return;
    }
    TEST_C("gpu — accessor functions return correct values");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("Accessors");
    char *argv[] = { "test_scene", NULL };
    bool ok = forge_scene_init(&scene, &cfg, 1, argv);
    ASSERT_TRUE_C(ok);

    /* forge_scene_device */
    ASSERT_TRUE_C(forge_scene_device(&scene) != NULL);

    /* forge_scene_dt — should be 0 before any frame */
    ASSERT_NEAR_C(forge_scene_dt(&scene), 0.0f, EPSILON);

    /* forge_scene_cam_pos */
    vec3 cam = forge_scene_cam_pos(&scene);
    ASSERT_NEAR_C(cam.x, DEF_CAM_POS_X, EPSILON);
    ASSERT_NEAR_C(cam.y, DEF_CAM_POS_Y, EPSILON);
    ASSERT_NEAR_C(cam.z, DEF_CAM_POS_Z, EPSILON);

    /* forge_scene_swapchain_format — should be a valid format */
    SDL_GPUTextureFormat fmt = forge_scene_swapchain_format(&scene);
    ASSERT_TRUE_C(fmt != SDL_GPU_TEXTUREFORMAT_INVALID);

cleanup:
    forge_scene_destroy(&scene);
    END_TEST_C();
}

static void test_gpu_light_vp_matches_manual(void)
{
    if (!gpu_available) {
        TEST("gpu — light_vp matches manual computation (SKIPPED)");
        SKIP_TEST();
        return;
    }
    TEST_C("gpu — light_vp matches manual computation");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("LightVP");
    char *argv[] = { "test_scene", NULL };
    bool ok = forge_scene_init(&scene, &cfg, 1, argv);
    ASSERT_TRUE_C(ok);

    /* Manually compute what the light VP should be */
    vec3 light_dir = vec3_normalize(cfg.light_dir);
    vec3 light_pos = vec3_scale(light_dir, cfg.shadow_height);
    mat4 lv = mat4_look_at(
        light_pos,
        vec3_create(0.0f, 0.0f, 0.0f),
        vec3_create(0.0f, 1.0f, 0.0f));
    mat4 lp = mat4_orthographic(
        -cfg.shadow_ortho_size, cfg.shadow_ortho_size,
        -cfg.shadow_ortho_size, cfg.shadow_ortho_size,
        cfg.shadow_near, cfg.shadow_far);
    mat4 expected = mat4_multiply(lp, lv);

    mat4 actual = forge_scene_light_vp_mat(&scene);

    /* Compare all 16 elements */
    for (int i = 0; i < 16; i++) {
        ASSERT_NEAR_C(actual.m[i], expected.m[i], EPSILON);
    }

cleanup:
    forge_scene_destroy(&scene);
    END_TEST_C();
}

static void test_gpu_destroy_is_idempotent(void)
{
    if (!gpu_available) {
        TEST("gpu — destroy is idempotent (SKIPPED)");
        SKIP_TEST();
        return;
    }
    TEST_C("gpu — destroy is idempotent (double destroy)");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("Idempotent Test");
    char *argv[] = { "test_scene", NULL };
    ASSERT_TRUE_C(forge_scene_init(&scene, &cfg, 1, argv));
cleanup:
    forge_scene_destroy(&scene);
    forge_scene_destroy(&scene);  /* second call must be safe */
    END_TEST_C();
}

/* ── 9b. Custom pipeline creation and draw_mesh_ex ─────────────────────── */

static void test_gpu_create_pipeline_wireframe(void)
{
    if (!gpu_available) {
        TEST("gpu — create_pipeline wireframe (SKIPPED)");
        SKIP_TEST();
        return;
    }
    TEST_C("gpu — create_pipeline returns non-NULL wireframe pipeline");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("Pipeline Test");
    char *argv[] = { "test_scene", NULL };
    /* Initialize before any ASSERT_TRUE_C that could goto cleanup */
    SDL_GPUGraphicsPipeline *wireframe = NULL;
    SDL_GPUGraphicsPipeline *double_sided = NULL;

    ASSERT_TRUE_C(forge_scene_init(&scene, &cfg, 1, argv));

    wireframe = forge_scene_create_pipeline(
        &scene, SDL_GPU_CULLMODE_NONE, SDL_GPU_FILLMODE_LINE);
    ASSERT_TRUE_C(wireframe != NULL);

    double_sided = forge_scene_create_pipeline(
        &scene, SDL_GPU_CULLMODE_NONE, SDL_GPU_FILLMODE_FILL);
    ASSERT_TRUE_C(double_sided != NULL);

    /* Pipelines should be distinct objects */
    ASSERT_TRUE_C(wireframe != double_sided);
    ASSERT_TRUE_C(wireframe != scene.scene_pipeline);

cleanup:
    if (wireframe)
        SDL_ReleaseGPUGraphicsPipeline(scene.device, wireframe);
    if (double_sided)
        SDL_ReleaseGPUGraphicsPipeline(scene.device, double_sided);
    forge_scene_destroy(&scene);
    END_TEST_C();
}

static void test_gpu_create_pipeline_null_scene(void)
{
    TEST("gpu — create_pipeline with NULL scene returns NULL")
    SDL_GPUGraphicsPipeline *p = forge_scene_create_pipeline(
        NULL, SDL_GPU_CULLMODE_BACK, SDL_GPU_FILLMODE_FILL);
    ASSERT_TRUE(p == NULL);
    END_TEST();
}

static void test_gpu_create_pipeline_uninit_scene(void)
{
    TEST("gpu — create_pipeline with uninitialized scene returns NULL")
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    /* scene_vs and scene_fs are NULL — not initialized */
    SDL_GPUGraphicsPipeline *p = forge_scene_create_pipeline(
        &scene, SDL_GPU_CULLMODE_BACK, SDL_GPU_FILLMODE_FILL);
    ASSERT_TRUE(p == NULL);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 6. Robustness — tests for fixes applied during PR review
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── 7a. Light direction degenerate inputs (GPU — exercises real init) ──── */

static void test_light_dir_zero_uses_default(void)
{
    if (!gpu_available) {
        TEST("gpu — zero light_dir falls back to default (SKIPPED)");
        SKIP_TEST();
        return;
    }
    TEST_C("gpu — zero light_dir falls back to default");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("Light Test");
    cfg.light_dir = vec3_create(0.0f, 0.0f, 0.0f);
    char *argv[] = { "test_scene", NULL };
    ASSERT_TRUE_C(forge_scene_init(&scene, &cfg, 1, argv));
    /* Init should have fallen back to a valid normalized direction */
    ASSERT_NEAR_C(vec3_length(scene.light_dir), 1.0f, EPSILON);
    /* Verify fallback matches the default light direction */
    vec3 expected = vec3_normalize(vec3_create(0.4f, 0.8f, 0.6f));
    ASSERT_NEAR_C(scene.light_dir.x, expected.x, EPSILON);
    ASSERT_NEAR_C(scene.light_dir.y, expected.y, EPSILON);
    ASSERT_NEAR_C(scene.light_dir.z, expected.z, EPSILON);
cleanup:
    forge_scene_destroy(&scene);
    END_TEST_C();
}

static void test_light_dir_straight_up_uses_alt_up(void)
{
    if (!gpu_available) {
        TEST("gpu — light_dir (0,1,0) init succeeds with alt up (SKIPPED)");
        SKIP_TEST();
        return;
    }
    TEST_C("gpu — light_dir (0,1,0) init succeeds with alt up");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("Light Up Test");
    cfg.light_dir = vec3_create(0.0f, 1.0f, 0.0f);
    char *argv[] = { "test_scene", NULL };
    ASSERT_TRUE_C(forge_scene_init(&scene, &cfg, 1, argv));
    /* Dir should be normalized */
    ASSERT_NEAR_C(vec3_length(scene.light_dir), 1.0f, EPSILON);
    /* Light VP should be valid (not degenerate) — project origin */
    mat4 lvp = forge_scene_light_vp_mat(&scene);
    vec4 clip = mat4_multiply_vec4(lvp, vec4_create(0.0f, 0.0f, 0.0f, 1.0f));
    /* All components must be finite (rejects both NaN and +/-Inf) */
    ASSERT_TRUE_C(forge_isfinite(clip.x));
    ASSERT_TRUE_C(forge_isfinite(clip.y));
    ASSERT_TRUE_C(forge_isfinite(clip.z));
    ASSERT_TRUE_C(forge_isfinite(clip.w) && clip.w > 0.0f);
cleanup:
    forge_scene_destroy(&scene);
    END_TEST_C();
}

static void test_light_dir_straight_down_uses_alt_up(void)
{
    if (!gpu_available) {
        TEST("gpu — light_dir (0,-1,0) init succeeds with alt up (SKIPPED)");
        SKIP_TEST();
        return;
    }
    TEST_C("gpu — light_dir (0,-1,0) init succeeds with alt up");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("Light Down Test");
    cfg.light_dir = vec3_create(0.0f, -1.0f, 0.0f);
    char *argv[] = { "test_scene", NULL };
    ASSERT_TRUE_C(forge_scene_init(&scene, &cfg, 1, argv));
    /* Dir should be normalized */
    ASSERT_NEAR_C(vec3_length(scene.light_dir), 1.0f, EPSILON);
    /* Light VP should be valid (not degenerate) — project origin */
    mat4 lvp = forge_scene_light_vp_mat(&scene);
    vec4 clip = mat4_multiply_vec4(lvp, vec4_create(0.0f, 0.0f, 0.0f, 1.0f));
    /* All components must be finite (rejects both NaN and +/-Inf) */
    ASSERT_TRUE_C(forge_isfinite(clip.x));
    ASSERT_TRUE_C(forge_isfinite(clip.y));
    ASSERT_TRUE_C(forge_isfinite(clip.z));
    ASSERT_TRUE_C(forge_isfinite(clip.w) && clip.w > 0.0f);
cleanup:
    forge_scene_destroy(&scene);
    END_TEST_C();
}

static void test_light_dir_diagonal_keeps_default_up(void)
{
    if (!gpu_available) {
        TEST("gpu — diagonal light_dir init succeeds and is normalized (SKIPPED)");
        SKIP_TEST();
        return;
    }
    TEST_C("gpu — diagonal light_dir init succeeds and is normalized");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("Light Diag Test");
    cfg.light_dir = vec3_create(LIGHT_DIR_X, LIGHT_DIR_Y, LIGHT_DIR_Z);
    char *argv[] = { "test_scene", NULL };
    ASSERT_TRUE_C(forge_scene_init(&scene, &cfg, 1, argv));
    /* Dir should be normalized */
    ASSERT_NEAR_C(vec3_length(scene.light_dir), 1.0f, EPSILON);
cleanup:
    forge_scene_destroy(&scene);
    END_TEST_C();
}

/* ── 7b. UI resource cleanup independent of ui_enabled ───────────────────── */

static void test_destroy_without_ui_zeroes_scene(void)
{
    if (!gpu_available) {
        TEST("robustness — destroy without UI zeroes scene (SKIPPED)");
        SKIP_TEST();
        return;
    }
    TEST_C("robustness — destroy without UI zeroes scene");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    ForgeSceneConfig cfg = forge_scene_default_config("No UI Test");
    /* No font_path = UI disabled */
    char *argv[] = { "test_scene", NULL };
    ASSERT_TRUE_C(forge_scene_init(&scene, &cfg, 1, argv));
    ASSERT_TRUE_C(!scene.ui_enabled);
cleanup:
    forge_scene_destroy(&scene);
    /* After destroy, owned pointers/state from the init path should be reset */
    if (scene.device != NULL || scene.window != NULL ||
        scene.scene_pipeline != NULL || scene.shadow_pipeline != NULL ||
        scene.shadow_pipeline_pos != NULL ||
        scene.shadow_pipeline_tex != NULL ||
        scene.textured_pipeline != NULL ||
        scene.grid_pipeline != NULL || scene.sky_pipeline != NULL ||
        scene.ui_pipeline != NULL || scene.ui_atlas_texture != NULL ||
        scene.ui_enabled) {
        SDL_Log("    FAIL: scene not fully reset after destroy");
        fail_count++;
        _test_failed = 1;
    }
    END_TEST_C();
}

/* ── 7c. Stage entry guards for NULL cmd ─────────────────────────────────── */

static void test_shadow_pass_null_cmd_is_safe(void)
{
    TEST("robustness — begin_shadow_pass with NULL cmd does not crash");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.cmd = NULL;
    /* Should early-return without touching NULL cmd */
    forge_scene_begin_shadow_pass(&scene);
    ASSERT_TRUE(scene.cmd == NULL);
    END_TEST();
}

static void test_main_pass_null_cmd_is_safe(void)
{
    TEST("robustness — begin_main_pass with NULL cmd does not crash");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.cmd = NULL;
    forge_scene_begin_main_pass(&scene);
    ASSERT_TRUE(scene.cmd == NULL);
    END_TEST();
}

static void test_end_ui_null_cmd_is_safe(void)
{
    TEST("robustness — end_ui with NULL cmd does not crash");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.ui_enabled = true;
    scene.cmd = NULL;
    forge_scene_end_ui(&scene);
    ASSERT_TRUE(scene.cmd == NULL);
    END_TEST();
}

/* ── 7d. Public helper NULL guards ────────────────────────────────────── */

static void test_create_shader_null_scene(void)
{
    TEST("robustness — create_shader with NULL scene returns NULL");
    SDL_GPUShader *s = forge_scene_create_shader(
        NULL, SDL_GPU_SHADERSTAGE_VERTEX, NULL, 0, NULL, 0, NULL, 0, 0, 0, 0, 0);
    ASSERT_TRUE(s == NULL);
    END_TEST();
}

static void test_create_shader_null_device(void)
{
    TEST("robustness — create_shader with NULL device returns NULL");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    SDL_GPUShader *s = forge_scene_create_shader(
        &scene, SDL_GPU_SHADERSTAGE_VERTEX, NULL, 0, NULL, 0, NULL, 0, 0, 0, 0, 0);
    ASSERT_TRUE(s == NULL);
    END_TEST();
}

static void test_upload_buffer_null_scene(void)
{
    TEST("robustness — upload_buffer with NULL scene returns NULL");
    SDL_GPUBuffer *b = forge_scene_upload_buffer(NULL, 0, NULL, 0);
    ASSERT_TRUE(b == NULL);
    END_TEST();
}

static void test_upload_buffer_null_data(void)
{
    TEST("robustness — upload_buffer with NULL data and size>0 returns NULL");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    /* Set a fake non-NULL device so the NULL-data guard is what triggers,
     * not the NULL-device guard. */
    scene.device = (SDL_GPUDevice *)1;
    SDL_GPUBuffer *b = forge_scene_upload_buffer(&scene, 0, NULL, 64);
    ASSERT_TRUE(b == NULL);
    END_TEST();
}

static void test_upload_texture_null_scene(void)
{
    TEST("robustness — upload_texture with NULL scene returns NULL");
    SDL_GPUTexture *t = forge_scene_upload_texture(NULL, NULL, false);
    ASSERT_TRUE(t == NULL);
    END_TEST();
}

static void test_upload_texture_null_surface(void)
{
    TEST("robustness — upload_texture with NULL surface returns NULL");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.device = (SDL_GPUDevice *)1; /* fake non-NULL device */
    SDL_GPUTexture *t = forge_scene_upload_texture(&scene, NULL, false);
    ASSERT_TRUE(t == NULL);
    END_TEST();
}

static void test_upload_buffer_zero_size(void)
{
    TEST("robustness — upload_buffer with size 0 returns NULL");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.device = (SDL_GPUDevice *)1; /* fake non-NULL device */
    Uint8 dummy = 0;
    SDL_GPUBuffer *b = forge_scene_upload_buffer(&scene, 0, &dummy, 0);
    ASSERT_TRUE(b == NULL);
    END_TEST();
}

static void test_draw_shadow_mesh_null_buffers_is_safe(void)
{
    TEST("robustness — draw_shadow_mesh with NULL buffers does not crash");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.pass = (SDL_GPURenderPass *)1;
    scene.shadow_pipeline = (SDL_GPUGraphicsPipeline *)1;
    forge_scene_draw_shadow_mesh(&scene, NULL, NULL, 0, mat4_identity());
    /* reached here without crashing */
    END_TEST();
}

static void test_draw_shadow_mesh_pos_null_buffers_is_safe(void)
{
    TEST("robustness — draw_shadow_mesh_pos with NULL buffers does not crash");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.pass = (SDL_GPURenderPass *)1;
    scene.shadow_pipeline = (SDL_GPUGraphicsPipeline *)1;
    scene.shadow_pipeline_pos = (SDL_GPUGraphicsPipeline *)1;
    forge_scene_draw_shadow_mesh_pos(&scene, NULL, NULL, 0, mat4_identity());
    /* reached here without crashing */
    END_TEST();
}

static void test_draw_shadow_mesh_pos_zero_count_is_safe(void)
{
    TEST("robustness — draw_shadow_mesh_pos with zero index count is a no-op");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.pass = (SDL_GPURenderPass *)1;
    scene.shadow_pipeline = (SDL_GPUGraphicsPipeline *)1;
    scene.shadow_pipeline_pos = (SDL_GPUGraphicsPipeline *)1;
    /* Non-NULL buffers but zero count — should early-return without drawing */
    forge_scene_draw_shadow_mesh_pos(&scene, (SDL_GPUBuffer *)1,
                                      (SDL_GPUBuffer *)1, 0, mat4_identity());
    END_TEST();
}

static void test_draw_shadow_mesh_pos_null_pass_is_safe(void)
{
    TEST("robustness — draw_shadow_mesh_pos with NULL pass is a no-op");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.shadow_pipeline = (SDL_GPUGraphicsPipeline *)1;
    scene.shadow_pipeline_pos = (SDL_GPUGraphicsPipeline *)1;
    /* pass is NULL (zeroed) — should early-return */
    forge_scene_draw_shadow_mesh_pos(&scene, (SDL_GPUBuffer *)1,
                                      (SDL_GPUBuffer *)1, 36, mat4_identity());
    END_TEST();
}

static void test_draw_shadow_textured_mesh_null_buffers_is_safe(void)
{
    TEST("robustness — draw_shadow_textured_mesh with NULL buffers does not crash");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.pass = (SDL_GPURenderPass *)1;
    scene.shadow_pipeline = (SDL_GPUGraphicsPipeline *)1;
    scene.shadow_pipeline_tex = (SDL_GPUGraphicsPipeline *)1;
    forge_scene_draw_shadow_textured_mesh(&scene, NULL, NULL, 0, mat4_identity());
    END_TEST();
}

static void test_draw_textured_mesh_null_buffers_is_safe(void)
{
    TEST("robustness — draw_textured_mesh with NULL buffers does not crash");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.pass = (SDL_GPURenderPass *)1;
    scene.textured_pipeline = (SDL_GPUGraphicsPipeline *)1;
    float uv[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    forge_scene_draw_textured_mesh(&scene, NULL, NULL, 0,
                                    mat4_identity(), NULL, NULL, uv);
    END_TEST();
}

static void test_draw_textured_mesh_null_pass_is_safe(void)
{
    TEST("robustness — draw_textured_mesh with NULL pass is a no-op");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    float uv[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    forge_scene_draw_textured_mesh(&scene, (SDL_GPUBuffer *)1,
                                    (SDL_GPUBuffer *)1, 36,
                                    mat4_identity(),
                                    (SDL_GPUTexture *)1,
                                    (SDL_GPUSampler *)1, uv);
    END_TEST();
}

static void test_draw_textured_mesh_null_uv_transform_is_safe(void)
{
    TEST("robustness — draw_textured_mesh with NULL uv_transform is a no-op");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.pass = (SDL_GPURenderPass *)1;
    scene.textured_pipeline = (SDL_GPUGraphicsPipeline *)1;
    forge_scene_draw_textured_mesh(&scene, (SDL_GPUBuffer *)1,
                                    (SDL_GPUBuffer *)1, 36,
                                    mat4_identity(),
                                    (SDL_GPUTexture *)1,
                                    (SDL_GPUSampler *)1, NULL);
    END_TEST();
}

static void test_bind_textured_resources_null_texture_is_safe(void)
{
    TEST("robustness — bind_textured_resources with NULL texture returns false");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.pass = (SDL_GPURenderPass *)1;
    scene.textured_pipeline = (SDL_GPUGraphicsPipeline *)1;
    ASSERT_TRUE(!forge_scene_bind_textured_resources(&scene, NULL,
                                                      (SDL_GPUSampler *)1));
    END_TEST();
}

static void test_draw_textured_mesh_no_bind_null_buffers_is_safe(void)
{
    TEST("robustness — draw_textured_mesh_no_bind with NULL buffers is a no-op");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.pass = (SDL_GPURenderPass *)1;
    scene.textured_pipeline = (SDL_GPUGraphicsPipeline *)1;
    float uv[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    forge_scene_draw_textured_mesh_no_bind(&scene, NULL, NULL, 0,
                                            mat4_identity(), uv);
    /* reached here without crashing */
    END_TEST();
}

static void test_draw_textured_mesh_no_bind_null_uv_is_safe(void)
{
    TEST("robustness — draw_textured_mesh_no_bind with NULL uv_transform is a no-op");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.pass = (SDL_GPURenderPass *)1;
    scene.textured_pipeline = (SDL_GPUGraphicsPipeline *)1;
    forge_scene_draw_textured_mesh_no_bind(&scene, (SDL_GPUBuffer *)1,
                                            (SDL_GPUBuffer *)1, 36,
                                            mat4_identity(), NULL);
    END_TEST();
}

static void test_draw_mesh_null_buffers_is_safe(void)
{
    TEST("robustness — draw_mesh with NULL buffers does not crash");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.pass = (SDL_GPURenderPass *)1;
    float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    forge_scene_draw_mesh(&scene, NULL, NULL, 0, mat4_identity(), color);
    /* reached here without crashing */
    END_TEST();
}

static void test_draw_mesh_zero_index_count_is_safe(void)
{
    TEST("robustness — draw_mesh with zero index_count does not crash");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    scene.pass = (SDL_GPURenderPass *)1;
    float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    /* Non-NULL pass + buffers, but zero index_count should be a no-op */
    forge_scene_draw_mesh(&scene, (SDL_GPUBuffer *)1, (SDL_GPUBuffer *)1,
                          0, mat4_identity(), color);
    /* reached here without crashing */
    END_TEST();
}

static void test_destroy_null_scene_is_safe(void)
{
    TEST("robustness — destroy(NULL) does not crash");
    forge_scene_destroy(NULL);
    /* reached here without crashing */
    END_TEST();
}

static void test_destroy_uninitialized_zeroed_scene(void)
{
    TEST("robustness — destroy zeroed (never init'd) scene is safe");
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    forge_scene_destroy(&scene);
    /* reached here without crashing */
    END_TEST();
}

/* ── 7. Config validation ─────────────────────────────────────────────── */

static void test_validate_default_config_passes(void)
{
    TEST("default config passes validation");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    ASSERT_TRUE(forge_scene__validate_config(&cfg));
    END_TEST();
}

static void test_validate_zero_shadow_map_size(void)
{
    TEST("zero shadow_map_size rejected");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    cfg.shadow_map_size = 0;
    ASSERT_TRUE(!forge_scene__validate_config(&cfg));
    END_TEST();
}

static void test_validate_negative_shadow_map_size(void)
{
    TEST("negative shadow_map_size rejected");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    cfg.shadow_map_size = -1;
    ASSERT_TRUE(!forge_scene__validate_config(&cfg));
    END_TEST();
}

static void test_validate_zero_fov(void)
{
    TEST("zero fov_deg rejected");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    cfg.fov_deg = 0.0f;
    ASSERT_TRUE(!forge_scene__validate_config(&cfg));
    END_TEST();
}

static void test_validate_fov_too_large(void)
{
    TEST("fov_deg >= 180 rejected");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    cfg.fov_deg = 180.0f;
    ASSERT_TRUE(!forge_scene__validate_config(&cfg));
    END_TEST();
}

static void test_validate_negative_near_plane(void)
{
    TEST("negative near_plane rejected");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    cfg.near_plane = -1.0f;
    ASSERT_TRUE(!forge_scene__validate_config(&cfg));
    END_TEST();
}

static void test_validate_near_equals_far(void)
{
    TEST("near_plane == far_plane rejected");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    cfg.near_plane = 100.0f;
    cfg.far_plane = 100.0f;
    ASSERT_TRUE(!forge_scene__validate_config(&cfg));
    END_TEST();
}

static void test_validate_inverted_near_far(void)
{
    TEST("near_plane > far_plane rejected");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    cfg.near_plane = 100.0f;
    cfg.far_plane = 1.0f;
    ASSERT_TRUE(!forge_scene__validate_config(&cfg));
    END_TEST();
}

static void test_validate_zero_shadow_ortho(void)
{
    TEST("zero shadow_ortho_size rejected");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    cfg.shadow_ortho_size = 0.0f;
    ASSERT_TRUE(!forge_scene__validate_config(&cfg));
    END_TEST();
}

static void test_validate_inverted_shadow_near_far(void)
{
    TEST("shadow_near > shadow_far rejected");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    cfg.shadow_near = 50.0f;
    cfg.shadow_far = 1.0f;
    ASSERT_TRUE(!forge_scene__validate_config(&cfg));
    END_TEST();
}

static void test_validate_zero_shadow_height(void)
{
    TEST("zero shadow_height rejected");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    cfg.shadow_height = 0.0f;
    ASSERT_TRUE(!forge_scene__validate_config(&cfg));
    END_TEST();
}

static void test_validate_negative_shadow_height(void)
{
    TEST("negative shadow_height rejected");
    ForgeSceneConfig cfg = forge_scene_default_config("Test");
    cfg.shadow_height = -5.0f;
    ASSERT_TRUE(!forge_scene__validate_config(&cfg));
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * 8. NaN / Infinity Robustness
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_validate_rejects_nan_fov(void)
{
    TEST("config validation rejects NaN fov_deg");
    ForgeSceneConfig cfg = forge_scene_default_config("NaN FOV");
    cfg.fov_deg = forge_test_nan();
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    char *argv[] = { "test", NULL };
    bool inited = forge_scene_init(&scene, &cfg, 1, argv);
    if (inited) forge_scene_destroy(&scene);
    ASSERT_TRUE(!inited);
    END_TEST();
}

static void test_validate_rejects_inf_near_plane(void)
{
    TEST("config validation rejects Inf near_plane");
    ForgeSceneConfig cfg = forge_scene_default_config("Inf Near");
    cfg.near_plane = forge_test_inf();
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    char *argv[] = { "test", NULL };
    bool inited = forge_scene_init(&scene, &cfg, 1, argv);
    if (inited) forge_scene_destroy(&scene);
    ASSERT_TRUE(!inited);
    END_TEST();
}

static void test_validate_rejects_nan_shadow_height(void)
{
    TEST("config validation rejects NaN shadow_height");
    ForgeSceneConfig cfg = forge_scene_default_config("NaN Shadow");
    cfg.shadow_height = forge_test_nan();
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    char *argv[] = { "test", NULL };
    bool inited = forge_scene_init(&scene, &cfg, 1, argv);
    if (inited) forge_scene_destroy(&scene);
    ASSERT_TRUE(!inited);
    END_TEST();
}

static void test_validate_rejects_neg_inf_far_plane(void)
{
    TEST("config validation rejects -Inf far_plane");
    ForgeSceneConfig cfg = forge_scene_default_config("NegInf Far");
    cfg.far_plane = -forge_test_inf();
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    char *argv[] = { "test", NULL };
    bool inited = forge_scene_init(&scene, &cfg, 1, argv);
    if (inited) forge_scene_destroy(&scene);
    ASSERT_TRUE(!inited);
    END_TEST();
}

static void test_validate_rejects_nan_cam_start_pos(void)
{
    TEST("config validation rejects NaN cam_start_pos");
    ForgeSceneConfig cfg = forge_scene_default_config("NaN Cam");
    cfg.cam_start_pos = vec3_create(forge_test_nan(), 2.0f, 5.0f);
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    char *argv[] = { "test", NULL };
    bool inited = forge_scene_init(&scene, &cfg, 1, argv);
    if (inited) forge_scene_destroy(&scene);
    ASSERT_TRUE(!inited);
    END_TEST();
}

static void test_validate_rejects_nan_light_dir(void)
{
    TEST("config validation rejects NaN light_dir");
    ForgeSceneConfig cfg = forge_scene_default_config("NaN Light");
    cfg.light_dir = vec3_create(forge_test_nan(), 0.8f, 0.6f);
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    char *argv[] = { "test", NULL };
    bool inited = forge_scene_init(&scene, &cfg, 1, argv);
    if (inited) forge_scene_destroy(&scene);
    ASSERT_TRUE(!inited);
    END_TEST();
}

static void test_validate_rejects_inf_grid_spacing(void)
{
    TEST("config validation rejects Inf grid_spacing");
    ForgeSceneConfig cfg = forge_scene_default_config("Inf Grid");
    cfg.grid_spacing = forge_test_inf();
    ForgeScene scene;
    SDL_memset(&scene, 0, sizeof(scene));
    char *argv[] = { "test", NULL };
    bool inited = forge_scene_init(&scene, &cfg, 1, argv);
    if (inited) forge_scene_destroy(&scene);
    ASSERT_TRUE(!inited);
    END_TEST();
}

/* ══════════════════════════════════════════════════════════════════════════
 * Main
 * ══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    SDL_Log("=== Scene Renderer Library Tests ===\n");

    /* ── Groups 1–5 are pure math/config — no SDL_Init needed ── */

    /* ── Group 1: Config defaults ────────────────────────────── */
    SDL_Log("--- 1. Configuration Defaults ---");
    test_config_default_title();
    test_config_default_title_null();
    test_config_default_camera();
    test_config_default_projection();
    test_config_default_lighting();
    test_config_default_light_dir_normalized();
    test_config_default_shadow();
    test_config_default_grid();
    test_config_default_font_disabled();
    test_config_default_clear_color();

    /* ── Group 2: Config overrides ───────────────────────────── */
    SDL_Log("--- 2. Config Overrides ---");
    test_config_override_camera();
    test_config_override_projection();
    test_config_override_shadow_size();

    /* ── Group 3: Light VP math ──────────────────────────────── */
    SDL_Log("--- 3. Light View-Projection Math ---");
    test_light_vp_camera_position();
    test_light_vp_matrix_not_identity();
    test_light_vp_origin_projects_near_center();
    test_light_vp_direction_consistency();

    /* ── Group 4: Camera math ────────────────────────────────── */
    SDL_Log("--- 4. Camera Math ---");
    test_camera_default_orientation();
    test_camera_right_vector();
    test_camera_pitch_looks_down();
    test_camera_yaw_turns_right();
    test_camera_view_matrix();
    test_camera_perspective_aspect();

    /* ── Group 5: Struct sizes ───────────────────────────────── */
    SDL_Log("--- 5. Struct Sizes and Layout ---");
    test_struct_scene_vertex_size();
    test_struct_vert_uniforms_size();
    test_struct_frag_uniforms_size();
    test_struct_textured_vertex_size();
    test_struct_textured_frag_uniforms_size();
    test_struct_grid_vert_uniforms_size();
    test_struct_grid_frag_uniforms_size();
    test_struct_shadow_uniforms_size();
    test_struct_ui_uniforms_size();
    test_struct_vertex_offsets();

    /* ── Group 6: Robustness — NULL cmd guards (no GPU needed) ── */
    SDL_Log("--- 6. Robustness Tests ---");
    test_shadow_pass_null_cmd_is_safe();
    test_main_pass_null_cmd_is_safe();
    test_end_ui_null_cmd_is_safe();
    test_create_shader_null_scene();
    test_create_shader_null_device();
    test_upload_buffer_null_scene();
    test_upload_buffer_null_data();
    test_upload_buffer_zero_size();
    test_upload_texture_null_scene();
    test_upload_texture_null_surface();
    test_draw_shadow_mesh_null_buffers_is_safe();
    test_draw_shadow_mesh_pos_null_buffers_is_safe();
    test_draw_shadow_mesh_pos_zero_count_is_safe();
    test_draw_shadow_mesh_pos_null_pass_is_safe();
    test_draw_shadow_textured_mesh_null_buffers_is_safe();
    test_draw_textured_mesh_null_buffers_is_safe();
    test_draw_textured_mesh_null_pass_is_safe();
    test_draw_textured_mesh_null_uv_transform_is_safe();
    test_bind_textured_resources_null_texture_is_safe();
    test_draw_textured_mesh_no_bind_null_buffers_is_safe();
    test_draw_textured_mesh_no_bind_null_uv_is_safe();
    test_draw_mesh_null_buffers_is_safe();
    test_draw_mesh_zero_index_count_is_safe();
    test_destroy_null_scene_is_safe();
    test_destroy_uninitialized_zeroed_scene();

    /* ── Group 7: Config validation ───────────────────────────── */
    SDL_Log("--- 7. Config Validation ---");
    test_validate_default_config_passes();
    test_validate_zero_shadow_map_size();
    test_validate_negative_shadow_map_size();
    test_validate_zero_fov();
    test_validate_fov_too_large();
    test_validate_negative_near_plane();
    test_validate_near_equals_far();
    test_validate_inverted_near_far();
    test_validate_zero_shadow_ortho();
    test_validate_inverted_shadow_near_far();
    test_validate_zero_shadow_height();
    test_validate_negative_shadow_height();

    /* ── Group 8: NaN / Infinity robustness ──────────────────── */
    SDL_Log("--- 8. NaN / Infinity Robustness ---");
    test_validate_rejects_nan_fov();
    test_validate_rejects_inf_near_plane();
    test_validate_rejects_nan_shadow_height();
    test_validate_rejects_neg_inf_far_plane();
    test_validate_rejects_nan_cam_start_pos();
    test_validate_rejects_nan_light_dir();
    test_validate_rejects_inf_grid_spacing();

    /* ── Group 9: GPU integration (requires SDL video + GPU) ── */
    SDL_Log("--- 9. GPU Integration Tests ---");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("  SDL_Init(SDL_INIT_VIDEO) failed — skipping GPU tests: %s",
                SDL_GetError());
    } else {
        check_gpu_availability();
        test_light_dir_zero_uses_default();
        test_light_dir_straight_up_uses_alt_up();
        test_light_dir_straight_down_uses_alt_up();
        test_light_dir_diagonal_keeps_default_up();
        test_destroy_without_ui_zeroes_scene();
        test_gpu_init_destroy_no_font();
        test_gpu_init_with_font();
        test_gpu_init_custom_config();
        test_gpu_accessors();
        test_gpu_light_vp_matches_manual();
        test_gpu_destroy_is_idempotent();
        test_gpu_create_pipeline_wireframe();
        run_model_gpu_tests();
        SDL_Quit();
    }

    /* ── Pipeline guard tests (no GPU required) ───────────────── */
    test_gpu_create_pipeline_null_scene();
    test_gpu_create_pipeline_uninit_scene();

    /* ── Groups 10–17: Model tests (in test_scene_model.c) ──── */
    run_model_tests();

    /* ── Groups 18–26: Skinned model tests (in test_scene_skinned.c) ── */
    run_skinned_tests();

    /* ── Groups 27–30: Transparency sorting tests ─────────────── */
    run_transparency_tests();

    /* ── Summary ─────────────────────────────────────────────── */
    SDL_Log("\n=== Results: %d/%d passed, %d failed, %d skipped ===",
            pass_count, test_count, fail_count, skip_count);

    return fail_count > 0 ? 1 : 0;
}
