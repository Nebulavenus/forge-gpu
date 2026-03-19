/*
 * Audio Lesson 06 — DSP Effects
 *
 * Demonstrates: callback-based DSP effect system with biquad filter, delay
 * line, Schroeder reverb, and chorus.  Per-channel and master-bus effect
 * chains integrated into the mixer.  Presets combine effects for underwater,
 * cave, and radio simulations.
 *
 * Two audio sources (speech clip + music track) loaded via WAV.  Colored
 * spheres per source pulse with VU levels.  Three draggable UI windows
 * provide source selection, per-effect parameter control with bypass
 * toggles, and master output metering.
 *
 * Uses forge_scene.h for all rendering boilerplate (device, window,
 * pipelines, camera, grid, shadow map, UI).
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause/resume audio
 *   R                 — restart current source from beginning
 *   1-2               — switch audio source
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "math/forge_math.h"
#include "audio/forge_audio.h"

#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

#define NUM_SOURCES       2

/* Audio source file paths (relative to working directory) */
static const char *SOURCE_PATHS[NUM_SOURCES] = {
    "lessons/audio/06-dsp-effects/assets/speech_lecture.wav",
    "lessons/audio/06-dsp-effects/assets/music_ambient.wav",
};

static const char *SOURCE_NAMES[NUM_SOURCES] = {
    "Speech (Lecture)",
    "Music (Ambient)",
};

static const float SOURCE_COLORS[NUM_SOURCES][3] = {
    { 0.30f, 0.70f, 0.95f },  /* cyan - speech */
    { 0.95f, 0.55f, 0.20f },  /* orange - music */
};

/* Sphere rendering */
#define SPHERE_RADIUS     0.6f
#define SPHERE_SLICES     20
#define SPHERE_STACKS     10
#define SPHERE_SPACING    3.0f
#define SPHERE_Y          2.0f

/* Camera */
#define CAM_START_X       1.5f
#define CAM_START_Y       4.0f
#define CAM_START_Z       7.0f
#define CAM_START_PITCH  -0.52f

/* Audio */
#define MIX_BUFFER_FRAMES 4096
#define DT_CLAMP          0.1f
#define VOLUME_MAX        1.5f
#define MASTER_VOL_DEFAULT 0.7f

/* VU meter smoothing */
#define VU_ATTACK         0.3f
#define VU_RELEASE        0.05f
#define VU_REFERENCE_FPS  60.0f
#define VU_PEAK_HOLD_TIME 1.5f
#define VU_PEAK_DECAY     0.6f

/* UI layout */
#define PANEL_X           10.0f
#define PANEL_Y           10.0f
#define PANEL_W           340.0f
#define LH                22.0f
#define SH                26.0f
#define SEP               4.0f
#define VU_H              160.0f
#define CB_H              22.0f
#define WIN_H             440.0f
#define WIN_TOP           280.0f
#define WIN_GAP           12.0f
#define FONT_SIZE         16.0f

/* Effect presets */
#define NUM_PRESETS        4
static const char *PRESET_NAMES[NUM_PRESETS] = {
    "None", "Underwater", "Cave", "Radio"
};

/* ── Types ────────────────────────────────────────────────────────── */

