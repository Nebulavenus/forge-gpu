# forge-gpu — Lesson Plan

## Foundation

Infrastructure that supports all lessons:

- [x] **Project scaffolding** — CMakeLists.txt, FetchContent for SDL3, common/ header, directory structure
- [x] **zlib LICENSE**
- [x] **Math library** — `common/math/forge_math.h` (vectors, matrices, documented, tested)
- [x] **Math lessons** — `lessons/math/` with standalone programs teaching concepts
- [x] **Test suite** — Automated tests for math library (CTest integration)
- [x] **Skills** — `/math-lesson`, `/new-lesson`, etc. for AI-assisted development

## GPU Lessons

- [x] **Lesson 01 — Hello Window** — SDL callbacks, GPU device creation, swapchain, clear screen via render pass
- [x] **Lesson 02 — First Triangle** — Vertex buffers, shaders (SPIRV/DXIL), graphics pipeline, sRGB swapchain
- [x] **Lesson 03 — Uniforms & Motion** — Uniform buffers, passing time to shaders, spinning triangle
- [x] **Lesson 04 — Textures & Samplers** — Loading images, texture sampling, UV coordinates
- [x] **Lesson 05 — Mipmaps** — Mip chain generation, trilinear filtering, why mipmaps fix aliasing; fragment derivatives (ddx/ddy) and how the GPU picks the mip level; helper invocations and 2x2 quad execution
- [x] **Lesson 06 — Depth Buffer & 3D Transforms** — MVP matrices, perspective projection, depth testing, window resize handling
- [x] **Lesson 07 — Camera & Input** — First-person camera with quaternion orientation (references Math Lesson 08 — Orientation and Math Lesson 09 — View Matrix); SDL event handling for keyboard and mouse input; delta time to decouple movement speed from frame rate; small scene with several objects to make navigation interesting
- [x] **Lesson 08 — Loading a Mesh (OBJ)** — Wavefront OBJ format; parsing vertices, normals, UVs, and faces; de-indexed rendering; quad triangulation; file-based texture loading with mipmaps; reusable OBJ parser library (`common/obj/forge_obj.h`)
- [x] **Lesson 09 — Loading a Scene (glTF)** — glTF 2.0 format; JSON scene description + binary buffers; node hierarchy, meshes, accessors, and buffer views; PBR materials; multi-material indexed rendering; reusable glTF parser library (`common/gltf/forge_gltf.h`)
- [x] **Lesson 10 — Basic Lighting** — Blinn-Phong shading (ambient + diffuse + specular), world-space normal transformation, half-vector specular, lighting uniforms (light direction, camera position, shininess)
- [x] **Lesson 11 — Compute Shaders** — Compute pipelines, storage textures, dispatch groups, fullscreen triangle, compute-then-render pattern
- [x] **Lesson 12 — Shader Grid** — Procedural grid rendering with fwidth()/smoothstep() anti-aliasing, distance fade, multiple pipelines in one render pass, combining procedural geometry with 3D models
- [x] **Lesson 13 — Instanced Rendering** — Per-instance vertex buffers, per-instance model matrices as vertex attributes, multi-model scenes (BoxTextured + Duck), efficient draw calls (3 calls for 303 objects: 47 boxes + 256 ducks)
- [x] **Lesson 14 — Environment Mapping** — Cube map textures, skybox rendering (z=w depth technique), environment reflections (reflect + cube map sample + lerp), multi-pipeline render pass, equirectangular-to-cubemap conversion
- [x] **Lesson 15 — Cascaded Shadow Maps (CSM)** — View-dependent shadow partitioning for large outdoor scenes

### Core Rendering Techniques

