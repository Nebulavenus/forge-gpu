# Lesson 01 — Hello Window

## What you'll learn

- What **SDL** is and why we use it for graphics programming
- What a **GPU** is and why rendering uses it instead of the CPU
- How to create a **window** — the rectangle on screen where your program draws
- How to initialise SDL3 and create a **GPU device**
- What a **swapchain** is and how it connects the GPU to your window
- The **callback architecture** — how SDL runs your program
- The per-frame rhythm: **command buffer → render pass → submit**
- How clearing the screen already uses the same machinery as drawing

## Prerequisites

This is the first GPU lesson — no earlier GPU lessons are required. If you are
new to the C programming language, work through
[Engine 01 — Intro to C](../../engine/01-intro-to-c/) first. That lesson
covers the types, functions, pointers, and structs used throughout this code.

## Result

A dark blue-grey window — not exciting to look at, but the entire GPU frame
pipeline is running under the hood.

![Lesson 01 result](assets/screenshot.png)

## Background

### What is SDL?

**SDL** (Simple DirectMedia Layer) is a cross-platform library written in C
that provides access to graphics, audio, input, and windowing on every major
operating system. Instead of writing separate code for Windows, macOS, and
Linux, you write your program once using SDL's functions and it works
everywhere.

SDL3 is the latest major version. This lesson series uses SDL3's **GPU API**,
which provides a single interface for sending work to the graphics hardware
regardless of which low-level graphics system the operating system uses.

### What is a GPU?

A **GPU** (Graphics Processing Unit) is a processor designed to perform many
simple operations in parallel. Your computer's main processor (the **CPU**)
is good at running complex, sequential tasks. A GPU is good at running
thousands of identical operations simultaneously — exactly what rendering
requires, because every pixel on screen needs similar calculations performed
independently.

When you draw a 3D scene, the GPU processes every vertex and every pixel in
parallel. A modern display at 1920×1080 has over two million pixels, and the
GPU recomputes all of them every frame (typically 60 times per second). The
CPU could not keep up with this workload, but the GPU handles it because of
its massively parallel design.

### What is a window?

A **window** is the rectangular area on your screen where a program displays
its content. When you open a web browser, a text editor, or a game, each one
gets its own window managed by the operating system. In this lesson, we create
a window using `SDL_CreateWindow` and then draw into it using the GPU.

### What are Vulkan, Direct3D 12, and Metal?

These are the low-level **graphics APIs** — the interfaces that let software
talk to the GPU hardware:

| API          | Platform          | Maintained by |
|--------------|-------------------|---------------|
| **Vulkan**   | Windows, Linux, Android | Khronos Group |
| **Direct3D 12** | Windows, Xbox  | Microsoft     |
| **Metal**    | macOS, iOS        | Apple         |

Each API speaks a different language, so a program written for Vulkan would
not run on macOS (which only supports Metal). SDL's GPU API solves this
problem: you write your rendering code once using SDL's functions, and SDL
translates it to whichever low-level API is available on the current platform.

### What is the callback architecture?

Most programs you have written probably have a `main` function where execution
starts and you control the flow. SDL3 uses a different pattern called
**callbacks**: instead of you writing the main loop, SDL provides the main
loop and calls *your* functions at the right moments.

You implement four **callback functions** that SDL calls automatically:

| Callback          | When SDL calls it                 | Your job                                  |
|-------------------|-----------------------------------|-------------------------------------------|
| `SDL_AppInit`     | Once, at startup                  | Create the window, GPU device, and any resources |
| `SDL_AppEvent`    | Once per input event (key press, mouse move, quit) | Respond to user input         |
| `SDL_AppIterate`  | Once per frame                    | Record and submit GPU work (rendering)    |
| `SDL_AppQuit`     | Once, at shutdown                 | Release all resources and clean up        |

This pattern ensures that SDL handles platform-specific details (event
pumping, timing, mobile lifecycle) while you focus on what to draw.

The line `#define SDL_MAIN_USE_CALLBACKS 1` at the top of `main.c` tells SDL
to use this callback architecture. You do not write a `main` function — SDL
provides one internally.

## Key concepts

### GPU device

A **GPU device** is a handle that represents your connection to the GPU
hardware. All GPU operations — creating resources, recording commands,
submitting work — go through this device.

`SDL_CreateGPUDevice` creates the device. You tell it which **shader
formats** your program can provide (SPIRV for Vulkan, DXIL for Direct3D 12,
MSL for Metal), and SDL picks the best available backend:

```c
SDL_GPUDevice *device = SDL_CreateGPUDevice(
    SDL_GPU_SHADERFORMAT_SPIRV |   /* Vulkan  */
    SDL_GPU_SHADERFORMAT_DXIL  |   /* D3D12   */
    SDL_GPU_SHADERFORMAT_MSL,      /* Metal   */
    true,                          /* enable debug/validation */
    NULL                           /* no backend preference   */
);
```

