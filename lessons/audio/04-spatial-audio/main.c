/*
 * Audio Lesson 04 — Spatial Audio
 *
 * Demonstrates: 3D audio positioning with distance attenuation, stereo
 * panning from 3D position, and Doppler pitch shifting.  Four colored
 * spheres orbit the listener at different distances and speeds, each
 * emitting a looping sound.  The spatial layer computes per-source
 * attenuation, pan, and optional Doppler — then the mixer processes
 * the audio unchanged.
 *
 * Uses forge_scene.h for all rendering boilerplate (device, window,
 * pipelines, camera, grid, shadow map, sky, UI) — this file focuses
 * on the spatial audio system.
 *
 * Controls:
 *   WASD / Arrow keys — move camera (also moves listener)
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause/resume audio stream
 *   R                 — reset orbit angles and camera
 *   D                 — toggle Doppler on/off
 *   1                 — cycle attenuation: Linear → Inverse → Exponential
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Audio library — WAV loading, mixing, spatial audio */
#include "audio/forge_audio.h"

/* Procedural geometry — sphere mesh */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer — replaces ~500 lines of rendering boilerplate */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

/* Number of spatial sources */
#define NUM_SOURCES       4

/* Sphere rendering */
#define SPHERE_RADIUS     0.4f
#define SPHERE_SLICES     24
#define SPHERE_STACKS     12
#define SPHERE_Y          1.0f   /* height above ground */

/* Camera start position */
#define CAM_START_X       0.0f
#define CAM_START_Y       3.0f
#define CAM_START_Z       8.0f
#define CAM_START_PITCH  -0.2f

/* Audio mixing */
/* Must be >= FORGE_AUDIO_SAMPLE_RATE * DT_CLAMP to avoid underflow
 * on slow frames.  44100 * 0.1 = 4410, so 4480 gives headroom. */
#define MIX_BUFFER_FRAMES 4480

/* UI panel layout */
#define PANEL_X           10.0f
#define PANEL_Y           10.0f
#define PANEL_W           420.0f
#define PANEL_H           680.0f
#define LABEL_HEIGHT      22.0f
#define SLIDER_HEIGHT     26.0f
#define CHECKBOX_HEIGHT   24.0f
#define SEPARATOR_HEIGHT  4.0f

/* Font size for UI text (pixels) */
#define FONT_SIZE         16.0f

/* Delta time clamp to prevent physics/audio jumps on long frames */
#define DT_CLAMP          0.1f

/* Initial volume for each spatial source (before attenuation).
 * Kept low because 4 simultaneous sources sum in the mixer. */
#define SOURCE_VOLUME     0.2f

/* Default master volume — conservative to avoid clipping 4 sources */
#define MASTER_VOLUME_DEFAULT 0.6f

/* Minimum dt for velocity computation — avoids division by near-zero */
#define DT_VELOCITY_MIN   0.001f

/* Maximum volume slider range (>1.0 allows boost) */
#define VOLUME_MAX        2.0f

/* Muted sphere color (gray) */
#define MUTED_R  0.35f
#define MUTED_G  0.35f
#define MUTED_B  0.35f

/* Source orbit parameters: {radius, speed (radians/sec), start_angle} */
static const float SOURCE_ORBITS[NUM_SOURCES][3] = {
    {  3.0f, 1.0f,   0.0f          },  /* close, fast */
    {  6.0f, 0.6f,   FORGE_PI*0.5f },  /* medium */
    { 10.0f, 0.35f,  FORGE_PI       },  /* far, slow */
    { 15.0f, 0.2f,   FORGE_PI*1.5f },  /* very far, very slow */
};

/* Sphere colors (RGB) — one distinct color per source */
static const float SOURCE_COLORS[NUM_SOURCES][3] = {
    { 0.9f, 0.25f, 0.25f },  /* red */
    { 0.25f, 0.8f,  0.3f },  /* green */
    { 0.3f, 0.45f,  0.9f },  /* blue */
    { 0.9f, 0.8f,  0.15f },  /* yellow */
};

/* Source labels */
static const char *SOURCE_NAMES[NUM_SOURCES] = {
    "Wind",
    "Fan Hum",
    "Alarm",
    "Steam",
};

/* WAV file paths (relative to CWD) */
static const char *WAV_PATHS[NUM_SOURCES] = {
    "assets/audio/spatial_1.wav",
    "assets/audio/spatial_2.wav",
    "assets/audio/spatial_3.wav",
    "assets/audio/spatial_4.wav",
};