- [x] **Lesson 16 — Blending** — Alpha blending, blend equations and factors, transparency sorting, additive and premultiplied-alpha modes; when to use alpha test vs alpha blend; blend state configuration in SDL GPU
- [x] **Lesson 17 — Normal Maps** — Tangent-space normal mapping for surface detail without extra geometry; tangent and bitangent computation; TBN matrix construction; sampling and decoding normal maps in the fragment shader; comparing flat vs per-vertex vs normal-mapped shading
- [x] **Lesson 18 — Blinn-Phong with Materials** — Extending Lesson 10 with per-object material properties (ambient, diffuse, specular colors, shininess); material uniforms or material buffers; multiple objects with distinct materials in one scene; foundation for PBR-style material workflows
- [x] **Lesson 19 — Debug Lines** — Immediate-mode debug drawing for lines, circles, and coordinate-axis gizmos; dynamic vertex buffer updated each frame; line rendering pipeline (no depth test option, overlay mode); useful as a diagnostic tool for every lesson that follows

### Atmosphere & Post-Processing

- [x] **Lesson 20 — Linear Fog** — Depth-based distance fog in the fragment shader; linear, exponential, and exponential-squared falloff; fog color blending; integrating fog with existing lit scenes
- [x] **Lesson 21 — HDR & Tone Mapping** — Rendering to floating-point (R16G16B16A16_FLOAT) render targets; why LDR clamps highlights; tone mapping operators (Reinhard, ACES); gamma correction pipeline; fullscreen blit pass from HDR to swapchain
- [x] **Lesson 22 — Bloom (Jorge Jimenez)** — Dual-filter (downsample + upsample) bloom using the Jimenez method from SIGGRAPH 2014; threshold extraction; progressive downsample chain; tent-filter upsample with additive blending; combining bloom with tone-mapped output (depends on Lesson 21)

### Lighting Extensions

- [x] **Lesson 23 — Point Lights & Shadows** — Multiple point light sources; omnidirectional shadow mapping with cube map depth textures; shadow bias and Peter Panning; attenuation falloff
- [x] **Lesson 24 — Gobo Spotlight** — Projected-texture (cookie/gobo) spotlight; spotlight cone with inner/outer angles and smooth falloff; projecting a texture pattern through the light; shadow map for the spotlight frustum; theatrical/cinematic lighting applications

### Noise & Procedural

> **Prerequisite:** Math Lessons 12–14 must be completed before starting this
> section. Lesson 25 relies on hash functions (Math Lesson 12), gradient noise
> (Math Lesson 13), and blue noise dithering (Math Lesson 14). Lesson 26 builds
> on the noise techniques from Lesson 25.

