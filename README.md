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

### Math Lessons (lessons/math/)

*Coming soon* — Standalone programs teaching the math behind graphics:

- Vectors, dot/cross products, normalization
- Matrices, transformations, coordinate spaces
- Quaternions, interpolation, and more

Each math lesson includes a small demo program and updates the shared math
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

## Building

You need:

- **CMake 3.24+**
- **A C compiler** (MSVC, GCC, or Clang)
- **A GPU** with Vulkan, Direct3D 12, or Metal support

SDL3 is fetched automatically — no manual installation required.

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

Run a lesson:

```bash
# Windows
build\lessons\01-hello-window\Debug\01-hello-window.exe

# Linux / macOS
./build/lessons/01-hello-window/01-hello-window
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

After installing, make sure the Vulkan SDK `dxc` is on your PATH, or use
the full path. On Windows the default location is:

```text
C:\VulkanSDK\<version>\Bin\dxc.exe
```

> **Heads up:** If you just type `dxc` and get *"SPIR-V CodeGen not
> available"*, you're hitting the Windows SDK `dxc` instead of the Vulkan
> SDK one. Use the full path to the Vulkan SDK `dxc` or put its `Bin/`
> directory earlier on your PATH.

From a lesson's directory (e.g. `lessons/03-uniforms-and-motion/`):

```bash
# Compile HLSL → SPIR-V (Vulkan)
dxc -spirv -T vs_6_0 -E main shaders/triangle.vert.hlsl -Fo shaders/triangle.vert.spv
dxc -spirv -T ps_6_0 -E main shaders/triangle.frag.hlsl -Fo shaders/triangle.frag.spv

# Compile HLSL → DXIL (Direct3D 12)
dxc -T vs_6_0 -E main shaders/triangle.vert.hlsl -Fo shaders/triangle.vert.dxil
dxc -T ps_6_0 -E main shaders/triangle.frag.hlsl -Fo shaders/triangle.frag.dxil
```

Then embed the compiled bytecodes as C headers using the helper script:

```bash
python ../../scripts/bin_to_header.py shaders/triangle.vert.spv triangle_vert_spirv shaders/triangle_vert_spirv.h
python ../../scripts/bin_to_header.py shaders/triangle.frag.spv triangle_frag_spirv shaders/triangle_frag_spirv.h
python ../../scripts/bin_to_header.py shaders/triangle.vert.dxil triangle_vert_dxil shaders/triangle_vert_dxil.h
python ../../scripts/bin_to_header.py shaders/triangle.frag.dxil triangle_frag_dxil shaders/triangle_frag_dxil.h
```

The flags: `-T vs_6_0` = vertex shader model 6.0, `-T ps_6_0` = pixel
(fragment) shader model 6.0, `-E main` = entry point function name,
`-spirv` = emit SPIR-V instead of DXIL.

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
│       └── 03-uniforms-and-motion/
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
├── scripts/               Build helpers (shader compilation, etc.)
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

## Philosophy

- **Learning enables building** — understand fundamentals, then use AI to build faster
- **One concept at a time** — each lesson builds on the last
- **No magic** — every constant is named, every GPU call is explained
- **Reusable code** — math library and skills are production-ready, not just educational
- **C99** — matching SDL's own style, accessible to the widest audience
- **SDL callbacks** — modern SDL3 application model (`SDL_AppInit` / `SDL_AppIterate` / etc.)
- **Dual audience** — lessons teach humans, skills teach AI — both learn the same patterns

## License

[zlib](LICENSE) — the same license as SDL itself.
