# GPU Lessons

Learn modern GPU programming with SDL's GPU API.

## Why GPU Lessons?

These lessons teach you how to use GPUs for real-time rendering — the foundation
of games, simulations, and visual applications. SDL's GPU API gives you direct
access to modern graphics hardware (Vulkan, Direct3D 12, Metal) with a clean,
portable C interface.

## Philosophy

- **One concept at a time** — Each lesson builds on the previous
- **Explain the why** — Not just API calls, but why GPUs work this way
- **Production-ready patterns** — Code you can use in real projects
- **No magic** — Every constant, every pipeline state is explained
- **Math library integration** — Use `common/math/` for all math operations
- **Cross-reference theory** — Links to math lessons explaining concepts

## Lessons

| # | Name | What you'll learn | Math concepts |
|---|------|-------------------|---------------|
| 01 | [Hello Window](01-hello-window/) | GPU device, swapchain, render passes | — |
| 02 | [First Triangle](02-first-triangle/) | Vertex buffers, shaders, pipelines | [Vectors](../math/01-vectors/) |
| 03 | [Uniforms & Motion](03-uniforms-and-motion/) | Push uniforms, animation | [Vectors](../math/01-vectors/) |
| 04 | [Textures & Samplers](04-textures-and-samplers/) | Textures, samplers, UV coordinates, index buffers | [Bilinear Interpolation](../math/03-bilinear-interpolation/) |
| 05 | [Mipmaps](05-mipmaps/) | Mipmap generation, trilinear filtering, sampler modes | [Mipmaps & LOD](../math/04-mipmaps-and-lod/) |
| 06 | [Depth Buffer & 3D Transforms](06-depth-and-3d/) | MVP pipeline, depth testing, back-face culling | [Matrices](../math/05-matrices/) |
| 07 | [Camera & Input](07-camera-and-input/) | First-person camera, keyboard/mouse input, delta time | [Orientation](../math/08-orientation/), [View Matrix](../math/09-view-matrix/) |
| 08 | [Loading a Mesh (OBJ)](08-mesh-loading/) | OBJ parsing, de-indexing, file-based textures, mipmaps | [Vectors](../math/01-vectors/), [Mipmaps & LOD](../math/04-mipmaps-and-lod/) |
| 09 | [Loading a Scene (glTF)](09-scene-loading/) | glTF parsing, multi-material meshes, scene hierarchy, indexed drawing | [Matrices](../math/05-matrices/) |
| 10 | [Basic Lighting (Blinn-Phong)](10-basic-lighting/) | Ambient, diffuse, specular lighting, world-space normals | [Vectors](../math/01-vectors/) |
| 11 | [Compute Shaders](11-compute-shaders/) | Compute pipelines, storage textures, dispatch groups | — |
| 12 | [Shader Grid](12-shader-grid/) | Procedural anti-aliased grid, screen-space derivatives, multiple pipelines | — |
| 13 | [Instanced Rendering](13-instanced-rendering/) | Per-instance vertex buffers, repeated geometry | [Matrices](../math/05-matrices/) |
| 14 | [Environment Mapping](14-environment-mapping/) | Cube map textures, skybox, reflective surfaces | [Vectors](../math/01-vectors/) |
| 15 | [Cascaded Shadow Maps](15-cascaded-shadow-maps/) | Shadow mapping, PCF soft shadows, cascade splitting | [Projections](../math/06-projections/) |
| 16 | [Blending](16-blending/) | Alpha blending, alpha testing, additive blending, blend state | — |
| 17 | [Normal Maps](17-normal-maps/) | Tangent-space normal mapping, TBN matrix | [Vectors](../math/01-vectors/) |
| 18 | [Blinn-Phong with Materials](18-blinn-phong-materials/) | Per-material lighting, material properties | — |
| 19 | [Debug Lines](19-debug-lines/) | Immediate-mode debug drawing, grids, axes, wireframes | [Vectors](../math/01-vectors/) |
| 20 | [Linear Fog](20-linear-fog/) | Distance fog, linear/exponential/exponential-squared modes | — |
| 21 | [HDR & Tone Mapping](21-hdr-tone-mapping/) | Floating-point render targets, Reinhard/ACES tone mapping, exposure | [Color Spaces](../math/11-color-spaces/) |
| 22 | [Bloom (Jimenez Dual-Filter)](22-bloom/) | Downsample/upsample chain, Karis averaging, tent filter | — |
| 23 | [Point Light Shadows](23-point-light-shadows/) | Omnidirectional shadows, cube map depth textures | [Projections](../math/06-projections/) |
| 24 | [Gobo Spotlight](24-gobo-spotlight/) | Projected-texture spotlight, cone falloff, pattern projection | — |
| 25 | [Shader Noise](25-shader-noise/) | Hash functions, value/Perlin noise, fBm, domain warping | [Hash Functions](../math/12-hash-functions/), [Gradient Noise](../math/13-gradient-noise/) |
| 26 | [Procedural Sky (Hillaire)](26-procedural-sky/) | Atmospheric scattering, Rayleigh/Mie, LUT transmittance | — |
| 27 | [SSAO](27-ssao/) | Screen-space ambient occlusion, G-buffer, hemisphere sampling | [Blue Noise](../math/14-blue-noise-sequences/) |