- [x] **Lesson 25 — Shader Noise** — Applying noise in fragment shaders; GPU-friendly hash functions for white noise (see [Math Lesson 12 — Hash Functions & White Noise](#planned-math-lessons)); Perlin/simplex noise for smooth randomness (see [Math Lesson 13 — Gradient Noise](#planned-math-lessons)); blue noise dithering for banding reduction (see [Math Lesson 14 — Blue Noise & Low-Discrepancy Sequences](#planned-math-lessons)); octave stacking (fBm) for natural patterns; practical uses — procedural textures, terrain variation, dissolve effects
- [x] **Lesson 26 — Procedural Sky (Hillaire)** — Single-scattering atmospheric model based on Sébastien Hillaire's approach; Rayleigh and Mie scattering; sun disc rendering; time-of-day color variation; LUT-based or per-pixel evaluation; integrating as a skybox replacement

### Screen-Space Effects

- [x] **Lesson 27 — SSAO** — Screen-space ambient occlusion; sampling hemisphere kernel in view space; depth buffer reconstruction; random rotation via noise texture; blur pass for smooth results; combining AO factor with lighting (depends on Lesson 21 for render-to-texture pattern)
- [ ] **Lesson 28 — Screen-Space Reflections (SSR)** — Ray marching against the depth buffer in screen space; hierarchical tracing for performance; handling misses and fallback to environment map; combining SSR with existing reflections from Lesson 14

### Reflections

- [ ] **Lesson 29 — Planar Reflections** — Rendering the scene from a mirrored camera; oblique near-plane clipping to prevent geometry behind the mirror from appearing; reflection texture compositing; application to water surfaces and mirrors

### Animation

- [ ] **Lesson 30 — Transform Animations** — Keyframe interpolation for translation, rotation (slerp), and scale; animation clips with timestamps; playing, blending, and looping animations; loading animation data from glTF
- [ ] **Lesson 31 — Skinning Animations** — Skeletal animation with joint hierarchies; bind-pose inverse matrices; vertex skinning with joint indices and weights (4 joints per vertex); computing the skin matrix in the vertex shader; loading skinned meshes from glTF

### Advanced Rendering

- [ ] **Lesson 32 — Vertex Pulling** — Programmable vertex fetch using storage buffers instead of vertex input; raw buffer access in the vertex shader; decoupling vertex layout from pipeline input state; use cases — flexible vertex formats, mesh compression, compute-to-vertex pipelines
- [ ] **Lesson 33 — Indirect Drawing** — GPU-driven draw calls with `SDL_DrawGPUPrimitivesIndirect` / `SDL_DrawGPUIndexedPrimitivesIndirect`; filling indirect argument buffers from compute shaders; basic GPU culling (frustum cull in compute, emit surviving draws); reducing CPU draw-call overhead
- [ ] **Lesson 34 — Particle Animations** — Billboard quad particles facing the camera; GPU particle buffer updated via compute shader; spawn, simulate (gravity, drag, lifetime), and render loop; atlas-based animated particles; additive and soft-particle blending (depends on Lessons 11 and 16)
- [ ] **Lesson 35 — Imposters** — Billboard LOD representations of complex meshes; baking an imposter atlas (multiple view angles); selecting the correct atlas frame based on view direction; cross-fading between imposter and full mesh; application to distant trees, props, and crowd rendering

### Advanced Materials & Effects

- [ ] **Lesson 36 — Translucent Materials** — Approximating light transmission through thin and thick surfaces; wrap lighting for subsurface scattering approximation; thickness maps; back-face lighting contribution; application to foliage, wax, skin, and fabric
- [ ] **Lesson 37 — Water Caustics** — Projecting animated caustic patterns onto underwater surfaces; caustic texture animation (scrolling, distortion); light attenuation with water depth; combining with existing lighting and shadow systems
- [ ] **Lesson 38 — IBL with Probes** — Image-based lighting using irradiance maps (diffuse) and pre-filtered environment maps (specular); split-sum approximation with a BRDF LUT; placing reflection probes in a scene; blending between probes; integrating IBL as ambient lighting replacement

### Volumetric & Terrain

- [ ] **Lesson 39 — Volumetric Fog** — Ray marching through participating media in a froxel grid or screen-space pass; Beer-Lambert absorption; in-scattering from lights with shadow map sampling; temporal reprojection for performance; combining volumetric fog with scene rendering
- [ ] **Lesson 40 — Grass with Animations & Imposters** — Dense grass field rendering; geometry instancing or compute-generated grass blades; wind animation using noise-based displacement; LOD transition from full blades to imposter cards at distance; terrain integration (depends on Lessons 13, 25, 35)
- [ ] **Lesson 41 — Height Map Terrain** — GPU terrain from height map; LOD with distance-based tessellation or geo-clipmaps; normal computation from height samples; texture splatting with blend maps; integrating with grass rendering

## UI Lessons

Build an immediate-mode UI system from scratch — font parsing, text rendering,
layout, and interactive controls. UI lessons produce CPU-side data (textures,
vertices, indices, UVs) with no GPU dependency.

### Font Fundamentals

- [x] **UI Lesson 01 — TTF Parsing** — TrueType file structure; table directory; extracting glyph outlines, advance widths, and bounding boxes; font units vs pixel coordinates; units-per-em scaling
- [x] **UI Lesson 02 — Glyph Rasterization** — Converting quadratic Bézier outlines to bitmaps; scanline rasterization with non-zero winding rule; supersampled anti-aliasing; contour reconstruction with implicit midpoint rule; single-channel alpha output
- [x] **UI Lesson 03 — Font Atlas** — Rectangle packing algorithms; arranging glyphs into a power-of-two texture atlas; computing UV coordinates per glyph; padding to prevent bleed; atlas as a single-channel texture

### Text Layout

- [x] **UI Lesson 04 — Text Layout** — Pen/cursor model; horizontal metrics (advance width, bearings); baseline positioning; building textured quads per character; index buffers with CCW winding; whitespace handling; line breaking and wrapping; text alignment (left, center, right); ForgeUiVertex format

### Immediate-Mode UI

- [x] **UI Lesson 05 — Immediate-Mode Basics** — Retained vs immediate-mode UI; frame-based widget declaration; widget identity (ID generation); hot/active state tracking; input routing (mouse position, clicks)
- [x] **UI Lesson 06 — Checkboxes and Sliders** — External mutable state; checkbox toggle with bool*; slider drag interaction; value mapping (pixel to normalized to user value); active persistence outside widget bounds; track/thumb/value-label draw elements
- [x] **UI Lesson 07 — Text Input** — Text input field with cursor and keyboard focus; character insertion and deletion; focused ID state machine; cursor movement and positioning; generating draw data for interactive controls

### Layout and Containers

- [x] **UI Lesson 08 — Layout System** — Automatic horizontal and vertical stacking; padding and spacing; ForgeUiLayout cursor model; layout stack with push/pop nesting; layout_next() for automatic widget positioning; layout-aware widget variants
- [ ] **UI Lesson 09 — Panels and Clipping** — Static containers with scroll; clipping rects and scissor rect output for the renderer; scroll offset and content bounds; nested panels
- [ ] **UI Lesson 10 — Windows** — Draggable panels with title bars; z-ordering (bring-to-front on click); collapse/expand; move interaction; builds on layout (Lesson 08) and clipping (Lesson 09)

### Application Patterns

- [ ] **UI Lesson 11 — Game UI** — Health bars, inventories, HUD elements, menus; game-specific patterns using the immediate-mode controls from earlier lessons; fixed and proportional layout for different screen sizes
- [ ] **UI Lesson 12 — Dev UI** — Property editors, debug overlays, console, performance graphs; developer-facing tools for inspecting game state; collapsible sections and tree views

## Engine Lessons

Practical engineering lessons covering the toolchain and infrastructure behind
graphics applications.

### C Language

- [x] **Engine Lesson 01 — Intro to C with SDL** — Types, functions, control flow, arrays, strings, pointers, structs, dynamic memory; using SDL's cross-platform APIs (SDL_Log, SDL_malloc, SDL_free, SDL_memcpy) instead of C standard library equivalents and why

### Build Systems

- [x] **Engine Lesson 02 — CMake Fundamentals** — Targets, properties, `add_executable`, `target_link_libraries`, generator expressions; what happens when you forget to link; reading CMake error messages
- [x] **Engine Lesson 03 — FetchContent & Dependencies** — How forge-gpu downloads SDL automatically; adding your own dependencies; version pinning; offline builds

### C Language (continued)

- [x] **Engine Lesson 04 — Pointers & Memory** — Stack vs heap, `malloc`/`free`, pointer arithmetic, `sizeof`/`offsetof`; how vertex buffer uploads work under the hood
- [x] **Engine Lesson 05 — Header-Only Libraries** — How `forge_math.h` works; `static inline`; include guards vs `#pragma once`; one-definition rule

### Debugging

- [x] **Engine Lesson 06 — Reading Error Messages** — Compiler errors, linker errors, runtime crashes; what each type means and how to fix them
- [x] **Engine Lesson 07 — Using a Debugger** — Breakpoints, stepping, inspecting variables; finding crashes and logic errors
- [x] **Engine Lesson 08 — Debugging Graphics with RenderDoc** — GPU frame capture; debug groups and labels (`SDL_PushGPUDebugGroup`, `SDL_PopGPUDebugGroup`, `SDL_InsertGPUDebugLabel`); RenderDoc Event Browser, Pipeline State, Texture/Mesh Viewer; RenderDoc in-application API for programmatic capture
- [x] **Engine Lesson 09 — HLSL Shared Headers** — `#include` in HLSL; `.hlsli` files for shared constants and utilities; include guards; `-I` search paths in dxc; comparison to C header-only libraries (Engine Lesson 05); real example from GPU Lesson 26 (`atmosphere_params.hlsli`)

### Rasterization

- [x] **Engine Lesson 10 — CPU Rasterization** — Edge function triangle rasterization; barycentric coordinate interpolation; bounding box optimization; texture sampling (nearest-neighbor); alpha blending (source-over compositing); indexed drawing; winding order; header-only rasterizer library (`common/raster/forge_raster.h`) with ForgeUiVertex-compatible vertex format; BMP image output

## Math Lessons

- [x] **Math Lesson 01 — Vectors** — vec2/vec3/vec4, dot product, cross product, normalization, lerp
- [x] **Math Lesson 02 — Coordinate Spaces** — Model, world, view, projection transforms
- [x] **Math Lesson 03 — Bilinear Interpolation** — How LINEAR texture filtering works: two nested lerps blending the 4 nearest texels; comparison with NEAREST; builds on lerp from Math Lesson 01
- [x] **Math Lesson 04 — Mipmaps & LOD** — Mip chain as power-of-two downsampling, log2 for level selection, how screen-space derivatives map to mip levels; connects bilinear interpolation (Math Lesson 03) to trilinear filtering
- [x] **Math Lesson 05 — Matrices** — mat4x4, transformations, rotations, composition
- [x] **Math Lesson 06 — Projections** — Perspective, orthographic, frustums, clip space, NDC, perspective-correct interpolation
- [x] **Math Lesson 07 — Floating Point** — Brief intro to fixed-point as motivation; IEEE 754 floating-point representation (sign, exponent, mantissa); how precision varies across the number line; epsilon and testing for equality (absolute vs relative tolerance); precision pitfalls in graphics: z-fighting and why depth buffer precision is non-linear; 32-bit vs 64-bit (float vs double) trade-offs — when each is appropriate and why GPUs favor 32-bit
- [x] **Math Lesson 08 — Orientation** — *(Larger lesson, like Projections.)* Four representations of 3D rotation and how to convert between them. **Euler angles:** pitch, yaw, and roll diagram; rotation order conventions; gimbal lock explained and visualized. **Rotation matrices:** how each basis-axis rotation matrix (Rx, Ry, Rz) is constructed; rotation around an arbitrary axis (Rodrigues' formula). **Axis-angle:** compact representation (axis + angle); why it is commonly used as an input/interface format even when quaternions are the storage format. **Quaternions:** the imaginary-number basis (i, j, k) and their properties; representation as (w, x, y, z); identity quaternion; multiplication and how it composes rotations; conjugate and inverse; rotating a vector by a quaternion; unit quaternion constraint; slerp for smooth interpolation. Conversions: Euler↔matrix, axis-angle↔quaternion, quaternion↔matrix, and the full round-trip
- [x] **Math Lesson 09 — View Matrix & Virtual Camera** — Building a view matrix from scratch; the camera as an inverse transform (world-to-view); constructing the view matrix from position + quaternion orientation; extracting forward/right/up vectors from a quaternion; look-at as a special case; how the view matrix feeds into the MVP pipeline (connects to Math Lesson 02 — Coordinate Spaces and Math Lesson 06 — Projections)
- [x] **Math Lesson 10 — Anisotropy vs Isotropy** — Direction-dependent vs direction-independent behavior; isotropic sampling (equal in all directions) vs anisotropic filtering (stretches samples along the axis of greatest compression); anisotropic noise (Perlin/simplex stretched along a direction, e.g. wood grain, brushed metal); anisotropic friction in rigid body physics (ice rink, grooved surfaces, tire grip); eigenvalues of the screen-space Jacobian and how they drive the GPU's anisotropic sampler; practical demo comparing isotropic vs anisotropic texture filtering on a tilted plane

- [x] **Math Lesson 11 — Color Spaces** — Gamma correction (sRGB piecewise transfer function vs pow(x,2.2)); linear vs gamma space and why all math must be in linear; luminance (BT.709 coefficients, human spectral sensitivity); RGB↔HSL and RGB↔HSV conversions; CIE 1931 XYZ device-independent color; CIE xyY chromaticity and the chromaticity diagram; gamut (sRGB, DCI-P3, Rec.2020 triangles); tone mapping (Reinhard, ACES Narkowicz fit); exposure (photographic stops); prerequisite for GPU Lesson 21 (HDR & Tone Mapping)

### Planned Math Lessons

> **Scheduling:** Math Lessons 12–14 are prerequisites for GPU Lesson 25
> (Shader Noise) and must be implemented before the Noise & Procedural section
> begins.

- [x] **Math Lesson 12 — Hash Functions & White Noise** — Integer hash functions for GPU use (Wang, PCG, xxHash-style); mapping hash output to uniform floats; why shader noise avoids `rand()` and relies on deterministic hashing; the "magic numbers" in common hash functions and where they come from; visualizing white noise patterns; seeding with position, time, and frame index
- [x] **Math Lesson 13 — Gradient Noise (Perlin & Simplex)** — Lattice-based noise: random gradients at grid points, dot product with distance vector, smooth interpolation; Ken Perlin's original noise and improved noise (2002); simplex noise — fewer samples, better isotropy, skewed grid; octave stacking (fBm) for multi-scale detail; lacunarity, persistence, and their visual effects; domain warping for organic shapes
- [x] **Math Lesson 14 — Blue Noise & Low-Discrepancy Sequences** — Why uniform random sampling clumps and gaps; blue noise — energy concentrated away from low frequencies; generating blue noise (void-and-cluster, Mitchell's best candidate); low-discrepancy sequences (Halton, Sobol, R2); application to dithering (replacing banding with imperceptible noise), sampling (anti-aliasing, AO kernels), and stippling

## Developer Experience / Tooling

Improvements to make development easier:

- [x] **Run script/utility** — `python scripts/run.py <name-or-number>` runs lessons easily
  - Example: `python scripts/run.py 02` or `python scripts/run.py first-triangle`
  - Supports `math/01` prefix, substring matching, lists all lessons when run with no args

- [x] **Shader compilation helper** — `python scripts/compile_shaders.py` compiles all HLSL shaders
  - Auto-detects dxc from VULKAN_SDK or PATH
  - Compiles to SPIRV and DXIL, generates C byte-array headers
  - Can target a specific lesson: `python scripts/compile_shaders.py 02`

- [x] **Setup script** — `python scripts/setup.py` verifies the full environment
  - Checks git, CMake, C compiler (MSVC/GCC/Clang), Python, Pillow
  - Detects Vulkan SDK, dxc shader compiler, GPU via vulkaninfo
  - `--fix` installs missing Python packages, `--build` configures + builds

- [x] **Screenshot generation** — `scripts/capture_lesson.py` + `common/capture/forge_capture.h`
  - Programmatic capture via `--screenshot` / `--capture-dir` CLI flags
  - BMP→PNG/GIF conversion, README auto-update

## Open Questions

- ~~Shader workflow~~ **Resolved:** ship HLSL source + pre-compiled SPIRV & DXIL headers; compile with `dxc` from Vulkan SDK