typedef struct app_state {
    ForgeScene scene;

    /* Sphere geometry */
    SDL_GPUBuffer *sphere_vb;
    SDL_GPUBuffer *sphere_ib;
    Uint32         sphere_index_count;

    /* Audio output */
    SDL_AudioStream *audio_stream;

    /* Audio sources */
    ForgeAudioBuffer buffers[NUM_SOURCES];
    ForgeAudioSource sources[NUM_SOURCES];
    bool             source_loaded[NUM_SOURCES];
    int              active_source;

    /* Mixer */
    ForgeAudioMixer mixer;
    float           mix_scratch[MIX_BUFFER_FRAMES * 2];

    /* DSP effect instances — allocated once, reused across presets */
    ForgeAudioBiquad  biquad1;   /* first filter (LP/HP/BP) */
    ForgeAudioBiquad  biquad2;   /* second filter (for radio preset) */
    ForgeAudioDelay   delay;
    ForgeAudioReverb  reverb;
    ForgeAudioChorus  chorus;
    bool              effects_initialized;

    /* Current preset applied to master bus */
    int active_preset;

    /* Per-effect manual controls */
    float filter_cutoff;   /* Hz */
    float filter_q;        /* resonance */
    int   filter_type;     /* 0=LP, 1=HP, 2=BP */
    float delay_time;      /* seconds */
    float delay_feedback;  /* [0..1) */
    float delay_wet;       /* [0..1] */
    float reverb_room;     /* [0..1] */
    float reverb_damping;  /* [0..1] */
    float reverb_wet;      /* [0..1] */
    float chorus_rate;     /* Hz */
    float chorus_depth;    /* seconds */
    float chorus_wet;      /* [0..1] */

    /* Bypass toggles */
    bool filter_bypass;
    bool delay_bypass;
    bool reverb_bypass;
    bool chorus_bypass;

    /* VU metering — smoothed levels for UI display */
    float source_vu_l[NUM_SOURCES];       /* smoothed left level per source [0..1] */
    float source_vu_r[NUM_SOURCES];       /* smoothed right level per source [0..1] */
    float source_peak_l[NUM_SOURCES];     /* peak hold left per source [0..1] */
    float source_peak_r[NUM_SOURCES];     /* peak hold right per source [0..1] */
    float source_peak_timer_l[NUM_SOURCES]; /* seconds until left peak decays */
    float source_peak_timer_r[NUM_SOURCES]; /* seconds until right peak decays */
    float master_vu_l;         /* smoothed master left level [0..1] */
    float master_vu_r;         /* smoothed master right level [0..1] */
    float master_peak_l;       /* master peak hold left [0..1] */
    float master_peak_r;       /* master peak hold right [0..1] */
    float master_peak_timer_l; /* seconds until master left peak decays */
    float master_peak_timer_r; /* seconds until master right peak decays */

    /* UI state */
    ForgeUiWindowState win_source;   /* draggable window: source selector + VU */
    ForgeUiWindowState win_effects;  /* draggable window: effect controls */
    ForgeUiWindowState win_master;   /* draggable window: master volume + output */
    float              master_volume; /* master bus gain [0..VOLUME_MAX] */
    bool               audio_paused;  /* true when SDL audio stream is paused */
    float              sample_carry;  /* fractional sample accumulator (avoids dt truncation) */
} app_state;

/* ── Helpers ──────────────────────────────────────────────────────── */

