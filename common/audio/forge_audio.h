/*
 * forge_audio.h — Header-only audio library for forge-gpu
 *
 * Grows lesson by lesson:
 *   Lesson 01 — WAV loading (any format → F32 stereo), audio buffer
 *               management, source playback state, additive mixing with
 *               volume and pan.
 *   Lesson 02 — Fade envelopes (linear ramp with auto-stop), fire-and-forget
 *               source pool for polyphonic playback.
 *   Lesson 03 — Multi-channel mixer with per-channel volume/pan/mute/solo,
 *               tanh soft clipping on the master bus, peak metering with
 *               hold-and-decay indicators.
 *
 * Usage:
 *   #include "audio/forge_audio.h"
 *
 * The library uses SDL3 for WAV decoding and format conversion.  All sample
 * data is stored as 32-bit float, stereo, 44100 Hz — the internal canonical
 * format.  SDL_AudioStream handles conversion from any input format.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_AUDIO_H
#define FORGE_AUDIO_H

#include <SDL3/SDL.h>

/* Portable isfinite — SDL_stdinc.h does not yet provide this */
#ifndef forge_isfinite
#define forge_isfinite(x) (!SDL_isinf(x) && !SDL_isnan(x))
#endif

/* ── Constants ─────────────────────────────────────────────────────────── */

/* Canonical internal format — all loaded audio is converted to this */
#define FORGE_AUDIO_SAMPLE_RATE  44100
#define FORGE_AUDIO_CHANNELS     2
#define FORGE_AUDIO_FORMAT       SDL_AUDIO_F32

/* Minimum meaningful fade distance — below this, snap to target */
#define FORGE_AUDIO_FADE_EPSILON 1e-6f

/* ── Types ─────────────────────────────────────────────────────────────── */

/* A buffer of decoded audio samples in canonical format (F32, stereo, 44100).
 *
 * sample_count is the total number of float values (frames * channels).
 * For stereo audio, sample_count = frames * 2.
 *
 * Owned by the caller — free with forge_audio_buffer_free(). */
typedef struct ForgeAudioBuffer {
    float *data;          /* interleaved F32 samples (L R L R …) */
    int    sample_count;  /* total float values (frames * channels) */
    int    channels;      /* always FORGE_AUDIO_CHANNELS after load */
    int    sample_rate;   /* always FORGE_AUDIO_SAMPLE_RATE after load */
} ForgeAudioBuffer;

/* A playback instance referencing a ForgeAudioBuffer.
 *
 * Multiple sources can reference the same buffer (e.g. many gunshot sounds
 * from one loaded WAV).  The source tracks its own cursor, volume, pan,
 * and loop state.
 *
 * Pan: -1.0 = full left, 0.0 = center, +1.0 = full right.
 * Volume: linear gain, 0.0 = silent, 1.0 = unity.
 * Note: with linear panning, center pan (0.0) splits volume equally
 * between channels — each channel receives volume/2 amplitude. */
typedef struct ForgeAudioSource {
    const ForgeAudioBuffer *buffer;  /* audio data (not owned) */
    int    cursor;       /* current sample offset (in floats, not frames) */
    float  volume;       /* linear gain [0..∞), typically [0..1] */
    float  pan;          /* stereo pan [-1..+1], 0 = center */
    bool   looping;      /* wrap cursor at end instead of stopping */
    bool   playing;      /* true = actively mixing into output */

    /* Fade envelope (Lesson 02) — multiplied on top of volume during mixing.
     * Default: fade_volume=1.0, fade_target=1.0, fade_rate=0.0 (no fade). */
    float  fade_volume;  /* current fade multiplier [0..1] */
    float  fade_target;  /* where the fade is heading */
    float  fade_rate;    /* |change| per second (0 = no fade active) */
} ForgeAudioSource;

/* ── Buffer functions ──────────────────────────────────────────────────── */

/* Load a WAV file and convert to canonical format (F32, stereo, 44100 Hz).
 *
 * Uses SDL_LoadWAV to decode, then SDL_AudioStream to convert from the WAV's
 * native format to our canonical format.  This handles 8-bit, 16-bit, 24-bit,
 * 32-bit int, and 32-bit float inputs at any sample rate and channel count.
 *
 * Returns true on success.  On failure, logs the error and returns false;
 * the buffer is zeroed. */