/* Attenuation model names for UI */
static const char *ATTENUATION_NAMES[] = {
    "Linear",
    "Inverse",
    "Exponential",
};

/* ── Types ────────────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* Sphere GPU geometry */
    SDL_GPUBuffer *sphere_vb;          /* vertex buffer for sphere mesh */
    SDL_GPUBuffer *sphere_ib;          /* index buffer for sphere mesh */
    Uint32         sphere_index_count; /* number of indices in sphere mesh */

    /* Audio */
    SDL_AudioStream         *audio_stream;               /* output stream to default playback device */
    ForgeAudioBuffer         buffers[NUM_SOURCES];        /* decoded WAV data per source (F32 stereo) */
    ForgeAudioSource         sources[NUM_SOURCES];        /* playback sources (all looping) */
    ForgeAudioSpatialSource  spatial[NUM_SOURCES];        /* 3D position wrappers per source */
    ForgeAudioMixer          mixer;                       /* multi-channel mixer with master bus */
    ForgeAudioListener       listener;                    /* listener built from camera each frame */
    float                    mix_scratch[MIX_BUFFER_FRAMES * 2]; /* stereo scratch buffer for per-frame mixing */
    bool                     audio_paused;                /* true when audio stream is paused via P key */

    /* Orbit state */
    float orbit_angles[NUM_SOURCES];       /* current angle per source (radians) */
    vec3  source_positions[NUM_SOURCES];   /* computed 3D world positions */
    vec3  prev_positions[NUM_SOURCES];     /* previous frame positions (for Doppler velocity) */

    /* Per-source UI controls */
    float source_volumes[NUM_SOURCES];     /* per-source volume [0..VOLUME_MAX] */
    bool  source_muted[NUM_SOURCES];       /* per-source mute toggle */
    bool  orbit_paused[NUM_SOURCES];       /* per-source orbit freeze toggle */

    /* UI state */
    ForgeUiWindowState ui_window;          /* draggable panel position and state */
    bool               doppler_enabled;    /* global Doppler toggle (D key) */
    int                attenuation_model;  /* 0=linear, 1=inverse, 2=exponential */
    float              master_volume;      /* master volume [0..VOLUME_MAX] */
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
        "Audio Lesson 04 \xe2\x80\x94 Spatial Audio");
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
    for (int i = 0; i < NUM_SOURCES; i++) {
        if (!forge_audio_load_wav(WAV_PATHS[i], &state->buffers[i])) {
            SDL_Log("WARNING: Could not load '%s' — source %d will be silent",
                    WAV_PATHS[i], i + 1);
            all_loaded = false;
        }
    }
    if (!all_loaded) {
        SDL_Log("NOTE: Place spatial WAV files in assets/audio/ (see README)");
    }

    /* Create sources, spatial wrappers, and set up mixer */
    state->mixer = forge_audio_mixer_create();
    for (int i = 0; i < NUM_SOURCES; i++) {
        state->sources[i] = forge_audio_source_create(
            &state->buffers[i], SOURCE_VOLUME, true);
        state->sources[i].playing = true;

        vec3 start_pos = vec3_create(
            SOURCE_ORBITS[i][0] * SDL_cosf(SOURCE_ORBITS[i][2]),
            SPHERE_Y,
            SOURCE_ORBITS[i][0] * SDL_sinf(SOURCE_ORBITS[i][2]));

        int ch_idx = forge_audio_mixer_add_channel(
            &state->mixer, &state->sources[i]);

        state->spatial[i] = forge_audio_spatial_source_create(
            &state->sources[i], start_pos, &state->mixer, ch_idx);

        state->orbit_angles[i] = SOURCE_ORBITS[i][2];
        state->source_positions[i] = start_pos;
        state->prev_positions[i] = start_pos;

        if (ch_idx < 0) {
            SDL_Log("WARNING: Could not add channel %d to mixer", i + 1);
        }
    }

    state->audio_paused = false;
    state->doppler_enabled = false;
    state->attenuation_model = 0;  /* linear */
    state->master_volume = MASTER_VOLUME_DEFAULT;

    for (int i = 0; i < NUM_SOURCES; i++) {
        state->source_volumes[i] = 1.0f;  /* per-source gain, multiplied with spatial */
        state->source_muted[i] = false;
        state->orbit_paused[i] = false;
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

    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        SDL_Scancode sc = event->key.scancode;

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

        /* R = reset orbits and positions (must also reset prev_positions
         * and spatial velocity to avoid a Doppler spike on the next frame) */
        if (sc == SDL_SCANCODE_R) {
            for (int i = 0; i < NUM_SOURCES; i++) {
                state->orbit_angles[i] = SOURCE_ORBITS[i][2];
                vec3 start_pos = vec3_create(
                    SOURCE_ORBITS[i][0] * SDL_cosf(SOURCE_ORBITS[i][2]),
                    SPHERE_Y,
                    SOURCE_ORBITS[i][0] * SDL_sinf(SOURCE_ORBITS[i][2]));
                state->source_positions[i] = start_pos;
                state->prev_positions[i] = start_pos;
                state->spatial[i].position = start_pos;
                state->spatial[i].velocity = vec3_create(0.0f, 0.0f, 0.0f);
            }
            SDL_Log("Orbits reset");
        }

        /* D = toggle Doppler */
        if (sc == SDL_SCANCODE_D) {
            state->doppler_enabled = !state->doppler_enabled;
            for (int i = 0; i < NUM_SOURCES; i++) {
                state->spatial[i].doppler_enabled = state->doppler_enabled;
                if (!state->doppler_enabled) {
                    state->sources[i].playback_rate = 1.0f;
                }
            }
            SDL_Log("Doppler: %s", state->doppler_enabled ? "ON" : "OFF");
        }

        /* 1 = cycle attenuation model */
        if (sc == SDL_SCANCODE_1) {
            state->attenuation_model = (state->attenuation_model + 1) % 3;
            for (int i = 0; i < NUM_SOURCES; i++) {
                state->spatial[i].attenuation =
                    (ForgeAudioAttenuationModel)state->attenuation_model;
            }
            SDL_Log("Attenuation: %s",
                    ATTENUATION_NAMES[state->attenuation_model]);
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
    if (dt > DT_CLAMP) dt = DT_CLAMP;

    /* ── Update orbit positions ──────────────────────────────────── */

    for (int i = 0; i < NUM_SOURCES; i++) {
        state->prev_positions[i] = state->source_positions[i];
        if (!state->orbit_paused[i]) {
            state->orbit_angles[i] += SOURCE_ORBITS[i][1] * dt;
        }

        float radius = SOURCE_ORBITS[i][0];
        float angle  = state->orbit_angles[i];
        state->source_positions[i] = vec3_create(
            radius * SDL_cosf(angle),
            SPHERE_Y,
            radius * SDL_sinf(angle));
    }

    /* ── Build listener from camera ──────────────────────────────── */

    quat cam_orient = quat_from_euler(
        state->scene.cam_yaw, state->scene.cam_pitch, 0.0f);
    state->listener = forge_audio_listener_from_camera(
        state->scene.cam_position, cam_orient);

    /* ── Apply spatial audio ─────────────────────────────────────── */

    for (int i = 0; i < NUM_SOURCES; i++) {
        state->spatial[i].position = state->source_positions[i];

        /* Compute velocity from position delta for Doppler */
        if (dt > DT_VELOCITY_MIN) {
            vec3 delta = vec3_sub(state->source_positions[i],
                                  state->prev_positions[i]);
            state->spatial[i].velocity = vec3_scale(delta, 1.0f / dt);
        }

        forge_audio_spatial_apply(&state->listener, &state->spatial[i]);

        /* Layer per-source UI volume and mute on top of spatial attenuation.
         * Use the bound channel index rather than assuming i == channel. */
        int ch = state->spatial[i].channel;
        if (ch >= 0 && ch < state->mixer.channel_count) {
            state->mixer.channels[ch].volume *= state->source_volumes[i];
            state->mixer.channels[ch].mute = state->source_muted[i];
        }
    }

    /* ── Audio mixing ────────────────────────────────────────────── */

    if (!state->audio_paused) {
        state->mixer.master_volume = state->master_volume;
        forge_audio_mixer_update_peaks(&state->mixer, dt);

        int frames_needed = (int)((float)FORGE_AUDIO_SAMPLE_RATE * dt);
        if (frames_needed < 1) frames_needed = 1;
        if (frames_needed > MIX_BUFFER_FRAMES)
            frames_needed = MIX_BUFFER_FRAMES;

        forge_audio_mixer_mix(&state->mixer,
                              state->mix_scratch, frames_needed);

        if (state->audio_stream) {
            int bytes = frames_needed * FORGE_AUDIO_CHANNELS
                      * (int)sizeof(float);
            if (!SDL_PutAudioStreamData(state->audio_stream,
                                         state->mix_scratch, bytes)) {
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
        mat4 model = mat4_multiply(
            mat4_translate(state->source_positions[i]),
            mat4_scale_uniform(SPHERE_RADIUS));
        forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                     state->sphere_index_count, model);
    }
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */

    forge_scene_begin_main_pass(s);
    for (int i = 0; i < NUM_SOURCES; i++) {
        mat4 model = mat4_multiply(
            mat4_translate(state->source_positions[i]),
            mat4_scale_uniform(SPHERE_RADIUS));

        /* Gray when muted, source color when active */
        float color[4];
        if (state->source_muted[i]) {
            color[0] = MUTED_R;
            color[1] = MUTED_G;
            color[2] = MUTED_B;
        } else {
            color[0] = SOURCE_COLORS[i][0];
            color[1] = SOURCE_COLORS[i][1];
            color[2] = SOURCE_COLORS[i][2];
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
            if (forge_ui_wctx_window_begin(wctx, "Spatial Audio",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;

                /* ── Global controls ─────────────────────────────── */

                /* Attenuation model display */
                {
                    char buf[64];
                    SDL_snprintf(buf, sizeof(buf), "Atten: %s [1]",
                                 ATTENUATION_NAMES[state->attenuation_model]);
                    forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                }

                /* Doppler toggle */
                forge_ui_ctx_checkbox_layout(ui, "Doppler [D]",
                    &state->doppler_enabled, CHECKBOX_HEIGHT);

                /* When toggled via UI checkbox, update all spatial sources */
                for (int i = 0; i < NUM_SOURCES; i++) {
                    state->spatial[i].doppler_enabled =
                        state->doppler_enabled;
                    if (!state->doppler_enabled) {
                        state->sources[i].playback_rate = 1.0f;
                    }
                }

                /* Master volume slider */
                forge_ui_ctx_label_layout(ui, "Master Volume", LABEL_HEIGHT);
                forge_ui_ctx_slider_layout(ui, "##master_vol",
                    &state->master_volume, 0.0f, VOLUME_MAX, SLIDER_HEIGHT);

                forge_ui_ctx_separator_layout(ui, SEPARATOR_HEIGHT);

                /* ── Per-source strips ───────────────────────────── */

                for (int i = 0; i < NUM_SOURCES; i++) {
                    /* Source label with color hint */
                    forge_ui_ctx_label_layout(ui, SOURCE_NAMES[i],
                                              LABEL_HEIGHT);

                    /* Spatial readout: distance, effective gain, pan.
                     * Effective gain includes spatial attenuation AND the
                     * per-source UI slider, matching what the mixer uses. */
                    {
                        vec3 diff = vec3_sub(state->source_positions[i],
                                             state->listener.position);
                        float dist = vec3_length(diff);
                        float effective_vol = state->sources[i].volume
                                            * state->source_volumes[i];
                        char buf[80];
                        SDL_snprintf(buf, sizeof(buf),
                                     "dist=%.1f  gain=%.2f  pan=%+.2f",
                                     (double)dist,
                                     (double)effective_vol,
                                     (double)state->sources[i].pan);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }

                    /* Playback rate if Doppler active */
                    if (state->doppler_enabled) {
                        char buf[48];
                        SDL_snprintf(buf, sizeof(buf), "rate=%.3f",
                                     (double)state->sources[i].playback_rate);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }

                    /* Per-source volume slider */
                    {
                        char label[32];
                        SDL_snprintf(label, sizeof(label), "##svol_%d", i);
                        forge_ui_ctx_slider_layout(ui, label,
                            &state->source_volumes[i],
                            0.0f, VOLUME_MAX, SLIDER_HEIGHT);
                    }

                    /* Mute and orbit-pause checkboxes */
                    {
                        char label[32];
                        SDL_snprintf(label, sizeof(label), "Mute##m%d", i);
                        forge_ui_ctx_checkbox_layout(ui, label,
                            &state->source_muted[i], CHECKBOX_HEIGHT);
                    }
                    {
                        char label[32];
                        SDL_snprintf(label, sizeof(label),
                                     "Freeze orbit##f%d", i);
                        forge_ui_ctx_checkbox_layout(ui, label,
                            &state->orbit_paused[i], CHECKBOX_HEIGHT);
                    }

                    forge_ui_ctx_separator_layout(ui, SEPARATOR_HEIGHT);
                }

                /* Controls reminder */
                forge_ui_ctx_label_layout(ui, "P:pause  R:reset  D:doppler",
                                          LABEL_HEIGHT);
                forge_ui_ctx_label_layout(ui, "1:cycle atten model",
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
