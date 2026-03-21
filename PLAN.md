# forge-gpu — Lesson Plan

## Libraries

Header-only C99 libraries in `common/`, grown incrementally through the lesson
tracks. Suitable for learning, prototyping, and small-to-medium 3D games
(arena shooters, puzzle games, platformers with discrete levels). The practical
ceiling is PS2/early-PS3 era complexity — fixed or semi-open environments, tens
of unique meshes, up to ~100 rigid bodies, moderate draw counts.

| Library | Path | What it provides |
|---------|------|------------------|
| **Math** | `common/math/` | vec2/3/4, mat4, quaternions, transforms, projections |
| **Arena** | `common/arena/` | Bump allocator — fast alloc, bulk free |
| **Containers** | `common/containers/` | Dynamic arrays, hash maps (fat-pointer pattern) |
| **OBJ** | `common/obj/` | Wavefront .obj parser (simple models) |
| **glTF** | `common/gltf/` | glTF 2.0 — scenes, materials, hierarchy, skinning, morph targets |
| **UI** | `common/ui/` | Immediate-mode: TTF parsing, atlas, layout, controls, windows |
| **Physics** | `common/physics/` | Particles, rigid bodies, GJK/EPA collisions, constraints |
| **Audio** | `common/audio/` | Playback, mixing, spatial audio, DSP effects (SDL3 streams) |
| **Shapes** | `common/shapes/` | Procedural geometry: sphere, torus, capsule, cube, etc. |
| **Scene** | `common/scene/` | One-call renderer: shadows, Blinn-Phong, grid, sky, camera, UI |
| **Pipeline** | `common/pipeline/` | Runtime loader for .fmesh and compressed textures |
| **Raster** | `common/raster/` | CPU triangle rasterizer (edge function method) |
| **Capture** | `common/capture/` | Screenshot and GIF capture |
| **AI** | `common/ai/` | Steering, boids, pathfinding (A*, navmesh), FSMs, behavior trees, GOAP |
| **Anim** | `common/anim/` | Blending, layering, blend trees, state machines, IK, root motion |
| **ECS** | `common/ecs/` | Entity-component-system: archetypes, sparse sets, parallel systems |

The **Python asset pipeline** (`pipeline/`, pip-installable) handles offline
processing: scanning, fingerprinting, texture compression, mesh optimization.
C tools in `tools/` do the heavy lifting (meshoptimizer, MikkTSpace, basisu).

### What would outgrow these libraries

- Open worlds (no streaming, spatial partitioning)
- Large-scale physics (deep stacking, broad contact graphs)
- Networking
- Runtime asset streaming, virtual texturing

These are infrastructure gaps, not architectural problems — the libraries could
be extended toward larger projects without a full rewrite.

## Completed

The following foundations, tooling, and lesson ranges are complete:

- **Foundation** — Project scaffolding, math library, test suite, skills
- **GPU Lessons 01–46** — From Hello Window through Particle Animations
- **Math Lessons 01–18** — From Vectors through Scalar Field Gradients
- **Engine Lessons 01–13** — From Intro to C through Stretchy Containers
- **UI Lessons 01–15** — From TTF Parsing through Dev UI
- **Developer tooling** — Run script, shader compilation, setup script, screenshot capture
- **Physics Lessons 01–10** — From Point Particles through EPA Penetration Depth
- **Audio Lessons 01–06** — From Audio Basics through DSP Effects
- **Asset Lessons 01–13** — From Pipeline Scaffold through Morph Targets

## GPU Lessons — Remaining

Lessons 48–49 and 51–57 have no cross-track blockers. The **Terrain &
Vegetation** arc (Lessons 47, 50) depends on Asset Lessons 18–19 (procedural
textures). The **Particle Effects** arc (Lessons 58–59) is blocked by Asset
Lesson 20 (particle effect definitions).

### Terrain & Vegetation