static inline bool forge_audio_load_wav(const char *path,
                                         ForgeAudioBuffer *buf)
{
    if (!path || !buf) {
        SDL_Log("ERROR: forge_audio_load_wav: invalid args (path=%p, buf=%p)",
                (const void *)path, (void *)buf);
        return false;
    }

    SDL_AudioSpec wav_spec;
    Uint8 *wav_data = NULL;
    Uint32 wav_len = 0;

    SDL_memset(buf, 0, sizeof(*buf));

    /* Decode WAV file — SDL handles RIFF parsing and basic decompression */
    if (!SDL_LoadWAV(path, &wav_spec, &wav_data, &wav_len)) {
        SDL_Log("ERROR: forge_audio_load_wav: SDL_LoadWAV failed for '%s': %s",
                path, SDL_GetError());
        return false;
    }

    /* Target format: F32, stereo, 44100 Hz */
    SDL_AudioSpec target_spec;
    target_spec.format   = FORGE_AUDIO_FORMAT;
    target_spec.channels = FORGE_AUDIO_CHANNELS;
    target_spec.freq     = FORGE_AUDIO_SAMPLE_RATE;

    /* Create a conversion stream from WAV format to canonical format */
    SDL_AudioStream *stream = SDL_CreateAudioStream(&wav_spec, &target_spec);
    if (!stream) {
        SDL_Log("ERROR: forge_audio_load_wav: SDL_CreateAudioStream failed: %s",
                SDL_GetError());
        SDL_free(wav_data);
        return false;
    }

    /* Push all WAV data into the stream and flush */
    if (!SDL_PutAudioStreamData(stream, wav_data, (int)wav_len)) {
        SDL_Log("ERROR: forge_audio_load_wav: SDL_PutAudioStreamData failed: %s",
                SDL_GetError());
        SDL_DestroyAudioStream(stream);
        SDL_free(wav_data);
        return false;
    }
    if (!SDL_FlushAudioStream(stream)) {
        SDL_Log("ERROR: forge_audio_load_wav: SDL_FlushAudioStream failed: %s",
                SDL_GetError());
        SDL_DestroyAudioStream(stream);
        SDL_free(wav_data);
        return false;
    }

    /* Read the converted F32 data */
    int available = SDL_GetAudioStreamAvailable(stream);
    if (available <= 0) {
        SDL_Log("ERROR: forge_audio_load_wav: No converted data available for '%s'",
                path);
        SDL_DestroyAudioStream(stream);
        SDL_free(wav_data);
        return false;
    }

    float *converted = (float *)SDL_malloc((size_t)available);
    if (!converted) {
        SDL_Log("ERROR: forge_audio_load_wav: Failed to allocate %d bytes", available);
        SDL_DestroyAudioStream(stream);
        SDL_free(wav_data);
        return false;
    }

    int got = SDL_GetAudioStreamData(stream, converted, available);
    if (got <= 0) {
        SDL_Log("ERROR: forge_audio_load_wav: SDL_GetAudioStreamData failed: %s",
                SDL_GetError());
        SDL_free(converted);
        SDL_DestroyAudioStream(stream);
        SDL_free(wav_data);
        return false;
    }

    /* Fill buffer struct */
    buf->data         = converted;
    buf->sample_count = got / (int)sizeof(float);
    buf->channels     = FORGE_AUDIO_CHANNELS;
    buf->sample_rate  = FORGE_AUDIO_SAMPLE_RATE;

    SDL_DestroyAudioStream(stream);
    SDL_free(wav_data);

    int frames = buf->sample_count / buf->channels;
    float duration = (float)frames / (float)buf->sample_rate;
    SDL_Log("Loaded '%s': %d frames, %.2f s, converted to F32 stereo %d Hz",
            path, frames, (double)duration, buf->sample_rate);

    return true;
}

/* Free a buffer's sample data.  Safe to call on a zeroed buffer. */
static inline void forge_audio_buffer_free(ForgeAudioBuffer *buf)
{
    if (buf && buf->data) {
        SDL_free(buf->data);
        buf->data = NULL;
        buf->sample_count = 0;
    }
}

/* Return the number of audio frames in a buffer (sample_count / channels). */
static inline int forge_audio_buffer_frames(const ForgeAudioBuffer *buf)
{
    if (!buf || buf->channels == 0) return 0;
    return buf->sample_count / buf->channels;
}

/* Return the duration of a buffer in seconds. */
static inline float forge_audio_buffer_duration(const ForgeAudioBuffer *buf)
{
    if (!buf || buf->sample_rate == 0 || buf->channels == 0) return 0.0f;
    return (float)(buf->sample_count / buf->channels)
         / (float)buf->sample_rate;
}

/* ── Source functions ──────────────────────────────────────────────────── */

/* Create a source referencing the given buffer.
 * volume: linear gain (1.0 = unity), looping: wrap at end. */
static inline ForgeAudioSource forge_audio_source_create(
    const ForgeAudioBuffer *buffer, float volume, bool looping)
{
    ForgeAudioSource src;
    SDL_memset(&src, 0, sizeof(src));
    src.buffer      = buffer;
    src.volume      = volume;
    src.pan         = 0.0f;
    src.looping     = looping;
    src.playing     = false;
    src.cursor      = 0;
    src.fade_volume = 1.0f;
    src.fade_target = 1.0f;
    src.fade_rate   = 0.0f;
    return src;
}

