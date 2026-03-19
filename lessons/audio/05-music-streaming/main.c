/*
 * Audio Lesson 05 — Music and Streaming
 *
 * Demonstrates: streaming WAV playback from disk using a ring buffer,
 * crossfading between tracks, loop-with-intro, and adaptive music layers
 * with per-layer weight control.
 *
 * Six tracks from the Cyberpunk Dynamic Bundle (Volumes I-III) are
 * available via a dropdown selector.  Each track has 3 layers that
 * stream independently but stay sample-aligned.  An "Intensity" slider
 * automatically maps to layer weights for adaptive music simulation.
 *
 * Uses forge_scene.h for all rendering boilerplate (device, window,
 * pipelines, camera, grid, shadow map, UI).
 *
 * Controls:
 *   WASD / Arrow keys — move camera
 *   Mouse             — look around (click to capture, Escape to release)
 *   Space / Shift     — fly up / down
 *   P                 — pause/resume audio
 *   R                 — restart current track from beginning
 *   N                 — crossfade to next track
 *   1-3               — toggle layer on/off
 *   Escape            — release mouse / quit
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
/* Math library — vectors, matrices, quaternions */
#include "math/forge_math.h"

/* Audio library — streaming, crossfade, layer groups */
#include "audio/forge_audio.h"

/* Procedural geometry — sphere mesh */
#define FORGE_SHAPES_IMPLEMENTATION
#include "shapes/forge_shapes.h"

/* Scene renderer — replaces rendering boilerplate */
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

/* ── Constants ────────────────────────────────────────────────────── */

#define NUM_TRACKS        6
#define MAX_TRACK_LAYERS  3

/* Sphere rendering (speaker visualization) */
#define SPHERE_RADIUS     0.5f
#define SPHERE_SLICES     20
#define SPHERE_STACKS     10
#define SPHERE_Y          1.5f

/* Camera */
#define CAM_START_X       0.0f
#define CAM_START_Y       3.0f
#define CAM_START_Z       8.0f
#define CAM_START_PITCH  -0.45f

/* Audio */
#define MIX_BUFFER_FRAMES 4480
#define DT_CLAMP          0.1f
#define VOLUME_MAX        1.5f
#define CROSSFADE_DEFAULT 2.0f
#define CROSSFADE_MIN     0.5f
#define CROSSFADE_MAX     5.0f
#define MASTER_VOL_DEFAULT 0.7f

/* VU meter smoothing (exponential approach, frame-rate independent).
 * These are base rates at 60 FPS — scaled by dt in vu_update(). */
#define VU_ATTACK         0.3f
#define VU_RELEASE        0.05f
#define VU_REFERENCE_FPS  60.0f
#define VU_PEAK_HOLD_TIME 1.5f
#define VU_PEAK_DECAY     0.6f

/* Buffer fill sparkline history */
#define SPARKLINE_HISTORY 64

/* UI layout */
#define PANEL_X           10.0f
#define PANEL_Y           10.0f
#define PANEL_W           380.0f
#define PANEL_H           780.0f
#define LH                22.0f   /* label height */
#define SH                26.0f   /* slider height */
#define SEP               4.0f    /* separator height */
#define VU_H              16.0f   /* VU meter height */
#define CB_H              22.0f   /* checkbox height */
#define SPARK_H           30.0f   /* sparkline height */
#define VU_TALL           90.0f  /* per-layer VU meter tall height */
#define WIN_H             420.0f /* UI window height */
#define WIN_TOP           270.0f /* UI window top offset */
#define WIN_GAP           12.0f  /* gap between UI windows */
#define PAUSE_BTN_FRAC    0.38f  /* pause button width fraction */
#define INTENSITY_BASE_W  0.3f   /* minimum base layer weight in intensity mode */
#define INTENSITY_FADE_DUR 0.15f /* weight fade duration for intensity changes */
#define FONT_SIZE         16.0f

/* ── Track metadata ───────────────────────────────────────────────── */

typedef struct TrackInfo {
    const char *name;
    const char *album;
    int         layer_count;
    const char *layer_names[MAX_TRACK_LAYERS];
    const char *file_paths[MAX_TRACK_LAYERS];
} TrackInfo;

