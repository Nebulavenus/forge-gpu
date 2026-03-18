# Audio Library (`common/audio/`)

Header-only audio library for forge-gpu. The audio lessons teach concepts;
this library is what remains when the learning is done. Every lesson extends
it, and it is the primary deliverable of the audio track.

The library must be correct, efficient, tested, and safe. Every function
documents its purpose, handles edge cases, and has corresponding tests in
`tests/audio/`.

## Usage

```c
#include "audio/forge_audio.h"
```

The library depends on SDL3 for WAV loading and format conversion. It provides
decode/conversion helpers and a mixer function (`forge_audio_source_mix`) that
mixes into caller-provided buffers — the audio device and output stream are
created by lesson code. Later lessons will add spatial audio using
`common/math/forge_math.h` for vector calculations.

## API Reference

### Lesson 01 — Audio Basics

| Function | Signature | Purpose |
|---|---|---|
| `forge_audio_load_wav` | `(const char *path, ForgeAudioBuffer *buf) → bool` | Load WAV, convert to F32 stereo 44100 Hz |
| `forge_audio_buffer_free` | `(ForgeAudioBuffer *buf)` | Free buffer sample data |
| `forge_audio_buffer_frames` | `(const ForgeAudioBuffer *buf) → int` | Frame count (samples / channels) |
| `forge_audio_buffer_duration` | `(const ForgeAudioBuffer *buf) → float` | Duration in seconds |
| `forge_audio_source_create` | `(const ForgeAudioBuffer *buf, float vol, bool loop) → ForgeAudioSource` | Create playback source |
| `forge_audio_source_reset` | `(ForgeAudioSource *src)` | Rewind cursor to start |
| `forge_audio_source_progress` | `(const ForgeAudioSource *src) → float` | Playback fraction [0..1] |
| `forge_audio_source_mix` | `(ForgeAudioSource *src, float *out, int frames)` | Additive mix with volume and pan |

### Lesson 02 — Sound Effects

| Function | Signature | Purpose |
|---|---|---|
| `forge_audio_source_fade` | `(ForgeAudioSource *src, float target, float duration)` | Start a linear fade toward target over duration |
| `forge_audio_source_fade_in` | `(ForgeAudioSource *src, float duration)` | Fade from 0→1, starts playback |
| `forge_audio_source_fade_out` | `(ForgeAudioSource *src, float duration)` | Fade from current→0, auto-stops on completion |
| `forge_audio_source_fade_update` | `(ForgeAudioSource *src, float dt)` | Advance fade envelope by dt seconds (call per frame) |
| `forge_audio_pool_init` | `(ForgeAudioPool *pool)` | Zero all pool slots |
| `forge_audio_pool_play` | `(ForgeAudioPool *pool, const ForgeAudioBuffer *buf, float vol, bool loop) → int` | Fire-and-forget play, returns slot index or -1 |
| `forge_audio_pool_get` | `(ForgeAudioPool *pool, int index) → ForgeAudioSource*` | Get source at slot index, or NULL |
| `forge_audio_pool_mix` | `(ForgeAudioPool *pool, float *out, int frames)` | Mix all active sources, reclaim finished slots |
| `forge_audio_pool_stop_all` | `(ForgeAudioPool *pool)` | Stop all sources |
| `forge_audio_pool_active_count` | `(const ForgeAudioPool *pool) → int` | Count of currently playing sources |

### Lesson 03 — Audio Mixing

| Function | Signature | Purpose |
|---|---|---|
| `forge_audio_mixer_create` | `(void) → ForgeAudioMixer` | Create zeroed mixer with master volume 1.0 |
| `forge_audio_mixer_add_channel` | `(ForgeAudioMixer *mixer, ForgeAudioSource *src) → int` | Add source as channel, returns index or -1 |
| `forge_audio_mixer_mix` | `(ForgeAudioMixer *mixer, float *out, int frames)` | Mix all channels with volume/pan/mute/solo, soft clip via tanh |
| `forge_audio_mixer_update_peaks` | `(ForgeAudioMixer *mixer, float dt)` | Decay peak hold values over time |
| `forge_audio_channel_peak` | `(const ForgeAudioMixer *mixer, int ch, float *l, float *r)` | Read per-channel peak levels |
| `forge_audio_mixer_master_peak` | `(const ForgeAudioMixer *mixer, float *l, float *r)` | Read master peak levels |

### Planned API (future lessons)

| Lesson | Functions | Purpose |
|---|---|---|
| 04 — Spatial Audio | `forge_audio_source_set_position()`, `forge_audio_listener_set()`, `forge_audio_attenuation()` | 3D positioning, distance attenuation, Doppler |
| 05–06 | *See [PLAN.md](../../PLAN.md)* | Streaming, DSP effects |

## Design

- **Header-only** — `static inline` functions, no separate compilation unit
- **Uses SDL3 audio** — `SDL_LoadWAV` for decoding and `SDL_AudioStream` for format conversion
- **No external dependencies** — SDL3 only (WAV loading via `SDL_LoadWAV`);
  later lessons will add `forge_math.h` for spatial audio calculations
- **Naming** — `forge_audio_` prefix for functions, `ForgeAudio` for types
- **Tested** — Every function has tests for correctness and edge cases in
  `tests/audio/`

## Where It's Used

| Lesson | What it uses |
|---|---|
| [Audio 01](../../lessons/audio/01-audio-basics/) | `forge_audio_load_wav`, `ForgeAudioBuffer`, `ForgeAudioSource`, `forge_audio_source_mix` |
| [Audio 02](../../lessons/audio/02-sound-effects/) | `ForgeAudioPool`, `forge_audio_pool_play`, `forge_audio_pool_mix`, `forge_audio_source_fade_in`, `forge_audio_source_fade_out` |
| [Audio 03](../../lessons/audio/03-audio-mixing/) | `ForgeAudioMixer`, `forge_audio_mixer_create`, `forge_audio_mixer_add_channel`, `forge_audio_mixer_mix`, `forge_audio_mixer_update_peaks` |
