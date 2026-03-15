/*
 * Audio Lesson 01 — Audio Basics
 *
 * Demonstrates: PCM fundamentals, WAV loading via SDL_LoadWAV, format
 * conversion to F32 stereo, audio mixing, and SDL3 audio stream playback.
 *
 * Four sound sources represented as colored spheres: two one-shot sounds
 * (click, snare) and two looping sounds (drum loop, pad loop).  A UI panel
 * provides master volume, per-source controls (play/stop, volume, pan),
 * and progress bars.
 *
 * Uses forge_scene.h for all rendering boilerplate (device, window, pipelines,
 * camera, grid, shadow map, sky, UI) — this file focuses on audio.
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   1–4               — toggle play/stop for each source
 *   R                 — reset all sources to beginning
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Audio library — WAV loading, mixing, playback */
#include "audio/forge_audio.h"

/* Procedural geometry — sphere mesh */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer — replaces ~500 lines of rendering boilerplate */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

/* Number of audio sources */
#define NUM_SOURCES       4

/* Sphere rendering */
#define SPHERE_RADIUS     0.5f
#define SPHERE_SLICES     32
#define SPHERE_STACKS     16
#define SPHERE_SPACING    3.0f   /* distance between sphere centers on X */
#define SPHERE_Y          1.0f   /* height above ground */

/* Camera start position */
#define CAM_START_X       4.5f   /* center on the 4 spheres */
#define CAM_START_Y       3.0f
#define CAM_START_Z       8.0f
#define CAM_START_PITCH  -0.25f

/* Audio mixing */
#define MIX_BUFFER_FRAMES 4096   /* scratch buffer size for mixing */

/* UI panel layout */
#define PANEL_X           10.0f
#define PANEL_Y           10.0f
#define PANEL_W           260.0f
#define PANEL_H           480.0f
#define LABEL_HEIGHT      24.0f
#define SLIDER_HEIGHT     28.0f
#define CHECKBOX_HEIGHT   24.0f
#define PROGRESS_HEIGHT   16.0f

/* Colors for playing/stopped spheres (RGBA) */
#define COLOR_PLAYING_R   0.2f
#define COLOR_PLAYING_G   0.8f
#define COLOR_PLAYING_B   0.3f

#define COLOR_STOPPED_R   0.4f
#define COLOR_STOPPED_G   0.4f
#define COLOR_STOPPED_B   0.4f

/* Scale pulse when playing */
#define PULSE_AMPLITUDE   0.05f
#define PULSE_SPEED       4.0f

/* Source labels */
static const char *SOURCE_NAMES[NUM_SOURCES] = {
    "Click", "Snare", "Drum Loop", "Pad Loop"
};

/* WAV file paths (relative to CWD) */
static const char *WAV_PATHS[NUM_SOURCES] = {
    "assets/audio/click.wav",
    "assets/audio/snare.wav",
    "assets/audio/drum_loop.wav",
    "assets/audio/pad_loop.wav"
};

/* Which sources loop */
static const bool SOURCE_LOOPING[NUM_SOURCES] = {
    false, false, true, true
};