- [ ] **Lesson 47 — Height Map Terrain** — GPU terrain from pipeline-generated noise heightmaps (Asset Lesson 18); vertex displacement from R16 heightmap in the vertex shader; normal computation from height samples via central differences; texture splatting with slope- and height-based blend maps (rock, dirt, grass); chunked terrain mesh with distance-based LOD (geo-clipmaps or quadtree); scattered tree placement using density maps derived from terrain slope and height; tree and grass rendering as instanced alpha-tested billboards using pipeline-generated vegetation textures (Asset Lesson 19), full grass animation in Lesson 50 (depends on GPU Lessons 13, 25 and Asset Lessons 18–19)
- [ ] **Lesson 48 — Imposters** — Billboard LOD representations of complex meshes; baking an imposter atlas (multiple view angles) to an offscreen render target; selecting the correct atlas frame based on view direction; cross-fading between imposter and full mesh; application to distant trees, props, and crowd rendering
- [ ] **Lesson 49 — Level of Detail** — Distance-based LOD system for mesh rendering; discrete LOD with multiple mesh detail levels and screen-size switching thresholds; continuous LOD with cross-fade (alpha dithering) to hide pop-in; compute-based LOD selection using camera distance and bounding sphere screen coverage; integrating imposters from Lesson 48 as the lowest LOD tier; LOD bias and hysteresis to prevent rapid switching; application to the terrain scene — full tree meshes up close, simplified meshes at mid range, imposters at distance (depends on Lessons 13, 38, 47, 48)
- [ ] **Lesson 50 — Grass Rendering** — Dense grass on the height map terrain from Lesson 47; begins with instanced alpha-tested billboard quads using pipeline-generated grass blade atlas textures (Asset Lesson 19); then adds compute-generated geometry blades with per-vertex deformation — base anchored, tip displaced by layered noise wind; per-blade normals for correct lighting; LOD transition from geometry blades (near) to billboard quads (mid) to culled (far) using the LOD system from Lesson 49; density controlled by terrain slope and height maps; frustum and distance culling via compute shader (depends on GPU Lessons 13, 16, 25, 47, 49 and Asset Lesson 19)

### PBR Introduction

- [ ] **Lesson 51 — PBR Shading Model** — Cook-Torrance microfacet BRDF; GGX normal distribution, Schlick-GGX geometry, Schlick Fresnel; metallic-roughness workflow; energy conservation; side-by-side comparison with Blinn-Phong; demo with material parameter sliders (metallic, roughness, base color)
- [ ] **Lesson 52 — PBR Textures** — Albedo, metallic-roughness, normal, AO, and emissive maps; glTF PBR material loading; sRGB vs. linear color space and gamma correction; demo rendering a glTF model with full PBR texture set
- [ ] **Lesson 53 — IBL and Environment Lighting** — Image-based lighting as PBR ambient: irradiance maps (diffuse), pre-filtered environment maps (specular), BRDF integration LUT; split-sum approximation; combining IBL with direct PBR lighting; reflection probes and blending between probes
- [ ] **Lesson 54 — PBR in forge_scene.h** — Adding PBR as a lighting mode alongside Blinn-Phong in the scene renderer; `ForgeSceneLightingMode` enum; PBR shader variants; IBL fallback when no environment map is set; all subsequent lessons and demos can opt into PBR with one flag

### Advanced Materials & Effects

- [ ] **Lesson 55 — Translucent Materials** — Approximating light transmission through thin and thick surfaces; wrap lighting for subsurface scattering approximation; thickness maps; back-face lighting contribution; application to foliage, wax, skin, and fabric (works with both Blinn-Phong and PBR paths)
- [ ] **Lesson 56 — Water Caustics** — Projecting animated caustic patterns onto underwater surfaces; caustic texture animation (scrolling, distortion); light attenuation with water depth; combining with existing lighting and shadow systems

### Volumetric

- [ ] **Lesson 57 — Volumetric Fog** — Ray marching through participating media in a froxel grid or screen-space pass; Beer-Lambert absorption; in-scattering from lights with shadow map sampling; temporal reprojection for performance; combining volumetric fog with terrain and vegetation scene from Lessons 47–50

### Particle Effects (depends on Asset Lesson 20)

