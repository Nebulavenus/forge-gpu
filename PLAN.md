# forge-gpu — Lesson Plan

## Completed

The following foundations, tooling, and lesson ranges are complete:

- **Foundation** — Project scaffolding, math library, test suite, skills
- **GPU Lessons 01–41** — From Hello Window through Scene Model Loading
- **Math Lessons 01–17** — From Vectors through Implicit 2D Curves
- **Engine Lessons 01–12** — From Intro to C through Memory Arenas
- **UI Lessons 01–15** — From TTF Parsing through Dev UI
- **Developer tooling** — Run script, shader compilation, setup script, screenshot capture
- **Asset Lessons 01–09** — From Pipeline Scaffold through Scene Hierarchy

## GPU Lessons — Remaining

### Reusable Scene Infrastructure

- [x] **Lesson 40 — Scene Renderer** — A reusable `common/scene/forge_scene.h` header-only library that packages the rendering stack lessons 01–39 built piece by piece: SDL GPU device/window/swapchain setup, depth texture management with auto-resize, directional shadow map with PCF sampling, Blinn-Phong lighting uniforms, procedural grid floor, quaternion FPS camera with mouse/keyboard input, shader creation from SPIRV/DXIL bytecode, GPU buffer upload helpers, and forge UI initialization with font atlas + rendering pipeline. One `forge_scene_init()` call replaces 500–600 lines of boilerplate per lesson. The lesson itself demonstrates the library by rendering a lit scene with shadows, grid, UI panel, and camera controls in under 200 lines of application code. This is the foundation that physics, audio, and all future GPU lessons build on — agents writing those lessons include one header and focus entirely on the lesson's subject matter, not rendering plumbing.

### Scene Model Loading

- [x] **Lesson 41 — Scene Model Loading** — Extends `forge_scene.h` with pipeline model rendering: `ForgeSceneModel` struct with GPU buffers, per-material textures, and `ForgePipelineScene` node hierarchy; `forge_scene_load_model()` loads `.fscene` + `.fmesh` + `.fmat`, uploads buffers, loads textures with fallbacks; `forge_scene_draw_model()` traverses nodes, binds per-primitive materials, selects pipeline variant (opaque/blend/double-sided); `forge_scene_draw_model_shadows()` for depth-only pass; three models (CesiumMilkTruck with mesh instancing, Suzanne with PBR textures, Duck). (depends on GPU Lessons 39, 40 and Asset Lesson 09)

### Pipeline Texture & Animation

- [x] **Lesson 42 — Pipeline Texture Compression** — Adds GPU block-compressed texture loading to `forge_scene.h`: processes model textures through the Python pipeline's texture plugin (BC7 for color/emissive via Basis Universal, BC5 for normal maps); loads KTX2 containers with pre-computed mip chains; uploads compressed blocks directly to GPU without CPU decoding; `forge_scene_load_pipeline_texture()` detects `.meta.json` sidecars and uses pre-made mips instead of GPU blit chain; compares VRAM usage and load times between raw PNG and compressed paths. (depends on GPU Lesson 41 and Asset Lesson 02)
- [x] **Lesson 43 — Pipeline Skinned Animations** — Extends `forge_scene.h` with skeletal animation support through pipeline assets: loads `.fskin` (joint hierarchy + inverse bind matrices), `.fanim` (keyframe data), and `.fanims` (animation manifest); computes joint matrices per frame; adds a skinned vertex shader (72-byte vertices with joints/weights); `ForgeSceneSkinnedModel` struct with joint buffer and animation state; `forge_scene_draw_skinned_model()` and `forge_scene_update_animation()`; demonstrates with CesiumMan walking in a scene. (depends on GPU Lesson 41, Asset Lessons 10–12)
- [ ] **Lesson 44 — Pipeline Morph Target Animations** — Extends `forge_scene.h` with morph target (blend shape) support through pipeline assets: loads `.fmorph` (morph target deltas for position, normal, tangent); evaluates morph weights from `.fanim` keyframes on the CPU or via compute shader; uploads per-vertex displacement deltas to a GPU storage buffer; vertex shader blends base mesh with weighted morph targets; `ForgeSceneMorphModel` struct with morph buffer and weight state; demonstrates with AnimatedMorphCube and AnimatedMorphSphere from glTF sample assets; combines with skinning for facial animation use cases. (depends on GPU Lesson 43)