/* Reset cursor to the beginning. Does not change playing state. */
static inline void forge_audio_source_reset(ForgeAudioSource *src)
{
    if (src) src->cursor = 0;
}

/* Return playback progress as a fraction [0..1].
 * Returns 0 if the buffer is NULL or empty. */
static inline float forge_audio_source_progress(const ForgeAudioSource *src)
{
    if (!src || !src->buffer || src->buffer->sample_count == 0) return 0.0f;
    return (float)src->cursor / (float)src->buffer->sample_count;
}

/* Mix this source's audio into an output buffer (additive).
 *
 * out:    destination buffer (interleaved F32 stereo, NOT zeroed by this call)
 * frames: number of stereo frames to mix
 *
 * The source's cursor advances by the number of frames consumed.  If the
 * source reaches the end of its buffer:
 *   - looping=true:  cursor wraps to the beginning
 *   - looping=false: playing is set to false, remaining output is untouched
 *
 * Panning uses a linear pan law:
 *   gain_L = volume * (1 - pan) / 2
 *   gain_R = volume * (1 + pan) / 2
 * where pan ∈ [-1, +1].  At center (pan=0), each channel receives
 * volume/2 — the total energy is split between left and right. */
static inline void forge_audio_source_mix(ForgeAudioSource *src,
                                           float *out, int frames)
{
    if (!src || !src->playing || !src->buffer || !out) return;
    if (frames <= 0) return;

    /* Guard against empty buffers — prevents modulo-by-zero in the cursor
     * wrap logic and infinite loops where to_mix stays 0. Placed before the
     * data-pointer check so that zero-sample buffers clear playing state
     * regardless of whether data is NULL. */
    if (src->buffer->sample_count <= 0) {
        src->cursor = 0;
        src->playing = false;
        return;
    }

    /* Invalid state: positive sample_count but no data — stop the source
     * to prevent it from being permanently stuck in a playing-but-silent state */
    if (!src->buffer->data) {
        src->playing = false;
        return;
    }

    /* Normalize cursor to valid, channel-aligned position. Prevents
     * zero-progress infinite loops when cursor is odd (stereo) or
     * out of bounds from external manipulation. */
    if (src->cursor < 0) src->cursor = 0;
    if (src->cursor >= src->buffer->sample_count) {
        if (src->looping) {
            src->cursor %= src->buffer->sample_count;
        } else {
            src->cursor = src->buffer->sample_count;
            src->playing = false;
            return;
        }
    }
    if ((src->cursor % FORGE_AUDIO_CHANNELS) != 0) {
        src->cursor -= (src->cursor % FORGE_AUDIO_CHANNELS);
    }

    if (src->volume <= 0.0f || src->fade_volume <= 0.0f) {
        /* Advance cursor even at zero volume so progress tracking works */
        int advance = frames * FORGE_AUDIO_CHANNELS;
        src->cursor += advance;
        int total = src->buffer->sample_count;
        if (src->cursor >= total) {
            if (src->looping) {
                src->cursor %= total;
            } else {
                src->cursor = total;
                src->playing = false;
            }
        }
        return;
    }

    /* Clamp pan to [-1, +1] */
    float pan = src->pan;
    if (pan < -1.0f) pan = -1.0f;
    if (pan >  1.0f) pan =  1.0f;

    float gain_l = src->volume * (1.0f - pan) * 0.5f;
    float gain_r = src->volume * (1.0f + pan) * 0.5f;

    /* Apply fade envelope on top of volume/pan gains.
     * Fade is sampled once per mix call (not per sample) — the caller advances
     * the fade per-frame via fade_update before mixing.  For very short fades
     * at low frame rates this is a step rather than a per-sample ramp, but the
     * simplicity is worth the tradeoff for a fixed-size pool mixer. */
    float fade = src->fade_volume;
    if (fade < 0.0f) fade = 0.0f;
    if (fade > 1.0f) fade = 1.0f;
    gain_l *= fade;
    gain_r *= fade;

    const float *data = src->buffer->data;
    int total = src->buffer->sample_count;
    int remaining = frames;

    while (remaining > 0 && src->playing) {
        /* How many frames can we read before hitting the buffer end? */
        int samples_left = total - src->cursor;
        int frames_left = samples_left / FORGE_AUDIO_CHANNELS;
        int to_mix = remaining < frames_left ? remaining : frames_left;

        /* Additive mix with per-channel gain (includes fade envelope) */
        for (int i = 0; i < to_mix; i++) {
            int idx = src->cursor + i * FORGE_AUDIO_CHANNELS;
            float l = data[idx];
            float r = data[idx + 1];
            out[i * 2]     += l * gain_l;
            out[i * 2 + 1] += r * gain_r;
        }

        src->cursor += to_mix * FORGE_AUDIO_CHANNELS;
        out += to_mix * 2;
        remaining -= to_mix;

        /* End of buffer */
        if (src->cursor >= total) {
            if (src->looping) {
                src->cursor = 0;
            } else {
                src->cursor = total;
                src->playing = false;
            }
        }
    }
}

