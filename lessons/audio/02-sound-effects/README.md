# Audio Lesson 02 — Sound Effects

Source pool for fire-and-forget playback and volume fading for click-free
start/stop transitions.

## What you'll learn

- **Source pool** — a fixed-size array of audio sources that handles slot
  allocation and reclamation automatically, enabling fire-and-forget playback
- **Polyphony** — triggering the same sound multiple times without cancelling
  previous instances (each press allocates a new pool slot)
- **Volume fading** — linear ramps on a per-source fade envelope, independent
  of the source's base volume, for smooth transitions
- **Fade-in / fade-out** — starting and stopping sounds without audible clicks
  by ramping the fade envelope between 0 and 1
- **Auto-stop on fade completion** — sources that fade to zero volume
  automatically stop and free their pool slot

## Result

![Sound Effects](assets/screenshot.png)

Interactive scene with five spheres — four for polyphonic one-shot sound
effects and one for an ambient loop with fade toggle. A UI panel shows master
volume, fade duration, active source count, and pool slot status.

## Key concepts

- Fixed-size source pool for fire-and-forget playback
- Polyphonic overlapping sounds via pool slot allocation
- Per-source linear fade envelope (`fade_volume`, `fade_target`, `fade_rate`)
- Auto-stop and slot reclamation on fade-out completion

## Prerequisites

- [Audio Lesson 01 — Audio Basics](../01-audio-basics/) (WAV loading, sources,
  mixing, SDL audio streams)

## Audio files

This lesson needs five WAV files in `assets/audio/`:

| File | Purpose | Duration |
|---|---|---|
| `impact.wav` | Short one-shot | < 0.5s |
| `whoosh.wav` | Short one-shot | < 1s |
| `chime.wav` | Medium one-shot | 1–3s |
| `explosion.wav` | Medium one-shot | 1–3s |
| `wind_loop.wav` | Looping ambient | 5–10s |

Any sample rate, bit depth, or channel count works — the library converts
everything to F32 stereo 44100 Hz on load. Source your own files from a
personal library or [Freesound](https://freesound.org/).

## Controls

| Key | Action |
|---|---|
| WASD / Arrows | Move camera |
| Mouse | Look around (click to capture) |
| Space / Shift | Fly up / down |
| 1–4 | Trigger one-shot sounds (overlapping) |
| 5 | Toggle ambient loop (fade in / fade out) |
| R | Stop all sounds |
| P | Pause / resume audio stream |
| Escape | Release mouse / quit |

## Library additions

Lesson 02 adds two features to
[`forge_audio.h`](../../../common/audio/README.md):

### Fade envelope

Three fields on `ForgeAudioSource` — `fade_volume`, `fade_target`,
`fade_rate` — define a linear fade that runs independently of the source's
base volume. The fade multiplier is applied during mixing on top of
volume and pan gains.

```c
/* Start a fade */
forge_audio_source_fade(src, target, duration);

/* Convenience wrappers */
forge_audio_source_fade_in(src, duration);   /* 0→1, starts playback */
forge_audio_source_fade_out(src, duration);  /* current→0, auto-stops */

/* Call once per frame to advance the fade */
forge_audio_source_fade_update(src, dt);
```

Default values (`fade_volume=1.0`, `fade_rate=0.0`) mean existing code from
Lesson 01 behaves identically — no changes needed.

### Source pool

`ForgeAudioPool` is a fixed-size array of 32 sources. Call `pool_play()` to
fire a sound — the pool finds an idle slot, creates a source, and starts
playback. `pool_mix()` mixes all active sources and reclaims finished slots.

```c
ForgeAudioPool pool;
forge_audio_pool_init(&pool);

/* Fire-and-forget: returns slot index or -1 if full */
int slot = forge_audio_pool_play(&pool, &buffer, 1.0f, false);

/* Mix all active sources (call per frame) */
forge_audio_pool_mix(&pool, output, frames);

/* Query / control */
int n = forge_audio_pool_active_count(&pool);
forge_audio_pool_stop_all(&pool);
ForgeAudioSource *src = forge_audio_pool_get(&pool, slot);
```

## How it works

### Fire-and-forget playback

Without a pool, the caller must track each source individually — deciding
when to create, when to reuse, and when to stop. This becomes unwieldy when
sounds overlap (rapid gunshots, footsteps, UI clicks). The source pool
handles this:

1. `pool_play()` scans for the first slot with `playing == false`
2. It creates a fresh `ForgeAudioSource` in that slot and sets `playing = true`
3. Non-looping sources automatically set `playing = false` when they reach the
   buffer end — the slot is then available for the next `pool_play()` call
4. `pool_mix()` iterates all 32 slots, mixing only the active ones

### Fade envelope

Audio clicks occur when a waveform is abruptly cut — the instantaneous jump
from a nonzero sample to zero creates a broadband impulse. Fading avoids this
by multiplying the output by a ramp that smoothly transitions between 0 and 1.

The fade runs per-frame via `forge_audio_source_fade_update(src, dt)`, which
adjusts `fade_volume` by `fade_rate * dt` toward `fade_target`. The rate is
computed from the distance and requested duration:

```text
fade_rate = |fade_target - fade_volume| / duration
```

When a fade-out reaches zero, the source auto-stops — freeing its pool slot
without the caller needing to check.

## Building

```bash
cmake -B build
cmake --build build --target 02-sound-effects --config Debug
```

## AI skill

The source pool and fade patterns are captured in
[`.claude/skills/forge-audio-pool/SKILL.md`](../../../.claude/skills/forge-audio-pool/SKILL.md) —
use it to add fire-and-forget playback and fade transitions to any project.

## Exercises

1. **Pitch variation** — Add a random pitch offset to each one-shot trigger
   so repeated sounds feel less mechanical. You'll need to resample or adjust
   the cursor step rate.

2. **Pool priority** — When the pool is full, steal the oldest or
   quietest source instead of returning -1. Track a generation counter or
   compare remaining playback time.

3. **Exponential fade** — Replace the linear fade with an exponential curve
   (`fade_volume *= decay_per_second ^ dt`). Compare how the two feel for
   short vs. long fades.

4. **Per-source fade** — Add fade-in to every one-shot in the pool (e.g.
   5 ms fade-in) to eliminate all onset clicks, even for sounds that start
   with a nonzero sample.