static SDL_GPUBuffer *upload_shape_vb(ForgeScene *scene,
                                       const ForgeShape *shape)
{
    ForgeSceneVertex *verts = SDL_calloc((size_t)shape->vertex_count,
                                         sizeof(ForgeSceneVertex));
    if (!verts) return NULL;
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

static void vu_update(float *vu, float *peak, float *peak_timer,
                      float level, float dt)
{
    float base = (level > *vu) ? VU_ATTACK : VU_RELEASE;
    float alpha = 1.0f - SDL_powf(1.0f - base, dt * VU_REFERENCE_FPS);
    *vu += (level - *vu) * alpha;

    if (level >= *peak) {
        *peak = level;
        *peak_timer = VU_PEAK_HOLD_TIME;
    } else if (*peak_timer > 0.0f) {
        *peak_timer -= dt;
    } else {
        *peak -= VU_PEAK_DECAY * dt;
        if (*peak < 0.0f) *peak = 0.0f;
    }
}

/* Initialize all DSP effect instances */
static bool init_effects(app_state *state)
{
    if (state->effects_initialized) return true;

    forge_audio_biquad_init(&state->biquad1);
    forge_audio_biquad_init(&state->biquad2);

    if (!forge_audio_delay_init(&state->delay, 0.3f)) {
        SDL_Log("ERROR: Failed to init delay line");
        return false;
    }

    if (!forge_audio_reverb_init(&state->reverb, 0.5f, 0.5f)) {
        SDL_Log("ERROR: Failed to init reverb");
        forge_audio_delay_free(&state->delay);
        return false;
    }

    if (!forge_audio_chorus_init(&state->chorus, 1.0f, 0.005f)) {
        SDL_Log("ERROR: Failed to init chorus");
        forge_audio_delay_free(&state->delay);
        forge_audio_reverb_free(&state->reverb);
        return false;
    }

    state->effects_initialized = true;
    return true;
}

/* Apply a preset to the master effect chain */
static void apply_preset(app_state *state, int preset)
{
    ForgeAudioEffectChain *chain = &state->mixer.master_effects;
    state->active_preset = preset;

    /* Reset all bypass toggles — presets own the full chain */
    state->filter_bypass = true;
    state->delay_bypass  = true;
    state->reverb_bypass = true;
    state->chorus_bypass = true;

    switch (preset) {
    case 0: /* None */
        forge_audio_effect_chain_clear(chain);
        break;
    case 1: /* Underwater */
        forge_audio_preset_underwater(chain, &state->biquad1, &state->reverb);
        state->filter_bypass = false;
        state->reverb_bypass = false;
        state->filter_cutoff = 500.0f;
        state->filter_q = 0.7f;
        state->filter_type = 0;
        state->reverb_room = 0.8f;
        state->reverb_damping = 0.7f;
        state->reverb_wet = 0.6f;
        break;
    case 2: /* Cave */
        forge_audio_preset_cave(chain, &state->reverb, &state->delay);
        state->reverb_bypass = false;
        state->delay_bypass  = false;
        state->reverb_room = 0.95f;
        state->reverb_damping = 0.3f;
        state->reverb_wet = 0.7f;
        state->delay_time = 0.4f;
        state->delay_feedback = 0.5f;
        state->delay_wet = 0.4f;
        break;
    case 3: /* Radio */
        forge_audio_preset_radio(chain, &state->biquad1, &state->biquad2);
        state->filter_bypass = false;
        state->filter_cutoff = 800.0f;
        state->filter_q = 0.707f;
        state->filter_type = 1;
        break;
    }
}

/* Update effect parameters in-place without rebuilding the chain.
 * Called when sliders change — the chain structure stays the same,
 * only the parameters on the existing effect instances are updated. */
static void update_effect_params(app_state *state)
{
    forge_audio_biquad_set(&state->biquad1,
        (ForgeAudioBiquadType)state->filter_type,
        state->filter_cutoff, state->filter_q);
    forge_audio_delay_set_time(&state->delay, state->delay_time);
    state->delay.feedback = state->delay_feedback;
    forge_audio_reverb_set(&state->reverb,
        state->reverb_room, state->reverb_damping);
    state->chorus.rate  = state->chorus_rate;
    state->chorus.depth = state->chorus_depth;

    /* Update wet levels on chain effects that have them */
    ForgeAudioEffectChain *chain = &state->mixer.master_effects;
    for (int i = 0; i < chain->effect_count; i++) {
        void *ud = chain->effects[i].userdata;
        if (ud == &state->delay)  chain->effects[i].wet = state->delay_wet;
        if (ud == &state->reverb) chain->effects[i].wet = state->reverb_wet;
        if (ud == &state->chorus) chain->effects[i].wet = state->chorus_wet;
    }
}

/* Rebuild the effect chain from scratch — called when bypass toggles
 * change, since that alters which effects are in the chain. */
static void rebuild_effect_chain(app_state *state)
{
    ForgeAudioEffectChain *chain = &state->mixer.master_effects;
    forge_audio_effect_chain_clear(chain);

    if (!state->filter_bypass) {
        forge_audio_biquad_set(&state->biquad1,
            (ForgeAudioBiquadType)state->filter_type,
            state->filter_cutoff, state->filter_q);
        int idx = forge_audio_effect_chain_add(chain,
            forge_audio_biquad_process, &state->biquad1);
        if (idx >= 0) chain->effects[idx].bypass = false;
    }

    if (!state->delay_bypass) {
        forge_audio_delay_set_time(&state->delay, state->delay_time);
        state->delay.feedback = state->delay_feedback;
        int idx = forge_audio_effect_chain_add(chain,
            forge_audio_delay_process, &state->delay);
        if (idx >= 0) chain->effects[idx].wet = state->delay_wet;
    }

    if (!state->reverb_bypass) {
        forge_audio_reverb_set(&state->reverb,
            state->reverb_room, state->reverb_damping);
        int idx = forge_audio_effect_chain_add(chain,
            forge_audio_reverb_process, &state->reverb);
        if (idx >= 0) chain->effects[idx].wet = state->reverb_wet;
    }

    if (!state->chorus_bypass) {
        state->chorus.rate  = state->chorus_rate;
        state->chorus.depth = state->chorus_depth;
        int idx = forge_audio_effect_chain_add(chain,
            forge_audio_chorus_process, &state->chorus);
        if (idx >= 0) chain->effects[idx].wet = state->chorus_wet;
    }
}

/* ── SDL_AppInit ─────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;

    ForgeSceneConfig cfg = forge_scene_default_config(
        "Audio Lesson 06 \xe2\x80\x94 DSP Effects");
    cfg.cam_start_pos   = vec3_create(CAM_START_X, CAM_START_Y, CAM_START_Z);
    cfg.cam_start_pitch = CAM_START_PITCH;
    cfg.font_path       = "assets/fonts/liberation_mono/LiberationMono-Regular.ttf";
    cfg.font_size       = FONT_SIZE;

    if (!forge_scene_init(&state->scene, &cfg, argc, argv))
        return SDL_APP_FAILURE;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("ERROR: SDL_InitSubSystem(SDL_INIT_AUDIO): %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Sphere geometry */
    ForgeShape sphere = forge_shapes_sphere(SPHERE_SLICES, SPHERE_STACKS);
    if (sphere.vertex_count == 0) return SDL_APP_FAILURE;
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

    /* Audio output stream */
    SDL_AudioSpec spec;
    spec.format   = FORGE_AUDIO_FORMAT;
    spec.channels = FORGE_AUDIO_CHANNELS;
    spec.freq     = FORGE_AUDIO_SAMPLE_RATE;
    state->audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
    if (!state->audio_stream) {
        SDL_Log("WARNING: SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
        SDL_Log("NOTE: Continuing without audio (no audio device available)");
    } else if (!SDL_ResumeAudioStreamDevice(state->audio_stream)) {
        SDL_Log("WARNING: SDL_ResumeAudioStreamDevice failed: %s", SDL_GetError());
        SDL_DestroyAudioStream(state->audio_stream);
        state->audio_stream = NULL;
    }

    /* Load audio sources */
    for (int i = 0; i < NUM_SOURCES; i++) {
        if (forge_audio_load_wav(SOURCE_PATHS[i], &state->buffers[i])) {
            state->sources[i] = forge_audio_source_create(
                &state->buffers[i], 1.0f, true);
            state->sources[i].playing = true;
            state->source_loaded[i] = true;
        } else {
            SDL_Log("NOTE: Could not load '%s'", SOURCE_PATHS[i]);
        }
    }

    /* Set up mixer with loaded sources */
    state->mixer = forge_audio_mixer_create();
    for (int i = 0; i < NUM_SOURCES; i++) {
        if (state->source_loaded[i]) {
            int ch = forge_audio_mixer_add_channel(&state->mixer,
                                                    &state->sources[i]);
            if (ch >= 0 && i > 0) {
                state->mixer.channels[ch].mute = true;
            }
        }
    }
    state->active_source = 0;

    /* Initialize DSP effects */
    if (!init_effects(state)) {
        SDL_Log("WARNING: DSP effects unavailable (allocation failure)");
    }

    /* Default effect parameter values */
    state->filter_cutoff   = 1000.0f;
    state->filter_q        = 0.707f;
    state->filter_type     = 0;
    state->delay_time      = 0.3f;
    state->delay_feedback  = 0.3f;
    state->delay_wet       = 0.5f;
    state->reverb_room     = 0.5f;
    state->reverb_damping  = 0.5f;
    state->reverb_wet      = 0.5f;
    state->chorus_rate     = 1.0f;
    state->chorus_depth    = 0.005f;
    state->chorus_wet      = 0.5f;
    state->filter_bypass   = true;
    state->delay_bypass    = true;
    state->reverb_bypass   = true;
    state->chorus_bypass   = true;
    state->active_preset   = 0;
    state->master_volume   = MASTER_VOL_DEFAULT;

    /* Three windows arranged horizontally */
    state->win_source = forge_ui_window_state_default(
        PANEL_X, WIN_TOP, PANEL_W, WIN_H);
    state->win_effects = forge_ui_window_state_default(
        PANEL_X + PANEL_W + WIN_GAP, WIN_TOP, PANEL_W + 40.0f, WIN_H);
    state->win_master = forge_ui_window_state_default(
        PANEL_X + (PANEL_W + WIN_GAP) * 2.0f + 40.0f, WIN_TOP, PANEL_W, WIN_H);

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    app_state *state = (app_state *)appstate;

    SDL_AppResult result = forge_scene_handle_event(&state->scene, event);
    if (result != SDL_APP_CONTINUE) return result;

    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat) {
        SDL_Scancode sc = event->key.scancode;

        if (sc == SDL_SCANCODE_P && state->audio_stream) {
            if (state->audio_paused) {
                if (SDL_ResumeAudioStreamDevice(state->audio_stream))
                    state->audio_paused = false;
                else
                    SDL_Log("ERROR: SDL_ResumeAudioStreamDevice: %s",
                            SDL_GetError());
            } else {
                if (SDL_PauseAudioStreamDevice(state->audio_stream))
                    state->audio_paused = true;
                else
                    SDL_Log("ERROR: SDL_PauseAudioStreamDevice: %s",
                            SDL_GetError());
            }
        }

        if (sc == SDL_SCANCODE_R) {
            int s = state->active_source;
            if (s >= 0 && s < NUM_SOURCES && state->source_loaded[s]) {
                forge_audio_source_reset(&state->sources[s]);
                state->sources[s].playing = true;
            }
        }

        /* 1-2: switch source */
        for (int k = 0; k < NUM_SOURCES; k++) {
            if (sc == SDL_SCANCODE_1 + k && state->source_loaded[k]) {
                /* Mute all, unmute selected */
                for (int j = 0; j < state->mixer.channel_count; j++) {
                    state->mixer.channels[j].mute = (j != k);
                }
                state->active_source = k;
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
    if (dt > DT_CLAMP) dt = DT_CLAMP;

    /* ── Audio mixing ────────────────────────────────────────────── */
    state->mixer.master_volume = state->master_volume;

    if (!state->audio_paused && state->audio_stream) {
        float exact = (float)FORGE_AUDIO_SAMPLE_RATE * dt + state->sample_carry;
        int frames = (int)exact;
        state->sample_carry = exact - (float)frames;
        if (frames < 1) frames = 1;
        if (frames > MIX_BUFFER_FRAMES) frames = MIX_BUFFER_FRAMES;

        forge_audio_mixer_mix(&state->mixer, state->mix_scratch, frames);

        if (!SDL_PutAudioStreamData(state->audio_stream,
                                    state->mix_scratch,
                                    frames * FORGE_AUDIO_CHANNELS
                                    * (int)sizeof(float))) {
            SDL_Log("ERROR: SDL_PutAudioStreamData: %s", SDL_GetError());
        }
    }

    /* Update VU meters */
    forge_audio_mixer_update_peaks(&state->mixer, dt);
    for (int i = 0; i < NUM_SOURCES; i++) {
        if (i < state->mixer.channel_count) {
            float pl = 0.0f, pr = 0.0f;
            forge_audio_channel_peak(&state->mixer, i, &pl, &pr);
            vu_update(&state->source_vu_l[i], &state->source_peak_l[i],
                      &state->source_peak_timer_l[i], pl, dt);
            vu_update(&state->source_vu_r[i], &state->source_peak_r[i],
                      &state->source_peak_timer_r[i], pr, dt);
        }
    }
    {
        float ml = 0.0f, mr = 0.0f;
        forge_audio_mixer_master_peak(&state->mixer, &ml, &mr);
        vu_update(&state->master_vu_l, &state->master_peak_l,
                  &state->master_peak_timer_l, ml, dt);
        vu_update(&state->master_vu_r, &state->master_peak_r,
                  &state->master_peak_timer_r, mr, dt);
    }

    /* ── 3D rendering ────────────────────────────────────────────── */

    /* Build sphere model matrices for the active source */
    mat4 sphere_models[NUM_SOURCES];
    float sphere_colors[NUM_SOURCES][4];
    for (int i = 0; i < NUM_SOURCES; i++) {
        float x = (float)i * SPHERE_SPACING;
        float vu = (state->source_vu_l[i] + state->source_vu_r[i]) * 0.5f;
        float pulse = 1.0f + vu * 0.3f;
        bool is_active = (i == state->active_source);
        float brightness = is_active ? 1.0f : 0.3f;

        sphere_models[i] = mat4_multiply(
            mat4_translate(vec3_create(x, SPHERE_Y, 0.0f)),
            mat4_scale(vec3_create(
                SPHERE_RADIUS * pulse,
                SPHERE_RADIUS * pulse,
                SPHERE_RADIUS * pulse)));

        sphere_colors[i][0] = SOURCE_COLORS[i][0] * brightness;
        sphere_colors[i][1] = SOURCE_COLORS[i][1] * brightness;
        sphere_colors[i][2] = SOURCE_COLORS[i][2] * brightness;
        sphere_colors[i][3] = 1.0f;
    }

    /* Shadow pass */
    forge_scene_begin_shadow_pass(s);
    for (int i = 0; i < NUM_SOURCES; i++) {
        forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                     state->sphere_index_count,
                                     sphere_models[i]);
    }
    forge_scene_end_shadow_pass(s);

    /* Main pass */
    forge_scene_begin_main_pass(s);
    forge_scene_draw_grid(s);
    for (int i = 0; i < NUM_SOURCES; i++) {
        forge_scene_draw_mesh(s, state->sphere_vb, state->sphere_ib,
                              state->sphere_index_count,
                              sphere_models[i], sphere_colors[i]);
    }
    forge_scene_end_main_pass(s);

    /* ── UI ──────────────────────────────────────────────────────── */
    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = !s->mouse_captured && (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (!wctx) goto ui_done;
        ForgeUiContext *ui = wctx->ctx;

        /* ═══ Window 1: Source Controls ═══ */
        if (forge_ui_wctx_window_begin(wctx, "Source Controls",
                                        &state->win_source)) {
            forge_ui_ctx_label_layout(ui, "Active Source:", LH);
            for (int i = 0; i < NUM_SOURCES; i++) {
                if (!state->source_loaded[i]) continue;
                bool is_active = (i == state->active_source);
                char label[64];
                SDL_snprintf(label, sizeof(label), "%s%s",
                             is_active ? "> " : "  ", SOURCE_NAMES[i]);

                bool was_active = is_active;
                forge_ui_ctx_checkbox_layout(ui, label, &is_active, CB_H);
                if (is_active && !was_active) {
                    for (int j = 0; j < state->mixer.channel_count; j++) {
                        state->mixer.channels[j].mute = (j != i);
                    }
                    state->active_source = i;
                }
            }

            forge_ui_ctx_separator_layout(ui, SEP);

            {
                int idx = state->active_source;
                char vu_label[64];
                SDL_snprintf(vu_label, sizeof(vu_label), "VU: L %.2f  R %.2f",
                             state->source_vu_l[idx], state->source_vu_r[idx]);
                forge_ui_ctx_label_layout(ui, vu_label, LH);
                forge_ui_ctx_vu_meter_layout(ui,
                    state->source_vu_l[idx], state->source_vu_r[idx],
                    state->source_peak_l[idx], state->source_peak_r[idx],
                    VU_H);
            }

            forge_ui_ctx_separator_layout(ui, SEP);

            {
                int idx = state->active_source;
                if (state->source_loaded[idx]) {
                    float progress = forge_audio_source_progress(
                        &state->sources[idx]);
                    char prog_label[64];
                    SDL_snprintf(prog_label, sizeof(prog_label),
                                 "Progress: %.0f%%", progress * 100.0f);
                    forge_ui_ctx_label_layout(ui, prog_label, LH);

                    bool playing = state->sources[idx].playing;
                    forge_ui_ctx_checkbox_layout(ui, "Playing", &playing, CB_H);
                    state->sources[idx].playing = playing;
                }
            }

            forge_ui_wctx_window_end(wctx);
        }

        /* ═══ Window 2: Effects ═══ */
        if (forge_ui_wctx_window_begin(wctx, "Effects",
                                        &state->win_effects)) {
            forge_ui_ctx_label_layout(ui, "Presets:", LH);
            for (int p = 0; p < NUM_PRESETS; p++) {
                bool is_active = (state->active_preset == p);
                bool was = is_active;
                forge_ui_ctx_checkbox_layout(ui, PRESET_NAMES[p], &is_active, CB_H);
                if (is_active && !was) apply_preset(state, p);
            }

            forge_ui_ctx_separator_layout(ui, SEP);

            /* Snapshot bypass states before UI to detect changes once
             * at the end rather than per-section (avoids redundant
             * rebuild_effect_chain / update_effect_params calls). */
            bool old_filter_bypass = state->filter_bypass;
            bool old_delay_bypass  = state->delay_bypass;
            bool old_reverb_bypass = state->reverb_bypass;
            bool old_chorus_bypass = state->chorus_bypass;

            /* Filter */
            forge_ui_ctx_label_layout(ui, "Biquad Filter:", LH);
            forge_ui_ctx_checkbox_layout(ui, "Bypass Filter",
                                     &state->filter_bypass, CB_H);
            forge_ui_ctx_slider_layout(ui, "Cutoff",
                &state->filter_cutoff, 20.0f, 20000.0f, SH);
            forge_ui_ctx_slider_layout(ui, "Q",
                &state->filter_q, 0.1f, 10.0f, SH);
            {
                /* Radio-button group: checking one unchecks the others.
                 * Re-derive from filter_type each frame so deselecting
                 * the active type has no effect (it stays selected). */
                bool is_lp = (state->filter_type == 0);
                bool is_hp = (state->filter_type == 1);
                bool is_bp = (state->filter_type == 2);
                forge_ui_ctx_checkbox_layout(ui, "Low-Pass", &is_lp, CB_H);
                forge_ui_ctx_checkbox_layout(ui, "High-Pass", &is_hp, CB_H);
                forge_ui_ctx_checkbox_layout(ui, "Band-Pass", &is_bp, CB_H);
                /* Only switch type when a new one is checked */
                if (is_lp) state->filter_type = 0;
                else if (is_hp) state->filter_type = 1;
                else if (is_bp) state->filter_type = 2;
                /* else: user unchecked the active type — keep current */
            }

            forge_ui_ctx_separator_layout(ui, SEP);

            /* Delay */
            forge_ui_ctx_label_layout(ui, "Delay:", LH);
            forge_ui_ctx_checkbox_layout(ui, "Bypass Delay",
                                     &state->delay_bypass, CB_H);
            forge_ui_ctx_slider_layout(ui, "Time (s)",
                &state->delay_time, 0.01f, 2.0f, SH);
            forge_ui_ctx_slider_layout(ui, "Feedback",
                &state->delay_feedback, 0.0f, 0.95f, SH);
            forge_ui_ctx_slider_layout(ui, "Delay Wet",
                &state->delay_wet, 0.0f, 1.0f, SH);

            forge_ui_ctx_separator_layout(ui, SEP);

            /* Reverb */
            forge_ui_ctx_label_layout(ui, "Reverb:", LH);
            forge_ui_ctx_checkbox_layout(ui, "Bypass Reverb",
                                     &state->reverb_bypass, CB_H);
            forge_ui_ctx_slider_layout(ui, "Room Size",
                &state->reverb_room, 0.0f, 1.0f, SH);
            forge_ui_ctx_slider_layout(ui, "Damping",
                &state->reverb_damping, 0.0f, 1.0f, SH);
            forge_ui_ctx_slider_layout(ui, "Reverb Wet",
                &state->reverb_wet, 0.0f, 1.0f, SH);

            forge_ui_ctx_separator_layout(ui, SEP);

            /* Chorus */
            forge_ui_ctx_label_layout(ui, "Chorus:", LH);
            forge_ui_ctx_checkbox_layout(ui, "Bypass Chorus",
                                     &state->chorus_bypass, CB_H);
            forge_ui_ctx_slider_layout(ui, "Rate (Hz)",
                &state->chorus_rate, 0.1f, 5.0f, SH);
            forge_ui_ctx_slider_layout(ui, "Depth",
                &state->chorus_depth, 0.001f, 0.02f, SH);
            forge_ui_ctx_slider_layout(ui, "Chorus Wet",
                &state->chorus_wet, 0.0f, 1.0f, SH);

            /* Apply changes once: rebuild chain if any bypass toggled,
             * otherwise just update parameters in-place */
            if (state->filter_bypass != old_filter_bypass
                || state->delay_bypass != old_delay_bypass
                || state->reverb_bypass != old_reverb_bypass
                || state->chorus_bypass != old_chorus_bypass) {
                rebuild_effect_chain(state);
            } else {
                update_effect_params(state);
            }

            forge_ui_wctx_window_end(wctx);
        }

        /* ═══ Window 3: Master & Output ═══ */
        if (forge_ui_wctx_window_begin(wctx, "Master & Output",
                                        &state->win_master)) {
            forge_ui_ctx_slider_layout(ui, "Master Volume",
                &state->master_volume, 0.0f, VOLUME_MAX, SH);

            forge_ui_ctx_separator_layout(ui, SEP);

            forge_ui_ctx_label_layout(ui, "Master VU:", LH);
            forge_ui_ctx_vu_meter_layout(ui,
                state->master_vu_l, state->master_vu_r,
                state->master_peak_l, state->master_peak_r,
                VU_H);

            forge_ui_ctx_separator_layout(ui, SEP);

            {
                ForgeAudioEffectChain *chain = &state->mixer.master_effects;
                char summary[128];
                SDL_snprintf(summary, sizeof(summary),
                             "Active effects: %d / %d",
                             chain->effect_count, FORGE_AUDIO_MAX_EFFECTS);
                forge_ui_ctx_label_layout(ui, summary, LH);

                if (state->active_preset > 0) {
                    char preset_label[64];
                    SDL_snprintf(preset_label, sizeof(preset_label),
                                 "Preset: %s",
                                 PRESET_NAMES[state->active_preset]);
                    forge_ui_ctx_label_layout(ui, preset_label, LH);
                }
            }

            forge_ui_wctx_window_end(wctx);
        }
    }
ui_done:
    forge_scene_end_ui(s);

    return forge_scene_end_frame(s);
}

/* ── SDL_AppQuit ─────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    app_state *state = (app_state *)appstate;
    if (!state) return;

    /* Free DSP effect instances */
    if (state->effects_initialized) {
        forge_audio_delay_free(&state->delay);
        forge_audio_reverb_free(&state->reverb);
        forge_audio_chorus_free(&state->chorus);
    }

    /* Free audio buffers */
    for (int i = 0; i < NUM_SOURCES; i++) {
        if (state->source_loaded[i]) {
            forge_audio_buffer_free(&state->buffers[i]);
        }
    }

    if (state->audio_stream)
        SDL_DestroyAudioStream(state->audio_stream);

    if (forge_scene_device(&state->scene)) {
        if (!SDL_WaitForGPUIdle(forge_scene_device(&state->scene)))
            SDL_Log("ERROR: SDL_WaitForGPUIdle: %s", SDL_GetError());
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
