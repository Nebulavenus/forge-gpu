/*
 * GPU Lesson 42 — Pipeline Texture Compression
 *
 * Demonstrates GPU block-compressed texture loading via the asset pipeline.
 * Textures are pre-compressed to BC7 (color) or BC5 (normals) at build time
 * by the pipeline (basisu + forge_texture_tool) and stored as .ftex files.
 * At runtime, the pre-transcoded blocks are loaded and uploaded directly to
 * the GPU — no transcoding, no mip generation, just read and upload.
 * Compressed textures use ~4x less VRAM than raw RGBA8.
 *
 * The demo renders ABeautifulGame — a chess set with 15 PBR materials and 33
 * textures at 2048x2048.  A UI panel shows VRAM savings and load timing.
 *
 * Controls:
 *   WASD / Mouse  — move/look
 *   Space / Shift — fly up/down
 *   Escape        — release mouse cursor
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <stddef.h>

/* Math library */
#include "math/forge_math.h"

/* Pipeline runtime — .fmesh, .fscene, .fmat, .ftex loaders */
#define FORGE_PIPELINE_IMPLEMENTATION
#include "pipeline/forge_pipeline.h"

/* Scene renderer with model support */
#define FORGE_SCENE_MODEL_SUPPORT
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

#ifdef FORGE_CAPTURE
#include "capture/forge_capture.h"
#endif

/* ── Constants ─────────────────────────────────────────────────────────── */

/* UI panel layout */
#define PANEL_X      10
#define PANEL_Y      10
#define PANEL_W      360
#define PANEL_H      300
#define LABEL_HEIGHT 20

/* Unit conversions */
#define MS_PER_SEC   1000.0f
#define BYTES_PER_MB (1024.0f * 1024.0f)

/* Threshold below which VRAM values are treated as zero (avoids div-by-zero) */
#define VRAM_EPSILON 0.001f

/* Camera starting position — elevated 3/4 view looking down at the board */
#define CAM_START_X     -8.0f
#define CAM_START_Y      7.0f
#define CAM_START_Z      12.0f
#define CAM_START_YAW   -0.65f
#define CAM_START_PITCH -0.55f

/* ABeautifulGame is modeled at small scale (board spans ~0.5 units).
 * Scale up so the chess set fills the scene at a comfortable view distance. */
#define MODEL_SCALE     20.0f

/* Maximum path buffer for exe-relative asset paths */
#define ASSET_PATH_SIZE 512

/* ── Application state ─────────────────────────────────────────────────── */

typedef struct {
    ForgeScene      scene;   /* rendering stack (device, pipelines, camera, UI) */
    ForgeSceneModel chess;   /* ABeautifulGame — 15 materials, 49 nodes         */

    /* Load timing */
    float              load_time_ms; /* time to load and transcode textures */
    ForgeUiWindowState ui_window;    /* draggable window state */
} AppState;

/* Build an absolute asset path: exe_dir + relative.
 * `base` must end with a path separator (SDL_GetBasePath guarantees this).
 * Returns a pointer to buf on success, NULL on overflow. */
static const char *asset_path(char *buf, size_t buf_size,
                              const char *base, const char *relative)
{
    int len = SDL_snprintf(buf, buf_size, "%s%s", base, relative);
    if (len < 0 || (size_t)len >= buf_size) {
        SDL_Log("asset_path: path too long: %s%s", base, relative);
        return NULL;
    }
    return buf;
}

