# forge-gpu

Learn real-time graphics programming from the ground up using
[SDL's GPU API](https://wiki.libsdl.org/SDL3/CategoryGPU) and C.

Each lesson is a standalone program that introduces one new concept, building
toward a complete understanding of modern GPU rendering. Every line is
commented to explain *why*, not just *what*.

## Lessons

| # | Name | What you'll learn |
|---|------|-------------------|
| 01 | [Hello Window](lessons/01-hello-window/) | GPU device, swapchain, command buffers, render passes |
| 02 | First Triangle | *coming soon* — vertex buffers, shaders, graphics pipeline |
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
│   └── 01-hello-window/
│       ├── main.c
│       ├── CMakeLists.txt
│       └── README.md
├── skills/          Claude Code skill files for working with SDL GPU
├── PLAN.md          Lesson roadmap and progress
├── CLAUDE.md        AI coding guidelines for this project
└── CMakeLists.txt   Root build — fetches SDL3, wires all lessons
```

## Philosophy

- **One concept at a time** — each lesson builds on the last
- **No magic** — every constant is named, every GPU call is explained
- **C99** — matching SDL's own style, accessible to the widest audience
- **SDL callbacks** — lessons use `SDL_AppInit` / `SDL_AppIterate` / `SDL_AppEvent` / `SDL_AppQuit`, the modern SDL3 application model

## License

[zlib](LICENSE) — the same license as SDL itself.
