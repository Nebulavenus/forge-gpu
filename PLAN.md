# forge-gpu — Lesson Plan

## Completed

The following foundations, tooling, and lesson ranges are complete:

- **Foundation** — Project scaffolding, math library, test suite, skills
- **GPU Lessons 01–45** — From Hello Window through Scene Transparency Sorting
- **Math Lessons 01–18** — From Vectors through Scalar Field Gradients
- **Engine Lessons 01–13** — From Intro to C through Stretchy Containers
- **UI Lessons 01–15** — From TTF Parsing through Dev UI
- **Developer tooling** — Run script, shader compilation, setup script, screenshot capture
- **Physics Lessons 01–07** — From Point Particles through Collision Shapes
- **Audio Lessons 01–02** — Audio Basics and Sound Effects
- **Asset Lessons 01–13** — From Pipeline Scaffold through Morph Targets

## GPU Lessons — Remaining

### Advanced Rendering

- [ ] **Lesson 46 — Particle Animations** — Billboard quad particles facing the camera; GPU particle buffer updated via compute shader; spawn, simulate (gravity, drag, lifetime), and render loop; atlas-based animated particles; additive and soft-particle blending (depends on GPU Lessons 11, 16 and Physics Lesson 01)
- [ ] **Lesson 47 — Imposters** — Billboard LOD representations of complex meshes; baking an imposter atlas (multiple view angles); selecting the correct atlas frame based on view direction; cross-fading between imposter and full mesh; application to distant trees, props, and crowd rendering

### Advanced Materials & Effects

- [ ] **Lesson 48 — Translucent Materials** — Approximating light transmission through thin and thick surfaces; wrap lighting for subsurface scattering approximation; thickness maps; back-face lighting contribution; application to foliage, wax, skin, and fabric
- [ ] **Lesson 49 — Water Caustics** — Projecting animated caustic patterns onto underwater surfaces; caustic texture animation (scrolling, distortion); light attenuation with water depth; combining with existing lighting and shadow systems
- [ ] **Lesson 50 — IBL with Probes** — Image-based lighting using irradiance maps (diffuse) and pre-filtered environment maps (specular); split-sum approximation with a BRDF LUT; placing reflection probes in a scene; blending between probes; integrating IBL as ambient lighting replacement

### Volumetric & Terrain

- [ ] **Lesson 51 — Volumetric Fog** — Ray marching through participating media in a froxel grid or screen-space pass; Beer-Lambert absorption; in-scattering from lights with shadow map sampling; temporal reprojection for performance; combining volumetric fog with scene rendering
- [ ] **Lesson 52 — Grass with Animations & Imposters** — Dense grass field rendering; geometry instancing or compute-generated grass blades; wind animation using noise-based displacement; LOD transition from full blades to imposter cards at distance; terrain integration (depends on Lessons 13, 25, 46)
- [ ] **Lesson 53 — Height Map Terrain** — GPU terrain from height map; LOD with distance-based tessellation or geo-clipmaps; normal computation from height samples; texture splatting with blend maps; integrating with grass rendering

## Physics Lessons — New Track

A new header-only library (`common/physics/`) built lesson by lesson, covering
particle dynamics, rigid body simulation, collision detection, and contact
resolution. 15 lessons in four arcs — each lesson extends `forge_physics.h`
with tested, documented functions.

All physics lessons use `forge_scene.h` (GPU Lesson 40) for rendering — the
scene renderer provides Blinn-Phong lighting, shadow maps, grid floor, camera
controls, and UI panels out of the box. Physics lessons focus entirely on
simulation code; rendering is a single `#include` and a few function calls.

### Collision Detection