/* ── Fade functions (Lesson 02) ────────────────────────────────────────── */

/* Advance fade envelope by dt seconds.  Call once per frame (not per mix
 * call) for smooth, frame-rate-independent ramps.  When a fade-out reaches
 * target 0.0, playing is set to false (auto-stop). */
static inline void forge_audio_source_fade_update(ForgeAudioSource *src,
                                                    float dt)
{
    if (!src || src->fade_rate <= 0.0f || dt <= 0.0f) return;

    if (src->fade_volume < src->fade_target) {
        src->fade_volume += src->fade_rate * dt;
        if (src->fade_volume >= src->fade_target) {
            src->fade_volume = src->fade_target;
            src->fade_rate = 0.0f;  /* fade complete */
        }
    } else if (src->fade_volume > src->fade_target) {
        src->fade_volume -= src->fade_rate * dt;
        if (src->fade_volume <= src->fade_target) {
            src->fade_volume = src->fade_target;
            src->fade_rate = 0.0f;  /* fade complete */
            /* Auto-stop when fade-out reaches zero */
            if (src->fade_target <= 0.0f) {
                src->playing = false;
            }
        }
    }
}

/* Start a fade from current fade_volume toward target over duration seconds.
 * Duration <= 0 snaps immediately. */
static inline void forge_audio_source_fade(ForgeAudioSource *src,
                                            float target, float duration)
{
    if (!src) return;
    if (target < 0.0f) target = 0.0f;
    if (target > 1.0f) target = 1.0f;

    if (duration <= 0.0f) {
        src->fade_volume = target;
        src->fade_target = target;
        src->fade_rate   = 0.0f;
        /* Auto-stop on instant fade to zero */
        if (target <= 0.0f) {
            src->playing = false;
        }
        return;
    }

    float distance = target - src->fade_volume;
    if (distance < 0.0f) distance = -distance;
    if (distance < FORGE_AUDIO_FADE_EPSILON) {
        src->fade_rate = 0.0f;
        return;
    }

    src->fade_target = target;
    src->fade_rate   = distance / duration;
}

/* Fade in from silence: set fade_volume to 0, start playback, fade to 1.0.
 * Duration <= 0 snaps immediately. */
static inline void forge_audio_source_fade_in(ForgeAudioSource *src,
                                               float duration)
{
    if (!src) return;
    src->fade_volume = 0.0f;
    src->playing = true;
    forge_audio_source_fade(src, 1.0f, duration);
}

/* Fade out from current level to 0.  When the fade completes, playing is
 * set to false automatically (in forge_audio_source_fade_update). */
static inline void forge_audio_source_fade_out(ForgeAudioSource *src,
                                                float duration)
{
    if (!src) return;
    forge_audio_source_fade(src, 0.0f, duration);
}

/* ── Source pool (Lesson 02) ──────────────────────────────────────────── */

/* A fixed-size pool of audio sources for fire-and-forget playback.
 *
 * Call pool_play() to start a sound — the pool finds an idle slot, creates
 * a source, and begins playback.  pool_mix() mixes all active sources and
 * reclaims finished slots.  No manual tracking needed.
 *
 * The pool does not own the ForgeAudioBuffers — the caller must keep them
 * alive for the lifetime of any playing source that references them. */

#ifndef FORGE_AUDIO_POOL_MAX_SOURCES
#define FORGE_AUDIO_POOL_MAX_SOURCES 32
#endif

typedef struct ForgeAudioPool {
    ForgeAudioSource sources[FORGE_AUDIO_POOL_MAX_SOURCES]; /* playback slots (only valid after pool_play) */
} ForgeAudioPool;

/* Zero all pool slots.  Only slots returned by pool_play() have valid
 * source state — unplayed slots are zeroed (fade_volume=0, playing=false). */
static inline void forge_audio_pool_init(ForgeAudioPool *pool)
{
    if (!pool) return;
    SDL_memset(pool, 0, sizeof(*pool));
}

/* Start playing a buffer in the first available slot.
 * Returns the slot index [0..MAX-1] on success, or -1 if the pool is full. */
static inline int forge_audio_pool_play(ForgeAudioPool *pool,
                                         const ForgeAudioBuffer *buffer,
                                         float volume, bool looping)
{
    if (!pool || !buffer) return -1;

    for (int i = 0; i < FORGE_AUDIO_POOL_MAX_SOURCES; i++) {
        if (!pool->sources[i].playing) {
            pool->sources[i] = forge_audio_source_create(buffer, volume,
                                                          looping);
            pool->sources[i].playing = true;
            return i;
        }
    }
    return -1;  /* pool full */
}

/* Get a pointer to the source at the given index, regardless of playing state.
 * Returns NULL if the index is out of range. */