- [ ] **Lesson 58 — Data-Driven Particle Effects** — Emitter pool with shared GPU particle buffer; loading .fpart effect definitions at runtime; parameter-driven compute simulation (one shader, many effects); multi-effect rendering batched by blend mode; spawning effects from gameplay events
- [ ] **Lesson 59 — Particle Ribbons & Trails** — Generating ribbon geometry from particle position history; catmull-rom smoothing; UV scrolling along trail length; application to sword swings, missile trails, magic effects

### Visibility & Culling

- [ ] **Lesson 60 — Occlusion Culling** — GPU-driven occlusion culling using hierarchical z-buffer (Hi-Z); downsampled depth pyramid from the previous frame; compute shader tests object bounding boxes against the Hi-Z pyramid to discard fully occluded objects before drawing; two-phase rendering (occluder pass with large objects, then Hi-Z test for the rest); integration with indirect drawing from Lesson 38; comparison of draw call counts and frame time with and without occlusion culling; application to dense scenes with many hidden objects (depends on Lessons 13, 25, 38)
- [ ] **Lesson 61 — Portal-Based Visibility** — Visibility determination for indoor environments using portals; defining portal polygons between rooms/sectors; frustum narrowing through portal sequences; recursive portal traversal with shrinking clip regions; sector-based scene organization; rendering only visible sectors and their contents; anti-portal support for large occluders; debug visualization of portal frustums and sector visibility; application to a multi-room interior scene (depends on Lessons 13, 38)

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
- [x] **Physics Lesson 10 — EPA Penetration Depth** — Expanding Polytope Algorithm for penetration depth and contact normal from a GJK simplex; polytope expansion and convergence; connecting EPA output to contact generation
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

### Dependency chain

Some later lessons across tracks depend on authored asset formats that need the
web editor. The order is:

1. **Web Frontend** (Asset Lessons 14–17) — editor for authoring effect
   definitions, import settings, scene composition, and navmesh editing
2. **Procedural Textures** (Asset Lessons 18–19) — noise heightmaps and
   vegetation textures for the terrain/vegetation GPU arc
3. **Effect & Asset Authoring** (Asset Lessons 20–22) — particle effect
   definitions, animation event data, material definitions — all need the
   editor
4. **Navigation Mesh Tooling** (Asset Lessons 23–24) — navmesh generation
   and editing — also needs the web editor
5. **Downstream consumers** — GPU Lessons 58–59 (particle effects), Anim
   Lesson 09 (animation events), AI Lessons 05–06 (navmesh pathfinding)

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

### Web Frontend (blocks Effect & Asset Authoring)

**Stack:** FastAPI backend (Python) serving a Vite + React + TypeScript SPA.
TanStack Router and Query for routing and data fetching. shadcn/ui + Tailwind
for UI components. Authored content (scenes, materials, effects) persisted as
JSON/TOML files — no database. react-three-fiber for 3D preview (Lesson 15),
reactflow for node-graph editing (Lessons 20+).

- [x] **Asset Lesson 14 — Web UI Scaffold** — FastAPI backend with REST endpoints and WebSocket for build status; Vite + React + TypeScript frontend scaffolded with shadcn/ui and Tailwind; TanStack Router for navigation, TanStack Query for data fetching; asset browser listing processed assets with thumbnails and filtering by type/status; real-time pipeline build status via WebSocket; OpenAPI schema generation from FastAPI with TypeScript type codegen
- [x] **Asset Lesson 15 — Asset Preview** — react-three-fiber 3D mesh preview with orbit controls and Blinn-Phong lighting; texture preview with zoom, pan, and channel isolation (R/G/B/A/RGB); material preview showing all texture maps applied; side-by-side source vs. processed comparison
- [ ] **Asset Lesson 16 — Import Settings Editor** — Per-asset import configuration stored as TOML sidecar files; texture compression quality, mesh LOD thresholds, atlas packing options; form UI with shadcn/ui controls; save settings and trigger pipeline re-processing; TanStack Query cache invalidation on rebuild
- [ ] **Asset Lesson 17 — Scene Editor** — Visual scene composition with react-three-fiber viewport; place, move, rotate, scale objects with transform gizmos; scene hierarchy panel; scene saved as JSON files; undo/redo with command pattern using React state

