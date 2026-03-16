/*
 * Audio Lesson 02 — Sound Effects
 *
 * Demonstrates: Source pool for fire-and-forget playback, volume fading for
 * click-free start/stop transitions, polyphonic one-shot sounds.
 *
 * Five spheres: four for one-shot sound categories (keys 1-4, overlapping
 * playback = polyphony via source pool) and one for an ambient loop (key 5,
 * toggles with fade-in/fade-out).  A UI panel provides master volume, fade
 * duration, active source count, and per-slot status.
 *
 * Uses forge_scene.h for all rendering boilerplate (device, window, pipelines,
 * camera, grid, shadow map, sky, UI) — this file focuses on audio.
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   1–4               — trigger one-shot sounds (overlapping)
 *   5                 — toggle ambient loop (fade in/out)
 *   R                 — stop all sounds
 *   P                 — pause/resume audio stream
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Audio library — WAV loading, mixing, fading, source pool */
#include "audio/forge_audio.h"

/* Procedural geometry — sphere mesh */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer — replaces ~500 lines of rendering boilerplate */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

/* Sound categories */
#define NUM_ONESHOTS      4
#define AMBIENT_INDEX     4   /* index into WAV arrays for the ambient loop */
#define NUM_SOUNDS        5   /* total WAV files */

/* Sphere rendering */
#define NUM_SPHERES       5
#define SPHERE_RADIUS     0.5f
#define SPHERE_SLICES     32
#define SPHERE_STACKS     16
#define SPHERE_SPACING    3.0f   /* distance between sphere centers on X */
#define SPHERE_Y          1.0f   /* height above ground */

/* Camera start position */
#define CAM_START_X       6.0f   /* center on the 5 spheres */
#define CAM_START_Y       3.0f
#define CAM_START_Z       8.0f
#define CAM_START_PITCH  -0.25f

/* Audio mixing */
#define MIX_BUFFER_FRAMES 4096   /* scratch buffer size for mixing */

/* Fade defaults */
#define FADE_DURATION_DEFAULT 0.5f
#define FADE_DURATION_MIN     0.1f
#define FADE_DURATION_MAX     3.0f

/* UI panel layout */
#define PANEL_X           10.0f
#define PANEL_Y           10.0f
#define PANEL_W           280.0f
#define PANEL_H           520.0f
#define LABEL_HEIGHT      24.0f
#define SLIDER_HEIGHT     28.0f
#define PROGRESS_HEIGHT   16.0f

/* Colors for sphere states (RGBA) */
#define COLOR_ACTIVE_R    0.2f
#define COLOR_ACTIVE_G    0.8f
#define COLOR_ACTIVE_B    0.3f

#define COLOR_IDLE_R      0.4f
#define COLOR_IDLE_G      0.4f
#define COLOR_IDLE_B      0.4f

#define COLOR_FADING_R    0.9f
#define COLOR_FADING_G    0.8f
#define COLOR_FADING_B    0.2f

/* Scale pulse when playing */
#define PULSE_AMPLITUDE   0.05f
#define PULSE_SPEED       4.0f

/* Flash duration for one-shot trigger feedback (seconds) */
#define FLASH_DURATION    0.3f

/* Font size for UI text (pixels) */
#define FONT_SIZE         16.0f

/* Progress bar alpha */
#define BAR_ALPHA         0.8f

/* UI separator height (pixels) */
#define SEPARATOR_HEIGHT  6.0f

/* Fade target above which the ambient is considered "fading in" */
#define AMBIENT_FADE_IN_THRESHOLD  0.5f

/* Maximum pool slots shown in the UI panel */
#define MAX_VISIBLE_POOL_SLOTS     8

/* Sphere labels */
static const char *SOUND_NAMES[NUM_SOUNDS] = {
    "Impact", "Whoosh", "Chime", "Explosion", "Wind Loop"
};

/* WAV file paths (relative to CWD) */
static const char *WAV_PATHS[NUM_SOUNDS] = {
    "assets/audio/impact.wav",
    "assets/audio/whoosh.wav",
    "assets/audio/chime.wav",
    "assets/audio/explosion.wav",
    "assets/audio/wind_loop.wav"
};