static inline ForgeAudioSource *forge_audio_pool_get(ForgeAudioPool *pool,
                                                      int index)
{
    if (!pool || index < 0 || index >= FORGE_AUDIO_POOL_MAX_SOURCES) {
        return NULL;
    }
    return &pool->sources[index];
}

/* Mix all active sources into out (additive, F32 stereo).
 * Reclaims slots where playing has become false (end-of-buffer or fade-out).
 * Does NOT call forge_audio_source_fade_update — call that per-source
 * per-frame before mixing. */
static inline void forge_audio_pool_mix(ForgeAudioPool *pool,
                                         float *out, int frames)
{
    if (!pool || !out || frames <= 0) return;

    for (int i = 0; i < FORGE_AUDIO_POOL_MAX_SOURCES; i++) {
        if (pool->sources[i].playing) {
            forge_audio_source_mix(&pool->sources[i], out, frames);
            /* If source stopped during mix (end-of-buffer), slot is reclaimed
             * automatically — playing is now false, so pool_play can reuse it */
        }
    }
}

/* Stop all sources in the pool immediately. */
static inline void forge_audio_pool_stop_all(ForgeAudioPool *pool)
{
    if (!pool) return;
    for (int i = 0; i < FORGE_AUDIO_POOL_MAX_SOURCES; i++) {
        pool->sources[i].playing = false;
    }
}

/* Return the number of currently active (playing) sources. */
static inline int forge_audio_pool_active_count(const ForgeAudioPool *pool)
{
    if (!pool) return 0;
    int count = 0;
    for (int i = 0; i < FORGE_AUDIO_POOL_MAX_SOURCES; i++) {
        if (pool->sources[i].playing) count++;
    }
    return count;
}

/* ── Mixer (Lesson 03) ─────────────────────────────────────────────── */

/* A multi-channel mixer with per-channel volume, pan, mute, solo,
 * peak metering, and soft clipping on the master bus.
 *
 * Each channel wraps a ForgeAudioSource pointer and adds per-channel
 * controls.  The mixer owns no audio data — sources and buffers are
 * managed by the caller.
 *
 * forge_audio_mixer_mix() is the core function: it mixes all active
 * channels into a stereo output buffer with the following signal chain:
 *   1. Per-channel volume + pan  →  additive sum
 *   2. Master volume
 *   3. Soft clip (tanh saturation)
 *   4. Peak metering */

#ifndef FORGE_AUDIO_MIXER_MAX_CHANNELS
#define FORGE_AUDIO_MIXER_MAX_CHANNELS 16
#endif

/* Peak hold decay rate: seconds before a peak indicator drops to zero */
#define FORGE_AUDIO_PEAK_HOLD_TIME 1.5f

/* Stack scratch buffer size (in floats) for per-channel mixing.
 * Falls back to heap allocation when total_samples exceeds this. */
#define FORGE_AUDIO__STACK_SCRATCH_SIZE 2048

/* Compensation factor to undo the center-pan halving applied by
 * forge_audio_source_mix (volume=1, pan=0 → gain = 0.5 per channel).
 * Coupled to the linear pan law: gain = volume * (1 ± pan) / 2. */
#define FORGE_AUDIO__CENTER_PAN_UNDO 2.0f

typedef struct ForgeAudioChannel {
    ForgeAudioSource *source;   /* audio source (not owned) */
    float  volume;              /* per-channel gain [0..∞), typically [0..2] */
    float  pan;                 /* stereo pan [-1..+1], 0 = center */
    bool   mute;                /* true = channel excluded from mix */
    bool   solo;                /* true = only solo'd channels are heard */
    float  peak_l;              /* instantaneous peak level, left — pre-master,
                                 * pre-clip, can exceed 1.0 when channel gain > 1.0 */
    float  peak_r;              /* instantaneous peak level, right — pre-master,
                                 * pre-clip, can exceed 1.0 when channel gain > 1.0 */
    float  peak_hold_l;         /* peak hold level, left — pre-master, pre-clip,
                                 * can exceed 1.0; decays over time */
    float  peak_hold_r;         /* peak hold level, right — pre-master, pre-clip,
                                 * can exceed 1.0; decays over time */
    float  peak_hold_timer_l;   /* seconds remaining before hold starts decay */
    float  peak_hold_timer_r;   /* seconds remaining before hold starts decay */
} ForgeAudioChannel;

typedef struct ForgeAudioMixer {
    ForgeAudioChannel channels[FORGE_AUDIO_MIXER_MAX_CHANNELS];
    int   channel_count;        /* number of active channels */
    float master_volume;        /* master gain applied after sum [0..∞) */
    float master_peak_l;        /* instantaneous master peak, left */
    float master_peak_r;        /* instantaneous master peak, right */
    float master_peak_hold_l;   /* master peak hold, left */
    float master_peak_hold_r;   /* master peak hold, right */
    float master_hold_timer_l;  /* seconds remaining before master hold decays */
    float master_hold_timer_r;  /* seconds remaining before master hold decays */
} ForgeAudioMixer;

