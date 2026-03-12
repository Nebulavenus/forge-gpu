# Lessons

Guided lesson tracks covering GPU programming, math, engine fundamentals,
UI, physics, audio, and asset pipeline development. Each track is a sequence
of self-contained programs that build on previous lessons.

## Tracks

| Track | Directory | Description |
|-------|-----------|-------------|
| GPU | `gpu/` | SDL GPU API -- rendering, pipelines, lighting, shadows, post-processing |
| Math | `math/` | Math fundamentals -- vectors, matrices, quaternions, projections, noise |
| Engine | `engine/` | Engine fundamentals -- C, CMake, debugging, shaders, memory |
| UI | `ui/` | Immediate-mode UI -- font parsing, atlas, text layout, controls, theming |
| Physics | `physics/` | Physics simulation -- particles, rigid bodies, collisions |
| Audio | `audio/` | Audio programming -- playback, mixing, spatial audio, DSP |
| Assets | `assets/` | Asset pipeline -- scanning, processing, bundles, materials, animations |

Each track has its own README with a full lesson list and track-specific
instructions. Every individual lesson directory contains a README explaining
what the lesson teaches and how to build and run it.

## Running a lesson

```bash
python scripts/run.py gpu/01      # by track and number
python scripts/run.py 01          # GPU lesson 01 (shorthand)
python scripts/run.py bloom       # by name fragment
```
