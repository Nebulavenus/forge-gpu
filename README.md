# forge-gpu

Learn real-time graphics programming from the ground up using
[SDL's GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU) and C.

Each lesson is a standalone program that introduces one new concept, building
toward a complete understanding of modern GPU rendering. Every line is
commented to explain *why*, not just *what*.

**This project teaches two audiences at once.** Each lesson teaches a human
reader, and each lesson also produces a reusable *skill file* that teaches an
AI agent the same pattern. The goal: enable people to use Claude (or any AI
assistant) to build games and renderers with SDL GPU confidently.

## Lessons

| # | Name | What you'll learn |
|---|------|-------------------|
| 01 | [Hello Window](lessons/01-hello-window/) | GPU device, swapchain, command buffers, render passes |
| 02 | [First Triangle](lessons/02-first-triangle/) | Vertex buffers, shaders, graphics pipeline |
| 03 | [Uniforms & Motion](lessons/03-uniforms-and-motion/) | Uniform buffers, push uniforms, animating with time |

See [PLAN.md](PLAN.md) for the full roadmap.

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

```
forge-gpu/
├── common/          Header-only utilities shared across lessons
│   └── forge.h
├── lessons/         One directory per lesson, each a standalone program
│   ├── 01-hello-window/
│   │   ├── main.c
│   │   ├── CMakeLists.txt
│   │   └── README.md
│   ├── 02-first-triangle/
│   │   ├── main.c
│   │   ├── shaders/        HLSL source + compiled SPIRV/DXIL headers
│   │   ├── CMakeLists.txt
│   │   └── README.md
│   └── 03-uniforms-and-motion/
│       ├── main.c
│       ├── shaders/        HLSL source + compiled SPIRV/DXIL headers
│       ├── CMakeLists.txt
│       └── README.md
├── .claude/
│   └── skills/      Claude Code skills — one per lesson, invokable with /name
│       └── sdl-gpu-setup/SKILL.md
├── PLAN.md          Lesson roadmap and progress
├── CLAUDE.md        AI coding guidelines for this project
└── CMakeLists.txt   Root build — fetches SDL3, wires all lessons
```

## AI Skills

Every lesson comes with a matching **[Claude Code skill](https://code.claude.com/docs/en/skills)**
in `.claude/skills/`. These are invokable with `/skill-name` or loaded
automatically when Claude determines they're relevant. Copy them into your own
project's `.claude/skills/` to teach your AI assistant the same patterns.

| Skill | Invoke with | Pattern |
|-------|-------------|---------|
| [sdl-gpu-setup](.claude/skills/sdl-gpu-setup/SKILL.md) | `/sdl-gpu-setup` | SDL3 GPU app with callbacks, window, swapchain, render loop |
| [first-triangle](.claude/skills/first-triangle/SKILL.md) | `/first-triangle` | Vertex buffers, shaders, graphics pipeline — draw colored geometry |
| [uniforms-and-motion](.claude/skills/uniforms-and-motion/SKILL.md) | `/uniforms-and-motion` | Push uniforms, passing time to shaders, animating geometry |
| [new-lesson](.claude/skills/new-lesson/SKILL.md) | `/new-lesson 04 textures "Add textures"` | Scaffold a new lesson with all required files |

**Usage:** Clone this repo (or copy the `.claude/skills/` directory into your
project), then tell Claude: *"use the sdl-gpu-setup skill to create an SDL GPU
application"* — or just type `/sdl-gpu-setup`.

## Learning with Claude

This project is designed to be explored with an AI assistant. If you're using
[Claude Code](https://claude.ai/code) or [Claude](https://claude.ai), you can
ask questions as you work through the lessons:

- *"What does SDL_ClaimWindowForGPUDevice actually do?"*
- *"Why do we need a transfer buffer to upload vertex data?"*
- *"Help me add a second triangle to Lesson 02"*

Claude has access to the lesson source, the skill files, and the SDL reference
code in `third_party/SDL`. Treat it like a knowledgeable study partner — ask
it to explain concepts, help debug your exercises, or build something new
using the patterns from the lessons.

## Philosophy

- **One concept at a time** — each lesson builds on the last
- **No magic** — every constant is named, every GPU call is explained
- **C99** — matching SDL's own style, accessible to the widest audience
- **SDL callbacks** — lessons use `SDL_AppInit` / `SDL_AppIterate` / `SDL_AppEvent` / `SDL_AppQuit`, the modern SDL3 application model
- **Dual audience** — every lesson teaches a human *and* produces a skill that teaches an AI the same pattern

## License

[zlib](LICENSE) — the same license as SDL itself.