/* Create a zeroed mixer with master_volume = 1.0. */
static inline ForgeAudioMixer forge_audio_mixer_create(void)
{
    ForgeAudioMixer m;
    SDL_memset(&m, 0, sizeof(m));
    m.master_volume = 1.0f;
    return m;
}

/* Add a source to the mixer.  Returns the channel index [0..MAX-1]
 * on success, or -1 if the mixer is full.  The channel defaults to
 * volume=1.0, pan=0, unmuted, not solo'd. */
static inline int forge_audio_mixer_add_channel(ForgeAudioMixer *mixer,
                                                  ForgeAudioSource *source)
{
    if (!mixer || !source) return -1;
    if (mixer->channel_count < 0) return -1;
    if (mixer->channel_count >= FORGE_AUDIO_MIXER_MAX_CHANNELS) return -1;

    int idx = mixer->channel_count;
    SDL_memset(&mixer->channels[idx], 0, sizeof(ForgeAudioChannel));
    mixer->channels[idx].source = source;
    mixer->channels[idx].volume = 1.0f;
    mixer->channels[idx].pan    = 0.0f;
    mixer->channel_count++;
    return idx;
}

/* Private helper: clamp channel_count to valid range [0, MAX]. */
static inline int forge_audio__clamped_channel_count(
    const ForgeAudioMixer *mixer)
{
    if (!mixer) return 0;
    int n = mixer->channel_count;
    if (n < 0) n = 0;
    if (n > FORGE_AUDIO_MIXER_MAX_CHANNELS) n = FORGE_AUDIO_MIXER_MAX_CHANNELS;
    return n;
}

/* Private helper: tanh approximation using SDL_expf.
 * Clamps extreme values to avoid overflow in exp. */
static inline float forge_audio__tanhf(float x)
{
    if (x >  10.0f) return  1.0f;
    if (x < -10.0f) return -1.0f;
    float e2x = SDL_expf(2.0f * x);
    return (e2x - 1.0f) / (e2x + 1.0f);
}

/* Mix all channels into out (stereo F32, frames * 2 floats).
 *
 * Unlike forge_audio_source_mix() which is additive, this function
 * zeroes the output buffer before mixing.  The signal chain is:
 *   1. Zero output
 *   2. Scan for any solo'd channels
 *   3. Per channel: skip if muted or (has_solo && !solo'd)
 *   4. Mix source samples with channel volume + pan law
 *   5. Track per-channel peak (max |sample| per side)
 *   6. Apply master_volume to summed output
 *   7. Soft-clip via tanh
 *   8. Track master peak
 *
 * Sources are advanced by forge_audio_source_mix(), which handles
 * looping and end-of-buffer logic.  The mixer uses a temporary
 * scratch buffer per channel to measure per-channel peaks before
 * the master sum. */
