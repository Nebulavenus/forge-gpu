# Lesson 01 — Hello Window

## What you'll learn

- How to initialise SDL3 and create a GPU device
- What a **swapchain** is and how it connects the GPU to your window
- The per-frame rhythm: **command buffer → render pass → submit**
- How clearing the screen already uses the same machinery as drawing

## Result

A dark blue-grey window — not exciting to look at, but the entire GPU frame
pipeline is running under the hood.

![Lesson 01 result](assets/screenshot.png)

## Key concepts

### GPU Device

`SDL_CreateGPUDevice` gives you a handle to the graphics backend.  SDL
automatically picks from Vulkan, Direct3D 12, or Metal depending on your
platform and the shader formats you declare.

### Command Buffers

All GPU work is recorded into a **command buffer** before being submitted in
one batch.  This lets the driver optimise the work and avoids back-and-forth
between CPU and GPU every call.

### Swapchain

The swapchain is a set of textures owned by the OS windowing system.  Each
frame you *acquire* one, render into it, and *present* it.  The OS composites
it to the screen.

### Render Pass

A render pass defines *what* you're rendering into (colour target, optional
depth buffer) and *how* it starts (clear, load previous contents, or don't
care).  Even a simple screen clear needs a render pass — there's no standalone
"clear screen" call in modern GPU APIs.

## Math connections

This lesson is purely setup — no math required yet. Starting with Lesson 02,
you'll need vectors and coordinate transforms to place geometry on screen:

- [Math 01 — Vectors](../../math/01-vectors/README.md) — positions, directions,
  and the building blocks for everything in GPU programming
- [Math 02 — Coordinate Spaces](../../math/02-coordinate-spaces/README.md) —
  how vertices move from model space to your screen

## Building

From the repository root:

```bash
cmake -B build
cmake --build build
```

Run:

```bash
python scripts/run.py 01

# Or directly:
# Windows
build\lessons\gpu\01-hello-window\Debug\01-hello-window.exe
# Linux / macOS
./build/lessons/gpu/01-hello-window/01-hello-window
```

## AI skill

This lesson has a matching Claude Code skill:
[`sdl-gpu-setup`](../../../.claude/skills/sdl-gpu-setup/SKILL.md) — invoke it
with `/sdl-gpu-setup` or copy it into your own project's `.claude/skills/`
directory.  It distils the setup pattern from this lesson into a reusable
reference that AI assistants can follow.

## Exercises

1. **Change the clear colour** — pick an RGB value you like and update the
   `CLEAR_*` defines.  Rebuild and confirm it changes.

2. **Animate the colour** — use `SDL_GetTicks()` to vary the clear colour
   over time.  Hint: `sinf()` mapped from [-1,1] to [0,1] gives a nice pulse.

3. **Print the frame time** — call `SDL_GetTicks()` at the start and end of
   each frame and `SDL_Log` the delta.  How fast is your machine clearing a
   screen?

4. **Handle window resize** — listen for `SDL_EVENT_WINDOW_RESIZED`.  Does the
   clear still work when you drag the window edges?  (Spoiler: yes — the
   swapchain handles it.  But verify it yourself.)