/* ── Types ────────────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* Sphere GPU geometry */
    SDL_GPUBuffer *sphere_vb;          /* vertex buffer for sphere mesh */
    SDL_GPUBuffer *sphere_ib;          /* index buffer for sphere mesh */
    Uint32         sphere_index_count; /* number of indices in sphere mesh */

    /* Audio */
    SDL_AudioStream   *audio_stream;   /* output stream to default playback device */
    ForgeAudioBuffer   buffers[NUM_SOUNDS];  /* decoded WAV data (F32 stereo) */
    ForgeAudioPool     pool;           /* fire-and-forget source pool (32 slots) */
    float              master_volume;  /* linear gain applied to final mix [0..1] */
    float              mix_scratch[MIX_BUFFER_FRAMES * 2]; /* stereo scratch buffer for per-frame mixing */
    bool               audio_paused;   /* true when audio stream is paused via P key */

    /* Ambient loop — managed outside the pool for direct fade control */
    ForgeAudioSource   ambient_src;    /* looping wind source with fade envelope */
    bool               ambient_active; /* user intent: should ambient play */

    /* Fade duration (configurable via UI slider, seconds) */
    float              fade_duration;

    /* Elapsed time since start (seconds, for pulse animation) */
    float elapsed;

    /* Per-sphere flash countdown (seconds remaining, for one-shot trigger feedback) */
    float sphere_flash[NUM_SPHERES];

    /* UI state */
    ForgeUiWindowState ui_window;
} app_state;

/* ── Helper: upload_shape_vb ─────────────────────────────────────── */

static SDL_GPUBuffer *upload_shape_vb(ForgeScene *scene,
                                       const ForgeShape *shape)
{
    ForgeSceneVertex *verts = SDL_calloc((size_t)shape->vertex_count,
                                         sizeof(ForgeSceneVertex));
    if (!verts) {
        SDL_Log("ERROR: Failed to allocate %d vertices for shape upload",
                shape->vertex_count);
        return NULL;
    }

    for (int i = 0; i < shape->vertex_count; i++) {
        verts[i].position = shape->positions[i];
        verts[i].normal   = shape->normals[i];
    }

    SDL_GPUBuffer *buf = forge_scene_upload_buffer(scene,
        SDL_GPU_BUFFERUSAGE_VERTEX, verts,
        (Uint32)shape->vertex_count * (Uint32)sizeof(ForgeSceneVertex));
    SDL_free(verts);
    return buf;
}