static const TrackInfo TRACKS[NUM_TRACKS] = {
    {
        "Not That Easy", "Cyberpunk I", 3,
        { "Base", "Mid", "Lead" },
        {
            "assets/audio/music_notthateasy_layer1.wav",
            "assets/audio/music_notthateasy_layer2.wav",
            "assets/audio/music_notthateasy_layer3.wav",
        },
    },
    {
        "After All", "Cyberpunk I", 3,
        { "Base", "Mid", "Lead" },
        {
            "assets/audio/music_afterall_layer1.wav",
            "assets/audio/music_afterall_layer2.wav",
            "assets/audio/music_afterall_layer3.wav",
        },
    },
    {
        "Destroy", "Cyberpunk I", 3,
        { "Base", "Mid", "Lead" },
        {
            "assets/audio/music_destroy_layer1.wav",
            "assets/audio/music_destroy_layer2.wav",
            "assets/audio/music_destroy_layer3.wav",
        },
    },
    {
        "Unknown Club", "Cyberpunk II", 3,
        { "Base", "Mid", "Lead" },
        {
            "assets/audio/music_unknownclub_layer1.wav",
            "assets/audio/music_unknownclub_layer2.wav",
            "assets/audio/music_unknownclub_layer3.wav",
        },
    },
    {
        "Time Disruptor", "Cyberpunk II", 3,
        { "Base", "Mid", "Lead" },
        {
            "assets/audio/music_timedisruptor_layer1.wav",
            "assets/audio/music_timedisruptor_layer2.wav",
            "assets/audio/music_timedisruptor_layer3.wav",
        },
    },
    {
        "Cobra", "Cyberpunk III", 3,
        { "Base", "Mid", "Lead" },
        {
            "assets/audio/music_cobra_layer1.wav",
            "assets/audio/music_cobra_layer2.wav",
            "assets/audio/music_cobra_layer3.wav",
        },
    },
};

/* Track dropdown item labels */
static const char *TRACK_LABELS[NUM_TRACKS] = {
    "Not That Easy",
    "After All",
    "Destroy",
    "Unknown Club",
    "Time Disruptor",
    "Cobra",
};

/* Sphere color per track (RGB) */
static const float TRACK_COLORS[NUM_TRACKS][3] = {
    { 0.30f, 0.45f, 0.90f },  /* blue */
    { 0.90f, 0.40f, 0.25f },  /* orange */
    { 0.90f, 0.25f, 0.30f },  /* red */
    { 0.25f, 0.80f, 0.30f },  /* green */
    { 0.85f, 0.75f, 0.20f },  /* yellow */
    { 0.70f, 0.30f, 0.85f },  /* purple */
};