## Prerequisites

### Knowledge

- **C programming** — Comfortable with structs, pointers, and functions
- **Basic 3D math** — Understanding of vectors helps (see [Math Lessons](../math/))
- **Willingness to learn** — GPU programming has a learning curve, but these lessons guide you through it

**No prior graphics experience needed!** These lessons start from the very beginning.

### Tools

- **CMake 3.24+** — Build system
- **C compiler** — MSVC (Windows), GCC, or Clang
- **GPU hardware** — Any GPU supporting Vulkan, Direct3D 12, or Metal

SDL3 is fetched automatically — no manual installation required.

## Learning Path

### For absolute beginners:

1. **Start with Lesson 01** — Get a window on screen and understand the basics
2. **Work through in order** — Each lesson builds on previous concepts
3. **Do the exercises** — Hands-on practice is essential
4. **Read math lessons as needed** — When GPU lessons reference math concepts, dive into the theory
5. **Ask questions** — Use Claude to explain concepts you don't understand

### If you have graphics experience:

- Skim early lessons to learn SDL GPU API patterns
- Focus on SDL-specific concepts (swapchain composition, push uniforms, etc.)
- Jump to specific topics you're interested in
- Use the skills to build quickly

## How GPU Lessons Work

Each lesson includes:

1. **Standalone program** — Builds and runs independently
2. **README** — Explains concepts, shows code, references math lessons
3. **Commented code** — Every line explains *why*, not just *what*
4. **Exercises** — Extend the lesson to reinforce learning
5. **Matching skill** — AI-invokable pattern for building with this technique

### Running a lesson

```bash
cmake -B build
cmake --build build --config Debug

# Easy way — use the run script
python scripts/run.py 02                  # by number
python scripts/run.py first-triangle      # by name

# Or run the executable directly
# Windows
build\lessons\gpu\02-first-triangle\Debug\02-first-triangle.exe

# Linux / macOS
./build/lessons/gpu/02-first-triangle/02-first-triangle
```

## Integration with Math

GPU lessons use the **forge-gpu math library** (`common/math/`) for all math operations.

**You'll see:**

- `vec2` for 2D positions and UV coordinates (HLSL: `float2`)
- `vec3` for 3D positions, colors, normals (HLSL: `float3`)
- `vec4` for homogeneous coordinates (HLSL: `float4`)
- `mat4` for transformations (HLSL: `float4x4`)

**When to read math lessons:**