### Procedural Textures

- [ ] **Asset Lesson 18 — Noise Texture Generation** — Pipeline plugin for generating procedural noise textures (Perlin, fBm, ridged multi-fractal, Worley); configurable resolution, octaves, frequency, persistence, and seed via `pipeline.toml`; outputs 16-bit heightmaps (R16 PNG or raw) and 8-bit RGBA noise maps; normal map derivation from heightmap gradients; BC5-compressed normal map output and BC4-compressed heightmap output; tiling support for seamless textures; integration with GPU Lesson 47 (Height Map Terrain) as the heightmap source; Python noise generation with NumPy; preview thumbnails in the pipeline cache (depends on Asset Lessons 01–02)
- [ ] **Asset Lesson 19 — Vegetation Texture Generation** — Pipeline plugin for generating procedural tree and grass textures in Python; tree bark textures via layered fBm with vertical grain bias and color variation; tree canopy/leaf cluster textures with randomized leaf shapes, vein detail, and seasonal color palettes; grass blade atlas with multiple blade shapes, width variation, and tip taper; alpha masks for alpha-tested rendering; all textures tileable where appropriate; color and shape parameters exposed in `pipeline.toml` for rapid iteration; BC7-compressed RGBA output with pre-multiplied alpha; generates texture atlases ready for instanced billboard rendering in GPU Lessons 47 and 50 (depends on Asset Lesson 18)

### Effect & Asset Authoring (blocks GPU Lessons 58–59)

- [ ] **Asset Lesson 20 — Particle Effect Definitions** — Data-driven particle effect format (.fpart); emission shape, spawn rate, forces, color/size curves, atlas region, blend mode; reactflow node-graph editor for authoring effects; pipeline plugin to validate and compile effect files; runtime loader in `common/pipeline/`
- [ ] **Asset Lesson 21 — Animation Event Data** — Authored event tracks (.faevt) attached to animation clips; frame-triggered events (sounds, particles, hitboxes); editor timeline UI for placing events; pipeline processing and runtime loading (connects to Anim Lesson 09)
- [ ] **Asset Lesson 22 — Material Definitions** — Data-driven material format (.fmat2) beyond per-mesh materials; texture references, shader parameters, render state; reactflow node-graph material editor with live preview; pipeline validation and compilation

### Navigation Mesh Tooling (blocks AI Lessons 05–06)

- [ ] **Asset Lesson 23 — NavMesh Generation** — C tool in `tools/nav/` that voxelizes scene collision geometry and builds a navigation mesh (heightfield → compact → contours → polygon mesh); `.fnav` binary format; pipeline plugin for batch processing; runtime loader in `common/pipeline/`
- [ ] **Asset Lesson 24 — NavMesh Editor** — Visual navmesh editing in the web UI; display auto-generated mesh overlaid on the 3D scene; add/remove/reshape polygons; mark areas (walkable, blocked, jump, door); set traversal costs per area; save edits as a layer on top of the generated mesh; re-bake workflow (edit → regenerate → re-apply layer)

## Game AI Lessons — Future Track

A header-only library (`common/ai/`) covering classical game AI techniques.
Each lesson builds a reusable system rendered with `forge_scene.h`. Library
grows incrementally like physics and audio — documented, tested, visual.

**Cross-track dependencies:** Steering and pathfinding lessons use physics
for collision queries and ground detection. NavMesh lessons (05–06) can use
hand-built meshes initially but benefit from the pipeline navmesh tools
(Asset Lessons 23–24) for authored levels. Decision-making lessons integrate
with the Animation track for state-driven animation transitions.

### Steering & Movement

- [ ] **AI Lesson 01 — Steering Behaviors** — Seek, flee, arrive, wander; Craig Reynolds' steering model; combining behaviors with weighted blending; demo with agents navigating toward/away from targets
- [ ] **AI Lesson 02 — Boids** — Flocking simulation: separation, alignment, cohesion; spatial neighbor queries; tunable weights and radii; demo with a flock navigating around obstacles
- [ ] **AI Lesson 03 — Obstacle Avoidance** — Raycasting and whisker-based avoidance; combining avoidance with steering; wall following; demo with agents navigating a field of obstacles