### Scene Transparency

- [ ] **Lesson 45 — Scene Transparency Sorting** — Extends `forge_scene.h` with correct transparent rendering: two-pass draw splitting opaque and blend submeshes; back-to-front depth sorting for transparent draws; alpha-tested shadow pass that skips `ALPHA_MASK` materials (and optionally uses a mask-aware shadow shader with `alpha_cutoff`); `ForgeSceneTransparentDraw` queue with centroid depth; integrates with `forge_scene_draw_model()` / `forge_scene_draw_model_shadows()`; demonstrates order-dependent vs order-independent results with CesiumMilkTruck windshield and foliage test assets. (depends on GPU Lessons 16, 41)

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
resolution. 14 lessons in five arcs — each lesson extends `forge_physics.h`
with tested, documented functions.

All physics lessons use `forge_scene.h` (GPU Lesson 40) for rendering — the
scene renderer provides Blinn-Phong lighting, shadow maps, grid floor, camera
controls, and UI panels out of the box. Physics lessons focus entirely on
simulation code; rendering is a single `#include` and a few function calls.

### Particle Dynamics

- [x] **Physics Lesson 01 — Point Particles** — Position, velocity, acceleration; symplectic Euler integration; gravity and drag forces; `forge_physics_` API scaffolding in `common/physics/forge_physics.h`
- [x] **Physics Lesson 02 — Springs and Constraints** — Hooke's law spring forces; damped springs; distance constraints with projection; chain and cloth-like particle systems
- [x] **Physics Lesson 03 — Particle Collisions** — Sphere-sphere and sphere-plane collision detection; impulse-based response; coefficient of restitution; simple O(n²) all-pairs (broadphase deferred to Lesson 07)

### Rigid Body Foundations

- [x] **Physics Lesson 04 — Rigid Body State and Orientation** — Mass, center of mass, inertia tensor; quaternion orientation; linear and angular velocity; state representation and integration; inertia tensor rotation to world space
- [x] **Physics Lesson 05 — Forces and Torques** — Applying forces at arbitrary points; gravity, drag, and friction as force generators; torque and angular acceleration; force accumulator pattern; combining linear and angular effects; gyroscopic stability
- [x] **Physics Lesson 06 — Resting Contacts and Friction** — Plane contact detection for boxes and spheres; sphere-sphere body-body collision detection; static and dynamic friction (Coulomb model); resting contact resolution; sphere stacking with body-body collisions; **first lesson with forge UI overlay** — sliders for friction/restitution/solver iterations, energy and contact count readouts

### Collision Detection

- [ ] **Physics Lesson 07 — Collision Shapes and Broadphase** — Sphere, AABB, OBB, capsule, convex hull representations; support functions for each shape; broadphase with sweep-and-prune or BVH; pairing broadphase with simple narrowphase from earlier lessons
- [ ] **Physics Lesson 08 — GJK Intersection Testing** — Gilbert-Johnson-Keerthi algorithm for boolean intersection testing; Minkowski difference intuition; simplex evolution; support function interface for convex shapes
- [ ] **Physics Lesson 09 — EPA Penetration Depth** — Expanding Polytope Algorithm for penetration depth and contact normal from a GJK simplex; polytope expansion and convergence; connecting EPA output to contact generation
- [ ] **Physics Lesson 10 — Contact Manifold** — Generating contact points from GJK/EPA results; contact point reduction (Sutherland-Hodgman clipping); manifold caching and persistent contact IDs across frames

### Rigid Body Dynamics

