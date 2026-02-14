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
| 03 | Uniforms & Motion | *coming soon* — uniform buffers, animating with time |

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
│   └── 02-first-triangle/
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
| [new-lesson](.claude/skills/new-lesson/SKILL.md) | `/new-lesson 03 uniforms "Add uniform buffers"` | Scaffold a new lesson with all required files |

**Usage:** Clone this repo (or copy the `.claude/skills/` directory into your
project), then tell Claude: *"use the sdl-gpu-setup skill to create an SDL GPU
application"* — or just type `/sdl-gpu-setup`.

## Philosophy

- **One concept at a time** — each lesson builds on the last
- **No magic** — every constant is named, every GPU call is explained
- **C99** — matching SDL's own style, accessible to the widest audience
- **SDL callbacks** — lessons use `SDL_AppInit` / `SDL_AppIterate` / `SDL_AppEvent` / `SDL_AppQuit`, the modern SDL3 application model
- **Dual audience** — every lesson teaches a human *and* produces a skill that teaches an AI the same pattern

## License

[zlib](LICENSE) — the same license as SDL itself.
