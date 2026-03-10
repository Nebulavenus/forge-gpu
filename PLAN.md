# forge-gpu — Lesson Plan

## Completed

The following foundations, tooling, and lesson ranges are complete:

- **Foundation** — Project scaffolding, math library, test suite, skills
- **GPU Lessons 01–38** — From Hello Window through Indirect Drawing
- **Math Lessons 01–15** — From Vectors through Bezier Curves
- **Engine Lessons 01–11** — From Intro to C through Git & Version Control
- **UI Lessons 01–13** — From TTF Parsing through Theming and Color System
- **Developer tooling** — Run script, shader compilation, setup script, screenshot capture
- **Asset Lessons 01–08** — From Pipeline Scaffold through Animations

## GPU Lessons — Remaining

### Advanced Rendering

- [x] **Lesson 38 — Indirect Drawing** — GPU-driven draw calls with `SDL_DrawGPUIndexedPrimitivesIndirect`; compute shader frustum culling fills indirect argument buffers; dual-camera split-screen debug visualization; per-object storage buffer transforms via instance vertex buffer pattern

### Asset Pipeline Integration

- [x] **Lesson 39 — Pipeline-Processed Assets** — Loading BC7 (albedo) and BC5 (normal map) compressed textures from the asset pipeline; reconstructing normals from two-channel BC5 (`z = sqrt(1 - x² - y²)`); loading optimized `.fmesh` files with tangents and LODs; CMake `forge-assets` build dependency; comparing raw vs. processed asset quality and load times. From this lesson onward, all assets come through the pipeline. (depends on Asset Lesson 06)

### Advanced Rendering (continued)

- [ ] **Lesson 40 — Particle Animations** — Billboard quad particles facing the camera; GPU particle buffer updated via compute shader; spawn, simulate (gravity, drag, lifetime), and render loop; atlas-based animated particles; additive and soft-particle blending (depends on GPU Lessons 11, 16 and Physics Lesson 01)
- [ ] **Lesson 41 — Imposters** — Billboard LOD representations of complex meshes; baking an imposter atlas (multiple view angles); selecting the correct atlas frame based on view direction; cross-fading between imposter and full mesh; application to distant trees, props, and crowd rendering

### Advanced Materials & Effects

- [ ] **Lesson 42 — Translucent Materials** — Approximating light transmission through thin and thick surfaces; wrap lighting for subsurface scattering approximation; thickness maps; back-face lighting contribution; application to foliage, wax, skin, and fabric
- [ ] **Lesson 43 — Water Caustics** — Projecting animated caustic patterns onto underwater surfaces; caustic texture animation (scrolling, distortion); light attenuation with water depth; combining with existing lighting and shadow systems
- [ ] **Lesson 44 — IBL with Probes** — Image-based lighting using irradiance maps (diffuse) and pre-filtered environment maps (specular); split-sum approximation with a BRDF LUT; placing reflection probes in a scene; blending between probes; integrating IBL as ambient lighting replacement

### Volumetric & Terrain

- [ ] **Lesson 45 — Volumetric Fog** — Ray marching through participating media in a froxel grid or screen-space pass; Beer-Lambert absorption; in-scattering from lights with shadow map sampling; temporal reprojection for performance; combining volumetric fog with scene rendering
- [ ] **Lesson 46 — Grass with Animations & Imposters** — Dense grass field rendering; geometry instancing or compute-generated grass blades; wind animation using noise-based displacement; LOD transition from full blades to imposter cards at distance; terrain integration (depends on Lessons 13, 25, 40)
- [ ] **Lesson 47 — Height Map Terrain** — GPU terrain from height map; LOD with distance-based tessellation or geo-clipmaps; normal computation from height samples; texture splatting with blend maps; integrating with grass rendering

## UI Lessons — Remaining

### Application Patterns

