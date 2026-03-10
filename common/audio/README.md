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

The library depends on SDL3 for audio stream management and WAV loading. It
uses `common/math/forge_math.h` for spatial audio calculations (vectors,
distance, dot products).

## API Reference

*The audio library grows with each lesson. API documentation will be added
as functions are implemented.*

### Planned API (from Audio Lessons)

| Lesson | Functions | Purpose |
|---|---|---|
| 01 — Audio Basics | `forge_audio_init()`, `forge_audio_load_wav()`, `forge_audio_play()` | Initialization, WAV loading, basic playback |
| 02 — Sound Effects | `forge_audio_play_oneshot()`, `forge_audio_set_volume()`, `forge_audio_fade()` | One-shot and looping sounds, volume, fading |
| 03 — Audio Mixing | `forge_audio_mixer_create()`, `forge_audio_mixer_set_volume()`, `forge_audio_mixer_set_pan()` | Multi-source mixing, panning, master volume |
| 04 — Spatial Audio | `forge_audio_source_set_position()`, `forge_audio_listener_set()`, `forge_audio_attenuation()` | 3D positioning, distance attenuation, Doppler |
| 05–06 | *See [PLAN.md](../../PLAN.md)* | Streaming, DSP effects |

## Design

- **Header-only** — `static inline` functions, no separate compilation unit
- **Uses SDL3 audio** — `SDL_AudioStream` as the backend for all playback
- **Uses forge_math** — Vectors (`vec3`) for spatial audio calculations
- **Naming** — `forge_audio_` prefix for functions, `ForgeAudio` for types
- **No external dependencies** — SDL3 only (WAV loading via `SDL_LoadWAV`)
- **Thread-safe where needed** — Audio callbacks run on a separate thread;
  shared state uses SDL atomics or mutexes
- **Tested** — Every function has tests for correctness and edge cases in
  `tests/audio/`

## Where It's Used

| Lesson | What it uses |
|---|---|
| *Coming soon* | See [PLAN.md](../../PLAN.md) for the roadmap |