- **Before GPU Lesson 02**: Read [Vectors](../math/01-vectors/) to understand positions and colors
- **Before GPU Lesson 04**: Read [Bilinear Interpolation](../math/03-bilinear-interpolation/) for texture filtering math
- **Before GPU Lesson 05**: Read [Mipmaps & LOD](../math/04-mipmaps-and-lod/) for mip chain and trilinear math
- **Before GPU Lesson 06**: Read [Matrices](../math/05-matrices/) for MVP transform walkthrough
- **Before GPU Lesson 07**: Read [Orientation](../math/08-orientation/) and [View Matrix](../math/09-view-matrix/) for camera math
- **Before GPU Lesson 08**: Review [Mipmaps & LOD](../math/04-mipmaps-and-lod/) for texture loading
- **Before GPU Lesson 15**: Read [Projections](../math/06-projections/) for shadow map frustums
- **Before GPU Lesson 21**: Read [Color Spaces](../math/11-color-spaces/) for HDR and tone mapping
- **Before GPU Lesson 25**: Read [Hash Functions](../math/12-hash-functions/) and [Gradient Noise](../math/13-gradient-noise/) for shader noise
- **Before GPU Lesson 27**: Read [Blue Noise](../math/14-blue-noise-sequences/) for SSAO sampling
- **When confused**: Math lessons explain the theory behind GPU operations

See [lessons/math/README.md](../math/README.md) for the complete math curriculum.

## SDL GPU API Overview

### What makes SDL GPU different?

**Explicit, low-level control** — Like Vulkan or Direct3D 12, but simpler:

- You manage buffers, pipelines, and render passes explicitly
- No hidden state or "magic" — everything is visible
- Efficient: close to the metal, minimal overhead

**Portable** — One API, multiple backends:

- Vulkan on Windows, Linux, Android
- Direct3D 12 on Windows, Xbox
- Metal on macOS, iOS
- Future: more backends as they're added

**Modern** — Designed for today's GPUs:

- Command buffers for parallel work submission
- Explicit synchronization (no implicit barriers)
- Transfer queues for async uploads
- Compute shaders and GPU-driven rendering

### Core concepts you'll learn:

1. **GPU Device** — Your connection to the graphics hardware
2. **Swapchain** — Double/triple buffering for presenting to the screen
3. **Command Buffers** — Record GPU work, submit in batches
4. **Buffers** — GPU memory for vertex data, uniforms, etc.
5. **Shaders** — Programs that run on the GPU (HLSL in forge-gpu)
6. **Pipelines** — Complete state for how rendering happens
7. **Render Passes** — Define rendering operations and their targets
8. **Transfer Operations** — Upload data from CPU to GPU

## Using Skills

Every GPU lesson has a matching **Claude Code skill** that teaches AI agents
the same pattern. Use these to build projects quickly:

**Available skills:**

- **`/sdl-gpu-setup`** — Scaffold a new SDL3 GPU application
- **`/first-triangle`** — Add vertex rendering with shaders
- **`/uniforms-and-motion`** — Pass per-frame data to shaders
- **`/textures-and-samplers`** — Load images, create textures/samplers, draw textured geometry
- **`/mipmaps`** — Mipmapped textures, trilinear filtering, LOD control
- **`/depth-and-3d`** — Depth buffer, MVP pipeline, 3D rendering
- **`/camera-and-input`** — First-person camera, keyboard/mouse input, delta time
- **`/mesh-loading`** — Load OBJ models, textured mesh rendering
- **`/scene-loading`** — Load glTF scenes with multi-material rendering
- **`/basic-lighting`** — Blinn-Phong ambient + diffuse + specular lighting
- **`/compute-shaders`** — Compute pipelines, storage textures, dispatch groups
- **`/shader-grid`** — Procedural anti-aliased grid with screen-space derivatives
- **`/instanced-rendering`** — Draw repeated geometry with per-instance buffers
- **`/environment-mapping`** — Cube map skybox and reflective surfaces
- **`/cascaded-shadow-maps`** — Cascaded shadow maps with PCF soft shadows
- **`/blending`** — Alpha blending, alpha testing, additive blending
- **`/normal-maps`** — Tangent-space normal mapping with TBN matrix
- **`/blinn-phong-materials`** — Per-material Blinn-Phong lighting
- **`/debug-lines`** — Immediate-mode debug line drawing
- **`/linear-fog`** — Depth-based distance fog (linear, exponential)
- **`/hdr-tone-mapping`** — HDR rendering with Reinhard/ACES tone mapping
- **`/bloom`** — Jimenez dual-filter bloom with Karis averaging
- **`/point-light-shadows`** — Omnidirectional point light shadows
- **`/gobo-spotlight`** — Projected-texture gobo spotlight
- **`/shader-noise`** — GPU noise functions (hash, Perlin, fBm, domain warp)
- **`/procedural-sky`** — Physically-based procedural atmospheric scattering
- **`/ssao`** — Screen-space ambient occlusion