- [x] **UI Lesson 14 — Game UI** — Health bars, inventories, HUD elements, menus; game-specific patterns using the immediate-mode controls from earlier lessons; fixed and proportional layout for different screen sizes
- [x] **UI Lesson 15 — Dev UI** — Property editors, debug overlays, console, performance graphs; developer-facing tools for inspecting game state; collapsible sections and tree views

## Physics Lessons — New Track

A new header-only library (`common/physics/`) built lesson by lesson, covering
particle dynamics, rigid body simulation, collision detection, and contact
resolution.

### Particle Dynamics

- [ ] **Physics Lesson 01 — Point Particles** — Position, velocity, acceleration; symplectic Euler integration; gravity and drag forces; `forge_physics_` API scaffolding in `common/physics/forge_physics.h`
- [ ] **Physics Lesson 02 — Springs and Constraints** — Hooke's law spring forces; damped springs; distance constraints with projection; chain and cloth-like particle systems
- [ ] **Physics Lesson 03 — Particle Collisions** — Sphere-sphere and sphere-plane collision detection; impulse-based response; coefficient of restitution; spatial partitioning for broadphase (uniform grid)

### Rigid Body Foundations

- [ ] **Physics Lesson 04 — Rigid Body State** — Mass, center of mass, inertia tensor; linear and angular velocity; state representation and integration; torque and angular acceleration
- [ ] **Physics Lesson 05 — Orientation and Angular Motion** — Quaternion representation for orientation; angular velocity integration; inertia tensor rotation to world space; gyroscopic stability
- [ ] **Physics Lesson 06 — Forces and Torques** — Applying forces at arbitrary points; gravity, drag, and friction as force generators; force accumulator pattern; combining linear and angular effects

### Collision Detection

- [ ] **Physics Lesson 07 — Collision Shapes** — Sphere, AABB, OBB, capsule, convex hull representations; support functions for each shape; broadphase with bounding volume hierarchy (BVH) or sweep-and-prune
- [ ] **Physics Lesson 08 — Narrow Phase: GJK and EPA** — Gilbert-Johnson-Keerthi algorithm for intersection testing; Expanding Polytope Algorithm for penetration depth and contact normal; Minkowski difference intuition
- [ ] **Physics Lesson 09 — Contact Manifold** — Generating contact points from GJK/EPA results; contact point reduction (clipping); manifold caching and warm-starting across frames; persistent contact IDs

### Rigid Body Dynamics

- [ ] **Physics Lesson 10 — Impulse-Based Resolution** — Computing collision impulses for linear and angular response; friction impulses (Coulomb model); sequential impulse solver; position correction (Baumgarte stabilization or split impulses)
- [ ] **Physics Lesson 11 — Constraint Solver** — Generalized constraints (contact, friction, joints); iterative solver (Gauss-Seidel); joint types: hinge, ball-socket, slider; constraint warm-starting for stability
- [ ] **Physics Lesson 12 — Simulation Loop** — Complete physics step: broadphase, narrowphase, contact generation, constraint solving, integration; fixed time-step with interpolation; sleeping and island detection for performance

## Audio Lessons — New Track

A new header-only library (`common/audio/`) built lesson by lesson, covering
sound playback, mixing, spatial audio, and music systems. Uses SDL3 audio
streams as the backend.

### Fundamentals

- [ ] **Audio Lesson 01 — Audio Basics** — PCM audio fundamentals; sample rate, bit depth, channels; loading WAV files; playing a sound with SDL audio streams; `forge_audio_` API scaffolding in `common/audio/forge_audio.h`
- [ ] **Audio Lesson 02 — Sound Effects** — Triggering one-shot and looping sounds; managing multiple concurrent audio streams; volume control and fade in/out; fire-and-forget playback API
- [ ] **Audio Lesson 03 — Audio Mixing** — Combining multiple audio sources into a single output; per-channel volume and panning; master volume; clipping prevention and normalization

### Spatial & Advanced