static inline void forge_audio_mixer_mix(ForgeAudioMixer *mixer,
                                           float *out, int frames)
{
    if (!mixer || !out || frames <= 0) return;

    size_t total_samples = (size_t)frames * FORGE_AUDIO_CHANNELS;
    size_t total_bytes = total_samples * sizeof(float);
    if (total_bytes / sizeof(float) != total_samples) return;  /* overflow */

    /* 1. Zero output buffer */
    SDL_memset(out, 0, total_bytes);

    int ch_count = forge_audio__clamped_channel_count(mixer);
    if (ch_count <= 0) {
        mixer->master_peak_l = 0.0f;
        mixer->master_peak_r = 0.0f;
        return;
    }

    /* 2. Check for any solo'd channels */
    bool has_solo = false;
    for (int ch = 0; ch < ch_count; ch++) {
        if (mixer->channels[ch].solo) {
            has_solo = true;
            break;
        }
    }

    /* 3-5. Per-channel mixing with peak tracking.
     * We use a stack-allocated scratch buffer for small frame counts,
     * or heap-allocate for large ones. */
    float stack_scratch[FORGE_AUDIO__STACK_SCRATCH_SIZE];
    float *scratch = NULL;
    bool scratch_heap = false;
    if (total_samples <= FORGE_AUDIO__STACK_SCRATCH_SIZE) {
        scratch = stack_scratch;
    } else {
        scratch = (float *)SDL_malloc(total_bytes);
        if (!scratch) {
            mixer->master_peak_l = 0.0f;
            mixer->master_peak_r = 0.0f;
            return;
        }
        scratch_heap = true;
    }

    for (int ch = 0; ch < ch_count; ch++) {
        ForgeAudioChannel *channel = &mixer->channels[ch];

        /* Reset instantaneous peaks before this mix pass */
        channel->peak_l = 0.0f;
        channel->peak_r = 0.0f;

        /* Skip conditions: muted, or solo is active and this channel isn't solo'd.
         * Skipped channels still advance their source cursor so looping stems
         * stay in sync when unmuted.  (Source_mix at volume 0 advances the
         * cursor without producing output — see the zero-volume fast path.) */
        bool skip_mix = false;
        if (channel->mute && !channel->solo) {
            skip_mix = true;
        }
        if (has_solo && !channel->solo) {
            skip_mix = true;
        }

        if (!channel->source || !channel->source->playing) continue;

        /* Sanitize public controls — reject NaN/Inf to prevent poison */
        if (!forge_isfinite(channel->volume)) channel->volume = 0.0f;
        if (!forge_isfinite(channel->pan))    channel->pan = 0.0f;

        if (skip_mix || channel->volume <= 0.0f) {
            /* Advance cursor to stay in sync without mixing */
            float saved_vol = channel->source->volume;
            channel->source->volume = 0.0f;
            float dummy[2] = {0};
            /* source_mix at volume 0 advances cursor via its fast path */
            forge_audio_source_mix(channel->source, dummy, frames);
            channel->source->volume = saved_vol;
            continue;
        }

        /* Mix this source into scratch buffer (zeroed first) */
        SDL_memset(scratch, 0, total_bytes);

        /* Temporarily set source volume to 1.0 and pan to 0.0 so we get
         * raw samples — we apply channel volume/pan ourselves.
         *
         * NOTE: This mutates the source struct briefly.  The mixer is
         * designed for single-threaded use; do not call from an audio
         * callback that reads the same sources concurrently. */
        float saved_vol = channel->source->volume;
        float saved_pan = channel->source->pan;
        channel->source->volume = 1.0f;
        channel->source->pan = 0.0f;
        forge_audio_source_mix(channel->source, scratch, frames);
        channel->source->volume = saved_vol;
        channel->source->pan = saved_pan;

        /* Apply channel volume and pan, add to output, track peaks.
         * Pan law: gain_L = volume*(1-pan)/2, gain_R = volume*(1+pan)/2 */
        float pan = channel->pan;
        if (pan < -1.0f) pan = -1.0f;
        if (pan >  1.0f) pan =  1.0f;
        float gain_l = channel->volume * (1.0f - pan) * 0.5f;
        float gain_r = channel->volume * (1.0f + pan) * 0.5f;

        float ch_peak_l = 0.0f;
        float ch_peak_r = 0.0f;

        for (int i = 0; i < frames; i++) {
            float raw_l = scratch[i * 2]     * FORGE_AUDIO__CENTER_PAN_UNDO;
            float raw_r = scratch[i * 2 + 1] * FORGE_AUDIO__CENTER_PAN_UNDO;

            float sl = raw_l * gain_l;
            float sr = raw_r * gain_r;

            out[i * 2]     += sl;
            out[i * 2 + 1] += sr;

            float abs_l = sl < 0.0f ? -sl : sl;
            float abs_r = sr < 0.0f ? -sr : sr;
            if (abs_l > ch_peak_l) ch_peak_l = abs_l;
            if (abs_r > ch_peak_r) ch_peak_r = abs_r;
        }

        channel->peak_l = ch_peak_l;
        channel->peak_r = ch_peak_r;

        /* Update peak hold */
        if (ch_peak_l >= channel->peak_hold_l) {
            channel->peak_hold_l = ch_peak_l;
            channel->peak_hold_timer_l = FORGE_AUDIO_PEAK_HOLD_TIME;
        }
        if (ch_peak_r >= channel->peak_hold_r) {
            channel->peak_hold_r = ch_peak_r;
            channel->peak_hold_timer_r = FORGE_AUDIO_PEAK_HOLD_TIME;
        }
    }

    if (scratch_heap) {
        SDL_free(scratch);
    }

    /* 6-8. Master volume, soft clip, master peak */
    if (!forge_isfinite(mixer->master_volume)) mixer->master_volume = 1.0f;
    if (mixer->master_volume < 0.0f) mixer->master_volume = 0.0f;

    float m_peak_l = 0.0f;
    float m_peak_r = 0.0f;

    for (int i = 0; i < frames; i++) {
        /* Apply master volume */
        float l = out[i * 2]     * mixer->master_volume;
        float r = out[i * 2 + 1] * mixer->master_volume;

        /* Soft-clip via tanh — output stays in [-1, 1] */
        l = forge_audio__tanhf(l);
        r = forge_audio__tanhf(r);

        out[i * 2]     = l;
        out[i * 2 + 1] = r;

        float abs_l = l < 0.0f ? -l : l;
        float abs_r = r < 0.0f ? -r : r;
        if (abs_l > m_peak_l) m_peak_l = abs_l;
        if (abs_r > m_peak_r) m_peak_r = abs_r;
    }

    mixer->master_peak_l = m_peak_l;
    mixer->master_peak_r = m_peak_r;

    /* Update master peak hold */
    if (m_peak_l >= mixer->master_peak_hold_l) {
        mixer->master_peak_hold_l = m_peak_l;
        mixer->master_hold_timer_l = FORGE_AUDIO_PEAK_HOLD_TIME;
    }
    if (m_peak_r >= mixer->master_peak_hold_r) {
        mixer->master_peak_hold_r = m_peak_r;
        mixer->master_hold_timer_r = FORGE_AUDIO_PEAK_HOLD_TIME;
    }
}

