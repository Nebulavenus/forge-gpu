# Lesson 01 — Hello Window

## What you'll learn

- What **SDL** is and why we use it for graphics programming
- What a **GPU** is and why rendering uses it instead of the CPU
- How to create a **window** — the rectangle on screen where your program draws
- How to initialize SDL3 and create a **GPU device**
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

### Initializing SDL

Before using any SDL function, you initialize the library by calling
`SDL_Init` with flags for the subsystems you need. For rendering, that means
video:

```c
if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("SDL_Init failed: %s", SDL_GetError());
    return SDL_APP_FAILURE;
}
```

SDL3 changed the return convention from SDL2: functions that can fail return
`true` on success and `false` on failure. When something goes wrong,
`SDL_GetError()` returns a human-readable description of the problem. This
pattern — check the return value, log the error, handle the failure — appears
in every lesson from here on.

### GPU device

A **GPU device** is a handle that represents your connection to the GPU
hardware. All GPU operations — creating resources, recording commands,
submitting work — go through this device. Think of it as opening a
communication channel: until you have a device, you cannot ask the GPU to do
anything.

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

The shader format flags deserve explanation. Vulkan, Direct3D 12, and Metal
each require shaders in a different compiled format. By listing all three, your
program can run on any platform — SDL selects whichever backend the current
system supports. This lesson has no shaders yet, but later lessons compile
HLSL source into SPIRV and DXIL bytecode that gets embedded in the program.

The `true` parameter enables **debug validation**, which makes the graphics
driver check for mistakes in your API usage. Always enable this during
development — the error messages it produces help catch problems early.

### Creating a window and claiming it for the GPU

A window and a GPU device start out unrelated — the window is managed by the
operating system, while the device talks to the graphics hardware. You connect
them with `SDL_ClaimWindowForGPUDevice`:

```c
SDL_Window *window = SDL_CreateWindow("Title", 1280, 720, 0);

/* Bind the window to the GPU device — this creates the swapchain */
SDL_ClaimWindowForGPUDevice(device, window);
```

This single call creates the **swapchain** — a set of textures that act as a
bridge between the GPU and the display. The GPU renders into one swapchain
texture while the operating system displays a previously completed one. This
double (or triple) buffering prevents **tearing**, where the display shows a
half-finished frame because the GPU was still writing to it.