- [x] **Physics Lesson 08 — Sweep-and-Prune Broadphase** — Sort-and-sweep broadphase using L07's AABBs; axis projection and pair tracking; incremental updates for moving objects
- [x] **Physics Lesson 09 — GJK Intersection Testing** — Gilbert-Johnson-Keerthi algorithm for boolean intersection testing; Minkowski difference intuition; simplex evolution; support function interface from L07
- [ ] **Physics Lesson 10 — EPA Penetration Depth** — Expanding Polytope Algorithm for penetration depth and contact normal from a GJK simplex; polytope expansion and convergence; connecting EPA output to contact generation
- [ ] **Physics Lesson 11 — Contact Manifold** — Generating contact points from GJK/EPA results; contact point reduction (Sutherland-Hodgman clipping); manifold caching and persistent contact IDs across frames

### Rigid Body Dynamics

- [ ] **Physics Lesson 12 — Impulse-Based Resolution** — Computing collision impulses for linear and angular response; friction impulses; sequential impulse solver; position correction (Baumgarte stabilization or split impulses)
- [ ] **Physics Lesson 13 — Constraint Solver** — Generalized constraints (contact, friction, joints); iterative solver (Gauss-Seidel); joint types: hinge, ball-socket, slider; constraint warm-starting for stability
- [ ] **Physics Lesson 14 — Stacking Stability** — Warm-starting across frames; bias factors and penetration slop; solver iteration count tuning; stable box stacks and pyramids; visual debugging of contact points and normals
- [ ] **Physics Lesson 15 — Simulation Loop** — Complete physics step: broadphase, narrowphase, contact generation, constraint solving, integration; fixed timestep with interpolation; sleeping and island detection for performance

## Audio Lessons — New Track

A new header-only library (`common/audio/`) built lesson by lesson, covering
sound playback, mixing, spatial audio, and music systems. Uses SDL3 audio
streams as the backend. Same quality bar as `forge_physics.h` — every function
documented, tested, numerically safe.

All audio lessons use `forge_scene.h` (GPU Lesson 40) for rendering and UI —
the scene renderer provides the full 3D scene (lighting, shadows, grid, camera)
and initializes the forge UI system (font atlas, rendering pipeline, panel
layout). Audio lessons focus entirely on audio code; the UI panel and 3D scene
come free. If a lesson needs a UI widget that does not exist (waveform display,
VU meter, frequency plot), the widget is added to `common/ui/` as part of that
lesson.

### Fundamentals

- [x] **Audio Lesson 03 — Audio Mixing** — `ForgeAudioMixer` struct with fixed-size channel array and master bus; `forge_audio_mixer_create()`, `forge_audio_mixer_add_channel()`, `forge_audio_mixer_mix()`; per-channel volume, pan, mute, and solo; master volume with soft clipping (tanh saturation) to prevent hard clipping; peak-hold VU meter per channel via `forge_audio_channel_peak()`; new `forge_ui_vu_meter()` widget in `common/ui/`; UI mixer panel resembling a DAW channel strip — vertical faders, pan sliders, mute/solo toggles, stereo VU meters with peak indicators; demo scene with 4–6 simultaneous looping sources (drums, bass, melody, ambience, FX) that the user mixes live; audio files: 4–6 looping stems from a single track, same length so they stay in sync — user-supplied WAVs via `audio.conf`

### Spatial & Advanced