/* ── Types ────────────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* Sphere GPU geometry */
    SDL_GPUBuffer *sphere_vb;
    SDL_GPUBuffer *sphere_ib;
    Uint32         sphere_index_count;

    /* Audio */
    SDL_AudioStream   *audio_stream;   /* output stream to device */
    ForgeAudioBuffer   buffers[NUM_SOURCES];
    ForgeAudioSource   sources[NUM_SOURCES];
    float              master_volume;
    float              mix_scratch[MIX_BUFFER_FRAMES * 2]; /* stereo scratch */

    /* Track elapsed time for scale pulse animation */
    float elapsed;

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
        "Audio Lesson 01 \xe2\x80\x94 Audio Basics");
    cfg.cam_start_pos   = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path       = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";
    cfg.font_size       = 16.0f;

    if (!forge_scene_init(&state->scene, &cfg, argc, argv)) {
        SDL_Log("ERROR: forge_scene_init failed");
        return SDL_APP_FAILURE;
    }

    /* Initialize audio subsystem (not initialized by forge_scene) */
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("ERROR: SDL_InitSubSystem(SDL_INIT_AUDIO) failed: %s", SDL_GetError());
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
        SDL_Log("WARNING: SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
        SDL_Log("NOTE: Continuing without audio (no audio device available)");
    } else {
        /* Resume the audio device (streams start paused) */
        if (!SDL_ResumeAudioStreamDevice(state->audio_stream)) {
            SDL_Log("WARNING: SDL_ResumeAudioStreamDevice failed: %s",
                    SDL_GetError());
            SDL_DestroyAudioStream(state->audio_stream);
            state->audio_stream = NULL;
        }
    }

    /* Load WAV files */
    bool all_loaded = true;
    for (int i = 0; i < NUM_SOURCES; i++) {
        if (!forge_audio_load_wav(WAV_PATHS[i], &state->buffers[i])) {
            SDL_Log("WARNING: Could not load '%s' — source %d will be silent",
                    WAV_PATHS[i], i);
            all_loaded = false;
        }
    }
    if (!all_loaded) {
        SDL_Log("NOTE: Place WAV files in assets/audio/ (see README)");
    }

    /* Create sources */
    for (int i = 0; i < NUM_SOURCES; i++) {
        state->sources[i] = forge_audio_source_create(
            &state->buffers[i], 1.0f, SOURCE_LOOPING[i]);
    }

    /* Start looping sources playing by default */
    for (int i = 0; i < NUM_SOURCES; i++) {
        if (SOURCE_LOOPING[i] && state->buffers[i].data) {
            state->sources[i].playing = true;
        }
    }

    state->master_volume = 0.8f;
    state->elapsed = 0.0f;
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

        /* 1–4 toggle play/stop for each source */
        if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_4) {
            int idx = (int)(sc - SDL_SCANCODE_1);
            if (idx < NUM_SOURCES && state->buffers[idx].data) {
                if (state->sources[idx].playing) {
                    state->sources[idx].playing = false;
                    SDL_Log("%s: stopped", SOURCE_NAMES[idx]);
                } else {
                    /* Restart from beginning for one-shot sounds */
                    if (!state->sources[idx].looping) {
                        forge_audio_source_reset(&state->sources[idx]);
                    }
                    state->sources[idx].playing = true;
                    SDL_Log("%s: playing", SOURCE_NAMES[idx]);
                }
            }
        }

        /* R = reset all sources */
        if (sc == SDL_SCANCODE_R) {
            for (int i = 0; i < NUM_SOURCES; i++) {
                forge_audio_source_reset(&state->sources[i]);
                if (SOURCE_LOOPING[i] && state->buffers[i].data) {
                    state->sources[i].playing = true;
                } else {
                    state->sources[i].playing = false;
                }
            }
            SDL_Log("All sources reset");
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

    /* ── Audio mixing ────────────────────────────────────────────── */
    /* Compute how many frames the audio device needs this frame,
     * mix all sources into a scratch buffer, apply master volume,
     * then push to the SDL audio stream. */
    {
        int frames_needed = (int)((float)FORGE_AUDIO_SAMPLE_RATE * dt);
        if (frames_needed < 1) frames_needed = 1;
        if (frames_needed > MIX_BUFFER_FRAMES) frames_needed = MIX_BUFFER_FRAMES;

        int samples = frames_needed * 2;  /* stereo */

        /* Zero the scratch buffer */
        SDL_memset(state->mix_scratch, 0,
                   (size_t)samples * sizeof(float));

        /* Mix all active sources additively */
        for (int i = 0; i < NUM_SOURCES; i++) {
            forge_audio_source_mix(&state->sources[i],
                                   state->mix_scratch, frames_needed);
        }

        /* Apply master volume */
        for (int i = 0; i < samples; i++) {
            state->mix_scratch[i] *= state->master_volume;
        }

        /* Push mixed audio to output stream (if available).
         * On failure, destroy the stream so we stop trying every frame. */
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
    for (int i = 0; i < NUM_SOURCES; i++) {
        float x = (float)i * SPHERE_SPACING;
        float scale = SPHERE_RADIUS;
        if (state->sources[i].playing) {
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
    for (int i = 0; i < NUM_SOURCES; i++) {
        float x = (float)i * SPHERE_SPACING;
        float scale = SPHERE_RADIUS;
        if (state->sources[i].playing) {
            scale += PULSE_AMPLITUDE *
                     SDL_sinf(state->elapsed * PULSE_SPEED);
        }
        mat4 model = mat4_multiply(
            mat4_translate(vec3_create(x, SPHERE_Y, 0.0f)),
            mat4_scale_uniform(scale));

        float color[4];
        if (state->sources[i].playing) {
            color[0] = COLOR_PLAYING_R;
            color[1] = COLOR_PLAYING_G;
            color[2] = COLOR_PLAYING_B;
        } else {
            color[0] = COLOR_STOPPED_R;
            color[1] = COLOR_STOPPED_G;
            color[2] = COLOR_STOPPED_B;
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
            if (forge_ui_wctx_window_begin(wctx, "Audio Controls",
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

                forge_ui_ctx_separator_layout(ui, 8.0f);

                /* Per-source controls */
                for (int i = 0; i < NUM_SOURCES; i++) {
                    /* Source label */
                    {
                        char buf[64];
                        float dur = forge_audio_buffer_duration(
                            &state->buffers[i]);
                        SDL_snprintf(buf, sizeof(buf), "[%d] %s (%.1fs)",
                                     i + 1, SOURCE_NAMES[i], (double)dur);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }

                    /* Play/stop checkbox — unique ID per source to avoid
                     * hover/highlight collision between checkboxes */
                    {
                        char cb_label[32];
                        SDL_snprintf(cb_label, sizeof(cb_label),
                                     "Playing##src_%d", i);
                        bool was_playing = state->sources[i].playing;
                        forge_ui_ctx_checkbox_layout(ui, cb_label,
                            &state->sources[i].playing, CHECKBOX_HEIGHT);
                        /* Restart one-shot from beginning when toggled on */
                        if (!was_playing && state->sources[i].playing
                            && !state->sources[i].looping) {
                            forge_audio_source_reset(&state->sources[i]);
                        }
                    }

                    /* Volume slider */
                    {
                        char id[32];
                        SDL_snprintf(id, sizeof(id), "##vol_%d", i);
                        forge_ui_ctx_slider_layout(ui, id,
                            &state->sources[i].volume,
                            0.0f, 1.0f, SLIDER_HEIGHT);
                    }

                    /* Progress bar */
                    {
                        float progress = forge_audio_source_progress(
                            &state->sources[i]);
                        ForgeUiColor bar_color = {
                            COLOR_PLAYING_R, COLOR_PLAYING_G,
                            COLOR_PLAYING_B, 0.8f
                        };
                        forge_ui_ctx_progress_bar_layout(ui, progress,
                            1.0f, bar_color, PROGRESS_HEIGHT);
                    }

                    /* Add spacing between sources (except after last) */
                    if (i < NUM_SOURCES - 1) {
                        forge_ui_ctx_separator_layout(ui, 6.0f);
                    }
                }

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
    for (int i = 0; i < NUM_SOURCES; i++) {
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