/* ── SDL_AppInit ─────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("ERROR: Failed to allocate app_state");
        return SDL_APP_FAILURE;
    }
    *appstate = state;

    /* Configure the scene renderer */
    ForgeSceneConfig cfg = forge_scene_default_config(
        "Audio Lesson 02 \xe2\x80\x94 Sound Effects");
    cfg.cam_start_pos   = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path       = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";
    cfg.font_size       = FONT_SIZE;

    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("ERROR: forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* Initialize audio subsystem (not initialized by forge_scene) */
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("ERROR: SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s",
                SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Generate and upload sphere geometry */
    ForgeShape sphere = forge_shapes_sphere(SPHERE_SLICES, SPHERE_STACKS);
    if (sphere.vertex_count == 0) {
        SDL_Log("ERROR: forge_shapes_sphere failed to allocate");
        return SDL_APP_FAILURE;
    }

    state->sphere_vb = upload_shape_vb(&state->scene, &sphere);
    state->sphere_ib = forge_scene_upload_buffer(&state->scene,
        SDL_GPU_BUFFERUSAGE_INDEX, sphere.indices,
        (Uint32)sphere.index_count * (Uint32)sizeof(uint32_t));
    state->sphere_index_count = (Uint32)sphere.index_count;
    forge_shapes_free(&sphere);

    if (!state->sphere_vb || !state->sphere_ib) {
        SDL_Log("ERROR: Failed to upload sphere geometry");
        return SDL_APP_FAILURE;
    }

    /* ── Audio setup ─────────────────────────────────────────────── */

    /* Open an audio stream to the default output device */
    SDL_AudioSpec audio_spec;
    audio_spec.format   = FORGE_AUDIO_FORMAT;
    audio_spec.channels = FORGE_AUDIO_CHANNELS;
    audio_spec.freq     = FORGE_AUDIO_SAMPLE_RATE;

    state->audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audio_spec, NULL, NULL);
    if (!state->audio_stream) {
        SDL_Log("WARNING: SDL_OpenAudioDeviceStream failed: %s",
                SDL_GetError());
        SDL_Log("NOTE: Continuing without audio (no audio device available)");
    } else {
        if (!SDL_ResumeAudioStreamDevice(state->audio_stream)) {
            SDL_Log("WARNING: SDL_ResumeAudioStreamDevice failed: %s",
                    SDL_GetError());
            SDL_DestroyAudioStream(state->audio_stream);
            state->audio_stream = NULL;
        }
    }

    /* Load WAV files */
    bool all_loaded = true;
    for (int i = 0; i < NUM_SOUNDS; i++) {
        if (!forge_audio_load_wav(WAV_PATHS[i], &state->buffers[i])) {
            SDL_Log("WARNING: Could not load '%s' — sound %d will be silent",
                    WAV_PATHS[i], i);
            all_loaded = false;
        }
    }
    if (!all_loaded) {
        SDL_Log("NOTE: Place WAV files in assets/audio/ (see README)");
    }

    /* Initialize the source pool */
    forge_audio_pool_init(&state->pool);

    /* Set up ambient source (managed separately for fade control) */
    state->ambient_src = forge_audio_source_create(
        &state->buffers[AMBIENT_INDEX], 1.0f, true);
    state->ambient_active = false;

    state->master_volume  = 0.8f;
    state->fade_duration  = FADE_DURATION_DEFAULT;
    state->audio_paused   = false;
    state->elapsed        = 0.0f;

    for (int i = 0; i < NUM_SPHERES; i++) {
        state->sphere_flash[i] = 0.0f;
    }

    state->ui_window = forge_ui_window_state_default(
        PANEL_X, PANEL_Y, PANEL_W, PANEL_H);

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    /* Let scene handle camera, mouse capture, quit, escape */
    SDL_AppResult result = forge_scene_handle_event(&state->scene, event);
    if (result != SDL_APP_CONTINUE) return result;

    /* Audio-specific keys */
    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        SDL_Scancode sc = event->key.scancode;

        /* 1–4 trigger one-shot sounds via the pool (fire-and-forget).
         * Each press adds a new source — overlapping = polyphony. */
        if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_4) {
            int idx = (int)(sc - SDL_SCANCODE_1);
            if (idx < NUM_ONESHOTS && state->buffers[idx].data) {
                int slot = forge_audio_pool_play(&state->pool,
                    &state->buffers[idx], 1.0f, false);
                if (slot >= 0) {
                    SDL_Log("%s: triggered (slot %d)", SOUND_NAMES[idx], slot);
                    state->sphere_flash[idx] = FLASH_DURATION;
                } else {
                    SDL_Log("%s: pool full, could not play", SOUND_NAMES[idx]);
                }
            }
        }

        /* 5 toggles ambient loop with fade */
        if (sc == SDL_SCANCODE_5) {
            if (state->buffers[AMBIENT_INDEX].data) {
                if (state->ambient_active) {
                    /* Fade out */
                    forge_audio_source_fade_out(&state->ambient_src,
                                                state->fade_duration);
                    state->ambient_active = false;
                    SDL_Log("Wind Loop: fading out (%.1fs)",
                            (double)state->fade_duration);
                } else {
                    /* Fade in — reset cursor if stopped */
                    if (!state->ambient_src.playing) {
                        forge_audio_source_reset(&state->ambient_src);
                    }
                    forge_audio_source_fade_in(&state->ambient_src,
                                               state->fade_duration);
                    state->ambient_active = true;
                    SDL_Log("Wind Loop: fading in (%.1fs)",
                            (double)state->fade_duration);
                }
            }
        }

        /* R = stop all sounds */
        if (sc == SDL_SCANCODE_R) {
            forge_audio_pool_stop_all(&state->pool);
            state->ambient_src.playing = false;
            state->ambient_src.fade_rate = 0.0f;
            state->ambient_src.fade_volume = 1.0f;
            state->ambient_active = false;
            for (int i = 0; i < NUM_SPHERES; i++) {
                state->sphere_flash[i] = 0.0f;
            }
            SDL_Log("All sounds stopped");
        }

        /* P = pause/resume audio stream */
        if (sc == SDL_SCANCODE_P) {
            if (state->audio_stream) {
                if (state->audio_paused) {
                    if (!SDL_ResumeAudioStreamDevice(state->audio_stream)) {
                        SDL_Log("ERROR: SDL_ResumeAudioStreamDevice: %s",
                                SDL_GetError());
                    } else {
                        state->audio_paused = false;
                        SDL_Log("Audio resumed");
                    }
                } else {
                    if (!SDL_PauseAudioStreamDevice(state->audio_stream)) {
                        SDL_Log("ERROR: SDL_PauseAudioStreamDevice: %s",
                                SDL_GetError());
                    } else {
                        state->audio_paused = true;
                        SDL_Log("Audio paused");
                    }
                }
            }
        }
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ──────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    ForgeScene *s = &state->scene;

    if (!forge_scene_begin_frame(s)) return SDL_APP_CONTINUE;

    float dt = forge_scene_dt(s);
    if (dt > 0.1f) dt = 0.1f;
    state->elapsed += dt;

    /* Update sphere flash — one-shot spheres glow while any pool source
     * using their buffer is still playing.  This ties the visual feedback
     * to the actual sound duration instead of an arbitrary timer. */
    for (int i = 0; i < NUM_ONESHOTS; i++) {
        bool active = false;
        for (int j = 0; j < FORGE_AUDIO_POOL_MAX_SOURCES; j++) {
            if (state->pool.sources[j].playing &&
                state->pool.sources[j].buffer == &state->buffers[i]) {
                active = true;
                break;
            }
        }
        state->sphere_flash[i] = active ? FLASH_DURATION : 0.0f;
    }

    /* ── Update fades and mix audio (skip while paused) ─────────── */

    if (!state->audio_paused) {
        /* Update fade on all pool sources */
        for (int i = 0; i < FORGE_AUDIO_POOL_MAX_SOURCES; i++) {
            if (state->pool.sources[i].playing) {
                forge_audio_source_fade_update(&state->pool.sources[i], dt);
            }
        }
        /* Update fade on the ambient source */
        forge_audio_source_fade_update(&state->ambient_src, dt);

        /* ── Audio mixing ────────────────────────────────────────── */
        int frames_needed = (int)((float)FORGE_AUDIO_SAMPLE_RATE * dt);
        if (frames_needed < 1) frames_needed = 1;
        if (frames_needed > MIX_BUFFER_FRAMES) frames_needed = MIX_BUFFER_FRAMES;

        int samples = frames_needed * 2;  /* stereo */

        /* Zero the scratch buffer */
        SDL_memset(state->mix_scratch, 0,
                   (size_t)samples * sizeof(float));

        /* Mix all pool sources additively */
        forge_audio_pool_mix(&state->pool, state->mix_scratch, frames_needed);

        /* Mix the ambient source */
        forge_audio_source_mix(&state->ambient_src,
                               state->mix_scratch, frames_needed);

        /* Apply master volume */
        for (int i = 0; i < samples; i++) {
            state->mix_scratch[i] *= state->master_volume;
        }

        /* Push mixed audio to output stream */
        if (state->audio_stream) {
            if (!SDL_PutAudioStreamData(state->audio_stream,
                                         state->mix_scratch,
                                         samples * (int)sizeof(float))) {
                SDL_Log("ERROR: SDL_PutAudioStreamData failed: %s",
                        SDL_GetError());
                SDL_DestroyAudioStream(state->audio_stream);
                state->audio_stream = NULL;
            }
        }
    }

    /* ── Shadow pass ─────────────────────────────────────────────── */

    forge_scene_begin_shadow_pass(s);
    for (int i = 0; i < NUM_SPHERES; i++) {
        float x = (float)i * SPHERE_SPACING;
        float scale = SPHERE_RADIUS;

        /* Pulse one-shot spheres when flashing, ambient sphere when playing */
        bool pulsing = false;
        if (i < NUM_ONESHOTS) {
            pulsing = (state->sphere_flash[i] > 0.0f);
        } else {
            pulsing = state->ambient_src.playing;
        }
        if (pulsing) {
            scale += PULSE_AMPLITUDE *
                     SDL_sinf(state->elapsed * PULSE_SPEED);
        }

        mat4 model = mat4_multiply(
            mat4_translate(vec3_create(x, SPHERE_Y, 0.0f)),
            mat4_scale_uniform(scale));
        forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                     state->sphere_index_count, model);
    }
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */

    forge_scene_begin_main_pass(s);
    for (int i = 0; i < NUM_SPHERES; i++) {
        float x = (float)i * SPHERE_SPACING;
        float scale = SPHERE_RADIUS;

        bool pulsing = false;
        if (i < NUM_ONESHOTS) {
            pulsing = (state->sphere_flash[i] > 0.0f);
        } else {
            pulsing = state->ambient_src.playing;
        }
        if (pulsing) {
            scale += PULSE_AMPLITUDE *
                     SDL_sinf(state->elapsed * PULSE_SPEED);
        }

        mat4 model = mat4_multiply(
            mat4_translate(vec3_create(x, SPHERE_Y, 0.0f)),
            mat4_scale_uniform(scale));

        /* Color: green = active, yellow = fading, gray = idle */
        float color[4];
        if (i < NUM_ONESHOTS) {
            /* One-shot sphere: green flash, else gray */
            if (state->sphere_flash[i] > 0.0f) {
                color[0] = COLOR_ACTIVE_R;
                color[1] = COLOR_ACTIVE_G;
                color[2] = COLOR_ACTIVE_B;
            } else {
                color[0] = COLOR_IDLE_R;
                color[1] = COLOR_IDLE_G;
                color[2] = COLOR_IDLE_B;
            }
        } else {
            /* Ambient sphere: green when playing full, yellow when fading */
            if (state->ambient_src.playing && state->ambient_src.fade_rate > 0.0f) {
                color[0] = COLOR_FADING_R;
                color[1] = COLOR_FADING_G;
                color[2] = COLOR_FADING_B;
            } else if (state->ambient_src.playing) {
                color[0] = COLOR_ACTIVE_R;
                color[1] = COLOR_ACTIVE_G;
                color[2] = COLOR_ACTIVE_B;
            } else {
                color[0] = COLOR_IDLE_R;
                color[1] = COLOR_IDLE_G;
                color[2] = COLOR_IDLE_B;
            }
        }
        color[3] = 1.0f;

        forge_scene_draw_mesh(s, state->sphere_vb, state->sphere_ib,
                              state->sphere_index_count, model, color);
    }
    forge_scene_draw_grid(s);
    forge_scene_end_main_pass(s);

    /* ── UI pass ─────────────────────────────────────────────────── */

    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = !state->scene.mouse_captured
                    && (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (wctx) {
            if (forge_ui_wctx_window_begin(wctx, "Sound Effects",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;

                /* Master volume */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Master: %.0f%%",
                                 (double)(state->master_volume * 100.0f));
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##master",
                                           &state->master_volume,
                                           0.0f, 1.0f, SLIDER_HEIGHT);

                /* Fade duration */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Fade: %.1fs",
                                 (double)state->fade_duration);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }
                forge_ui_ctx_slider_layout(ui, "##fade_dur",
                                           &state->fade_duration,
                                           FADE_DURATION_MIN, FADE_DURATION_MAX,
                                           SLIDER_HEIGHT);

                /* Active source count */
                {
                    int pool_active = forge_audio_pool_active_count(&state->pool);
                    int total_active = pool_active
                                     + (state->ambient_src.playing ? 1 : 0);
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Active: %d / %d",
                                 total_active,
                                 FORGE_AUDIO_POOL_MAX_SOURCES + 1);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                forge_ui_ctx_separator_layout(ui, SEPARATOR_HEIGHT);

                /* Ambient status */
                {
                    const char *status = "Stopped";
                    if (state->ambient_src.playing
                        && state->ambient_src.fade_rate > 0.0f) {
                        if (state->ambient_src.fade_target > AMBIENT_FADE_IN_THRESHOLD) {
                            status = "Fading In";
                        } else {
                            status = "Fading Out";
                        }
                    } else if (state->ambient_src.playing) {
                        status = "Playing";
                    }

                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "[5] %s: %s",
                                 SOUND_NAMES[AMBIENT_INDEX], status);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                /* Ambient fade volume bar */
                {
                    ForgeUiColor bar_color = {
                        COLOR_FADING_R, COLOR_FADING_G,
                        COLOR_FADING_B, BAR_ALPHA
                    };
                    forge_ui_ctx_progress_bar_layout(ui,
                        state->ambient_src.fade_volume,
                        1.0f, bar_color, PROGRESS_HEIGHT);
                }

                forge_ui_ctx_separator_layout(ui, SEPARATOR_HEIGHT);

                /* Active pool slots (first 8) */
                forge_ui_ctx_label_layout(ui, "Pool slots:", LABEL_HEIGHT);
                {
                    int shown = 0;
                    for (int i = 0;
                         i < FORGE_AUDIO_POOL_MAX_SOURCES && shown < MAX_VISIBLE_POOL_SLOTS;
                         i++) {
                        ForgeAudioSource *src = &state->pool.sources[i];
                        if (!src->playing) continue;

                        /* Find which sound this is by matching the buffer */
                        const char *name = "?";
                        for (int j = 0; j < NUM_ONESHOTS; j++) {
                            if (src->buffer == &state->buffers[j]) {
                                name = SOUND_NAMES[j];
                                break;
                            }
                        }

                        float progress = forge_audio_source_progress(src);
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), " %d: %s %.0f%%",
                                     i, name,
                                     (double)(progress * 100.0f));
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);

                        ForgeUiColor bar_color = {
                            COLOR_ACTIVE_R, COLOR_ACTIVE_G,
                            COLOR_ACTIVE_B, BAR_ALPHA
                        };
                        /* Show fade indicator if fading */
                        if (src->fade_rate > 0.0f) {
                            bar_color.r = COLOR_FADING_R;
                            bar_color.g = COLOR_FADING_G;
                            bar_color.b = COLOR_FADING_B;
                        }
                        forge_ui_ctx_progress_bar_layout(ui, progress,
                            1.0f, bar_color, PROGRESS_HEIGHT);
                        shown++;
                    }

                    if (shown == 0) {
                        forge_ui_ctx_label_layout(ui, " (none)", LABEL_HEIGHT);
                    }
                }

                forge_ui_ctx_separator_layout(ui, SEPARATOR_HEIGHT);

                /* Controls reminder */
                forge_ui_ctx_label_layout(ui, "1-4: one-shots", LABEL_HEIGHT);
                forge_ui_ctx_label_layout(ui, "5: ambient  R: stop",
                                          LABEL_HEIGHT);
                forge_ui_ctx_label_layout(ui, "P: pause/resume",
                                          LABEL_HEIGHT);

                forge_ui_wctx_window_end(wctx);
            }
        }
    }
    forge_scene_end_ui(s);

    return forge_scene_end_frame(s);
}

/* ── SDL_AppQuit ─────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (!state) return;

    /* Destroy audio resources */
    if (state->audio_stream) {
        SDL_DestroyAudioStream(state->audio_stream);
    }
    for (int i = 0; i < NUM_SOUNDS; i++) {
        forge_audio_buffer_free(&state->buffers[i]);
    }

    /* Release lesson-specific GPU resources */
    if (forge_scene_device(&state->scene)) {
        if (!SDL_WaitForGPUIdle(forge_scene_device(&state->scene))) {
            SDL_Log("ERROR: SDL_WaitForGPUIdle failed: %s", SDL_GetError());
        }
        if (state->sphere_vb)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_vb);
        if (state->sphere_ib)
            SDL_ReleaseGPUBuffer(forge_scene_device(&state->scene),
                                 state->sphere_ib);
    }

    forge_scene_destroy(&state->scene);
    SDL_free(state);
}
