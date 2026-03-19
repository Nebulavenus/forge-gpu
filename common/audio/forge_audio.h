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
 *   Lesson 04 — Spatial audio: distance attenuation (linear, inverse,
 *               exponential), stereo pan from 3D position, Doppler pitch
 *               shifting with fractional-rate sample interpolation.
 *   Lesson 05 — Music streaming: chunked WAV reader with ring buffer,
 *               crossfade between two streams (equal-power), loop-with-intro,
 *               adaptive music layers with per-layer weight fading.
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
#include "math/forge_math.h"
#include "arena/forge_arena.h"

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

/* Minimum layer sync threshold — prevents false re-seeks from float
 * precision loss on long tracks (millions of frames). */
#define FORGE_AUDIO_SYNC_THRESHOLD_FLOOR 1e-6f

/* Tanh soft-clip clamp: |x| beyond this value saturates to ±1.0.
 * tanh(10) ≈ 0.9999999958 — close enough to 1.0 for audio. */
#define FORGE_AUDIO_TANH_CLAMP 10.0f

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

    /* Playback rate (Lesson 04) — controls cursor advancement speed for
     * Doppler pitch shifting.  1.0 = normal speed, 2.0 = double speed (one
     * octave up), 0.5 = half speed (one octave down).  The mixer uses linear
     * interpolation between adjacent samples when the rate is non-integer.
     * At rate=1.0 with cursor_frac=0.0, behavior is identical to the
     * integer-step path from Lessons 01-03. */
    float  playback_rate;  /* cursor advance rate (1.0 = normal) */
    float  cursor_frac;    /* fractional sample position [0..1) */
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
    src.fade_volume    = 1.0f;
    src.fade_target    = 1.0f;
    src.fade_rate      = 0.0f;
    src.playback_rate  = 1.0f;
    src.cursor_frac    = 0.0f;
    return src;
}

