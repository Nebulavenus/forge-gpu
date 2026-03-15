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

### Planned API (future lessons)

| Lesson | Functions | Purpose |
|---|---|---|
| 02 — Sound Effects | `forge_audio_play_oneshot()`, `forge_audio_set_volume()`, `forge_audio_fade()` | One-shot and looping sounds, volume, fading |
| 03 — Audio Mixing | `forge_audio_mixer_create()`, `forge_audio_mixer_set_volume()`, `forge_audio_mixer_set_pan()` | Multi-source mixing, panning, master volume |
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
