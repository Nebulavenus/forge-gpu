/*
 * Audio Lesson 03 — Audio Mixing
 *
 * Demonstrates: Multi-channel mixer with per-channel volume, pan, mute,
 * and solo controls; soft clipping (tanh saturation) on the master bus;
 * peak metering with hold indicators; VU meter UI widget.
 *
 * Five colored spheres represent five audio stems loaded from WAV files.
 * A DAW-style mixer panel provides per-channel strips (label, VU meter,
 * volume slider, pan slider, mute/solo checkboxes) and a master strip.
 * Spheres pulse with their channel's peak level and turn gray when muted.
 *
 * Uses forge_scene.h for all rendering boilerplate (device, window,
 * pipelines, camera, grid, shadow map, sky, UI) — this file focuses
 * on the mixer.
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   1–5               — toggle mute on channels 1–5
 *   P                 — pause/resume audio stream
 *   R                 — reset all channels (unmute, center pan, unity volume)
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Audio library — WAV loading, mixing, fading, mixer */
#include "audio/forge_audio.h"

/* Procedural geometry — sphere mesh */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer — replaces ~500 lines of rendering boilerplate */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

/* Number of stems / channels */
#define NUM_STEMS         5

/* Sphere rendering */
#define SPHERE_RADIUS     0.5f
#define SPHERE_SLICES     32
#define SPHERE_STACKS     16
#define SPHERE_SPACING    3.0f   /* distance between sphere centers on X */
#define SPHERE_Y          1.0f   /* height above ground */

/* Camera start position — centered on the 5 spheres */
#define CAM_START_X       6.0f
#define CAM_START_Y       3.0f
#define CAM_START_Z       8.0f
#define CAM_START_PITCH  -0.25f

/* Audio mixing */
#define MIX_BUFFER_FRAMES 4096   /* scratch buffer size for mixing */

/* UI panel layout */
#define PANEL_X           10.0f
#define PANEL_Y           10.0f
#define PANEL_W           320.0f
#define PANEL_H           600.0f
#define LABEL_HEIGHT      22.0f
#define SLIDER_HEIGHT     26.0f
#define VU_HEIGHT         60.0f
#define CHECKBOX_HEIGHT   24.0f

/* Font size for UI text (pixels) */
#define FONT_SIZE         16.0f

/* UI separator height (pixels) */
#define SEPARATOR_HEIGHT  4.0f

/* Scale pulse when playing */
#define PULSE_AMPLITUDE   0.08f

/* Peak level below which spheres don't pulse (avoids jitter at silence) */
#define PEAK_VISUAL_THRESHOLD 0.01f

/* Maximum volume slider range (>1.0 allows boost) */
#define VOLUME_MAX        2.0f

/* Delta time clamp to prevent physics/audio jumps on long frames */
#define DT_CLAMP          0.1f

/* Sphere colors (RGB) — one distinct color per stem */
static const float STEM_COLORS[NUM_STEMS][3] = {
    { 0.9f, 0.25f, 0.25f },  /* red — stem 1 */
    { 0.25f, 0.45f, 0.9f },  /* blue — stem 2 */
    { 0.9f, 0.8f, 0.15f },   /* yellow — stem 3 */
    { 0.7f, 0.3f, 0.85f },   /* purple — stem 4 */
    { 0.15f, 0.85f, 0.8f },  /* cyan — stem 5 */
};

/* Muted sphere color */
#define MUTED_R  0.35f
#define MUTED_G  0.35f
#define MUTED_B  0.35f

/* Stem labels */
static const char *STEM_NAMES[NUM_STEMS] = {
    "Layer 1", "Layer 2", "Layer 3", "Layer 4", "Layer 5"
};

/* WAV file paths (relative to CWD) */
static const char *WAV_PATHS[NUM_STEMS] = {
    "assets/audio/stem_1.wav",
    "assets/audio/stem_2.wav",
    "assets/audio/stem_3.wav",
    "assets/audio/stem_4.wav",
    "assets/audio/stem_5.wav"
};