- [ ] **Audio Lesson 04 — Spatial Audio** — Distance-based attenuation (linear, inverse, exponential); stereo panning from 3D position; Doppler effect; listener orientation and position
- [ ] **Audio Lesson 05 — Music and Streaming** — Streaming large audio files from disk (OGG/MP3 decoding); crossfading between tracks; looping with intro sections; adaptive music layers that respond to game state
- [ ] **Audio Lesson 06 — DSP Effects** — Low-pass and high-pass filters; reverb (simple delay-line); echo and chorus; applying effects per-source and on the master bus; underwater/muffled presets

## Asset Pipeline — New Track

A hybrid Python + C track. The pipeline is a **reusable Python library** at
`pipeline/` in the repo root (`pip install -e ".[dev]"`). Each lesson adds
real functionality to the shared package. Processing plugins that need
high-performance C libraries (meshoptimizer, MikkTSpace) are compiled C tools
invoked as subprocesses. Procedural geometry generation lives in a header-only
C library (`common/shapes/forge_shapes.h`).

### Core Pipeline

- [x] **Asset Lesson 05 — Asset Bundles** — Packing multiple processed assets into bundle files; table of contents with offsets for random access; compression (zstd); dependency tracking between assets
- [x] **Asset Lesson 06 — Loading Processed Assets in C** — Reading `.fmesh` binary format and BC7/BC5 compressed textures from `assets/processed/`; header-only C loader (`common/pipeline/forge_pipeline.h`); CMake `forge-assets` target as a build dependency for GPU lessons 09+; integration test rendering a pipeline-processed model with normal mapping
- [x] **Asset Lesson 07 — Materials** — Full PBR material support: glTF parser extensions for materials, multi-primitive mesh processing, .fmesh v2 submesh table, .fmat material sidecars
- [x] **Asset Lesson 08 — Animations** — glTF animation parsing in `forge_gltf.h` and runtime evaluation in `forge_gltf_anim.h`: channels, samplers, binary search keyframes, lerp/slerp interpolation, looping, TRS application to nodes

### Project Integration

See [docs/PLAN-asset-integration.md](docs/PLAN-asset-integration.md) for the
full plan. Summary:

- [ ] Root `pipeline.toml` — BC7 albedo, BC5 normal maps, mesh optimization
- [ ] CMake `forge-assets` target — `add_dependencies(lesson_XX forge-assets)` for lessons 39+
- [ ] `.gitignore` — `.forge-cache/`, `assets/processed/`, `assets/bundles/`
- [ ] Process all existing models and textures through the pipeline
- [ ] GPU Lesson 08 README hint pointing to the asset pipeline track
- [x] GPU Lesson 39 teaches loading pipeline-processed assets (BC7/BC5, .fmesh); pipeline mandate starts here
- [ ] Update skills (dev-new-lesson, dev-physics-lesson, dev-final-pass, dev-publish-lesson) to mandate pipeline usage for lessons 39+
- [ ] CI integration — run real pipeline, publish pre-built assets as `assets-latest` release, add to merge gate

### Web Frontend

- [ ] **Asset Lesson 09 — Web UI Scaffold** — Embedded web server (Flask/FastAPI); static frontend with asset browser; listing processed assets with thumbnails; real-time build status via WebSocket
- [ ] **Asset Lesson 10 — Asset Preview** — 3D mesh preview with three.js or WebGPU; texture preview with zoom and channel isolation; material preview with lighting; side-by-side source vs. processed comparison
- [ ] **Asset Lesson 11 — Import Settings Editor** — Per-asset import configuration in the browser; texture compression quality, mesh LOD thresholds, atlas packing options; save settings and trigger re-import
- [ ] **Asset Lesson 12 — Scene Editor** — Visual scene composition: place, move, rotate, scale objects; save scene graph as JSON/glTF; integration with the C runtime for live preview; undo/redo with command pattern
