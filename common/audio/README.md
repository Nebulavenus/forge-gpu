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

The library depends on SDL3 for WAV loading and format conversion, and
`common/math/forge_math.h` for spatial audio vector calculations. It provides
decode/conversion helpers, a mixer, and a spatial audio layer that computes
distance attenuation, stereo pan, and Doppler pitch shifting from 3D positions.
The audio device and output stream are created by lesson code.

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

### Lesson 04 — Spatial Audio

| Function | Signature | Purpose |
|---|---|---|
| `forge_audio_listener_from_camera` | `(vec3 pos, quat orient) → ForgeAudioListener` | Build listener from camera position and orientation |
| `forge_audio_spatial_source_create` | `(ForgeAudioSource *src, vec3 pos, ForgeAudioMixer *mixer, int ch) → ForgeAudioSpatialSource` | Wrap a source with spatial parameters, bind to mixer channel |
| `forge_audio_spatial_attenuation` | `(model, dist, min, max, rolloff) → float` | Compute distance attenuation gain [0, 1] |
| `forge_audio_spatial_pan` | `(const ForgeAudioListener *l, vec3 pos) → float` | Stereo pan [-1, +1] from 3D position |
| `forge_audio_spatial_doppler` | `(const ForgeAudioListener *l, const ForgeAudioSpatialSource *s, float c) → float` | Doppler pitch factor |
| `forge_audio_spatial_apply` | `(const ForgeAudioListener *l, ForgeAudioSpatialSource *s)` | Apply attenuation, pan, Doppler; writes to bound mixer channel |

Also modified in Lesson 04: `ForgeAudioSource` gained `playback_rate` and
`cursor_frac` fields for fractional cursor advancement with linear
interpolation. At rate 1.0 with no fractional accumulation, the mixer takes
the integer-step fast path — identical to Lessons 01–03.

### Lesson 05 — Music & Streaming

| Function | Signature | Purpose |
|---|---|---|
| `forge_audio_stream_open` | `(const char *path, ForgeAudioStream *s) → bool` | Open WAV for chunked streaming via ring buffer |
| `forge_audio_stream_update` | `(ForgeAudioStream *s)` | Refill ring buffer from disk (call once per frame) |
| `forge_audio_stream_read` | `(ForgeAudioStream *s, float *out, int frames) → int` | Read from ring buffer into output (additive) |
| `forge_audio_stream_seek` | `(ForgeAudioStream *s, int frame)` | Seek to source frame, flush and refill ring |
| `forge_audio_stream_close` | `(ForgeAudioStream *s)` | Release ring buffer, converter, and file handle |
| `forge_audio_stream_progress` | `(const ForgeAudioStream *s) → float` | Playback progress [0..1] |
| `forge_audio_stream_set_loop` | `(ForgeAudioStream *s, int intro_frames)` | Enable looping with optional intro skip |
| `forge_audio_crossfader_init` | `(ForgeAudioCrossfader *xf)` | Initialize crossfader to defaults |
| `forge_audio_crossfader_play` | `(ForgeAudioCrossfader *xf, const char *path, float fade_dur, bool loop) → bool` | Start new track with equal-power crossfade |
| `forge_audio_crossfader_update` | `(ForgeAudioCrossfader *xf, float dt)` | Advance crossfade and update streams |
| `forge_audio_crossfader_read` | `(ForgeAudioCrossfader *xf, float *out, int frames)` | Read crossfaded audio (additive) |
| `forge_audio_crossfader_close` | `(ForgeAudioCrossfader *xf)` | Close both streams |
| `forge_audio_layer_group_init` | `(ForgeAudioLayerGroup *group)` | Initialize layer group |
| `forge_audio_layer_group_add` | `(ForgeAudioLayerGroup *group, const char *path, float weight) → int` | Add a streaming layer (returns index) |
| `forge_audio_layer_group_fade_weight` | `(ForgeAudioLayerGroup *group, int layer, float target, float dur)` | Fade layer weight over duration |
| `forge_audio_layer_group_update` | `(ForgeAudioLayerGroup *group, float dt)` | Update fades, sync cursors, refill rings |
| `forge_audio_layer_group_read` | `(ForgeAudioLayerGroup *group, float *out, int frames)` | Mix weighted layers into output (additive) |
| `forge_audio_layer_group_seek` | `(ForgeAudioLayerGroup *group, int frame)` | Seek all layers |
| `forge_audio_layer_group_close` | `(ForgeAudioLayerGroup *group)` | Close all layers |
| `forge_audio_layer_group_progress` | `(const ForgeAudioLayerGroup *group) → float` | Leader layer progress [0..1] |

### Planned API (future lessons)

| Lesson | Functions | Purpose |
|---|---|---|
| 06 | *See [PLAN.md](../../PLAN.md)* | DSP effects |

## Design

- **Header-only** — `static inline` functions, no separate compilation unit
- **Uses SDL3 audio** — `SDL_LoadWAV` for decoding and `SDL_AudioStream` for format conversion
- **Depends on** — SDL3 and `common/math/forge_math.h` (for spatial audio vectors and quaternions)
- **Naming** — `forge_audio_` prefix for functions, `ForgeAudio` for types
- **Tested** — Every function has tests for correctness and edge cases in
  `tests/audio/`

## Where It's Used

| Lesson | What it uses |
|---|---|
| [Audio 01](../../lessons/audio/01-audio-basics/) | `forge_audio_load_wav`, `ForgeAudioBuffer`, `ForgeAudioSource`, `forge_audio_source_mix` |
| [Audio 02](../../lessons/audio/02-sound-effects/) | `ForgeAudioPool`, `forge_audio_pool_play`, `forge_audio_pool_mix`, `forge_audio_source_fade_in`, `forge_audio_source_fade_out` |
| [Audio 03](../../lessons/audio/03-audio-mixing/) | `ForgeAudioMixer`, `forge_audio_mixer_create`, `forge_audio_mixer_add_channel`, `forge_audio_mixer_mix`, `forge_audio_mixer_update_peaks` |
| [Audio 04](../../lessons/audio/04-spatial-audio/) | `ForgeAudioListener`, `ForgeAudioSpatialSource`, `forge_audio_listener_from_camera`, `forge_audio_spatial_source_create`, `forge_audio_spatial_apply` |
| [Audio 05](../../lessons/audio/05-music-streaming/) | `ForgeAudioStream`, `ForgeAudioLayerGroup`, `forge_audio_stream_open`, `forge_audio_layer_group_add`, `forge_audio_layer_group_read`, `forge_audio_layer_group_fade_weight` |
