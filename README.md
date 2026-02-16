# forge-gpu

**Learn graphics fundamentals. Build real projects.**

A learning platform and building tool for real-time graphics with
[SDL's GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU), written in C.

## Why forge-gpu?

**Two ways to use this project:**

1. **Learn** — Follow guided lessons teaching GPU programming, math, and game techniques
   - Each lesson is a standalone program introducing one concept
   - Progressive curriculum from "Hello Window" to advanced rendering
   - Covers SDL GPU API, math fundamentals, techniques, and physics
   - Every line commented to explain *why*, not just *what*

2. **Forge** — Use skills and libraries to build games and tools with AI
   - Reusable math library (documented, readable, learning-focused)
   - Claude Code skills teaching AI agents the patterns from lessons
   - Copy skills to your project and build confidently with Claude
   - Understanding fundamentals makes working with AI more productive

**Philosophy:** Learn the concepts, then use them to build. When you hit a
problem building, dive deeper into the relevant lesson. Humans work better
with AI when they understand what they're building.

## Lessons

### GPU Lessons (lessons/gpu/)

Learn the SDL GPU API and modern rendering techniques:

| # | Name | What you'll learn |
|---|------|-------------------|
| 01 | [Hello Window](lessons/gpu/01-hello-window/) | GPU device, swapchain, command buffers, render passes |
| 02 | [First Triangle](lessons/gpu/02-first-triangle/) | Vertex buffers, shaders, graphics pipeline |
| 03 | [Uniforms & Motion](lessons/gpu/03-uniforms-and-motion/) | Uniform buffers, push uniforms, animating with time |
| 04 | [Textures & Samplers](lessons/gpu/04-textures-and-samplers/) | Loading images, GPU textures, samplers, UV coordinates, index buffers |
| 05 | [Mipmaps](lessons/gpu/05-mipmaps/) | Mipmap generation, trilinear filtering, sampler modes, procedural textures |
| 06 | [Depth Buffer & 3D Transforms](lessons/gpu/06-depth-and-3d/) | MVP matrices, depth testing, back-face culling, perspective projection |
| 07 | [Camera & Input](lessons/gpu/07-camera-and-input/) | First-person camera, keyboard/mouse input, delta time, multiple objects |

### Math Lessons (lessons/math/)

Standalone programs teaching the math behind graphics:

| # | Topic | What you'll learn |
|---|-------|-------------------|
| 01 | [Vectors](lessons/math/01-vectors/) | Addition, dot/cross products, normalization, lerp |
| 02 | [Coordinate Spaces](lessons/math/02-coordinate-spaces/) | Model, world, view, clip, NDC, screen spaces |
| 03 | [Bilinear Interpolation](lessons/math/03-bilinear-interpolation/) | LINEAR texture filtering, nested lerps, nearest vs linear |
| 04 | [Mipmaps & LOD](lessons/math/04-mipmaps-and-lod/) | Mip chains, trilinear interpolation, LOD selection |
| 05 | [Matrices](lessons/math/05-matrices/) | Identity, translation, scaling, rotation, composition, MVP pipeline |
| 06 | [Projections](lessons/math/06-projections/) | Perspective, orthographic, frustums, clip space, NDC |
| 07 | [Floating Point](lessons/math/07-floating-point/) | IEEE 754, precision, epsilon comparison, z-fighting, float vs double |
| 08 | [Orientation](lessons/math/08-orientation/) | Quaternions, Euler angles, axis-angle, rotation matrices, gimbal lock, slerp |
| 09 | [View Matrix](lessons/math/09-view-matrix/) | View matrix, virtual camera, basis extraction, look-at vs quaternion |

Each math lesson includes a demo program and updates the shared math
library (`common/math/`) with documented, reusable implementations.

See [PLAN.md](PLAN.md) for the full roadmap.

## Math Library

GPU lessons use a shared math library (`common/math/`) instead of writing
bespoke math in each lesson. The library is:

- **Documented** — inline comments explain every function and parameter
- **Readable** — written to be learned from, not just used
- **Reusable** — use it in lessons or copy it into your own projects
- **Learning-focused** — every function has a corresponding math lesson explaining the concept

When you need new math functionality, use the `/math-lesson` skill. It creates:

1. A small program demonstrating the concept
2. A README explaining the theory and where it's used
3. An update to the math library with documented implementation

The math library grows alongside the lessons, always staying readable and
well-documented.

## Getting Started

### Prerequisites

- **CMake 3.24+**
- **A C compiler** (MSVC, GCC, or Clang)
- **A GPU** with Vulkan, Direct3D 12, or Metal support
- **Python 3** (for helper scripts)

SDL3 is fetched automatically — no manual installation required.

**Verify your environment** with the setup script:

```bash
python scripts/setup.py
```

This checks all required tools, detects the Vulkan SDK and shader compiler,
and reports anything missing. Use `--fix` to install missing Python packages
or `--build` to configure and build in one step:

```bash
python scripts/setup.py --build
```

### Building

```bash
cmake -B build
cmake --build build --config Debug
```

Optionally, initialise the SDL source submodule if you want to browse the
SDL headers and GPU backend code locally (this is for reference only — the
build uses FetchContent):

```bash
git submodule update --init
```

### Running lessons

The easiest way to run a lesson is with the **run script**:

```bash
python scripts/run.py 01                  # by number
python scripts/run.py first-triangle      # by name
python scripts/run.py math/01             # math lesson
python scripts/run.py                     # list all lessons
```

You can also run executables directly:

```bash
# Windows
build\lessons\gpu\01-hello-window\Debug\01-hello-window.exe

# Linux / macOS
./build/lessons/gpu/01-hello-window/01-hello-window
```

## Testing

The math library has comprehensive automated tests covering all operations.

**Run all tests:**

```bash
cd build
ctest -C Debug --output-on-failure
```

**Run tests directly:**

```bash
cmake --build build --config Debug --target test_math
build/tests/math/Debug/test_math.exe
```

All tests use epsilon comparison for floating-point accuracy and return proper exit codes for CI/CD integration.

See [tests/math/README.md](tests/math/README.md) for adding new tests.

## Shader compilation

Pre-compiled shader bytecodes are checked in, so you **don't need any extra
tools just to build and run the lessons**. If you want to modify the HLSL
shader source, you'll need:

- **[Vulkan SDK](https://vulkan.lunarg.com/)** — provides `dxc` with SPIR-V
  support (the Windows SDK `dxc` can only compile DXIL, not SPIR-V)

After installing, make sure the `VULKAN_SDK` environment variable is set
(the installer does this automatically). On Windows the default location is:

```text
C:\VulkanSDK\<version>\Bin\dxc.exe
```

> **Heads up:** If you just type `dxc` and get *"SPIR-V CodeGen not
> available"*, you're hitting the Windows SDK `dxc` instead of the Vulkan
> SDK one. Use the full path to the Vulkan SDK `dxc` or put its `Bin/`
> directory earlier on your PATH.

### Compiling shaders

The **shader compilation script** handles everything — finds `dxc`, compiles
each HLSL file to both SPIR-V and DXIL, and generates C byte-array headers:

```bash
python scripts/compile_shaders.py            # all lessons
python scripts/compile_shaders.py 02         # just lesson 02
python scripts/compile_shaders.py -v         # verbose (show dxc commands)
```

## Project Structure

```text
forge-gpu/
├── lessons/
│   ├── math/              Math lessons — standalone programs + theory
│   │   ├── README.md      Overview and navigation
│   │   └── NN-concept/    Each concept: program, README, updates math lib
│   └── gpu/               GPU lessons — SDL API and rendering
│       ├── 01-hello-window/
│       ├── 02-first-triangle/
│       ├── 03-uniforms-and-motion/
│       ├── 04-textures-and-samplers/
│       ├── 05-mipmaps/
│       ├── 06-depth-and-3d/
│       └── 07-camera-and-input/
├── common/
│   ├── math/              Math library (header-only, documented, reusable)
│   │   ├── forge_math.h   Vectors, matrices, common operations
│   │   ├── README.md      API reference and usage guide
│   │   └── DESIGN.md      Design decisions and conventions
│   └── forge.h            Shared utilities for lessons
├── tests/                 Test suite
│   └── math/              Math library tests (CTest integration)
├── .claude/skills/        Claude Code skills (AI-invokable patterns)
│   ├── math-lesson/       Add math concept + lesson + update library
│   ├── new-lesson/        Create new GPU lesson
│   ├── sdl-gpu-setup/     Scaffold SDL3 GPU application
│   └── ...
├── scripts/               Helper scripts (run, setup, shader compilation)
├── third_party/SDL/       SDL3 source (submodule, for reference)
├── PLAN.md                Lesson roadmap and progress
├── CLAUDE.md              AI coding guidelines for this project
└── CMakeLists.txt         Root build configuration
```

**How it fits together:**

- **Math lessons** teach concepts and add to `common/math/`
- **GPU lessons** use the math library and link to math lessons for theory
- **Skills** automate lesson creation and teach AI agents the patterns
- **Math library** is reusable in lessons and your own projects

## Skills — Build with AI

Every lesson distills into a **[Claude Code skill](https://code.claude.com/docs/en/skills)**
that teaches AI agents the same pattern. Copy `.claude/skills/` into your own
project to enable Claude to build games and tools with you.

### Building skills (use these in your projects)

| Skill | Invoke with | What it does |
|-------|-------------|--------------|
| [sdl-gpu-setup](.claude/skills/sdl-gpu-setup/SKILL.md) | `/sdl-gpu-setup` | Scaffold SDL3 GPU app with window, swapchain, render loop |
| [first-triangle](.claude/skills/first-triangle/SKILL.md) | `/first-triangle` | Add vertex buffers, shaders, pipeline — draw colored geometry |
| [uniforms-and-motion](.claude/skills/uniforms-and-motion/SKILL.md) | `/uniforms-and-motion` | Pass data to shaders with push uniforms, animate geometry |
| [textures-and-samplers](.claude/skills/textures-and-samplers/SKILL.md) | `/textures-and-samplers` | Load images, create GPU textures/samplers, draw textured geometry |
| [mipmaps](.claude/skills/mipmaps/SKILL.md) | `/mipmaps` | Create mipmapped textures, trilinear filtering, LOD control |
| [depth-and-3d](.claude/skills/depth-and-3d/SKILL.md) | `/depth-and-3d` | Depth buffer, MVP pipeline, 3D rendering, back-face culling |
| [camera-and-input](.claude/skills/camera-and-input/SKILL.md) | `/camera-and-input` | First-person camera, keyboard/mouse input, delta time |

### Development skills (used within this repo)

| Skill | Invoke with | What it does |
|-------|-------------|--------------|
| [math-lesson](.claude/skills/math-lesson/SKILL.md) | `/math-lesson` | Add math concept: lesson + program + update library |
| [new-lesson](.claude/skills/new-lesson/SKILL.md) | `/new-lesson` | Scaffold a new GPU lesson with all required files |
| [publish-lesson](.claude/skills/publish-lesson/SKILL.md) | `/publish-lesson` | Validate, commit, and PR a completed lesson |

**How to use:**

1. Copy `.claude/skills/` into your project root
2. Tell Claude: *"use the sdl-gpu-setup skill to create an SDL GPU application"*
3. Or just type `/sdl-gpu-setup` in the chat
4. Claude can also invoke skills automatically when relevant

**Philosophy:** Skills teach AI agents the same patterns you learned from
lessons. When Claude knows the fundamentals (from skills), and you understand
them too (from lessons), you can build together confidently.

## Learning with Claude

This project is designed for exploration with an AI assistant. Using
[Claude Code](https://claude.ai/code) or [Claude](https://claude.ai), you can:

**While learning:**

- *"What does SDL_ClaimWindowForGPUDevice actually do?"*
- *"Why do we need a transfer buffer to upload vertex data?"*
- *"Explain the dot product and show me the math lesson"*

**While building:**

- *"Use the sdl-gpu-setup skill to create an SDL GPU application"*
- *"Add a rotating quad using the math library"*
- *"Help me add textures to my renderer"*

Claude has access to the lessons, skills, math library, and SDL reference code.
When you understand the fundamentals AND Claude knows the patterns (via skills),
you can build together productively. If you hit a problem, dive into the
relevant lesson to understand it better.

## License

[zlib](LICENSE) — the same license as SDL itself.