/* ── Types ────────────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;  /* rendering: device, window, pipelines, camera, UI */

    /* Sphere GPU geometry */
    SDL_GPUBuffer *sphere_vb;          /* vertex buffer for sphere mesh */
    SDL_GPUBuffer *sphere_ib;          /* index buffer for sphere mesh */
    Uint32         sphere_index_count; /* number of indices in sphere mesh */

    /* Audio */
    SDL_AudioStream    *audio_stream;  /* output stream to default playback device */
    ForgeAudioBuffer    buffers[NUM_STEMS];   /* decoded WAV data per stem (F32 stereo) */
    ForgeAudioSource    sources[NUM_STEMS];   /* playback sources (all looping) */
    ForgeAudioMixer     mixer;                /* multi-channel mixer with per-channel controls */
    float               mix_scratch[MIX_BUFFER_FRAMES * 2]; /* stereo scratch buffer for per-frame mixing */
    bool                audio_paused;  /* true when audio stream is paused via P key */

    /* UI state */
    ForgeUiWindowState ui_window;      /* draggable mixer panel position and state */
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
        "Audio Lesson 03 \xe2\x80\x94 Audio Mixing");
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

    /* Load WAV stems */
    bool all_loaded = true;
    for (int i = 0; i < NUM_STEMS; i++) {
        if (!forge_audio_load_wav(WAV_PATHS[i], &state->buffers[i])) {
            SDL_Log("WARNING: Could not load '%s' — channel %d will be silent",
                    WAV_PATHS[i], i + 1);
            all_loaded = false;
        }
    }
    if (!all_loaded) {
        SDL_Log("NOTE: Place stem WAV files in assets/audio/ (see README)");
    }

    /* Create sources (all looping) and set up mixer */
    state->mixer = forge_audio_mixer_create();
    for (int i = 0; i < NUM_STEMS; i++) {
        state->sources[i] = forge_audio_source_create(
            &state->buffers[i], 1.0f, true);
        state->sources[i].playing = true;
        int ch_idx = forge_audio_mixer_add_channel(&state->mixer, &state->sources[i]);
        if (ch_idx < 0) {
            SDL_Log("WARNING: Could not add channel %d to mixer", i + 1);
        }
    }

    state->audio_paused = false;

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

        /* 1–5 toggle mute on channels */
        if (sc >= SDL_SCANCODE_1 && sc <= (SDL_Scancode)(SDL_SCANCODE_1 + NUM_STEMS - 1)) {
            int ch = (int)(sc - SDL_SCANCODE_1);
            if (ch < state->mixer.channel_count) {
                state->mixer.channels[ch].mute =
                    !state->mixer.channels[ch].mute;
                SDL_Log("%s: %s", STEM_NAMES[ch],
                        state->mixer.channels[ch].mute ? "muted" : "unmuted");
            }
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

        /* R = reset all channels to defaults */
        if (sc == SDL_SCANCODE_R) {
            for (int i = 0; i < state->mixer.channel_count; i++) {
                state->mixer.channels[i].volume = 1.0f;
                state->mixer.channels[i].pan    = 0.0f;
                state->mixer.channels[i].mute   = false;
                state->mixer.channels[i].solo   = false;
            }
            state->mixer.master_volume = 1.0f;
            SDL_Log("Mixer reset to defaults");
        }
    }

    return SDL_APP_CONTINUE;
}

/* Compute per-channel pulse scale based on audio peak level.
 * Returns additional scale beyond SPHERE_RADIUS, or 0 if muted/silent. */
static float channel_pulse_scale(const ForgeAudioMixer *mixer, int ch,
                                  bool has_solo)
{
    bool effectively_muted = mixer->channels[ch].mute ||
                             (has_solo && !mixer->channels[ch].solo);
    if (effectively_muted) return 0.0f;

    float peak_max = mixer->channels[ch].peak_l > mixer->channels[ch].peak_r
                   ? mixer->channels[ch].peak_l : mixer->channels[ch].peak_r;
    if (peak_max <= PEAK_VISUAL_THRESHOLD) return 0.0f;

    float peak = (mixer->channels[ch].peak_l +
                  mixer->channels[ch].peak_r) * 0.5f;
    return PULSE_AMPLITUDE * peak;
}