/* Reset cursor to the beginning. Does not change playing state. */
static inline void forge_audio_source_reset(ForgeAudioSource *src)
{
    if (src) {
        src->cursor = 0;
        src->cursor_frac = 0.0f;
    }
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

    /* Sanitize playback_rate — zero, negative, or NaN would break cursor logic */
    float rate = src->playback_rate;
    if (!forge_isfinite(rate) || rate <= 0.0f) rate = 1.0f;

    if (src->volume <= 0.0f || src->fade_volume <= 0.0f) {
        /* Advance cursor even at zero volume so progress tracking works */
        if (rate == 1.0f && src->cursor_frac == 0.0f) {
            /* Fast path: integer-step advance (identical to Lessons 01-03) */
            int advance = frames * FORGE_AUDIO_CHANNELS;
            src->cursor += advance;
        } else {
            /* Fractional advance */
            float frac = src->cursor_frac + (float)frames * rate;
            int whole_frames = (int)frac;
            src->cursor_frac = frac - (float)whole_frames;
            src->cursor += whole_frames * FORGE_AUDIO_CHANNELS;
        }
        int total = src->buffer->sample_count;
        if (src->cursor >= total) {
            if (src->looping) {
                src->cursor %= total;
            } else {
                src->cursor = total;
                src->cursor_frac = 0.0f;
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
    int total_frames = total / FORGE_AUDIO_CHANNELS;

    /* ── Integer-step fast path (rate=1.0, cursor_frac=0.0) ────────────
     * When playback_rate is exactly 1.0 and there is no fractional
     * accumulation, use the original integer-step loop.  This preserves
     * bit-identical output for Lessons 01-03 and avoids the overhead of
     * per-sample interpolation. */
    if (rate == 1.0f && src->cursor_frac == 0.0f) {
        int remaining = frames;
        while (remaining > 0 && src->playing) {
            int samples_left = total - src->cursor;
            int frames_left = samples_left / FORGE_AUDIO_CHANNELS;
            int to_mix = remaining < frames_left ? remaining : frames_left;

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

            if (src->cursor >= total) {
                if (src->looping) {
                    src->cursor = 0;
                } else {
                    src->cursor = total;
                    src->playing = false;
                }
            }
        }
        return;
    }

    /* ── Fractional-step path (rate ≠ 1.0 or cursor_frac > 0) ─────────
     * Advances the cursor by `rate` frames per output frame, using linear
     * interpolation between adjacent samples.  Handles loop-boundary
     * wrapping so the interpolation "next frame" is frame 0 when the
     * cursor is at the last frame of a looping buffer. */
    float cursor_f = (float)(src->cursor / FORGE_AUDIO_CHANNELS) +
                     src->cursor_frac;

    for (int i = 0; i < frames; i++) {
        if (!src->playing) break;

        int frame0 = (int)cursor_f;
        float frac = cursor_f - (float)frame0;

        /* Clamp frame0 into range */
        if (frame0 < 0) frame0 = 0;
        if (frame0 >= total_frames) {
            if (src->looping) {
                frame0 %= total_frames;
            } else {
                src->cursor = total;
                src->cursor_frac = 0.0f;
                src->playing = false;
                break;
            }
        }

        /* Next frame for interpolation — wraps for looping buffers */
        int frame1 = frame0 + 1;
        if (frame1 >= total_frames) {
            frame1 = src->looping ? 0 : frame0;  /* clamp at end */
        }

        int idx0 = frame0 * FORGE_AUDIO_CHANNELS;
        int idx1 = frame1 * FORGE_AUDIO_CHANNELS;

        /* Linear interpolation between frame0 and frame1 */
        float l = data[idx0]     * (1.0f - frac) + data[idx1]     * frac;
        float r = data[idx0 + 1] * (1.0f - frac) + data[idx1 + 1] * frac;

        out[i * 2]     += l * gain_l;
        out[i * 2 + 1] += r * gain_r;

        cursor_f += rate;

        /* Handle wrap / end-of-buffer */
        if (cursor_f >= (float)total_frames) {
            if (src->looping) {
                cursor_f = SDL_fmodf(cursor_f, (float)total_frames);
            } else {
                src->cursor = total;
                src->cursor_frac = 0.0f;
                src->playing = false;
                break;
            }
        }
    }

    /* Write back cursor state */
    if (src->playing) {
        int frame_int = (int)cursor_f;
        if (frame_int >= total_frames) {
            frame_int = src->looping ? frame_int % total_frames : total_frames - 1;
        }
        src->cursor = frame_int * FORGE_AUDIO_CHANNELS;
        src->cursor_frac = cursor_f - (float)frame_int;
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

/* Tanh approximation using SDL_expf.
 * Clamps extreme values to avoid overflow in exp.  Useful as a soft-clip
 * function: maps any input smoothly into (-1, +1). */
static inline float forge_audio_tanhf(float x)
{
    if (x >  FORGE_AUDIO_TANH_CLAMP) return  1.0f;
    if (x < -FORGE_AUDIO_TANH_CLAMP) return -1.0f;
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
        l = forge_audio_tanhf(l);
        r = forge_audio_tanhf(r);

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

/* ── Spatial Audio (Lesson 04) ────────────────────────────────────── */

/* Spatial audio adds 3D positioning to the source/mixer architecture.
 * A ForgeAudioSpatialSource wraps a ForgeAudioSource* with position,
 * velocity, and distance parameters.  Each frame, forge_audio_spatial_apply()
 * computes distance attenuation, stereo pan from 3D position, and optional
 * Doppler pitch shift — then writes the results to the underlying source's
 * volume, pan, and playback_rate fields.  The mixer proceeds unchanged. */

/* Speed of sound in air at ~20 °C (meters per second) */
#define FORGE_AUDIO_SPEED_OF_SOUND        343.0f

/* Default spatial source parameters */
#define FORGE_AUDIO_DEFAULT_MIN_DISTANCE  1.0f
#define FORGE_AUDIO_DEFAULT_MAX_DISTANCE  50.0f
#define FORGE_AUDIO_DEFAULT_ROLLOFF       1.0f

/* Near-zero distance threshold for pan and Doppler calculations —
 * below this, source is treated as coincident with listener */
#define FORGE_AUDIO_NEAR_DISTANCE_EPSILON 0.001f

/* Mach clamp fraction — Doppler denominator/numerator floored at this
 * fraction of speed_of_sound to prevent infinity at Mach 1 */
#define FORGE_AUDIO_MACH_CLAMP_FRACTION   0.1f

/* Minimum layer weight for audibility — layers below this are skipped
 * during mixing to avoid processing inaudible sources */
#define FORGE_AUDIO_WEIGHT_EPSILON        0.001f

/* Doppler pitch clamp range — two octaves in each direction */
#define FORGE_AUDIO_DOPPLER_PITCH_MIN     0.5f   /* one octave down */
#define FORGE_AUDIO_DOPPLER_PITCH_MAX     2.0f   /* one octave up */

/* Distance attenuation models.
 * Each model maps a distance d (clamped to [min, max]) to a gain [0, 1].
 *
 *   LINEAR:      gain = 1 - rolloff * (d - min) / (max - min)
 *   INVERSE:     gain = min / (min + rolloff * (d - min))
 *   EXPONENTIAL: gain = pow(d / min, -rolloff)
 */
typedef enum ForgeAudioAttenuationModel {
    FORGE_AUDIO_ATTENUATION_LINEAR,
    FORGE_AUDIO_ATTENUATION_INVERSE,
    FORGE_AUDIO_ATTENUATION_EXPONENTIAL
} ForgeAudioAttenuationModel;

/* The listener represents the player's ears in 3D space.
 * Position and orientation come from the camera; velocity is used for
 * Doppler calculations.  `right` is stored (not recomputed) because it
 * is used for every source's pan calculation. */
typedef struct ForgeAudioListener {
    vec3 position;
    vec3 forward;
    vec3 up;
    vec3 right;
    vec3 velocity;
} ForgeAudioListener;

/* A spatial wrapper around a ForgeAudioSource.
 *
 * base_volume preserves the user-set volume so spatial attenuation can
 * scale it without permanently destroying volume differences between
 * sources (e.g. a loud alarm vs a quiet hum). */
typedef struct ForgeAudioSpatialSource {
    ForgeAudioSource        *source;           /* underlying source (not owned) */
    vec3                     position;          /* world-space position */
    vec3                     velocity;          /* world-space velocity (for Doppler) */
    float                    min_distance;      /* distance at which attenuation begins */
    float                    max_distance;      /* distance at which gain reaches 0 (linear) */
    float                    rolloff;           /* attenuation curve steepness */
    float                    base_volume;       /* source volume before attenuation */
    ForgeAudioAttenuationModel attenuation;     /* distance model */
    bool                     doppler_enabled;   /* compute Doppler pitch shift */
    ForgeAudioMixer         *mixer;             /* mixer to write channel volume/pan (NULL = source-only) */
    int                      channel;           /* mixer channel index (-1 = none) */
} ForgeAudioSpatialSource;

/* ── Spatial functions ────────────────────────────────────────────── */

/* Create a listener from a camera position and orientation quaternion.
 * Extracts forward, up, and right vectors from the quaternion.
 * Velocity defaults to zero — set it manually if the listener moves
 * and Doppler is needed. */
static inline ForgeAudioListener forge_audio_listener_from_camera(
    vec3 position, quat orientation)
{
    ForgeAudioListener l;
    l.position = position;
    l.forward  = quat_forward(orientation);
    l.up       = quat_up(orientation);
    l.right    = quat_right(orientation);
    l.velocity = vec3_create(0.0f, 0.0f, 0.0f);
    return l;
}

/* Create a spatial source wrapping an existing audio source.
 * Captures the source's current volume as base_volume.
 * Defaults: min=1, max=50, rolloff=1, LINEAR, no Doppler.
 *
 * If mixer is non-NULL, spatial_apply writes volume/pan directly to the
 * mixer channel — this is required for audible spatial effects when using
 * ForgeAudioMixer.  Pass NULL / -1 for non-mixer use. */
static inline ForgeAudioSpatialSource forge_audio_spatial_source_create(
    ForgeAudioSource *source, vec3 position,
    ForgeAudioMixer *mixer, int channel)
{
    ForgeAudioSpatialSource ss;
    SDL_memset(&ss, 0, sizeof(ss));
    ss.source          = source;
    ss.position        = position;
    ss.velocity        = vec3_create(0.0f, 0.0f, 0.0f);
    ss.min_distance    = FORGE_AUDIO_DEFAULT_MIN_DISTANCE;
    ss.max_distance    = FORGE_AUDIO_DEFAULT_MAX_DISTANCE;
    ss.rolloff         = FORGE_AUDIO_DEFAULT_ROLLOFF;
    ss.base_volume     = source ? source->volume : 1.0f;
    ss.attenuation     = FORGE_AUDIO_ATTENUATION_LINEAR;
    ss.doppler_enabled = false;
    ss.mixer           = mixer;
    ss.channel         = channel;
    return ss;
}

/* Compute distance attenuation gain for the given model.
 *
 * Returns a gain in [0, 1] (clamped).  Distance is clamped to [min, max]
 * before the model formula is applied.  Division by zero is guarded:
 * if min_distance <= 0, returns 1.0 (no attenuation). */
static inline float forge_audio_spatial_attenuation(
    ForgeAudioAttenuationModel model,
    float distance, float min_dist, float max_dist, float rolloff)
{
    /* Guard: NaN/Inf or non-positive parameters — no meaningful attenuation */
    if (!forge_isfinite(distance) || !forge_isfinite(min_dist) ||
        !forge_isfinite(max_dist) || !forge_isfinite(rolloff)) return 1.0f;
    if (min_dist <= 0.0f || max_dist < min_dist || rolloff <= 0.0f) {
        return 1.0f;
    }

    /* Clamp distance to [min, max] */
    if (distance < min_dist) distance = min_dist;
    if (distance > max_dist) distance = max_dist;

    float gain = 1.0f;

    switch (model) {
    case FORGE_AUDIO_ATTENUATION_LINEAR: {
        float range = max_dist - min_dist;
        if (range <= 0.0f) return 1.0f;
        gain = 1.0f - rolloff * (distance - min_dist) / range;
        break;
    }
    case FORGE_AUDIO_ATTENUATION_INVERSE:
        gain = min_dist / (min_dist + rolloff * (distance - min_dist));
        break;
    case FORGE_AUDIO_ATTENUATION_EXPONENTIAL:
        gain = SDL_powf(distance / min_dist, -rolloff);
        break;
    default:
        break;  /* unknown model — return 1.0 (no attenuation) */
    }

    /* Clamp to [0, 1] */
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 1.0f) gain = 1.0f;
    return gain;
}

/* Compute stereo pan [-1, +1] from a 3D source position relative to
 * the listener.
 *
 * Projects the listener→source direction onto the listener's right axis.
 * +1 = source is fully to the right, -1 = fully left, 0 = ahead/behind
 * or coincident.
 *
 * When the source is very close (< 0.001 units), returns 0 (center pan)
 * to avoid instability from normalizing a near-zero vector. */
static inline float forge_audio_spatial_pan(
    const ForgeAudioListener *listener, vec3 source_pos)
{
    if (!listener) return 0.0f;

    vec3 to_source = vec3_sub(source_pos, listener->position);
    float dist = vec3_length(to_source);
    if (dist < FORGE_AUDIO_NEAR_DISTANCE_EPSILON) return 0.0f;  /* coincident — center pan */

    vec3 dir = vec3_scale(to_source, 1.0f / dist);

    /* Dot with listener's right vector gives the left-right component */
    float pan = vec3_dot(dir, listener->right);

    /* Clamp to [-1, +1] for safety */
    if (pan < -1.0f) pan = -1.0f;
    if (pan >  1.0f) pan =  1.0f;
    return pan;
}

/* Compute Doppler pitch factor from relative velocities.
 *
 * Uses the classical Doppler formula:
 *   pitch = (speed_of_sound + v_listener) /
 *           (speed_of_sound + v_source)
 *
 * v_listener: component along listener→source axis (positive = toward source).
 * v_source:   component along listener→source axis (positive = away from listener).
 *
 * Guards:
 * - Coincident positions (dist < 0.001) → no Doppler (return 1.0)
 * - Source moving at speed of sound → clamp to avoid infinity
 * - Result clamped to [0.5, 2.0] for perceptual sanity
 *
 * Returns 1.0 when stationary, >1.0 when approaching, <1.0 when receding. */
static inline float forge_audio_spatial_doppler(
    const ForgeAudioListener *listener,
    const ForgeAudioSpatialSource *spatial,
    float speed_of_sound)
{
    if (!listener || !spatial) return 1.0f;
    if (speed_of_sound <= 0.0f) return 1.0f;

    vec3 to_source = vec3_sub(spatial->position, listener->position);
    float dist = vec3_length(to_source);
    if (dist < FORGE_AUDIO_NEAR_DISTANCE_EPSILON) return 1.0f;  /* coincident — no Doppler */

    vec3 dir = vec3_scale(to_source, 1.0f / dist);

    /* Radial velocity along the listener→source axis:
     *   v_listener > 0 → listener moves toward source
     *   v_source > 0   → source moves away from listener
     *
     * Classical Doppler formula (Wikipedia convention):
     *   pitch = (c + v_listener) / (c + v_source)
     *
     * When source approaches: v_source < 0 → denom shrinks → pitch > 1.
     * When source recedes:    v_source > 0 → denom grows   → pitch < 1. */
    float v_listener = vec3_dot(listener->velocity, dir);
    float v_source   = vec3_dot(spatial->velocity, dir);

    /* Floor for denominator/numerator — prevents infinity at Mach 1 */
    float min_vel = speed_of_sound * FORGE_AUDIO_MACH_CLAMP_FRACTION;

    float denom = speed_of_sound + v_source;
    if (denom < min_vel) denom = min_vel;

    float numer = speed_of_sound + v_listener;
    if (numer < min_vel) numer = min_vel;

    float pitch = numer / denom;

    /* Clamp to perceptually reasonable range */
    if (pitch < FORGE_AUDIO_DOPPLER_PITCH_MIN) pitch = FORGE_AUDIO_DOPPLER_PITCH_MIN;
    if (pitch > FORGE_AUDIO_DOPPLER_PITCH_MAX) pitch = FORGE_AUDIO_DOPPLER_PITCH_MAX;
    return pitch;
}

/* Apply spatial parameters from 3D positions.
 *
 * Call once per frame for each spatial source, after updating positions.
 * Computes distance attenuation, stereo pan, and optional Doppler from
 * the listener and source world-space positions.
 *
 * If the spatial source was created with a mixer binding (mixer != NULL),
 * the results are written directly to the mixer channel — this is
 * required for audible spatial effects when using ForgeAudioMixer.
 *
 * The source's volume, pan, and playback_rate fields are always updated
 * for readback (UI display, non-mixer use cases). */
static inline void forge_audio_spatial_apply(
    const ForgeAudioListener *listener,
    ForgeAudioSpatialSource *spatial)
{
    if (!listener || !spatial || !spatial->source) return;

    /* Distance from listener to source */
    vec3 diff = vec3_sub(spatial->position, listener->position);
    float distance = vec3_length(diff);

    /* 1. Attenuation */
    float gain = forge_audio_spatial_attenuation(
        spatial->attenuation, distance,
        spatial->min_distance, spatial->max_distance, spatial->rolloff);
    float volume = spatial->base_volume * gain;

    /* 2. Stereo pan */
    float pan = forge_audio_spatial_pan(listener, spatial->position);

    /* 3. Doppler (optional) */
    float rate = 1.0f;
    if (spatial->doppler_enabled) {
        rate = forge_audio_spatial_doppler(
            listener, spatial, FORGE_AUDIO_SPEED_OF_SOUND);
    }

    /* Write to source fields (for readback and non-mixer use) */
    spatial->source->volume = volume;
    spatial->source->pan = pan;
    spatial->source->playback_rate = rate;

    /* Write to mixer channel (what actually controls audio output) */
    if (spatial->mixer && spatial->channel >= 0 &&
        spatial->channel < forge_audio__clamped_channel_count(spatial->mixer)) {
        spatial->mixer->channels[spatial->channel].volume = volume;
        spatial->mixer->channels[spatial->channel].pan = pan;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Lesson 05 — Music Streaming
 *
 * Three systems for streaming music from disk:
 *
 * 1. ForgeAudioStream — Reads a WAV file in small chunks through a ring
 *    buffer, keeping memory usage constant regardless of track length.
 *    SDL_AudioStream handles format conversion (any WAV → F32 stereo 44100).
 *
 * 2. ForgeAudioCrossfader — Manages two streams and crossfades between them
 *    using equal-power gain curves (sqrt).  When a new track is requested,
 *    the outgoing stream fades out while the incoming stream fades in.
 *
 * 3. ForgeAudioLayerGroup — Streams multiple stems (layers) in lockstep,
 *    mixing them with per-layer weight.  Weights can be faded over time
 *    for adaptive music (e.g. adding drums when combat starts).
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── Streaming constants ──────────────────────────────────────────────── */

/* Frames per disk read — each chunk is 4096 stereo frames (~93 ms at 44100) */
#define FORGE_AUDIO_STREAM_CHUNK_FRAMES   4096

/* Number of chunks in the ring buffer — 4 chunks ≈ 0.37s at 44100 Hz */
#define FORGE_AUDIO_STREAM_RING_CHUNKS    4

/* Total ring buffer capacity in frames */
#define FORGE_AUDIO_STREAM_RING_FRAMES \
    (FORGE_AUDIO_STREAM_CHUNK_FRAMES * FORGE_AUDIO_STREAM_RING_CHUNKS)

/* Maximum number of adaptive music layers per group */
#define FORGE_AUDIO_MAX_LAYERS  8

/* ── WAV header parser (internal) ─────────────────────────────────────── */

/* Parse RIFF/WAV header to locate the 'fmt ' and 'data' chunks without
 * reading the entire file.  This allows streaming: we only need to know
 * the audio format and where the raw PCM data starts.
 *
 * Handles varied chunk orderings and skips unknown chunks (LIST, JUNK,
 * bext, etc.) as required by the WAV specification.
 *
 * Returns true on success, filling out_spec, out_data_offset, and
 * out_data_size.  On failure, returns false and logs an error. */
static inline bool forge_audio__wav_parse_header(
    SDL_IOStream *io,
    SDL_AudioSpec *out_spec,
    Uint32 *out_data_offset,
    Uint32 *out_data_size)
{
    if (!io || !out_spec || !out_data_offset || !out_data_size) return false;

    /* Read RIFF header (12 bytes) */
    Uint8 header[12];
    if (SDL_ReadIO(io, header, 12) != 12) {
        SDL_Log("ERROR: forge_audio__wav_parse_header: failed to read RIFF header");
        return false;
    }
    if (SDL_memcmp(header, "RIFF", 4) != 0 || SDL_memcmp(header + 8, "WAVE", 4) != 0) {
        SDL_Log("ERROR: forge_audio__wav_parse_header: not a RIFF/WAVE file");
        return false;
    }

    bool found_fmt = false;
    bool found_data = false;

    /* Walk chunks until we find both 'fmt ' and 'data' */
    while (!found_fmt || !found_data) {
        Uint8 chunk_header[8];
        if (SDL_ReadIO(io, chunk_header, 8) != 8) break;

        char id[5] = {0};
        SDL_memcpy(id, chunk_header, 4);
        Uint32 chunk_size = (Uint32)chunk_header[4]
                          | ((Uint32)chunk_header[5] << 8)
                          | ((Uint32)chunk_header[6] << 16)
                          | ((Uint32)chunk_header[7] << 24);

        if (SDL_memcmp(id, "fmt ", 4) == 0) {
            /* Read format chunk — minimum 16 bytes */
            if (chunk_size < 16) {
                SDL_Log("ERROR: forge_audio__wav_parse_header: fmt chunk too small (%u)", chunk_size);
                return false;
            }
            Uint8 fmt[40]; /* enough for WAVEFORMATEXTENSIBLE */
            Uint32 to_read = chunk_size < 40 ? chunk_size : 40;
            if (SDL_ReadIO(io, fmt, to_read) != (size_t)to_read) {
                SDL_Log("ERROR: forge_audio__wav_parse_header: failed to read fmt chunk");
                return false;
            }
            Uint16 format_tag = (Uint16)(fmt[0] | (fmt[1] << 8));
            Uint16 channels   = (Uint16)(fmt[2] | (fmt[3] << 8));
            Uint32 sample_rate = (Uint32)fmt[4] | ((Uint32)fmt[5] << 8)
                               | ((Uint32)fmt[6] << 16) | ((Uint32)fmt[7] << 24);
            Uint16 bits_per_sample = (Uint16)(fmt[14] | (fmt[15] << 8));

            if (channels == 0 || channels > 8 || sample_rate == 0 || sample_rate > 192000) {
                SDL_Log("ERROR: forge_audio__wav_parse_header: invalid fmt (channels=%u, rate=%u)",
                        channels, sample_rate);
                return false;
            }

            /* Map format tag to SDL_AudioFormat */
            SDL_AudioFormat sdl_fmt;
            if (format_tag == 3) {
                /* IEEE float — only 32-bit is supported for streaming */
                if (bits_per_sample != 32) {
                    SDL_Log("ERROR: forge_audio__wav_parse_header: unsupported float bit depth %u"
                            " (streaming only supports 32-bit float)", bits_per_sample);
                    return false;
                }
                sdl_fmt = SDL_AUDIO_F32LE;
            } else if (format_tag == 1) {
                /* PCM integer */
                switch (bits_per_sample) {
                case 8:  sdl_fmt = SDL_AUDIO_U8; break;
                case 16: sdl_fmt = SDL_AUDIO_S16LE; break;
                case 32: sdl_fmt = SDL_AUDIO_S32LE; break;
                default:
                    /* Note: 24-bit PCM is supported by forge_audio_load_wav()
                     * (via SDL_LoadWAV) but not by the streaming parser.
                     * 24-bit WAVs should use the non-streaming path. */
                    SDL_Log("ERROR: forge_audio__wav_parse_header: unsupported PCM bit depth %u"
                            " (streaming supports 8/16/32-bit PCM and 32-bit float;"
                            " use forge_audio_load_wav() for 24-bit WAV files)",
                            bits_per_sample);
                    return false;
                }
            } else if (format_tag == 0xFFFE) {
                /* WAVEFORMATEXTENSIBLE — check SubFormat GUID prefix */
                if (chunk_size >= 40) {
                    Uint16 sub_tag = (Uint16)(fmt[24] | (fmt[25] << 8));
                    if (sub_tag == 3) {
                        if (bits_per_sample != 32) {
                            SDL_Log("ERROR: forge_audio__wav_parse_header: unsupported extensible float bit depth %u",
                                    bits_per_sample);
                            return false;
                        }
                        sdl_fmt = SDL_AUDIO_F32LE;
                    } else if (sub_tag == 1) {
                        switch (bits_per_sample) {
                        case 16: sdl_fmt = SDL_AUDIO_S16LE; break;
                        case 32: sdl_fmt = SDL_AUDIO_S32LE; break;
                        default:
                            SDL_Log("ERROR: forge_audio__wav_parse_header: unsupported extensible PCM bit depth %u",
                                    bits_per_sample);
                            return false;
                        }
                    } else {
                        SDL_Log("ERROR: forge_audio__wav_parse_header: unsupported extensible sub-format %u",
                                sub_tag);
                        return false;
                    }
                } else {
                    SDL_Log("ERROR: forge_audio__wav_parse_header: extensible format but chunk too small");
                    return false;
                }
            } else {
                SDL_Log("ERROR: forge_audio__wav_parse_header: unsupported format tag 0x%04X",
                        format_tag);
                return false;
            }

            out_spec->format = sdl_fmt;
            out_spec->channels = (int)channels;
            out_spec->freq = (int)sample_rate;
            found_fmt = true;

            /* Skip remaining bytes if fmt chunk was larger than what we read */
            if (chunk_size > to_read) {
                if (SDL_SeekIO(io, (Sint64)(chunk_size - to_read), SDL_IO_SEEK_CUR) < 0) {
                    SDL_Log("ERROR: forge_audio__wav_parse_header: SDL_SeekIO (skip fmt remainder): %s", SDL_GetError());
                    return false;
                }
            }
        } else if (SDL_memcmp(id, "data", 4) == 0) {
            {
                Sint64 tell = SDL_TellIO(io);
                if (tell < 0) {
                    SDL_Log("ERROR: forge_audio__wav_parse_header: SDL_TellIO failed: %s",
                            SDL_GetError());
                    return false;
                }
                *out_data_offset = (Uint32)tell;
            }
            *out_data_size = chunk_size;
            found_data = true;
            /* Don't seek past data — we know where it is now */
            if (!found_fmt) {
                /* Need to keep scanning for fmt — skip over data */
                if (SDL_SeekIO(io, (Sint64)chunk_size, SDL_IO_SEEK_CUR) < 0) {
                    SDL_Log("ERROR: forge_audio__wav_parse_header: SDL_SeekIO (skip data): %s", SDL_GetError());
                    return false;
                }
            }
        } else {
            /* Unknown chunk (LIST, JUNK, bext, etc.) — skip */
            if (SDL_SeekIO(io, (Sint64)chunk_size, SDL_IO_SEEK_CUR) < 0) {
                SDL_Log("ERROR: forge_audio__wav_parse_header: SDL_SeekIO (skip chunk '%.4s'): %s", id, SDL_GetError());
                return false;
            }
        }

        /* WAV chunks are word-aligned — skip padding byte if odd size */
        if (chunk_size & 1) {
            if (SDL_SeekIO(io, 1, SDL_IO_SEEK_CUR) < 0) {
                SDL_Log("ERROR: forge_audio__wav_parse_header: SDL_SeekIO (padding): %s", SDL_GetError());
                return false;
            }
        }
    }

    if (!found_fmt || !found_data) {
        SDL_Log("ERROR: forge_audio__wav_parse_header: missing %s%s chunk(s)",
                found_fmt ? "" : "fmt ", found_data ? "" : "data");
        return false;
    }
    return true;
}

/* ── ForgeAudioStream — Chunked WAV reader ────────────────────────────── */

/* A streaming WAV reader that loads audio in small chunks through a ring
 * buffer, keeping memory usage constant (~128 KB) regardless of track length.
 *
 * The stream reads raw samples from the WAV file, pushes them through an
 * SDL_AudioStream for format conversion (any format → F32 stereo 44100 Hz),
 * and stores the converted output in a ring buffer.  Consumers call
 * forge_audio_stream_read() to pull frames from the ring buffer.
 *
 * For looping, the stream wraps back to either the file start or a
 * configurable loop-start point (for intro + loop body patterns). */
typedef struct ForgeAudioStream {
    SDL_IOStream   *io;              /* open file handle (owned) */
    Uint32          data_offset;     /* byte offset of PCM data in file */
    Uint32          data_size;       /* total bytes of PCM data */
    SDL_AudioSpec   file_spec;       /* native format of the WAV file */
    SDL_AudioStream *converter;      /* format conversion (file → canonical) */
    float          *ring;            /* ring buffer (RING_FRAMES * 2 floats) */
    int             ring_write;      /* next frame write position (ring index) */
    int             ring_read;       /* next frame read position (ring index) */
    int             ring_available;  /* frames available to read */
    int             total_frames;    /* total frames in file (estimated from data size) */
    int             cursor_frame;    /* logical frame position in source file */
    bool            playing;
    bool            looping;
    int             loop_start_frame; /* intro ends here; loop body starts (0 = loop entire file) */
    bool            finished;        /* reached end (non-looping) */
    float           duration;        /* total duration in seconds (of source file) */
    /* Pre-allocated scratch buffers (stream-lifetime, avoids per-frame malloc) */
    Uint8          *raw_buf;         /* disk read buffer (CHUNK_FRAMES * bpf bytes) */
    int             raw_buf_size;    /* size of raw_buf in bytes */
    float          *pull_buf;        /* converter pull buffer (CHUNK_FRAMES * 2 floats) */
} ForgeAudioStream;

/* Internal: compute bytes per frame for the file's native format */
static inline int forge_audio__stream_file_bpf(const ForgeAudioStream *s)
{
    return SDL_AUDIO_BYTESIZE(s->file_spec.format) * s->file_spec.channels;
}

/* Internal: read one chunk from disk, push through converter, write to ring.
 * Returns number of converted frames written to ring. */
static inline int forge_audio__stream_fill_chunk(ForgeAudioStream *s)
{
    if (!s || !s->io || !s->converter || !s->ring) return 0;

    int bpf = forge_audio__stream_file_bpf(s);
    if (bpf <= 0) return 0;

    /* How many source frames remain in the file?  Use Sint64 to avoid
     * integer overflow for files longer than ~6 minutes at 48 kHz stereo float. */
    Sint64 remaining_bytes = (Sint64)s->data_size - (Sint64)s->cursor_frame * bpf;
    if (remaining_bytes <= 0) return 0;

    int chunk_bytes = FORGE_AUDIO_STREAM_CHUNK_FRAMES * bpf;
    if (remaining_bytes < (Sint64)chunk_bytes) chunk_bytes = (int)remaining_bytes;

    /* Read raw data from disk using the pre-allocated buffer */
    if (chunk_bytes > s->raw_buf_size) chunk_bytes = s->raw_buf_size;

    size_t got = SDL_ReadIO(s->io, s->raw_buf, (size_t)chunk_bytes);
    if (got == 0) return 0;

    s->cursor_frame += (int)got / bpf;

    /* Push raw data into converter */
    if (!SDL_PutAudioStreamData(s->converter, s->raw_buf, (int)got)) {
        SDL_Log("WARN: forge_audio__stream_fill_chunk: SDL_PutAudioStreamData failed: %s",
                SDL_GetError());
        return 0;
    }

    /* Pull converted F32 stereo data from converter into ring buffer
     * using the pre-allocated pull buffer */
    int frames_written = 0;
    int ring_cap = FORGE_AUDIO_STREAM_RING_FRAMES;
    int pull_max = FORGE_AUDIO_STREAM_CHUNK_FRAMES;

    while (s->ring_available < ring_cap) {
        int want_frames = ring_cap - s->ring_available;
        if (want_frames > pull_max) want_frames = pull_max;

        int got_bytes = SDL_GetAudioStreamData(s->converter, s->pull_buf,
                                                want_frames * 2 * (int)sizeof(float));
        if (got_bytes < 0) {
            SDL_Log("ERROR: SDL_GetAudioStreamData failed: %s", SDL_GetError());
            s->playing = false;
            s->finished = true;
            return frames_written;
        }
        if (got_bytes == 0) break;

        int got_frames = got_bytes / (2 * (int)sizeof(float));
        for (int i = 0; i < got_frames; i++) {
            int wi = ((s->ring_write + i) % ring_cap) * 2;
            s->ring[wi]     = s->pull_buf[i * 2];
            s->ring[wi + 1] = s->pull_buf[i * 2 + 1];
        }
        s->ring_write = (s->ring_write + got_frames) % ring_cap;
        s->ring_available += got_frames;
        frames_written += got_frames;
    }

    return frames_written;
}

/* Open a WAV file for streaming.  Parses the header, allocates the ring
 * buffer, creates the format converter, and pre-fills the ring.
 *
 * The stream starts in playing state.  The caller must call
 * forge_audio_stream_update() each frame to keep the ring buffer fed.
 *
 * Returns true on success.  On failure, the stream is zeroed. */
static inline bool forge_audio_stream_open(const char *path,
                                            ForgeAudioStream *s)
{
    if (!path || !s) {
        SDL_Log("ERROR: forge_audio_stream_open: invalid args");
        return false;
    }
    SDL_memset(s, 0, sizeof(*s));

    s->io = SDL_IOFromFile(path, "rb");
    if (!s->io) {
        SDL_Log("ERROR: forge_audio_stream_open: cannot open '%s': %s", path, SDL_GetError());
        return false;
    }

    if (!forge_audio__wav_parse_header(s->io, &s->file_spec, &s->data_offset, &s->data_size)) {
        SDL_CloseIO(s->io);
        SDL_memset(s, 0, sizeof(*s));
        return false;
    }

    int bpf = SDL_AUDIO_BYTESIZE(s->file_spec.format) * s->file_spec.channels;
    if (bpf <= 0) {
        SDL_Log("ERROR: forge_audio_stream_open: invalid bytes-per-frame");
        SDL_CloseIO(s->io);
        SDL_memset(s, 0, sizeof(*s));
        return false;
    }
    Uint64 raw_frames = (Uint64)s->data_size / (Uint32)bpf;
    if (raw_frames > (Uint64)SDL_MAX_SINT32) {
        SDL_Log("ERROR: forge_audio_stream_open: file too large for streaming"
                " (%" SDL_PRIu64 " frames exceeds int range)", raw_frames);
        SDL_CloseIO(s->io);
        SDL_memset(s, 0, sizeof(*s));
        return false;
    }
    s->total_frames = (int)raw_frames;
    s->duration = (float)s->total_frames / (float)s->file_spec.freq;

    /* Create format converter: file format → canonical F32 stereo 44100 */
    SDL_AudioSpec dst_spec;
    dst_spec.format = FORGE_AUDIO_FORMAT;
    dst_spec.channels = FORGE_AUDIO_CHANNELS;
    dst_spec.freq = FORGE_AUDIO_SAMPLE_RATE;

    s->converter = SDL_CreateAudioStream(&s->file_spec, &dst_spec);
    if (!s->converter) {
        SDL_Log("ERROR: forge_audio_stream_open: SDL_CreateAudioStream failed: %s",
                SDL_GetError());
        SDL_CloseIO(s->io);
        SDL_memset(s, 0, sizeof(*s));
        return false;
    }

    /* Allocate ring buffer and scratch buffers (all stream-lifetime) */
    s->ring = (float *)SDL_calloc((size_t)(FORGE_AUDIO_STREAM_RING_FRAMES * 2), sizeof(float));
    s->raw_buf_size = FORGE_AUDIO_STREAM_CHUNK_FRAMES * bpf;
    s->raw_buf = (Uint8 *)SDL_malloc((size_t)s->raw_buf_size);
    s->pull_buf = (float *)SDL_malloc((size_t)(FORGE_AUDIO_STREAM_CHUNK_FRAMES * 2) * sizeof(float));
    if (!s->ring || !s->raw_buf || !s->pull_buf) {
        SDL_Log("ERROR: forge_audio_stream_open: buffer allocation failed");
        SDL_free(s->ring);
        SDL_free(s->raw_buf);
        SDL_free(s->pull_buf);
        SDL_DestroyAudioStream(s->converter);
        SDL_CloseIO(s->io);
        SDL_memset(s, 0, sizeof(*s));
        return false;
    }

    /* Seek to start of PCM data */
    if (SDL_SeekIO(s->io, (Sint64)s->data_offset, SDL_IO_SEEK_SET) < 0) {
        SDL_Log("ERROR: SDL_SeekIO failed in forge_audio_stream_open: %s", SDL_GetError());
        SDL_DestroyAudioStream(s->converter);
        SDL_free(s->ring);
        SDL_free(s->raw_buf);
        SDL_free(s->pull_buf);
        SDL_CloseIO(s->io);
        SDL_memset(s, 0, sizeof(*s));
        return false;
    }
    s->cursor_frame = 0;
    s->playing = true;

    /* Pre-fill the ring buffer */
    for (int i = 0; i < FORGE_AUDIO_STREAM_RING_CHUNKS; i++) {
        forge_audio__stream_fill_chunk(s);
    }

    return true;
}

/* Refill the ring buffer from disk.  Call once per frame.
 *
 * Reads chunks from the WAV file until the ring buffer is full or the file
 * ends.  For looping streams, wraps back to the loop start point when the
 * file end is reached.  For non-looping streams, marks the stream as
 * finished when all data has been consumed. */
static inline void forge_audio_stream_update(ForgeAudioStream *s)
{
    if (!s || !s->playing || s->finished) return;

    int bpf = forge_audio__stream_file_bpf(s);
    if (bpf <= 0) return;

    int ring_cap = FORGE_AUDIO_STREAM_RING_FRAMES;

    /* Fill until ring is full or file runs out */
    while (s->ring_available < ring_cap - FORGE_AUDIO_STREAM_CHUNK_FRAMES) {
        Sint64 remaining_bytes = (Sint64)s->data_size - (Sint64)s->cursor_frame * bpf;
        if (remaining_bytes <= 0) {
            if (s->looping) {
                /* Wrap to loop start point */
                int loop_start = s->loop_start_frame;
                Sint64 loop_byte_offset = (Sint64)loop_start * bpf;
                if (SDL_SeekIO(s->io, (Sint64)s->data_offset + loop_byte_offset,
                               SDL_IO_SEEK_SET) < 0) {
                    SDL_Log("ERROR: forge_audio_stream_update: SDL_SeekIO failed (loop wrap): %s",
                            SDL_GetError());
                    s->playing = false;
                    s->finished = true;
                    return;
                }
                s->cursor_frame = loop_start;

                /* Clear the converter to discard stale resampler tail data.
                 * Flush would push remaining samples from the file end into the
                 * output queue, splicing them before the loop-start data. */
                if (!SDL_ClearAudioStream(s->converter)) {
                    SDL_Log("ERROR: SDL_ClearAudioStream failed: %s", SDL_GetError());
                    s->playing = false;
                    s->finished = true;
                    return;
                }
            } else {
                /* Flush converter — there may be remaining converted data */
                if (!SDL_FlushAudioStream(s->converter)) {
                    SDL_Log("ERROR: SDL_FlushAudioStream failed: %s", SDL_GetError());
                    s->playing = false;
                    s->finished = true;
                    return;
                }
                /* Pull any last converted frames using pre-allocated pull_buf */
                int pull_max = FORGE_AUDIO_STREAM_CHUNK_FRAMES;
                while (s->ring_available < ring_cap) {
                    int want = ring_cap - s->ring_available;
                    if (want > pull_max) want = pull_max;
                    int got_bytes = SDL_GetAudioStreamData(s->converter, s->pull_buf,
                        want * 2 * (int)sizeof(float));
                    if (got_bytes < 0) {
                        SDL_Log("ERROR: SDL_GetAudioStreamData (drain) failed: %s",
                                SDL_GetError());
                        s->playing = false;
                        s->finished = true;
                        return;
                    }
                    if (got_bytes == 0) break;
                    int got_frames = got_bytes / (2 * (int)sizeof(float));
                    for (int i = 0; i < got_frames; i++) {
                        int wi = ((s->ring_write + i) % ring_cap) * 2;
                        s->ring[wi]     = s->pull_buf[i * 2];
                        s->ring[wi + 1] = s->pull_buf[i * 2 + 1];
                    }
                    s->ring_write = (s->ring_write + got_frames) % ring_cap;
                    s->ring_available += got_frames;
                }
                s->finished = true;
                return;
            }
        }

        int wrote = forge_audio__stream_fill_chunk(s);
        if (wrote == 0) break;  /* no progress — avoid infinite loop */
    }
}

/* Read frames from the ring buffer into an output buffer (additive).
 *
 * Adds the stream's audio to the output — does NOT zero the output first.
 * This allows mixing multiple streams into the same buffer.
 *
 * Returns the number of frames actually read.  If the ring is empty,
 * returns 0 (silence — the output is not modified). */
static inline int forge_audio_stream_read(ForgeAudioStream *s,
                                           float *out, int frames)
{
    if (!s || !out || frames <= 0) return 0;
    /* Allow reads when finished (playing=false, finished=true) so
     * remaining ring buffer data drains after the stream ends. */
    if (!s->playing && !s->finished) return 0;

    int ring_cap = FORGE_AUDIO_STREAM_RING_FRAMES;
    int to_read = frames;
    if (to_read > s->ring_available) to_read = s->ring_available;

    for (int i = 0; i < to_read; i++) {
        int ri = ((s->ring_read + i) % ring_cap) * 2;
        out[i * 2]     += s->ring[ri];
        out[i * 2 + 1] += s->ring[ri + 1];
    }
    s->ring_read = (s->ring_read + to_read) % ring_cap;
    s->ring_available -= to_read;

    /* If stream is finished and ring is empty, stop playing */
    if (s->finished && s->ring_available == 0) {
        s->playing = false;
    }

    return to_read;
}

/* Seek to a specific frame in the source file.  Flushes the converter
 * and ring buffer, then refills from the new position. */
static inline void forge_audio_stream_seek(ForgeAudioStream *s, int frame)
{
    if (!s || !s->io || !s->converter) return;

    int bpf = forge_audio__stream_file_bpf(s);
    if (bpf <= 0) return;

    if (frame < 0) frame = 0;
    if (frame > s->total_frames) frame = s->total_frames;

    /* Seek in file (Sint64 to avoid overflow for long files) */
    Sint64 byte_offset = (Sint64)frame * bpf;
    if (SDL_SeekIO(s->io, (Sint64)s->data_offset + byte_offset, SDL_IO_SEEK_SET) < 0) {
        SDL_Log("ERROR: forge_audio_stream_seek: SDL_SeekIO failed: %s", SDL_GetError());
        return; /* keep previous valid stream state */
    }

    /* Clear converter (discard stale data) before committing new state.
     * If clear fails, the file cursor is at the new position but we
     * don't update ring/cursor — the stream stops to avoid corruption. */
    if (!SDL_ClearAudioStream(s->converter)) {
        SDL_Log("ERROR: SDL_ClearAudioStream failed in seek: %s", SDL_GetError());
        s->playing = false;
        s->finished = true;
        return;
    }
    s->cursor_frame = frame;
    s->ring_read = 0;
    s->ring_write = 0;
    s->ring_available = 0;
    s->finished = false;
    s->playing = true;

    /* Refill ring */
    for (int i = 0; i < FORGE_AUDIO_STREAM_RING_CHUNKS; i++) {
        forge_audio__stream_fill_chunk(s);
    }
}

/* Close a stream, releasing all resources. */
static inline void forge_audio_stream_close(ForgeAudioStream *s)
{
    if (!s) return;
    if (s->converter) SDL_DestroyAudioStream(s->converter);
    if (s->ring) SDL_free(s->ring);
    if (s->raw_buf) SDL_free(s->raw_buf);
    if (s->pull_buf) SDL_free(s->pull_buf);
    if (s->io) SDL_CloseIO(s->io);
    SDL_memset(s, 0, sizeof(*s));
}

/* Return playback progress as a fraction [0..1].
 * Based on ring read position relative to estimated total converted frames.
 * Returns 0 if the stream is not open. */
static inline float forge_audio_stream_progress(const ForgeAudioStream *s)
{
    if (!s || s->total_frames <= 0) return 0.0f;

    /* Estimate converted frames from the source frame count and sample rate ratio */
    float ratio = (float)FORGE_AUDIO_SAMPLE_RATE / (float)s->file_spec.freq;
    float total_converted = (float)s->total_frames * ratio;
    if (total_converted <= 0.0f) return 0.0f;

    /* The cursor_frame tells us how far we've read from disk (in source frames).
     * Subtract the ring_available to get how far the consumer has actually consumed.
     * For looping streams, cursor_frame resets on wrap but ring_available still
     * holds pre-wrap frames — clamp to [0, total_converted] to avoid a jump. */
    float consumed_source = (float)s->cursor_frame * ratio - (float)s->ring_available;
    if (consumed_source < 0.0f) {
        /* Wrap: estimate position from what the ring still holds */
        consumed_source = total_converted + consumed_source;
        if (consumed_source < 0.0f) consumed_source = 0.0f;
    }

    float progress = consumed_source / total_converted;
    if (progress > 1.0f) progress = 1.0f;
    return progress;
}

/* Configure loop-with-intro.  The stream plays from the beginning through
 * the intro, then loops back to intro_frames on each wrap.
 *
 * intro_frames is in source file frames (before resampling).  Pass 0 to
 * loop the entire file from the start. */
static inline void forge_audio_stream_set_loop(ForgeAudioStream *s,
                                                int intro_frames)
{
    if (!s) return;
    s->looping = true;
    int clamped = intro_frames > 0 ? intro_frames : 0;
    if (clamped >= s->total_frames) {
        /* Loop body would be zero-length — clamp to leave at least one frame */
        clamped = s->total_frames > 0 ? s->total_frames - 1 : 0;
    }
    s->loop_start_frame = clamped;
}

/* ── ForgeAudioCrossfader — Two-stream crossfade ──────────────────────── */

/* Manages two ForgeAudioStream slots and crossfades between them using
 * equal-power gain curves.  When a new track is requested, it opens
 * in the inactive slot and fades in while the active slot fades out.
 *
 * Equal-power crossfade: gain_out = sqrt(1 - t), gain_in = sqrt(t).
 * This preserves perceived loudness at the midpoint, unlike linear
 * crossfade which produces a 3 dB dip at t = 0.5. */
typedef struct ForgeAudioCrossfader {
    ForgeAudioStream  streams[2];    /* slot A and B */
    int               active;        /* which slot is currently primary (0 or 1) */
    float             fade_progress; /* 0.0 = fully active, 1.0 = fully other */
    float             fade_duration; /* crossfade length in seconds */
    bool              fading;        /* true while a crossfade is in progress */
    float             volume;        /* master gain applied to both streams */
    ForgeArena        scratch;       /* frame-scoped arena for read scratch buffers */
} ForgeAudioCrossfader;

/* Initialize a crossfader to default state. */
static inline void forge_audio_crossfader_init(ForgeAudioCrossfader *xf)
{
    if (!xf) return;
    SDL_memset(xf, 0, sizeof(*xf));
    xf->volume = 1.0f;
    xf->scratch = forge_arena_create(0);
}

/* Start playing a new track with crossfade from the current track.
 *
 * Opens the new track in the inactive slot and begins a crossfade
 * over fade_duration seconds.  If no track is currently playing,
 * the new track starts immediately at full volume.
 *
 * If loop is true, the new track loops indefinitely. */
static inline bool forge_audio_crossfader_play(ForgeAudioCrossfader *xf,
                                                const char *path,
                                                float fade_duration,
                                                bool loop)
{
    if (!xf || !path) return false;

    int incoming = 1 - xf->active;

    /* Close any existing stream in the incoming slot */
    forge_audio_stream_close(&xf->streams[incoming]);

    if (!forge_audio_stream_open(path, &xf->streams[incoming])) {
        return false;
    }
    if (loop) {
        forge_audio_stream_set_loop(&xf->streams[incoming], 0);
    }

    /* If the active slot has a playing stream, start crossfade */
    if (xf->streams[xf->active].playing && fade_duration > 0.0f) {
        xf->fading = true;
        xf->fade_progress = 0.0f;
        xf->fade_duration = fade_duration;
    } else {
        /* No current track or no fade — switch immediately */
        int outgoing = xf->active;
        forge_audio_stream_close(&xf->streams[outgoing]);
        xf->active = incoming;
        xf->fading = false;
        xf->fade_progress = 0.0f;
    }

    return true;
}

/* Advance the crossfade and update both streams.  Call once per frame. */
static inline void forge_audio_crossfader_update(ForgeAudioCrossfader *xf,
                                                  float dt)
{
    if (!xf || !forge_isfinite(dt) || dt < 0.0f) return;

    /* Update both streams */
    forge_audio_stream_update(&xf->streams[0]);
    forge_audio_stream_update(&xf->streams[1]);

    /* Advance crossfade */
    if (xf->fading) {
        if (xf->fade_duration > 0.0f) {
            xf->fade_progress += dt / xf->fade_duration;
        } else {
            xf->fade_progress = 1.0f;
        }
        if (xf->fade_progress >= 1.0f) {
            xf->fade_progress = 1.0f;
            xf->fading = false;
            /* Swap active slot */
            int outgoing = xf->active;
            xf->active = 1 - xf->active;
            xf->fade_progress = 0.0f;
            /* Stop the outgoing stream */
            xf->streams[outgoing].playing = false;
        }
    }
}

/* Read from the crossfader into an output buffer (additive).
 *
 * During a crossfade, both streams are mixed with equal-power gains.
 * Outside a crossfade, only the active stream contributes. */
static inline void forge_audio_crossfader_read(ForgeAudioCrossfader *xf,
                                                float *out, int frames)
{
    if (!xf || !out || frames <= 0) return;

    /* Reset the scratch arena — all allocations from last read are freed */
    forge_arena_reset(&xf->scratch);
    size_t buf_bytes = (size_t)(frames * 2) * sizeof(float);

    if (xf->fading) {
        float t = xf->fade_progress;
        float gain_out = SDL_sqrtf(1.0f - t);  /* outgoing stream */
        float gain_in  = SDL_sqrtf(t);          /* incoming stream */

        float *tmp_out = (float *)forge_arena_alloc(&xf->scratch, buf_bytes);
        float *tmp_in  = (float *)forge_arena_alloc(&xf->scratch, buf_bytes);
        if (tmp_out && tmp_in) {
            SDL_memset(tmp_out, 0, buf_bytes);
            SDL_memset(tmp_in, 0, buf_bytes);
            forge_audio_stream_read(&xf->streams[xf->active], tmp_out, frames);
            forge_audio_stream_read(&xf->streams[1 - xf->active], tmp_in, frames);

            for (int i = 0; i < frames * 2; i++) {
                out[i] += (tmp_out[i] * gain_out + tmp_in[i] * gain_in) * xf->volume;
            }
        } else {
            SDL_Log("forge_audio: crossfader scratch arena OOM (%zu bytes)", buf_bytes * 2);
        }
    } else {
        /* Only active stream */
        if (xf->streams[xf->active].playing) {
            float *tmp = (float *)forge_arena_alloc(&xf->scratch, buf_bytes);
            if (tmp) {
                SDL_memset(tmp, 0, buf_bytes);
                forge_audio_stream_read(&xf->streams[xf->active], tmp, frames);
                for (int i = 0; i < frames * 2; i++) {
                    out[i] += tmp[i] * xf->volume;
                }
            }
        }
    }
}

/* Close both streams and reset the crossfader.
 * The crossfader must be re-initialized with forge_audio_crossfader_init()
 * before it can be used again. */
static inline void forge_audio_crossfader_close(ForgeAudioCrossfader *xf)
{
    if (!xf) return;
    forge_audio_stream_close(&xf->streams[0]);
    forge_audio_stream_close(&xf->streams[1]);
    forge_arena_destroy(&xf->scratch);
    SDL_memset(xf, 0, sizeof(*xf));
}

/* ── ForgeAudioLayerGroup — Adaptive music layers ─────────────────────── */

/* A single layer within an adaptive music group.  Each layer is a
 * ForgeAudioStream playing a stem (e.g. drums, bass, melody) with
 * a weight that controls its contribution to the mix.
 *
 * Weights can be faded over time for smooth transitions between
 * game states (e.g. fading in drums when combat starts). */
typedef struct ForgeAudioLayer {
    ForgeAudioStream  stream;
    float             weight;         /* current mix weight [0..1] */
    float             weight_target;  /* target weight for fading */
    float             weight_rate;    /* |change| per second (0 = no fade) */
    bool              active;         /* true if this layer is in use */
} ForgeAudioLayer;

/* A group of synchronized layers that stream in lockstep.
 *
 * All layers play the same-length audio files and stay sample-aligned.
 * The leader layer drives the cursor position; non-leaders re-sync if
 * they drift by more than 2 frames. */
typedef struct ForgeAudioLayerGroup {
    ForgeAudioLayer   layers[FORGE_AUDIO_MAX_LAYERS];
    int               layer_count;
    bool              playing;
    bool              looping;
    float             volume;         /* group master gain */
    int               leader;         /* index of the layer driving cursor sync */
    ForgeArena        scratch;        /* frame-scoped arena for read scratch buffers */
} ForgeAudioLayerGroup;

/* Initialize a layer group to default state. */
static inline void forge_audio_layer_group_init(ForgeAudioLayerGroup *group)
{
    if (!group) return;
    SDL_memset(group, 0, sizeof(*group));
    group->volume = 1.0f;
    group->scratch = forge_arena_create(0);
}

/* Add a layer to the group.  Opens the WAV file for streaming and sets
 * the initial weight.
 *
 * Returns the layer index (0-based), or -1 on failure. */
static inline int forge_audio_layer_group_add(ForgeAudioLayerGroup *group,
                                               const char *path,
                                               float weight)
{
    if (!group || !path) return -1;
    if (group->layer_count >= FORGE_AUDIO_MAX_LAYERS) {
        SDL_Log("ERROR: forge_audio_layer_group_add: max layers (%d) reached",
                FORGE_AUDIO_MAX_LAYERS);
        return -1;
    }

    int idx = group->layer_count;
    ForgeAudioLayer *layer = &group->layers[idx];
    SDL_memset(layer, 0, sizeof(*layer));

    if (!forge_audio_stream_open(path, &layer->stream)) {
        return -1;
    }

    if (!forge_isfinite(weight)) weight = 0.0f;
    float clamped = weight < 0.0f ? 0.0f : (weight > 1.0f ? 1.0f : weight);
    layer->weight = clamped;
    layer->weight_target = clamped;
    layer->weight_rate = 0.0f;
    layer->active = true;

    if (group->looping) {
        forge_audio_stream_set_loop(&layer->stream, 0);
    }

    group->layer_count++;
    return idx;
}

/* Start a weight fade on a specific layer.
 *
 * The weight moves from its current value toward target over duration
 * seconds.  Pass duration=0 to snap immediately. */
static inline void forge_audio_layer_group_fade_weight(
    ForgeAudioLayerGroup *group, int layer, float target, float duration)
{
    if (!group || layer < 0 || layer >= group->layer_count) return;

    ForgeAudioLayer *l = &group->layers[layer];
    if (!forge_isfinite(target)) target = 0.0f;
    l->weight_target = target < 0.0f ? 0.0f : (target > 1.0f ? 1.0f : target);

    if (!forge_isfinite(duration) || duration <= 0.0f) {
        l->weight = l->weight_target;
        l->weight_rate = 0.0f;
    } else {
        float diff = l->weight_target - l->weight;
        if (diff < 0.0f) diff = -diff;
        l->weight_rate = diff / duration;
    }
}

/* Update weight fades and sync layer cursors.  Call once per frame.
 *
 * Advances weight fades on all layers and ensures all layers stay
 * synchronized with the leader layer's cursor position. */
static inline void forge_audio_layer_group_update(
    ForgeAudioLayerGroup *group, float dt)
{
    if (!group || !group->playing || !forge_isfinite(dt) || dt < 0.0f) return;

    /* Update weight fades */
    for (int i = 0; i < group->layer_count; i++) {
        ForgeAudioLayer *l = &group->layers[i];
        if (!l->active || l->weight_rate <= 0.0f) continue;

        float step = l->weight_rate * dt;
        if (l->weight < l->weight_target) {
            l->weight += step;
            if (l->weight >= l->weight_target) {
                l->weight = l->weight_target;
                l->weight_rate = 0.0f;
            }
        } else if (l->weight > l->weight_target) {
            l->weight -= step;
            if (l->weight <= l->weight_target) {
                l->weight = l->weight_target;
                l->weight_rate = 0.0f;
            }
        }
    }

    /* Update all streams (refill ring buffers from disk) */
    for (int i = 0; i < group->layer_count; i++) {
        if (!group->layers[i].active) continue;
        forge_audio_stream_update(&group->layers[i].stream);
    }

    /* Sync non-leader layers to leader cursor.
     * All layers should read from the same logical position.  If a layer
     * drifts by more than 2 frames, re-seek it to match the leader. */
    if (group->leader >= 0 && group->leader < group->layer_count) {
        ForgeAudioStream *lead = &group->layers[group->leader].stream;
        float lead_progress = forge_audio_stream_progress(lead);

        for (int i = 0; i < group->layer_count; i++) {
            if (i == group->leader || !group->layers[i].active) continue;
            ForgeAudioStream *s = &group->layers[i].stream;
            float s_progress = forge_audio_stream_progress(s);

            /* If progress differs by more than ~2 frames worth, re-sync.
             * Use a minimum threshold to avoid false triggers from float
             * precision loss on large frame counts. */
            float threshold = 2.0f / (float)(s->total_frames > 0 ? s->total_frames : 1);
            if (threshold < FORGE_AUDIO_SYNC_THRESHOLD_FLOOR)
                threshold = FORGE_AUDIO_SYNC_THRESHOLD_FLOOR;
            float diff = lead_progress - s_progress;
            if (diff < 0.0f) diff = -diff;
            /* Wrap-aware: at a loop boundary (e.g. 0.01 vs 0.99) the
             * circular distance is 0.02, not 0.98. */
            if (lead->looping && s->looping && diff > 0.5f)
                diff = 1.0f - diff;
            if (diff > threshold) {
                /* Derive target frame from leader progress — this stays
                 * correct across loop boundaries because progress wraps
                 * consistently for both leader and follower. */
                int target_frame = (int)(lead_progress * (float)s->total_frames);
                if (target_frame < 0) target_frame = 0;
                if (target_frame > s->total_frames) target_frame = s->total_frames;
                forge_audio_stream_seek(s, target_frame);
            }
        }
    }
}

/* Read from all layers and mix into output buffer (additive).
 *
 * Each layer contributes proportionally to its weight.  The group
 * master volume is applied on top. */
static inline void forge_audio_layer_group_read(
    ForgeAudioLayerGroup *group, float *out, int frames)
{
    if (!group || !out || frames <= 0 || !group->playing) return;

    /* Reset the scratch arena — all per-layer buffers from last read are freed */
    forge_arena_reset(&group->scratch);
    size_t buf_bytes = (size_t)(frames * 2) * sizeof(float);

    for (int i = 0; i < group->layer_count; i++) {
        ForgeAudioLayer *l = &group->layers[i];
        if (!l->active) continue;

        /* Bump-allocate from the arena — no per-layer malloc/free */
        float *tmp = (float *)forge_arena_alloc(&group->scratch, buf_bytes);
        if (!tmp) {
            SDL_Log("forge_audio: layer group scratch arena OOM (%zu bytes)", buf_bytes);
            continue;
        }
        SDL_memset(tmp, 0, buf_bytes);

        /* Always read to keep the stream phase-locked with the leader,
         * even for silent layers — otherwise the ring fills up and sync
         * has to keep re-seeking. */
        forge_audio_stream_read(&l->stream, tmp, frames);

        float gain = l->weight * group->volume;
        if (gain < FORGE_AUDIO_WEIGHT_EPSILON) {
            continue; /* consumed but silent — skip accumulation */
        }
        for (int j = 0; j < frames * 2; j++) {
            out[j] += tmp[j] * gain;
        }
    }
}

/* Seek all layers to a specific source frame. */
static inline void forge_audio_layer_group_seek(
    ForgeAudioLayerGroup *group, int frame)
{
    if (!group) return;
    for (int i = 0; i < group->layer_count; i++) {
        if (!group->layers[i].active) continue;
        forge_audio_stream_seek(&group->layers[i].stream, frame);
    }
}

/* Close all layers and reset the group. */
static inline void forge_audio_layer_group_close(ForgeAudioLayerGroup *group)
{
    if (!group) return;
    for (int i = 0; i < group->layer_count; i++) {
        forge_audio_stream_close(&group->layers[i].stream);
    }
    forge_arena_destroy(&group->scratch);
    SDL_memset(group, 0, sizeof(*group));
}

/* Return playback progress [0..1] based on the leader layer. */
static inline float forge_audio_layer_group_progress(
    const ForgeAudioLayerGroup *group)
{
    if (!group || group->layer_count == 0) return 0.0f;
    int leader = group->leader;
    if (leader < 0 || leader >= group->layer_count) leader = 0;
    return forge_audio_stream_progress(&group->layers[leader].stream);
}

#endif /* FORGE_AUDIO_H */
