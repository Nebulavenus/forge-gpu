/*
 * forge_audio.h — Header-only audio library for forge-gpu
 *
 * Lesson 01 delivers: WAV loading (any format → F32 stereo), audio buffer
 * management, source playback state, and additive mixing with volume and pan.
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
#include <string.h>  /* memset */

/* ── Constants ─────────────────────────────────────────────────────────── */

/* Canonical internal format — all loaded audio is converted to this */
#define FORGE_AUDIO_SAMPLE_RATE  44100
#define FORGE_AUDIO_CHANNELS     2
#define FORGE_AUDIO_FORMAT       SDL_AUDIO_F32

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

    memset(buf, 0, sizeof(*buf));

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
    memset(&src, 0, sizeof(src));
    src.buffer  = buffer;
    src.volume  = volume;
    src.pan     = 0.0f;
    src.looping = looping;
    src.playing = false;
    src.cursor  = 0;
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

    if (src->volume <= 0.0f) {
        /* Advance cursor even at zero volume so progress tracking works */
        int advance = frames * FORGE_AUDIO_CHANNELS;
        src->cursor += advance;
        int total = src->buffer->sample_count;
        if (src->cursor >= total) {
            if (src->looping) {
                src->cursor = 0;
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

    const float *data = src->buffer->data;
    int total = src->buffer->sample_count;
    int remaining = frames;

    while (remaining > 0 && src->playing) {
        /* How many frames can we read before hitting the buffer end? */
        int samples_left = total - src->cursor;
        int frames_left = samples_left / FORGE_AUDIO_CHANNELS;
        int to_mix = remaining < frames_left ? remaining : frames_left;

        /* Additive mix with per-channel gain */
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

#endif /* FORGE_AUDIO_H */