/* Layer colors for VU meter / label tinting (per-layer index) */
static const float LAYER_COLORS[MAX_TRACK_LAYERS][3] = {
    { 0.30f, 0.70f, 0.95f },  /* cyan - base */
    { 0.50f, 0.90f, 0.40f },  /* green - mid */
    { 0.95f, 0.55f, 0.20f },  /* orange - lead */
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

    /* Layer groups — one per track (loaded on demand) */
    ForgeAudioLayerGroup groups[NUM_TRACKS];   /* streaming layer groups */
    bool                 track_loaded[NUM_TRACKS]; /* true once a track's WAVs are opened */
    int                  active_track;         /* index of currently playing track [0..NUM_TRACKS) */
    int                  incoming_track;       /* track fading in during crossfade, or -1 */
    float                crossfade_progress;   /* 0..1 during crossfade, 0 when idle */
    float                crossfade_duration;   /* crossfade length in seconds */
    bool                 crossfading;          /* true while a crossfade is in progress */

    /* Mixing — stereo scratch buffers for per-frame audio output */
    float mix_scratch[MIX_BUFFER_FRAMES * 2];  /* interleaved L/R float samples */
    float layer_tmp[MIX_BUFFER_FRAMES * 2];    /* scratch for per-layer reads */
    float out_tmp[MIX_BUFFER_FRAMES * 2];      /* scratch for crossfade outgoing */

    /* Per-layer VU metering (smoothed levels for UI display) */
    float layer_vu_l[MAX_TRACK_LAYERS];          /* smoothed left level [0..1] */
    float layer_vu_r[MAX_TRACK_LAYERS];          /* smoothed right level [0..1] */
    float layer_peak_l[MAX_TRACK_LAYERS];        /* peak hold left [0..1] */
    float layer_peak_r[MAX_TRACK_LAYERS];        /* peak hold right [0..1] */
    float layer_peak_timer_l[MAX_TRACK_LAYERS];  /* seconds until left peak decays */
    float layer_peak_timer_r[MAX_TRACK_LAYERS];  /* seconds until right peak decays */

    /* Master VU metering (smoothed levels for master bus) */
    float master_vu_l, master_vu_r;         /* smoothed master left/right [0..1] */
    float master_peak_l, master_peak_r;     /* peak hold master left/right [0..1] */
    float master_peak_timer_l;              /* seconds until master left peak decays */
    float master_peak_timer_r;              /* seconds until master right peak decays */

    /* Buffer fill sparkline — ring buffer utilization history for UI */
    float  buf_fill_history[SPARKLINE_HISTORY]; /* fill fraction [0..1] per sample */
    int    buf_fill_cursor;                     /* next write index in history ring */

    /* UI state — three draggable windows for transport, layers, and master */
    ForgeUiWindowState win_transport;   /* track selector, transport, progress */
    ForgeUiWindowState win_layers;      /* intensity mode, per-layer controls, VU */
    ForgeUiWindowState win_master;      /* master volume, streaming stats */
    float              master_volume;   /* master bus gain [0..VOLUME_MAX] */
    float              intensity;       /* 0..1 auto-drives layer weights */
    bool               intensity_mode;  /* true = intensity slider controls layers */
    float              layer_weights[MAX_TRACK_LAYERS]; /* per-layer weight [0..1] for UI */
    bool               layer_muted[NUM_TRACKS][MAX_TRACK_LAYERS]; /* per-track mute */
    int                track_selector;  /* dropdown selection index [0..NUM_TRACKS) */
    bool               dropdown_open;   /* true while track dropdown is expanded */
    bool               audio_paused;    /* true when SDL audio stream is paused */
    bool               looping;         /* true = tracks loop indefinitely */
    float              sample_carry;    /* fractional sample accumulator to avoid dt truncation */
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

static bool load_track(app_state *state, int track_idx)
{
    if (state->track_loaded[track_idx]) return true;

    ForgeAudioLayerGroup *group = &state->groups[track_idx];
    forge_audio_layer_group_init(group);
    group->looping = true;
    group->playing = true;

    const TrackInfo *track = &TRACKS[track_idx];
    bool ok = true;

    for (int i = 0; i < track->layer_count; i++) {
        if (!track->file_paths[i]) continue;
        float w = (i == 0) ? 1.0f : 0.5f;
        int idx = forge_audio_layer_group_add(group, track->file_paths[i], w);
        if (idx < 0) {
            SDL_Log("WARNING: Could not load '%s'", track->file_paths[i]);
            ok = false;
        } else {
            forge_audio_stream_set_loop(&group->layers[idx].stream, 0);
        }
    }

    /* All-or-nothing: only mark loaded if every layer opened and at least
     * one is playable.  Any failure closes the group and returns false. */
    if (!ok || group->layer_count == 0) {
        forge_audio_layer_group_close(group);
        state->track_loaded[track_idx] = false;
        return false;
    }
    state->track_loaded[track_idx] = true;
    return true;
}

/* Sync layer_weights[] from the active group state */
static void sync_weights_from_group(app_state *state, int track)
{
    for (int i = 0; i < MAX_TRACK_LAYERS; i++) {
        if (i < state->groups[track].layer_count) {
            state->layer_weights[i] = state->groups[track].layers[i].weight;
        } else {
            state->layer_weights[i] = 0.0f;
        }
    }
}

/* Start a crossfade to the given track.  Loads the track if needed, resets
 * its playback position, and sets up the crossfade state.  Returns true if
 * the crossfade was started, false if the track failed to load. */
static bool start_crossfade(app_state *state, int next)
{
    if (!state->track_loaded[next]) {
        if (!load_track(state, next)) {
            SDL_Log("WARN: failed to load track %d, skipping crossfade", next);
            return false;
        }
    } else {
        forge_audio_layer_group_seek(&state->groups[next], 0);
        state->groups[next].playing = true;
    }
    state->incoming_track = next;
    state->crossfading = true;
    state->crossfade_progress = 0.0f;
    state->track_selector = next;
    sync_weights_from_group(state, next);
    return true;
}

/* Apply intensity slider to layer weights (higher intensity = more layers) */
static void apply_intensity(app_state *state, int track)
{
    int n = state->groups[track].layer_count;
    if (n <= 0) return;

    for (int i = 0; i < n; i++) {
        /* Layer 0 always at intensity, layers 1..n-1 fade in progressively */
        float threshold = (float)i / (float)n;
        float w;
        if (state->intensity >= threshold + 1.0f / (float)n) {
            w = 1.0f;
        } else if (state->intensity <= threshold) {
            w = (i == 0) ? INTENSITY_BASE_W : 0.0f;  /* base always audible */
        } else {
            float local = (state->intensity - threshold) * (float)n;
            w = local;
            if (i == 0 && w < INTENSITY_BASE_W) w = INTENSITY_BASE_W;
        }
        state->layer_weights[i] = w;
        forge_audio_layer_group_fade_weight(&state->groups[track], i, w, INTENSITY_FADE_DUR);
    }
}

/* Compute peak level of a stereo buffer segment */
static void measure_peaks(const float *buf, int frames,
                          float *out_l, float *out_r)
{
    float pl = 0.0f, pr = 0.0f;
    for (int i = 0; i < frames; i++) {
        float al = SDL_fabsf(buf[i * 2]);
        float ar = SDL_fabsf(buf[i * 2 + 1]);
        if (al > pl) pl = al;
        if (ar > pr) pr = ar;
    }
    *out_l = pl;
    *out_r = pr;
}

/* Smooth VU update (attack/release, frame-rate independent).
 * Uses exponential approach: alpha = 1 - (1 - base_rate)^(dt * ref_fps)
 * so the smoothing behaves identically at any frame rate. */
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

/* ── SDL_AppInit ─────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) return SDL_APP_FAILURE;
    *appstate = state;

    ForgeSceneConfig cfg = forge_scene_default_config(
        "Audio Lesson 05 \xe2\x80\x94 Music & Streaming");
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
        return SDL_APP_FAILURE; /* SDL_AppQuit owns cleanup */
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

    /* Load first track */
    state->active_track = 0;
    state->incoming_track = -1;
    state->crossfade_duration = CROSSFADE_DEFAULT;
    state->looping = true;

    if (!load_track(state, 0)) {
        SDL_Log("NOTE: Place music WAV files in assets/audio/ (see README)");
    }

    sync_weights_from_group(state, 0);
    state->master_volume = MASTER_VOL_DEFAULT;
    state->intensity = 0.5f;
    state->intensity_mode = false;
    state->track_selector = 0;

    /* Three windows arranged horizontally, bottom-aligned.
     * Each window is independently draggable. */
    float win_h = WIN_H;
    float win_top = WIN_TOP;
    float win_gap = WIN_GAP;

    state->win_transport = forge_ui_window_state_default(
        PANEL_X, win_top, PANEL_W, win_h);
    state->win_layers = forge_ui_window_state_default(
        PANEL_X + PANEL_W + win_gap, win_top, PANEL_W, win_h);
    state->win_master = forge_ui_window_state_default(
        PANEL_X + (PANEL_W + win_gap) * 2.0f, win_top, PANEL_W, win_h);

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
                    SDL_Log("ERROR: SDL_ResumeAudioStreamDevice: %s", SDL_GetError());
            } else {
                if (SDL_PauseAudioStreamDevice(state->audio_stream))
                    state->audio_paused = true;
                else
                    SDL_Log("ERROR: SDL_PauseAudioStreamDevice: %s", SDL_GetError());
            }
        }

        if (sc == SDL_SCANCODE_R) {
            int restart_track = (state->crossfading && state->incoming_track >= 0)
                              ? state->incoming_track
                              : state->active_track;
            forge_audio_layer_group_seek(&state->groups[restart_track], 0);
        }

        if (sc == SDL_SCANCODE_N && !state->crossfading) {
            int next = (state->active_track + 1) % NUM_TRACKS;
            /* Failure just keeps the current track playing — no state to roll back */
            (void)start_crossfade(state, next);
        }

        /* 1-3: toggle layers */
        for (int k = 0; k < MAX_TRACK_LAYERS; k++) {
            if (sc == SDL_SCANCODE_1 + k && !state->intensity_mode) {
                int t = state->crossfading ? state->incoming_track
                                           : state->active_track;
                if (k < state->groups[t].layer_count) {
                    float cur = state->groups[t].layers[k].weight;
                    float target = cur > 0.5f ? 0.0f : 1.0f;
                    forge_audio_layer_group_fade_weight(
                        &state->groups[t], k, target, 0.3f);
                    state->layer_weights[k] = target;
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
    if (dt > DT_CLAMP) dt = DT_CLAMP;

    int display_track = state->crossfading && state->incoming_track >= 0
                      ? state->incoming_track : state->active_track;

    /* ── Crossfade ──────────────────────────────────────────────── */

    /* Gate time-based audio updates on pause — pausing should freeze all
     * crossfade progress, weight fades, and layer group state. */
    float audio_dt = state->audio_paused ? 0.0f : dt;

    if (state->crossfading && state->incoming_track >= 0) {
        state->crossfade_progress += (state->crossfade_duration > 0.0f)
            ? audio_dt / state->crossfade_duration : 1.0f;
        if (state->crossfade_progress >= 1.0f) {
            state->crossfade_progress = 1.0f;
            state->crossfading = false;
            state->groups[state->active_track].playing = false;
            state->active_track = state->incoming_track;
            state->incoming_track = -1;
        }
    }

    /* ── Intensity mode ─────────────────────────────────────────── */

    if (state->intensity_mode) {
        apply_intensity(state, display_track);
    }

    /* ── Update layer groups ────────────────────────────────────── */

    forge_audio_layer_group_update(&state->groups[state->active_track], audio_dt);
    if (state->crossfading && state->incoming_track >= 0)
        forge_audio_layer_group_update(&state->groups[state->incoming_track], audio_dt);

    /* ── Audio mixing with per-layer VU metering ────────────────── */

    if (!state->audio_paused && state->audio_stream) {
        /* Accumulate fractional samples to avoid truncation drift */
        float exact = (float)FORGE_AUDIO_SAMPLE_RATE * dt + state->sample_carry;
        int frames = (int)exact;
        state->sample_carry = exact - (float)frames;
        if (frames < 1) frames = 1;
        if (frames > MIX_BUFFER_FRAMES) frames = MIX_BUFFER_FRAMES;

        SDL_memset(state->mix_scratch, 0,
                   (size_t)(frames * 2) * sizeof(float));

        /* Read per-layer for VU metering on the display track */
        ForgeAudioLayerGroup *lg = &state->groups[display_track];

        for (int i = 0; i < lg->layer_count && i < MAX_TRACK_LAYERS; i++) {
            ForgeAudioLayer *l = &lg->layers[i];
            if (!l->active) continue;

            SDL_memset(state->layer_tmp, 0, (size_t)(frames * 2) * sizeof(float));
            forge_audio_stream_read(&l->stream, state->layer_tmp, frames);

            /* Measure this layer before applying weight */
            float raw_l, raw_r;
            measure_peaks(state->layer_tmp, frames, &raw_l, &raw_r);

            float gain = l->weight * lg->volume;
            if (state->layer_muted[display_track][i]) gain = 0.0f;

            for (int j = 0; j < frames * 2; j++) {
                state->mix_scratch[j] += state->layer_tmp[j] * gain;
            }

            /* Update VU with weighted level (separate L/R peak timers) */
            vu_update(&state->layer_vu_l[i], &state->layer_peak_l[i],
                      &state->layer_peak_timer_l[i], raw_l * gain, dt);
            vu_update(&state->layer_vu_r[i], &state->layer_peak_r[i],
                      &state->layer_peak_timer_r[i], raw_r * gain, dt);
        }

        /* If crossfading, blend with outgoing track */
        if (state->crossfading && state->incoming_track >= 0) {
            float t_xf = state->crossfade_progress;
            float gain_out = SDL_sqrtf(1.0f - t_xf);
            float gain_in  = SDL_sqrtf(t_xf);

            SDL_memset(state->out_tmp, 0, (size_t)(frames * 2) * sizeof(float));

            /* Apply mute state to outgoing track — save weights first
             * so the permanent weight isn't destroyed by muting. */
            {
                ForgeAudioLayerGroup *out_lg = &state->groups[state->active_track];
                float saved[FORGE_AUDIO_MAX_LAYERS];
                int li;
                for (li = 0; li < out_lg->layer_count; li++) {
                    saved[li] = out_lg->layers[li].weight;
                    if (state->layer_muted[state->active_track][li])
                        out_lg->layers[li].weight = 0.0f;
                }

                /* The active (outgoing) track needs separate read */
                forge_audio_layer_group_read(out_lg, state->out_tmp, frames);

                /* Restore weights so they survive across frames */
                for (li = 0; li < out_lg->layer_count; li++) {
                    out_lg->layers[li].weight = saved[li];
                }
            }

            for (int i = 0; i < frames * 2; i++) {
                state->mix_scratch[i] = state->mix_scratch[i] * gain_in
                                      + state->out_tmp[i] * gain_out;
            }
        }

        /* Master volume + soft clip */
        for (int i = 0; i < frames * 2; i++) {
            float sample = state->mix_scratch[i] * state->master_volume;
            if (sample > 1.0f || sample < -1.0f)
                sample = forge_audio_tanhf(sample);
            state->mix_scratch[i] = sample;
        }

        /* Master VU metering */
        float ml, mr;
        measure_peaks(state->mix_scratch, frames, &ml, &mr);
        vu_update(&state->master_vu_l, &state->master_peak_l,
                  &state->master_peak_timer_l, ml, dt);
        vu_update(&state->master_vu_r, &state->master_peak_r,
                  &state->master_peak_timer_r, mr, dt);

        /* Buffer fill sparkline */
        if (lg->layer_count > 0) {
            float fill = (float)lg->layers[0].stream.ring_available
                       / (float)FORGE_AUDIO_STREAM_RING_FRAMES;
            state->buf_fill_history[state->buf_fill_cursor] = fill;
            state->buf_fill_cursor = (state->buf_fill_cursor + 1) % SPARKLINE_HISTORY;
        }

        if (!SDL_PutAudioStreamData(state->audio_stream, state->mix_scratch,
                                     frames * 2 * (int)sizeof(float))) {
            SDL_Log("WARN: SDL_PutAudioStreamData: %s", SDL_GetError());
        }
    }

    /* ── Rendering ──────────────────────────────────────────────── */

    /* Speaker sphere that pulses with master level */
    float pulse = 1.0f + (state->master_vu_l + state->master_vu_r) * 0.15f;
    vec3 sphere_pos = vec3_create(0.0f, SPHERE_Y, 0.0f);
    mat4 sphere_model = mat4_multiply(
        mat4_translate(sphere_pos),
        mat4_scale_uniform(SPHERE_RADIUS * pulse));

    float sphere_color[4] = {
        TRACK_COLORS[display_track][0],
        TRACK_COLORS[display_track][1],
        TRACK_COLORS[display_track][2],
        1.0f
    };

    forge_scene_begin_shadow_pass(s);
    forge_scene_draw_shadow_mesh(s, state->sphere_vb, state->sphere_ib,
                                 state->sphere_index_count, sphere_model);
    forge_scene_end_shadow_pass(s);

    forge_scene_begin_main_pass(s);
    forge_scene_draw_grid(s);
    forge_scene_draw_mesh(s, state->sphere_vb, state->sphere_ib,
                          state->sphere_index_count,
                          sphere_model, sphere_color);
    forge_scene_end_main_pass(s);

    /* ── UI ──────────────────────────────────────────────────────── */

    float mx, my;
    Uint32 buttons = SDL_GetMouseState(&mx, &my);
    bool mouse_down = !state->scene.mouse_captured
                    && (buttons & SDL_BUTTON_LMASK) != 0;

    forge_scene_begin_ui(s, mx, my, mouse_down);
    {
        ForgeUiWindowContext *wctx = forge_scene_window_ui(s);
        if (!wctx) goto ui_done;
        ForgeUiContext *ui = wctx->ctx;

        int display_t = state->crossfading && state->incoming_track >= 0
                      ? state->incoming_track : state->active_track;
        ForgeAudioLayerGroup *lg = &state->groups[display_t];

        /* ════════════════════════════════════════════════════════
         * Window 1: Transport
         * ════════════════════════════════════════════════════════ */
        if (forge_ui_wctx_window_begin(wctx, "Transport",
                                        &state->win_transport)) {
            /* Track dropdown */
            if (forge_ui_ctx_dropdown_layout(ui, "Track",
                    &state->track_selector, &state->dropdown_open,
                    TRACK_LABELS, NUM_TRACKS, LH)) {
                int next = state->track_selector;
                if (next != state->active_track && !state->crossfading) {
                    if (!start_crossfade(state, next)) {
                        state->track_selector = state->active_track;
                    }
                }
            }

            {
                char buf[64];
                SDL_snprintf(buf, sizeof(buf), "%s  |  %s",
                             TRACKS[display_t].name, TRACKS[display_t].album);
                forge_ui_ctx_label_layout(ui, buf, LH);
            }

            forge_ui_ctx_separator_layout(ui, SEP);

            /* Button row: Play/Pause | Restart | Next (horizontal) */
            {
                ForgeUiRect row = forge_ui_ctx_layout_next(ui, LH + 4.0f);
                forge_ui_ctx_layout_push(ui, row,
                    FORGE_UI_LAYOUT_HORIZONTAL, 0.0f, 4.0f);

                if (forge_ui_ctx_button_layout(ui,
                        state->audio_paused ? "> Play" : "|| Pause",
                        row.w * PAUSE_BTN_FRAC)) {
                    if (state->audio_stream) {
                        if (state->audio_paused) {
                            if (SDL_ResumeAudioStreamDevice(state->audio_stream))
                                state->audio_paused = false;
                            else
                                SDL_Log("ERROR: SDL_ResumeAudioStreamDevice: %s", SDL_GetError());
                        } else {
                            if (SDL_PauseAudioStreamDevice(state->audio_stream))
                                state->audio_paused = true;
                            else
                                SDL_Log("ERROR: SDL_PauseAudioStreamDevice: %s", SDL_GetError());
                        }
                    }
                }
                if (forge_ui_ctx_button_layout(ui, "Restart", row.w * 0.30f)) {
                    forge_audio_layer_group_seek(&state->groups[display_t], 0);
                }
                if (forge_ui_ctx_button_layout(ui, "Next >>", row.w * 0.30f)) {
                    if (!state->crossfading) {
                        int next = (state->active_track + 1) % NUM_TRACKS;
                        /* Failure keeps the current track — no UI state to roll back */
                        (void)start_crossfade(state, next);
                    }
                }

                forge_ui_ctx_layout_pop(ui);
            }

            /* Progress bar + time */
            {
                float progress = forge_audio_layer_group_progress(lg);
                float dur = 0.0f;
                if (lg->layer_count > 0) dur = lg->layers[0].stream.duration;
                float elapsed = progress * dur;

                char buf[64];
                SDL_snprintf(buf, sizeof(buf), "%d:%02d / %d:%02d",
                             (int)elapsed / 60, (int)elapsed % 60,
                             (int)dur / 60, (int)dur % 60);
                forge_ui_ctx_label_layout(ui, buf, LH);

                ForgeUiColor bar_col = {
                    TRACK_COLORS[display_t][0],
                    TRACK_COLORS[display_t][1],
                    TRACK_COLORS[display_t][2], 1.0f
                };
                forge_ui_ctx_progress_bar_layout(ui, progress, 1.0f,
                                                 bar_col, 12.0f);
            }

            /* Crossfade row: slider + live bar */
            forge_ui_ctx_slider_layout(ui, "Crossfade (s)",
                &state->crossfade_duration, CROSSFADE_MIN, CROSSFADE_MAX, SH);
            if (state->crossfading) {
                ForgeUiColor xf_col = { 0.9f, 0.75f, 0.2f, 1.0f };
                forge_ui_ctx_progress_bar_layout(
                    ui, state->crossfade_progress, 1.0f, xf_col, 8.0f);
            }

            forge_ui_wctx_window_end(wctx);
        }

        /* ════════════════════════════════════════════════════════
         * Window 2: Layers
         * ════════════════════════════════════════════════════════ */
        if (forge_ui_wctx_window_begin(wctx, "Layers",
                                        &state->win_layers)) {
            /* Intensity mode */
            forge_ui_ctx_checkbox_layout(ui, "Intensity Mode",
                &state->intensity_mode, CB_H);
            if (state->intensity_mode) {
                forge_ui_ctx_slider_layout(ui, "Intensity",
                    &state->intensity, 0.0f, 1.0f, SH);
            }

            forge_ui_ctx_separator_layout(ui, SEP);

            /* Per-layer controls: name + mute, then slider */
            for (int i = 0; i < lg->layer_count && i < MAX_TRACK_LAYERS; i++) {
                /* Row: colored name + mute checkbox */
                {
                    ForgeUiRect row = forge_ui_ctx_layout_next(ui, CB_H);
                    forge_ui_ctx_layout_push(ui, row,
                        FORGE_UI_LAYOUT_HORIZONTAL,
                        FORGE_UI_LAYOUT_EXPLICIT_ZERO, 4.0f);

                    const char *lname = TRACKS[display_t].layer_names[i];
                    forge_ui_ctx_label_colored_layout(ui, lname,
                        row.w * 0.65f,
                        LAYER_COLORS[i][0], LAYER_COLORS[i][1],
                        LAYER_COLORS[i][2], 1.0f);

                    char mute_label[32];
                    SDL_snprintf(mute_label, sizeof(mute_label), "Mute##m%d", i);
                    forge_ui_ctx_checkbox_layout(ui, mute_label,
                        &state->layer_muted[display_t][i], row.w * 0.33f);

                    forge_ui_ctx_layout_pop(ui);
                }

                /* Weight slider (or read-only label in intensity mode) */
                if (!state->intensity_mode) {
                    char label[32];
                    SDL_snprintf(label, sizeof(label), "##lw_%d", i);
                    if (forge_ui_ctx_slider_layout(ui, label,
                            &state->layer_weights[i], 0.0f, 1.0f, SH)) {
                        forge_audio_layer_group_fade_weight(
                            lg, i, state->layer_weights[i], 0.1f);
                    }
                } else {
                    char buf[32];
                    SDL_snprintf(buf, sizeof(buf), "w = %.2f",
                                 (double)state->layer_weights[i]);
                    forge_ui_ctx_label_layout(ui, buf, LH);
                }
            }

            forge_ui_ctx_separator_layout(ui, SEP);

            /* VU meters — all layers side by side in one row, tall */
            {
                /* Labels row */
                ForgeUiRect lbl_row = forge_ui_ctx_layout_next(ui, LH);
                forge_ui_ctx_layout_push(ui, lbl_row,
                    FORGE_UI_LAYOUT_HORIZONTAL, 0.0f, 8.0f);
                for (int i = 0; i < lg->layer_count && i < MAX_TRACK_LAYERS; i++) {
                    forge_ui_ctx_label_colored_layout(ui,
                        TRACKS[display_t].layer_names[i],
                        lbl_row.w / (float)lg->layer_count,
                        LAYER_COLORS[i][0], LAYER_COLORS[i][1],
                        LAYER_COLORS[i][2], 1.0f);
                }
                forge_ui_ctx_layout_pop(ui);

                /* Meters row */
                float vu_tall = VU_TALL;
                ForgeUiRect vu_row = forge_ui_ctx_layout_next(ui, vu_tall);
                forge_ui_ctx_layout_push(ui, vu_row,
                    FORGE_UI_LAYOUT_HORIZONTAL, 0.0f, 8.0f);
                for (int i = 0; i < lg->layer_count && i < MAX_TRACK_LAYERS; i++) {
                    forge_ui_ctx_vu_meter_layout(ui,
                        state->layer_vu_l[i], state->layer_vu_r[i],
                        state->layer_peak_l[i], state->layer_peak_r[i],
                        vu_row.w / (float)lg->layer_count);
                }
                forge_ui_ctx_layout_pop(ui);
            }

            forge_ui_wctx_window_end(wctx);
        }

        /* ════════════════════════════════════════════════════════
         * Window 3: Master & Stats
         * ════════════════════════════════════════════════════════ */
        if (forge_ui_wctx_window_begin(wctx, "Master & Stats",
                                        &state->win_master)) {
            /* Master volume + VU side by side */
            {
                ForgeUiRect row = forge_ui_ctx_layout_next(ui, SH);
                forge_ui_ctx_layout_push(ui, row,
                    FORGE_UI_LAYOUT_HORIZONTAL, 0.0f, 4.0f);

                forge_ui_ctx_slider_layout(ui, "Volume",
                    &state->master_volume, 0.0f, VOLUME_MAX, row.w * 0.70f);

                forge_ui_ctx_vu_meter_layout(ui,
                    state->master_vu_l, state->master_vu_r,
                    state->master_peak_l, state->master_peak_r,
                    row.w * 0.28f);

                forge_ui_ctx_layout_pop(ui);
            }

            forge_ui_ctx_separator_layout(ui, SEP);

            /* Streaming stats */
            forge_ui_ctx_label_layout(ui, "Stream Buffer", LH);
            {
                ForgeUiColor spark_col = { 0.3f, 0.7f, 0.95f, 1.0f };
                forge_ui_ctx_sparkline_layout(ui,
                    state->buf_fill_history, SPARKLINE_HISTORY,
                    0.0f, 1.0f, spark_col, SPARK_H);
            }
            {
                int ring_avail = 0;
                if (lg->layer_count > 0)
                    ring_avail = lg->layers[0].stream.ring_available;
                float fill_pct = 100.0f * (float)ring_avail
                               / (float)FORGE_AUDIO_STREAM_RING_FRAMES;
                char buf[80];
                SDL_snprintf(buf, sizeof(buf), "Ring: %d/%d (%.0f%%)",
                             ring_avail, FORGE_AUDIO_STREAM_RING_FRAMES,
                             (double)fill_pct);
                forge_ui_ctx_label_layout(ui, buf, LH);
            }
            {
                float dur = 0.0f;
                if (lg->layer_count > 0)
                    dur = lg->layers[0].stream.duration;
                float full_mb = dur * (float)FORGE_AUDIO_SAMPLE_RATE
                              * (float)FORGE_AUDIO_CHANNELS * (float)sizeof(float)
                              * (float)lg->layer_count / (1024.0f * 1024.0f);
                float stream_kb = (float)(FORGE_AUDIO_STREAM_RING_FRAMES
                                         * FORGE_AUDIO_CHANNELS * (int)sizeof(float))
                                * (float)lg->layer_count / 1024.0f;
                char buf[80];
                SDL_snprintf(buf, sizeof(buf),
                    "%.0f KB streaming vs %.1f MB full load",
                    (double)stream_kb, (double)full_mb);
                forge_ui_ctx_label_layout(ui, buf, LH);
            }

            forge_ui_ctx_separator_layout(ui, SEP);
            forge_ui_ctx_label_layout(ui, "P:pause R:restart N:next 1-3:layers", LH);

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

    for (int i = 0; i < NUM_TRACKS; i++) {
        if (state->track_loaded[i])
            forge_audio_layer_group_close(&state->groups[i]);
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