/* Decay peak hold values over time.  Call once per frame with dt. */
static inline void forge_audio_mixer_update_peaks(ForgeAudioMixer *mixer,
                                                    float dt)
{
    if (!mixer || !forge_isfinite(dt) || dt <= 0.0f) return;

    int ch_count = forge_audio__clamped_channel_count(mixer);
    float decay_rate = 1.0f / FORGE_AUDIO_PEAK_HOLD_TIME;

    for (int ch = 0; ch < ch_count; ch++) {
        ForgeAudioChannel *c = &mixer->channels[ch];

        /* Left peak hold — if timer expires mid-frame, apply remaining
         * time as decay so large dt values work in a single call. */
        if (c->peak_hold_timer_l > 0.0f) {
            c->peak_hold_timer_l -= dt;
            if (c->peak_hold_timer_l < 0.0f) {
                float decay_dt = -c->peak_hold_timer_l;
                c->peak_hold_timer_l = 0.0f;
                c->peak_hold_l -= decay_rate * decay_dt;
                if (c->peak_hold_l < 0.0f) c->peak_hold_l = 0.0f;
            }
        } else {
            c->peak_hold_l -= decay_rate * dt;
            if (c->peak_hold_l < 0.0f) c->peak_hold_l = 0.0f;
        }

        /* Right peak hold */
        if (c->peak_hold_timer_r > 0.0f) {
            c->peak_hold_timer_r -= dt;
            if (c->peak_hold_timer_r < 0.0f) {
                float decay_dt = -c->peak_hold_timer_r;
                c->peak_hold_timer_r = 0.0f;
                c->peak_hold_r -= decay_rate * decay_dt;
                if (c->peak_hold_r < 0.0f) c->peak_hold_r = 0.0f;
            }
        } else {
            c->peak_hold_r -= decay_rate * dt;
            if (c->peak_hold_r < 0.0f) c->peak_hold_r = 0.0f;
        }
    }

    /* Master peak hold */
    if (mixer->master_hold_timer_l > 0.0f) {
        mixer->master_hold_timer_l -= dt;
        if (mixer->master_hold_timer_l < 0.0f) {
            float decay_dt = -mixer->master_hold_timer_l;
            mixer->master_hold_timer_l = 0.0f;
            mixer->master_peak_hold_l -= decay_rate * decay_dt;
            if (mixer->master_peak_hold_l < 0.0f) mixer->master_peak_hold_l = 0.0f;
        }
    } else {
        mixer->master_peak_hold_l -= decay_rate * dt;
        if (mixer->master_peak_hold_l < 0.0f) mixer->master_peak_hold_l = 0.0f;
    }
    if (mixer->master_hold_timer_r > 0.0f) {
        mixer->master_hold_timer_r -= dt;
        if (mixer->master_hold_timer_r < 0.0f) {
            float decay_dt = -mixer->master_hold_timer_r;
            mixer->master_hold_timer_r = 0.0f;
            mixer->master_peak_hold_r -= decay_rate * decay_dt;
            if (mixer->master_peak_hold_r < 0.0f) mixer->master_peak_hold_r = 0.0f;
        }
    } else {
        mixer->master_peak_hold_r -= decay_rate * dt;
        if (mixer->master_peak_hold_r < 0.0f) mixer->master_peak_hold_r = 0.0f;
    }
}

/* Read per-channel peak levels. */
static inline void forge_audio_channel_peak(const ForgeAudioMixer *mixer,
                                              int ch,
                                              float *out_l, float *out_r)
{
    int ch_count = forge_audio__clamped_channel_count(mixer);
    if (!mixer || ch < 0 || ch >= ch_count) {
        if (out_l) *out_l = 0.0f;
        if (out_r) *out_r = 0.0f;
        return;
    }
    if (out_l) *out_l = mixer->channels[ch].peak_l;
    if (out_r) *out_r = mixer->channels[ch].peak_r;
}

/* Read master peak levels. */
static inline void forge_audio_mixer_master_peak(const ForgeAudioMixer *mixer,
                                                   float *out_l, float *out_r)
{
    if (!mixer) {
        if (out_l) *out_l = 0.0f;
        if (out_r) *out_r = 0.0f;
        return;
    }
    if (out_l) *out_l = mixer->master_peak_l;
    if (out_r) *out_r = mixer->master_peak_r;
}

#endif /* FORGE_AUDIO_H */