The `true` parameter enables **debug validation**, which makes the graphics
driver check for mistakes in your API usage. Always enable this during
development — the error messages it produces help catch problems early.

### Command buffers

The CPU and GPU are separate processors that run at the same time. To avoid
the overhead of sending instructions one at a time, you record all the GPU
work for a frame into a **command buffer** and then submit the entire batch at
once.

The sequence each frame is:

1. **Acquire** a command buffer from the device
2. **Record** operations into it (render passes, draws, clears)
3. **Submit** the completed command buffer — the GPU executes everything

### Swapchain

The **swapchain** is a set of textures managed by the operating system's
windowing system. Each frame, your program *acquires* one of these textures,
renders into it, and then *presents* it — at which point the OS displays it in
your window.

The swapchain typically contains two or three textures and rotates between
them. While the GPU renders into one texture, the OS displays a previously
completed one. This avoids visual tearing (where the display shows a
partially rendered frame).

`SDL_ClaimWindowForGPUDevice` creates the swapchain by binding a window to
your GPU device. `SDL_AcquireGPUSwapchainTexture` grabs the next available
texture each frame.

### Render pass

A **render pass** defines *what* you are rendering into (a colour target, and
optionally a depth buffer) and *how* it starts:

- **Clear** — fill the target with a solid colour before drawing
- **Load** — keep the previous contents
- **Don't care** — the contents are undefined (used when you know you will
  overwrite every pixel)

Even a simple screen clear needs a render pass — there is no standalone "clear
screen" call in modern GPU APIs. This design exists because GPUs are optimised
to work within the render pass structure, which lets the driver manage memory
and scheduling efficiently.

### sRGB swapchain

The code requests an **sRGB** swapchain using
`SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR`. This tells the GPU to automatically
convert the linear colour values you provide into the sRGB colour space that
monitors expect. Without this conversion, colours appear washed out or too
dark. We will explain sRGB and gamma correction in detail in a later lesson —
for now, this is the correct default to use.

## Math connections

This lesson is purely setup — no math required yet. Starting with Lesson 02,
you'll need vectors and coordinate transforms to place geometry on screen:

- [Math 01 — Vectors](../../math/01-vectors/README.md) — positions, directions,
  and the building blocks for everything in GPU programming
- [Math 02 — Coordinate Spaces](../../math/02-coordinate-spaces/README.md) —
  how vertices move from model space to your screen

## Engine connections

This lesson uses C language features and build system patterns taught in the
engine lessons. If you are new to C or CMake, or if the build fails:

- [Engine 01 — Intro to C](../../engine/01-intro-to-c/) — the C language
  features used throughout this lesson: types, functions, pointers, structs,
  and `typedef`
- [Engine 02 — CMake Fundamentals](../../engine/02-cmake-fundamentals/) —
  `add_executable`, `target_link_libraries`, generator expressions, and how
  to read build errors
- [Engine 03 — FetchContent & Dependencies](../../engine/03-fetchcontent-dependencies/) —
  how SDL3 is automatically downloaded and built when you run `cmake -B build`
- [Engine 06 — Reading Error Messages](../../engine/06-reading-error-messages/) —
  how to diagnose compiler, linker, and runtime errors if something goes wrong

## Building

From the repository root:

```bash
cmake -B build
cmake --build build
```

The first `cmake -B build` command configures the project. If SDL3 is not
already installed on your system, CMake automatically downloads and builds it
(see [Engine 03](../../engine/03-fetchcontent-dependencies/) for details). The
second command compiles the lesson.

Run:

```bash
python scripts/run.py 01

# Or directly:
# Windows
build\lessons\gpu\01-hello-window\Debug\01-hello-window.exe
# Linux / macOS
./build/lessons/gpu/01-hello-window/01-hello-window
```

You should see a dark blue-grey window. The program logs which graphics
backend SDL chose (Vulkan, D3D12, or Metal) — check your terminal output.

## AI skill

This lesson has a matching Claude Code skill:
[`sdl-gpu-setup`](../../../.claude/skills/sdl-gpu-setup/SKILL.md) — invoke it
with `/sdl-gpu-setup` or copy it into your own project's `.claude/skills/`
directory. It distils the setup pattern from this lesson into a reusable
reference that AI assistants can follow.

## Exercises

1. **Change the clear colour** — pick an RGB value you like and update the
   `CLEAR_*` defines. Rebuild and confirm it changes.

2. **Animate the colour** — use `SDL_GetTicks()` to vary the clear colour
   over time. Hint: `sinf()` mapped from [-1,1] to [0,1] gives a nice pulse.

3. **Print the frame time** — call `SDL_GetTicks()` at the start and end of
   each frame and `SDL_Log` the delta. How fast is your machine clearing a
   screen?

4. **Handle window resize** — listen for `SDL_EVENT_WINDOW_RESIZED`. Does the
   clear still work when you drag the window edges? (Spoiler: yes — the
   swapchain handles it. But verify it yourself.)
