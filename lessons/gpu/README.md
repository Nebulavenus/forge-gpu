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
| 04 | [Textures & Samplers](04-textures-and-samplers/) | Textures, samplers, UV coordinates, index buffers | [Bilinear Interpolation](../math/04-bilinear-interpolation/) |
| 05 | [Mipmaps](05-mipmaps/) | Mipmap generation, trilinear filtering, sampler modes | [Mipmaps & LOD](../math/05-mipmaps-and-lod/) |

*More lessons coming soon:* Depth buffers, 3D transforms, lighting, and more.

See [PLAN.md](../../PLAN.md) for the full roadmap.

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
- **Before GPU Lesson 04**: Read [Bilinear Interpolation](../math/04-bilinear-interpolation/) for texture filtering math
- **Before GPU Lesson 05**: Read [Mipmaps & LOD](../math/05-mipmaps-and-lod/) for mip chain and trilinear math
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
- **Depth buffers** *(coming)* — Required for 3D scenes
- **Lighting** *(coming)* — Makes 3D objects look real

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

After completing the current lessons, you'll be ready for:

- **Depth Buffer & 3D Transforms** — MVP matrices, perspective projection, depth testing
- **Loading a Mesh** — OBJ or glTF parsing, indexed rendering
- **Lighting** — Diffuse + specular, normal vectors, Phong model
- **Advanced Techniques** — Shadows, post-processing, compute shaders

See [PLAN.md](../../PLAN.md) for the full roadmap.

---

**Welcome to GPU programming with SDL!** These lessons will take you from
"What's a vertex buffer?" to building real-time 3D applications. Let's build
something amazing! 🚀