### Pathfinding

- [ ] **AI Lesson 04 — Grid Pathfinding** — A* on a 2D grid; heuristics (Manhattan, Euclidean, octile); path smoothing; visualization of open/closed sets during search
- [ ] **AI Lesson 05 — Navigation Meshes** — NavMesh representation; point-in-polygon queries; A* on a polygon graph; funnel algorithm for string-pulling smooth paths; demo with agents walking a navmesh; can use hand-built meshes or pipeline-generated `.fnav` (Asset Lesson 23)
- [ ] **AI Lesson 06 — Dynamic Pathfinding** — Handling moving obstacles; replanning strategies; local avoidance (RVO/ORCA); combining global paths with local steering

### Decision Making

- [ ] **AI Lesson 07 — Finite State Machines** — FSM with states, transitions, and actions; visual state graph in UI; demo with enemy AI (patrol, chase, attack, flee); AI state transitions drive animation state machine (Anim Lesson 04)
- [ ] **AI Lesson 08 — Behavior Trees** — Nodes: sequence, selector, parallel, decorator; tick-based evaluation; blackboard data sharing; demo with complex NPC behavior
- [ ] **AI Lesson 09 — Utility AI** — Scoring actions by utility curves; action selection; response curves (linear, quadratic, logistic); demo comparing utility AI vs. FSM on the same scenario
- [ ] **AI Lesson 10 — Goal-Oriented Action Planning** — GOAP: world state, actions with preconditions and effects; A* over action space; plan execution and replanning; demo with an agent solving multi-step goals

## Animation Lessons — Future Track

A header-only library (`common/anim/`) covering animation systems for games.
Builds on the skeletal animation foundation from GPU Lessons 30–31 and the
pipeline animation tools. Rendered with `forge_scene.h`.

**Cross-track dependencies:** Root motion (Lesson 05) feeds velocity into
the physics rigid body system. IK (Lesson 06) uses physics raycasts for
foot placement. Animation events (Lesson 09) trigger audio and particle
effects. The AI decision-making track drives animation state transitions.

### Blending & Layering

- [ ] **Anim Lesson 01 — Animation Blending** — Linear interpolation between two clips; blend weights; synchronized blending (phase matching); crossfade transitions; demo blending walk/run cycles
- [ ] **Anim Lesson 02 — Layered Animation** — Additive layers (base + overlay); masked layers (upper body / lower body); layer weights and blending modes; demo with locomotion base + aiming overlay
- [ ] **Anim Lesson 03 — Blend Trees** — 1D and 2D blend spaces; parameterized blending (speed, direction); triangulation for 2D spaces; demo with directional locomotion

### State & Control

- [ ] **Anim Lesson 04 — Animation State Machines** — States, transitions, transition conditions; blend during transitions; transition interruption; visual state graph in UI
- [ ] **Anim Lesson 05 — Root Motion** — Extracting translation/rotation from animation data; applying root motion to character transform; feeding root motion velocity into physics rigid body; blending root motion with gameplay movement
- [ ] **Anim Lesson 06 — Inverse Kinematics** — Two-bone IK (analytical); CCD IK for chains; foot placement on uneven terrain using physics raycasts; hand reaching toward targets; IK/FK blending

### Physics Integration

- [ ] **Anim Lesson 07 — Ragdoll Transitions** — Mapping skeleton bones to physics rigid bodies with joint constraints; blended transition from animated pose to ragdoll (partial ragdoll on hit, full ragdoll on death); recovering from ragdoll back to animation (get-up sequences); ragdoll as secondary motion on animated characters (dangling limbs, hair)
- [ ] **Anim Lesson 08 — Physics-Driven Animation** — Procedural secondary motion: bone-attached spring-damper chains for hair, tails, capes; physics simulation on a subset of bones while the rest play authored animation; collision with body and environment