After claiming, you can also configure the swapchain's color format. The code
in `main.c` requests `SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR`, which gives
us an sRGB-aware swapchain (more on this below in [sRGB and linear
color](#srgb-and-linear-color)).

### Application state

SDL's callback architecture means your code is split across four separate
functions. Those functions need to share data — the GPU device created in
`SDL_AppInit` must be available in `SDL_AppIterate` to record commands, and
in `SDL_AppQuit` to be destroyed. SDL solves this with a shared state pointer.

You define a struct holding everything your application needs:

```c
typedef struct app_state {
    SDL_Window    *window;
    SDL_GPUDevice *device;
} app_state;
```

In `SDL_AppInit`, you allocate this struct and assign it to the `appstate`
output parameter. SDL then passes that pointer into every subsequent callback.
This is the mechanism by which your init, event, iterate, and quit functions
communicate — they all receive the same `app_state`.

As lessons add more resources (pipelines, textures, buffers), they all go into
this struct. The pattern stays the same: create in init, use in iterate,
destroy in quit.

### The frame loop

This is the most important concept in this lesson. Every GPU program — from
this simple color clear through complex 3D scenes with shadows, reflections,
and post-processing — runs the same fundamental loop every frame. The steps
grow more elaborate, but the structure never changes.

Here is what happens each time `SDL_AppIterate` is called:

#### 1. Acquire a command buffer

```c
SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
```

The CPU and GPU are separate processors running simultaneously. You cannot
call the GPU directly the way you call a C function — the GPU is busy
executing the *previous* frame's work while you are preparing the next one.
Instead, you record everything you want the GPU to do into a **command
buffer**, which is a list of instructions that the GPU will execute later as a
batch.

Acquiring a command buffer gives you an empty recording to fill. You will add
render passes, draw calls, and resource operations to it, then submit the
whole thing at once.

#### 2. Acquire the swapchain texture

```c
SDL_GPUTexture *swapchain = NULL;
SDL_AcquireGPUSwapchainTexture(cmd, window, &swapchain, NULL, NULL);
```

Before you can render anything to the screen, you need a texture to render
*into*. The swapchain provides this — each frame, you ask for the next
available texture, render into it, and when you submit the command buffer the
operating system displays it in your window.

The swapchain texture can be `NULL`. This happens when the window is minimized
or otherwise not visible — there is no surface to render to. When this occurs,
you skip the render pass and submit an empty command buffer. This is not an
error; it is normal operation that every SDL_GPU program must handle.

#### 3. Set up a color target

```c
SDL_GPUColorTargetInfo color_target = { 0 };
color_target.texture     = swapchain;
color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
color_target.store_op    = SDL_GPU_STOREOP_STORE;
color_target.clear_color = (SDL_FColor){ 0.02f, 0.02f, 0.03f, 1.0f };
```

A **color target** describes what the GPU will render into during a render pass
and how the GPU should initialize and finalize it. The fields:

- **`texture`** — the texture to render into (the swapchain texture we just
  acquired).
- **`load_op`** — what happens to the texture at the *start* of the render
  pass. `CLEAR` fills it with a solid color. `LOAD` preserves whatever was
  already there. `DONT_CARE` means the contents are undefined (used when you
  know every pixel will be overwritten, which lets the driver skip a memory
  read).
- **`store_op`** — what happens at the *end* of the render pass. `STORE`
  writes the results back to the texture. `DONT_CARE` discards them (used for
  temporary render targets that will not be read again).
- **`clear_color`** — the RGBA color to fill when `load_op` is `CLEAR`.

This structure appears in every lesson that renders anything. In later lessons
you will add depth targets, use `LOAD` to preserve previous passes, and render
into off-screen textures for post-processing — but the concept stays the same:
you always tell the GPU *what* to render into and *how* to begin and end.

#### 4. Begin and end a render pass

```c
SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
    cmd,
    &color_target, 1,   /* one color target */
    NULL                /* no depth/stencil target */
);

/* Draw commands would go here — we have none yet */

SDL_EndGPURenderPass(pass);
```

A **render pass** is a scope within a command buffer where drawing happens. All
draw commands, and even simple clears, must occur inside a render pass — there
is no standalone "clear screen" function in modern GPU APIs.

Why? The GPU hardware is designed around render passes. When you begin one, the
GPU configures its internal memory (called tile memory on mobile GPUs) for the
target you specified. Draws execute against that memory. When you end the
pass, the results are written out. This structure lets the driver schedule
memory and execution efficiently. It also means the driver knows the full scope
of your rendering up front, which enables important optimizations.

`SDL_BeginGPURenderPass` takes the command buffer, an array of color targets
(we have one), the count, and an optional depth/stencil target (NULL for now —
we add depth in Lesson 06). Between begin and end, you issue draw calls. In
this lesson we have no draw calls, so the only work the render pass does is the
clear — but the machinery is identical to what a complex scene uses.

#### 5. Submit the command buffer

```c
SDL_SubmitGPUCommandBuffer(cmd);
```

Submission hands the completed command buffer to the GPU for execution. The GPU
processes the recorded commands — in our case, clearing the swapchain texture
to a dark color — and presents the result to the window.

After submission, you must not touch that command buffer again. The GPU owns it
now and will release it when execution finishes. Next frame,
`SDL_AppIterate` is called again and the entire loop repeats: acquire a new
command buffer, acquire the next swapchain texture, set up targets, render,
submit.

#### Putting it together

Here is the complete frame loop as it appears in `main.c`. Every lesson from
here through Lesson 27 builds on this same structure — the only difference is
what happens between `SDL_BeginGPURenderPass` and `SDL_EndGPURenderPass`:

```c
/* 1. Acquire command buffer */
SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);

/* 2. Acquire swapchain texture */
SDL_GPUTexture *swapchain = NULL;
SDL_AcquireGPUSwapchainTexture(cmd, window, &swapchain, NULL, NULL);

if (swapchain) {
    /* 3. Set up color target */
    SDL_GPUColorTargetInfo color_target = { 0 };
    color_target.texture     = swapchain;
    color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op    = SDL_GPU_STOREOP_STORE;
    color_target.clear_color = (SDL_FColor){ 0.02f, 0.02f, 0.03f, 1.0f };

    /* 4. Render pass */
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, NULL);
    SDL_EndGPURenderPass(pass);
}

/* 5. Submit */
SDL_SubmitGPUCommandBuffer(cmd);
```

Notice the `if (swapchain)` guard. When the window is minimized, the
swapchain texture is `NULL` and there is nothing to render into. The command
buffer is still submitted — it simply contains no render pass.

### Handling events

`SDL_AppEvent` is called once for each input event — key presses, mouse
movement, window resize, and quit requests. For now, the only event we handle
is quit:

```c
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;   /* signal SDL to shut down */
    }
    return SDL_APP_CONTINUE;      /* keep running */
}
```

The return value controls program flow. `SDL_APP_CONTINUE` keeps the loop
running. `SDL_APP_SUCCESS` tells SDL to stop and call `SDL_AppQuit` for
cleanup. `SDL_APP_FAILURE` does the same but indicates an error occurred.
Later lessons add keyboard and mouse handling here for camera control and
interaction.

### Resource cleanup

When the program exits, `SDL_AppQuit` releases everything that was created
during init. Resources are released in **reverse order** — the last thing
created is the first thing destroyed:

```c
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    app_state *state = (app_state *)appstate;
    if (state) {
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
    }
}
```

Reverse order matters because resources depend on each other. The swapchain
(created by `SDL_ClaimWindowForGPUDevice`) depends on both the window and the
device, so you release it first. The window depends on the device, so it is
destroyed next. The device goes last. Destroying in the wrong order can cause
crashes or validation errors.

As lessons add more resources — pipelines, textures, buffers, samplers — they
all follow this same pattern: create in `SDL_AppInit`, use in
`SDL_AppIterate`, destroy in `SDL_AppQuit` in reverse order.

SDL calls `SDL_Quit()` automatically after `SDL_AppQuit` returns, so you do
not need to call it yourself.

### Error handling

Every SDL_GPU call that can fail is checked, and failures are handled
immediately. This is not optional — ignoring return values from GPU functions
leads to crashes, corrupted rendering, or silent failures that are difficult
to diagnose.

The pattern is consistent throughout every lesson:

```c
if (!SDL_SomeGPUFunction(...)) {
    SDL_Log("SDL_SomeGPUFunction failed: %s", SDL_GetError());
    /* clean up any resources already created */
    return SDL_APP_FAILURE;
}
```

Three elements: check the return value, log a descriptive message with
`SDL_GetError()`, and clean up before returning. In `SDL_AppInit`, cleanup
means destroying resources in reverse creation order. In `SDL_AppIterate`,
returning `SDL_APP_FAILURE` triggers `SDL_AppQuit`, which handles full
cleanup.

### sRGB and linear color

The code requests an **sRGB** swapchain using
`SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR`. This is a single line of setup, but
the concept behind it matters for understanding why colors look the way they
do.

Monitors do not display light linearly. If you send a pixel value of 0.5 to a
monitor, it does not produce half the brightness of 1.0 — it produces
something closer to a quarter. This nonlinear response is called **gamma**.
The **sRGB** color space compensates for this by encoding colors in a curve
that matches what monitors expect.

When you request `SDR_LINEAR`, the GPU automatically converts the linear color
values you provide into sRGB when writing to the swapchain. This means you can
work with physically correct linear values in your shaders and rendering code,
and the final image looks correct on screen.

That is why the clear color values (0.02, 0.02, 0.03) appear as a reasonable
dark grey rather than near-black — in linear space, low values are
perceptually darker than you might expect because the sRGB conversion
redistributes the range. We cover sRGB and gamma correction in full detail in
a later lesson. For now, the important point is: always request an sRGB
swapchain and work in linear color values.

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