- [x] **Audio Lesson 04 — Spatial Audio** — `ForgeAudioListener` struct (position, forward, up, right from camera quaternion via `forge_audio_listener_from_camera()`); `ForgeAudioSpatialSource` wrapping `ForgeAudioSource*` with position, velocity, and distance parameters; distance attenuation models (linear, inverse-distance, exponential) selectable per source via `ForgeAudioAttenuationModel` enum; `forge_audio_spatial_pan()` for automatic stereo panning from 3D position relative to listener orientation; min/max distance and rolloff factor per source; Doppler pitch shift via `forge_audio_spatial_doppler()` using fractional-rate sample interpolation (`playback_rate` + `cursor_frac` fields on `ForgeAudioSource`); `forge_audio_spatial_apply()` computes attenuation, pan, and Doppler per source per frame; demo scene with 4 colored spheres orbiting the camera at radii 3/6/10/15, each emitting a distinct looping sound (wind, fan, alarm, steam); UI panel with attenuation model selector, Doppler toggle, master volume, per-source distance/gain/pan readout
- [x] **Audio Lesson 05 — Music and Streaming** — `ForgeAudioStream` struct for chunked file reading without loading the entire file into memory; ring buffer feeding the SDL audio stream in small blocks; `forge_audio_stream_open()`, `forge_audio_stream_update()` (called each frame to refill the ring buffer), `forge_audio_stream_close()`; crossfade between two streams with configurable overlap duration via `forge_audio_crossfade()`; loop-with-intro support — a stream plays an intro section once then loops the body indefinitely via start/end loop markers; adaptive music layers — multiple stems stream simultaneously, `forge_audio_layer_set_weight()` fades layers in/out based on game state (e.g. combat intensity slider in UI); demo scene with a jukebox-style UI: track list, play/pause/skip, crossfade duration slider, layer weight sliders, playback progress bar; audio files: 2–3 long music tracks (> 30s each) plus 2–3 layered stems of the same track — user-supplied WAVs via `audio.conf`
- [x] **Audio Lesson 06 — DSP Effects** — `ForgeAudioEffect` interface with `process(float *samples, int frames)` callback; built-in effects: biquad filter (low-pass, high-pass, band-pass with cutoff and resonance), delay line (echo with feedback and wet/dry mix), simple reverb (parallel comb filters + series allpass filters, Schroeder model), chorus (modulated delay with LFO); per-source effect chain via `forge_audio_source_add_effect()` and master bus effect chain via `forge_audio_mixer_add_effect()`; presets: "underwater" (low-pass 500 Hz + reverb), "cave" (long reverb + echo), "radio" (band-pass 800–3000 Hz); demo scene with a sound source and real-time effect controls; UI panel with effect chain list, per-effect parameter sliders (cutoff, resonance, delay time, feedback, wet/dry), preset buttons, and a bypass toggle per effect; audio files: 1–2 voice or music clips that showcase filter effects clearly — user-supplied WAVs via `audio.conf`

## Asset Pipeline — Remaining

A hybrid Python + C track. The pipeline is a **reusable Python library** at
`pipeline/` in the repo root (`pip install -e ".[dev]"`). Each lesson adds
real functionality to the shared package. Processing plugins that need
high-performance C libraries (meshoptimizer, MikkTSpace) are compiled C tools
invoked as subprocesses. Procedural geometry generation lives in a header-only
C library (`common/shapes/forge_shapes.h`).

### Project Integration

See [docs/PLAN-asset-integration.md](docs/PLAN-asset-integration.md) for the
full plan. Summary:

- [ ] Root `pipeline.toml` — BC7 albedo, BC5 normal maps, mesh optimization
- [ ] CMake `forge-assets` target — `add_dependencies(lesson_XX forge-assets)` for lessons 39+
- [ ] `.gitignore` — `.forge-cache/`, `assets/processed/`, `assets/bundles/`
- [ ] Process all existing models and textures through the pipeline
- [ ] GPU Lesson 08 README hint pointing to the asset pipeline track
- [ ] Update skills (dev-gpu-lesson, dev-physics-lesson, dev-final-pass, dev-create-pr) to mandate pipeline usage for lessons 39+
- [ ] CI integration — run real pipeline, publish pre-built assets as `assets-latest` release, add to merge gate

### Web Frontend

- [ ] **Asset Lesson 14 — Web UI Scaffold** — Embedded web server (Flask/FastAPI); static frontend with asset browser; listing processed assets with thumbnails; real-time build status via WebSocket
- [ ] **Asset Lesson 15 — Asset Preview** — 3D mesh preview with three.js or WebGPU; texture preview with zoom and channel isolation; material preview with lighting; side-by-side source vs. processed comparison
- [ ] **Asset Lesson 16 — Import Settings Editor** — Per-asset import configuration in the browser; texture compression quality, mesh LOD thresholds, atlas packing options; save settings and trigger re-import
- [ ] **Asset Lesson 17 — Scene Editor** — Visual scene composition: place, move, rotate, scale objects; save scene graph as JSON/glTF; integration with the C runtime for live preview; undo/redo with command pattern