### Sequencing & Tools

- [ ] **Anim Lesson 09 — Animation Events & Notifies** — Frame-triggered events (footstep sounds, particle spawns, hitbox activation); event curves; integration with audio and physics systems; uses `.faevt` data from Asset Lesson 21 when available
- [ ] **Anim Lesson 10 — Animation Compression** — Curve fitting and keyframe reduction; quantization; error metrics; storage/memory trade-offs; comparing raw vs. compressed playback
- [ ] **Anim Lesson 11 — Animation Graph** — Connecting blend trees, state machines, and IK into a unified evaluation graph; data flow and evaluation order; runtime editing via UI
- [ ] **Anim Lesson 12 — Animation Debugging** — Pose visualization (skeleton overlay, bone axes); slow motion and frame stepping; graph inspection; per-bone contribution display

## ECS Lessons — Future Track

A header-only library (`common/ecs/`) implementing an entity-component-system
architecture. Starts with the fundamentals and builds toward a data-oriented
game framework. Rendered with `forge_scene.h`.

### Foundations

- [ ] **ECS Lesson 01 — Entities and Components** — Entity as an ID; component storage (struct-of-arrays); creating, destroying, and querying entities; demo managing a scene of objects
- [ ] **ECS Lesson 02 — Systems and Iteration** — System functions iterating over component sets; component masks and matching; system ordering and dependencies; demo with movement and rendering systems
- [ ] **ECS Lesson 03 — Archetypes** — Archetype-based storage (entities with identical component sets stored together); archetype graphs for add/remove operations; cache-friendly iteration

### Patterns

- [ ] **ECS Lesson 04 — Sparse Sets** — Sparse set data structure for fast component lookup; trade-offs vs. archetype storage; hybrid approaches; benchmark comparison
- [ ] **ECS Lesson 05 — Events and Messaging** — Entity events (created, destroyed, component added/removed); event queues; observer pattern; deferred structural changes
- [ ] **ECS Lesson 06 — Relationships and Hierarchies** — Parent-child relationships; transform propagation; entity graphs; scene hierarchy as ECS relationships
- [ ] **ECS Lesson 07 — Prefabs and Serialization** — Entity templates; instantiation from prefabs; serializing/deserializing world state; save/load

### Performance

- [ ] **ECS Lesson 08 — Parallel Systems** — Identifying independent systems for parallel execution; read/write component access declarations; job scheduling; thread-safe entity commands
- [ ] **ECS Lesson 09 — Change Detection** — Tracking which components changed per frame; dirty flags; reactive systems that run only when inputs change; reducing unnecessary work
- [ ] **ECS Lesson 10 — ECS Game Demo** — Complete small game using the ECS: entities for player, enemies, projectiles, pickups; systems for physics, AI, rendering, audio; demonstrating all prior ECS concepts working together

## Capstone — Demo Game

A small arena game that integrates every library and validates that they work
together. Not a lesson track — a single project in `demos/arena/` that
exercises the full stack.

**What it demonstrates:**

- **ECS** — all game objects are entities with components; systems handle
  physics, AI, animation, audio, and rendering
- **Physics** — rigid body collisions, ground detection, projectile dynamics
- **AI** — enemy agents with behavior trees, navmesh pathfinding, steering
  behaviors for local movement
- **Animation** — blended locomotion (walk/run/strafe), state machine
  transitions (idle → chase → attack → death), ragdoll on death, IK foot
  placement
- **Audio** — spatial audio for footsteps, impacts, and ambient sounds;
  adaptive music layers (exploration vs. combat intensity)
- **GPU** — PBR lighting via `forge_scene.h`, particle effects for impacts
  and abilities, volumetric fog for atmosphere
- **UI** — HUD (health, score), pause menu, debug overlays
- **Asset pipeline** — all assets (meshes, textures, navmesh, particle
  effects, animation events) processed through the pipeline

**Scope:** 1 arena environment, 1 player character, 2–3 enemy types, basic
combat (melee + projectile), pickups. Enough to prove integration, small
enough to finish.