/* ── SDL_AppInit ────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    AppState *state = (AppState *)SDL_calloc(1, sizeof(AppState));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        return SDL_APP_FAILURE;
    }
    *appstate = state;

    /* Resolve asset paths relative to the executable directory so the
     * lesson works regardless of the current working directory. */
    const char *exe_dir = SDL_GetBasePath();
    if (!exe_dir) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Build font path for the UI panel */
    char font_buf[ASSET_PATH_SIZE];
    const char *font = asset_path(font_buf, sizeof(font_buf), exe_dir,
                        "assets/fonts/liberation_mono/LiberationMono-Regular.ttf");
    if (!font) {
        SDL_Log("Failed to build font path — base path too long");
        return SDL_APP_FAILURE;
    }

    /* Configure scene renderer */
    ForgeSceneConfig config = forge_scene_default_config(
        "GPU Lesson 42 — Pipeline Texture Compression");
    config.cam_start_pos    = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    config.cam_start_yaw    = CAM_START_YAW;
    config.cam_start_pitch  = CAM_START_PITCH;
    config.font_path        = font;
    if (!forge_scene_init(&state->scene, &config, argc, argv)) {
        SDL_Log("forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* Load ABeautifulGame with pre-transcoded compressed textures (.ftex).
     * The load_model call detects .meta.json sidecars next to each texture
     * and loads the corresponding .ftex files (BC7/BC5 blocks + mip chain). */
    Uint64 t0 = SDL_GetPerformanceCounter();

    char fscene[ASSET_PATH_SIZE], fmesh[ASSET_PATH_SIZE];
    char fmat[ASSET_PATH_SIZE], base_dir[ASSET_PATH_SIZE];
    if (!asset_path(fscene,   sizeof(fscene),   exe_dir, "assets/ABeautifulGame/ABeautifulGame.fscene") ||
        !asset_path(fmesh,    sizeof(fmesh),    exe_dir, "assets/ABeautifulGame/ABeautifulGame.fmesh") ||
        !asset_path(fmat,     sizeof(fmat),     exe_dir, "assets/ABeautifulGame/ABeautifulGame.fmat")  ||
        !asset_path(base_dir, sizeof(base_dir), exe_dir, "assets/ABeautifulGame")) {
        return SDL_APP_FAILURE;
    }

    if (!forge_scene_load_model(&state->scene, &state->chess,
                                 fscene, fmesh, fmat, base_dir)) {
        SDL_Log("Failed to load ABeautifulGame model");
        return SDL_APP_FAILURE;
    }

    Uint64 t1 = SDL_GetPerformanceCounter();
    state->load_time_ms = (float)(t1 - t0) /
                          (float)SDL_GetPerformanceFrequency() * MS_PER_SEC;

    SDL_Log("Lesson 42: loaded ABeautifulGame in %.1f ms — "
            "%u/%u textures compressed, VRAM %.1f MB (uncompressed would be %.1f MB)",
            state->load_time_ms,
            state->chess.vram.compressed_texture_count,
            state->chess.vram.total_texture_count,
            (float)state->chess.vram.compressed_bytes / BYTES_PER_MB,
            (float)state->chess.vram.uncompressed_bytes / BYTES_PER_MB);

    /* ── UI window state ──────────────────────────────────────────── */
    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ──────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    AppState *state = (AppState *)appstate;
    return forge_scene_handle_event(&state->scene, event);
}

/* ── SDL_AppIterate ─────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *state = (AppState *)appstate;
    ForgeScene *s = &state->scene;

    if (!forge_scene_begin_frame(s)) return SDL_APP_CONTINUE;

    /* ── Shadow pass ──────────────────────────────────────────────────── */
    forge_scene_begin_shadow_pass(s);
    forge_scene_draw_model_shadows(s, &state->chess, mat4_scale_uniform(MODEL_SCALE));
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ────────────────────────────────────────────────────── */
    forge_scene_begin_main_pass(s);

    /* Draw grid floor */
    forge_scene_draw_grid(s);

    /* Draw chess set at world origin */
    forge_scene_draw_model(s, &state->chess, mat4_scale_uniform(MODEL_SCALE));

    forge_scene_end_main_pass(s);

    /* ── UI pass: compression stats ──────────────────────────────────── */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = !state->scene.mouse_captured
                    && (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (wctx) {
            if (forge_ui_wctx_window_begin(wctx, "Texture Compression",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;
                ForgeSceneModel *m = &state->chess;
                float compressed_mb   = (float)m->vram.compressed_bytes
                                        / BYTES_PER_MB;
                float uncompressed_mb = (float)m->vram.uncompressed_bytes
                                        / BYTES_PER_MB;
                float savings_pct     = 0.0f;
                if (uncompressed_mb > VRAM_EPSILON) {
                    savings_pct = (1.0f - compressed_mb / uncompressed_mb)
                                  * 100.0f;
                }

                char buf[128];

                SDL_snprintf(buf, sizeof(buf), "Model: ABeautifulGame");
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf),
                             "Textures: %u total, %u compressed",
                             m->vram.total_texture_count,
                             m->vram.compressed_texture_count);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                forge_ui_ctx_label_layout(ui, "---", LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf), "VRAM (compressed):   %.1f MB",
                             compressed_mb);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf),
                             "VRAM (uncompressed): %.1f MB",
                             uncompressed_mb);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf), "Savings: %.0f%%",
                             savings_pct);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                forge_ui_ctx_label_layout(ui, "---", LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf),
                             "Format: BC7 (color), BC5 (normals)");
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf), "Source: UASTC (KTX2)");
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf), "Load time: %.0f ms",
                             state->load_time_ms);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                SDL_snprintf(buf, sizeof(buf), "Draw calls: %u",
                             m->draw_calls);
                forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                forge_ui_wctx_window_end(wctx);
            }
        }
    }
    forge_scene_end_ui(s);

    return forge_scene_end_frame(s);
}

/* ── SDL_AppQuit ───────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    AppState *state = (AppState *)appstate;
    if (!state) return;

    forge_scene_free_model(&state->scene, &state->chess);
    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