- [ ] **Physics Lesson 11 — Impulse-Based Resolution** — Computing collision impulses for linear and angular response; friction impulses; sequential impulse solver; position correction (Baumgarte stabilization or split impulses)
- [ ] **Physics Lesson 12 — Constraint Solver** — Generalized constraints (contact, friction, joints); iterative solver (Gauss-Seidel); joint types: hinge, ball-socket, slider; constraint warm-starting for stability
- [ ] **Physics Lesson 13 — Stacking Stability** — Warm-starting across frames; bias factors and penetration slop; solver iteration count tuning; stable box stacks and pyramids; visual debugging of contact points and normals
- [ ] **Physics Lesson 14 — Simulation Loop** — Complete physics step: broadphase, narrowphase, contact generation, constraint solving, integration; fixed timestep with interpolation; sleeping and island detection for performance

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

- [ ] **Audio Lesson 01 — Audio Basics** — PCM audio fundamentals; sample rate, bit depth, channels; loading WAV files; playing a sound with SDL audio streams; `forge_audio_` API scaffolding in `common/audio/forge_audio.h`; forge UI panel with play/pause and volume slider
- [ ] **Audio Lesson 02 — Sound Effects** — Triggering one-shot and looping sounds; managing multiple concurrent audio streams; volume control and fade in/out; fire-and-forget playback API; UI panel showing active source list with per-source volume
- [ ] **Audio Lesson 03 — Audio Mixing** — Combining multiple audio sources into a single output; per-channel volume and panning; master volume; clipping prevention and normalization; UI mixer panel with per-channel sliders and VU meters

### Spatial & Advanced

- [ ] **Audio Lesson 04 — Spatial Audio** — Distance-based attenuation (linear, inverse, exponential); stereo panning from 3D position; Doppler effect; listener orientation and position; 3D scene with visible sound source markers; camera position as listener
- [ ] **Audio Lesson 05 — Music and Streaming** — Streaming large audio files from disk (OGG/MP3 decoding); crossfading between tracks; looping with intro sections; adaptive music layers that respond to game state; UI track selector with crossfade controls
- [ ] **Audio Lesson 06 — DSP Effects** — Low-pass and high-pass filters; reverb (simple delay-line); echo and chorus; applying effects per-source and on the master bus; underwater/muffled presets; UI panel with filter cutoff sliders and wet/dry mix

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
- [ ] Update skills (dev-new-lesson, dev-physics-lesson, dev-final-pass, dev-publish-lesson) to mandate pipeline usage for lessons 39+
- [ ] CI integration — run real pipeline, publish pre-built assets as `assets-latest` release, add to merge gate

### Skinned Animation Pipeline

- [x] **Asset Lesson 10 — Animation Loader and Per-Clip Export** — Runtime `.fanim` loader in `forge_pipeline.h`; per-clip export via `--split` flag in `forge-anim-tool`; `.fanims` stub manifest; animation pipeline plugin defaults to split mode
- [x] **Asset Lesson 11 — Animation Manifest and Named Lookup** — `.fanims` JSON manifest with loop flags, tags, and named clip lookup; `forge_pipeline_load_anim_set`, `forge_pipeline_find_clip`, and `forge_pipeline_load_clip` API; manifest generation with tags in `forge-anim-tool`
- [x] **Asset Lesson 12 — Skin Data and Skinned Vertices** — `.fskin` binary format for joint hierarchies and inverse bind matrices; skinned vertex support in `.fmesh` v3; `forge_pipeline_load_skins` API; mesh tool and scene tool extensions

### Web Frontend

- [ ] **Asset Lesson 13 — Web UI Scaffold** — Embedded web server (Flask/FastAPI); static frontend with asset browser; listing processed assets with thumbnails; real-time build status via WebSocket
- [ ] **Asset Lesson 14 — Asset Preview** — 3D mesh preview with three.js or WebGPU; texture preview with zoom and channel isolation; material preview with lighting; side-by-side source vs. processed comparison
- [ ] **Asset Lesson 15 — Import Settings Editor** — Per-asset import configuration in the browser; texture compression quality, mesh LOD thresholds, atlas packing options; save settings and trigger re-import
- [ ] **Asset Lesson 16 — Scene Editor** — Visual scene composition: place, move, rotate, scale objects; save scene graph as JSON/glTF; integration with the C runtime for live preview; undo/redo with command pattern