/* ── SDL_AppIterate ──────────────────────────────────────────────── */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;
    ForgeScene *s = &state->scene;

    if (!forge_scene_begin_frame(s)) return SDL_APP_CONTINUE;

    float dt = forge_scene_dt(s);
    if (dt > DT_CLAMP) dt = DT_CLAMP;

    /* ── Audio mixing (skip while paused) ─────────────────────── */

    if (!state->audio_paused) {
        /* Update peak hold decay */
        forge_audio_mixer_update_peaks(&state->mixer, dt);

        /* Mix all channels through the mixer */
        int frames_needed = (int)((float)FORGE_AUDIO_SAMPLE_RATE * dt);
        if (frames_needed < 1) frames_needed = 1;
        if (frames_needed > MIX_BUFFER_FRAMES)
            frames_needed = MIX_BUFFER_FRAMES;

        forge_audio_mixer_mix(&state->mixer,
                              state->mix_scratch, frames_needed);

        /* Push mixed audio to output stream */
        if (state->audio_stream) {
            int bytes = frames_needed * FORGE_AUDIO_CHANNELS * (int)sizeof(float);
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

    /* Check solo state once for both passes */
    bool has_solo = false;
    for (int j = 0; j < state->mixer.channel_count; j++) {
        if (state->mixer.channels[j].solo) {
            has_solo = true;
            break;
        }
    }

    forge_scene_begin_shadow_pass(s);
    for (int i = 0; i < NUM_STEMS; i++) {
        float x = (float)i * SPHERE_SPACING;
        float scale = SPHERE_RADIUS + channel_pulse_scale(&state->mixer, i, has_solo);

        mat4 model = mat4_multiply(
            mat4_translate(vec3_create(x, SPHERE_Y, 0.0f)),
            mat4_scale_uniform(scale));
        forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                     state->sphere_index_count, model);
    }
    forge_scene_end_shadow_pass(s);

    /* ── Main pass ───────────────────────────────────────────────── */

    forge_scene_begin_main_pass(s);
    for (int i = 0; i < NUM_STEMS; i++) {
        float x = (float)i * SPHERE_SPACING;
        float scale = SPHERE_RADIUS + channel_pulse_scale(&state->mixer, i, has_solo);

        /* Need mute state for color selection below */
        bool effectively_muted = state->mixer.channels[i].mute ||
                                 (has_solo && !state->mixer.channels[i].solo);

        mat4 model = mat4_multiply(
            mat4_translate(vec3_create(x, SPHERE_Y, 0.0f)),
            mat4_scale_uniform(scale));

        /* Color: stem color when active, gray when muted/silenced */
        float color[4];
        if (effectively_muted) {
            color[0] = MUTED_R;
            color[1] = MUTED_G;
            color[2] = MUTED_B;
        } else {
            color[0] = STEM_COLORS[i][0];
            color[1] = STEM_COLORS[i][1];
            color[2] = STEM_COLORS[i][2];
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
            if (forge_ui_wctx_window_begin(wctx, "Mixer",
                                            &state->ui_window)) {
                ForgeUiContext *ui = wctx->ctx;

                /* ── Per-channel strips ───────────────────────────── */
                for (int i = 0; i < state->mixer.channel_count; i++) {
                    ForgeAudioChannel *ch = &state->mixer.channels[i];

                    /* Channel label with number key hint */
                    {
                        char buf[64];
                        SDL_snprintf(buf, sizeof(buf), "[%d] %s",
                                     i + 1, STEM_NAMES[i]);
                        forge_ui_ctx_label_layout(ui, buf, LABEL_HEIGHT);
                    }

                    /* VU meter */
                    forge_ui_ctx_vu_meter_layout(ui,
                        ch->peak_l, ch->peak_r,
                        ch->peak_hold_l, ch->peak_hold_r,
                        VU_HEIGHT);

                    /* Volume slider */
                    {
                        char label[32];
                        SDL_snprintf(label, sizeof(label), "##vol_%d", i);
                        forge_ui_ctx_slider_layout(ui, label,
                            &ch->volume, 0.0f, VOLUME_MAX, SLIDER_HEIGHT);
                    }

                    /* Pan slider */
                    {
                        char label[32];
                        SDL_snprintf(label, sizeof(label), "##pan_%d", i);
                        forge_ui_ctx_slider_layout(ui, label,
                            &ch->pan, -1.0f, 1.0f, SLIDER_HEIGHT);
                    }

                    /* Mute / Solo checkboxes */
                    {
                        char label[32];
                        SDL_snprintf(label, sizeof(label), "Mute##m%d", i);
                        forge_ui_ctx_checkbox_layout(ui, label,
                            &ch->mute, CHECKBOX_HEIGHT);
                    }
                    {
                        char label[32];
                        SDL_snprintf(label, sizeof(label), "Solo##s%d", i);
                        forge_ui_ctx_checkbox_layout(ui, label,
                            &ch->solo, CHECKBOX_HEIGHT);
                    }

                    forge_ui_ctx_separator_layout(ui, SEPARATOR_HEIGHT);
                }

                /* ── Master strip ─────────────────────────────────── */
                forge_ui_ctx_label_layout(ui, "Master", LABEL_HEIGHT);

                /* Master VU meter */
                forge_ui_ctx_vu_meter_layout(ui,
                    state->mixer.master_peak_l,
                    state->mixer.master_peak_r,
                    state->mixer.master_peak_hold_l,
                    state->mixer.master_peak_hold_r,
                    VU_HEIGHT);

                /* Master volume slider */
                forge_ui_ctx_slider_layout(ui, "##master_vol",
                    &state->mixer.master_volume,
                    0.0f, VOLUME_MAX, SLIDER_HEIGHT);

                forge_ui_ctx_separator_layout(ui, SEPARATOR_HEIGHT);

                /* Controls reminder */
                forge_ui_ctx_label_layout(ui, "1-5: mute  R: reset",
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
    for (int i = 0; i < NUM_STEMS; i++) {
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