**How to use:**

1. Copy `.claude/skills/` into your project
2. Type `/skill-name` in Claude Code, or just describe what you want
3. Claude invokes the skill automatically when relevant

Skills know the project conventions and generate correct code.

See the main [README.md](../../README.md#skills--build-with-ai) for complete skills documentation.

## Building Real Projects

These lessons are **not just tutorials** — they're patterns you'll use in production:

- **Vertex buffers** (Lesson 02) — Every game draws geometry this way
- **Uniforms** (Lesson 03) — Every shader needs per-frame data
- **Textures** (Lesson 04) — Essential for realistic rendering
- **Mipmaps** (Lesson 05) — Prevent aliasing at any distance
- **Depth buffers** (Lesson 06) — Required for 3D scenes
- **Lighting** (Lesson 10) — Makes 3D objects look real
- **Shadow maps** (Lessons 15, 23, 24) — Dynamic shadows for directional and point lights
- **Post-processing** (Lessons 21, 22, 27) — HDR, bloom, and ambient occlusion

The code is **production-ready** — clean, efficient, well-documented. Copy it,
adapt it, build on it.

## Common Questions

### Do I need to learn Vulkan/D3D12/Metal first?

**No!** SDL GPU abstracts the complexity while keeping the power. You learn
modern GPU concepts without the boilerplate.

### What if I know OpenGL?

SDL GPU is **explicit** (like Vulkan), not **implicit** (like OpenGL):

- You create command buffers and submit them (no global state)
- Shaders are pre-compiled (no runtime compilation)
- Render passes are explicit (no default framebuffer)

The concepts transfer, but the API style is different.

### Why HLSL shaders?

- **Portable**: DXC compiles HLSL to SPIR-V (Vulkan), DXIL (D3D12), and MSL (Metal)
- **Modern**: Shader Model 6.0+ with advanced features
- **Familiar**: Similar to GLSL but with better tooling
- **Well-documented**: Microsoft's shader language with great resources

### Can I use this for 2D games?

**Absolutely!** Early lessons are 2D-focused. SDL GPU handles:

- Sprite rendering (textured quads)
- Particle systems
- 2D lighting
- UI rendering

3D is just an extension of the same concepts.

## Exercises Philosophy

Every lesson includes exercises. **Do them!** They're designed to:

1. **Reinforce concepts** — Apply what you just learned
2. **Build intuition** — Experiment and see what happens
3. **Prepare for next lesson** — Many exercises preview upcoming concepts
4. **Encourage creativity** — Make something uniquely yours

Don't just read the code — modify it, break it, fix it, extend it.

## Getting Help

**If you're stuck:**

1. **Re-read the lesson** — Concepts often click the second time
2. **Check the math lesson** — Theory might clarify the practice
3. **Ask Claude** — Describe what you're trying to do and what's not working
4. **Read SDL docs** — [wiki.libsdl.org/SDL3](https://wiki.libsdl.org/SDL3/CategoryGPU)
5. **Experiment** — Change values and see what happens

**Remember:** Getting a triangle on screen for the first time is a big deal.
GPU programming has a learning curve, but these lessons make it manageable.

## What's Next?

After completing the current 27 lessons, you'll have the skills to build
real-time 3D applications with lighting, shadows, post-processing, and
procedural content. Use the skills to build your own projects, or extend
the lessons with new techniques.

---

**Welcome to GPU programming with SDL!** These lessons will take you from
"What's a vertex buffer?" to building real-time 3D applications. Let's build
something amazing!
