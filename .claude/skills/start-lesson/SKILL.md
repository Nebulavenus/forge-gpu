---
name: start-lesson
description: Scaffold an advanced GPU lesson directory with minimal main.c and PLAN.md for iterative scene building
argument-hint: "[number] [name] [description]"
disable-model-invocation: true
---

Scaffold a new advanced GPU lesson. Unlike `/new-lesson` (which creates a
complete lesson in one pass), `/start-lesson` creates a minimal buildable
project and a PLAN.md that describes the scene to build iteratively.

**When to use this skill:**

- The lesson needs a complex scene before the new concept can be introduced
  (shadows, HDR, bloom, normal maps, full camera, multiple models, etc.)
- You want to build the scene incrementally with agent assistance
- The scene setup would be too large and error-prone to generate in one pass

**When to use `/new-lesson` instead:**

- The lesson is simple enough to scaffold completely in one pass
- No complex scene prerequisites — just a new API concept on a basic setup

**Workflow for advanced lessons:**

1. `/start-lesson` — Scaffold directory + minimal main.c + PLAN.md (this skill)
2. Build the scene iteratively (manual work with agent assistance)
3. `/create-lesson` — Add README, skill file, screenshot to the working scene
4. `/final-pass` — Quality review
5. `/publish-lesson` — Commit and PR

## Arguments

The user (or you) provides:

- **Number**: two-digit lesson number (e.g. 20)
- **Name**: short kebab-case name (e.g. pbr-materials)
- **Description**: what the lesson teaches (e.g. "Physically-based rendering
  with metallic-roughness workflow")

If any are missing, ask the user before proceeding.

## Steps

### 1. Start from a clean main branch

Before creating any files, ensure we're working from the latest main:

```bash
git checkout main
git pull origin main
```

### 2. Determine scene requirements

Ask the user (or infer from the description) what the scene needs:

- **Camera**: fly camera? orbit camera? fixed?
- **Lighting**: directional? point lights? shadows? which type?
- **Materials**: Blinn-Phong? PBR? normal maps? which maps?
- **Post-processing**: HDR? bloom? tone mapping?
- **Models**: which glTF models? how many?
- **Other features**: instancing? compute? debug lines?

These go into the PLAN.md so the scene can be built step by step.

### 3. Check what math and common code is needed

- Check `common/math/forge_math.h` for required math operations
- If new math is needed, note it in the PLAN.md (use `/math-lesson` later)
- Identify which existing lessons provide reusable patterns for the scene
  features (e.g. "shadows from Lesson 15", "bloom from Lesson 20")

### 4. Create the lesson directory

`lessons/gpu/NN-name/`

### 5. Create a minimal `main.c`

This is a **minimal buildable program** — just enough to compile, show a
window, and clear the screen. The user/agent builds the full scene on top of
this foundation.

```c
/*
 * GPU Lesson NN — Title
 *
 * [Brief description of what the lesson will teach]
 *
 * This is the starting scaffold. The full scene will be built iteratively —
 * see PLAN.md for the scene requirements and status.
 *
 * SPDX-License-Identifier: Zlib
 */
#define SDL_MAIN_USE_CALLBACKS 1

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720
#define WINDOW_TITLE  "Lesson NN — Title"

/* ------------------------------------------------------------------ */
/* Application state                                                   */
/* ------------------------------------------------------------------ */
typedef struct AppState {
    SDL_Window    *window;
    SDL_GPUDevice *device;
} AppState;

/* ------------------------------------------------------------------ */
/* SDL callbacks                                                       */
/* ------------------------------------------------------------------ */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    AppState *state = SDL_calloc(1, sizeof(AppState));
    if (!state) {
        SDL_Log("Failed to allocate AppState");
        return SDL_APP_FAILURE;
    }

    state->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV |
                                        SDL_GPU_SHADERFORMAT_DXIL,
                                        true, NULL);
    if (!state->device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    state->window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!state->window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(state->device, state->window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
        return SDL_APP_FAILURE;
    }

    *appstate = state;
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *state = appstate;

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture *swapchain_tex = NULL;
    Uint32 w, h;
    if (!SDL_WaitForGPUSwapchain(cmd, state->window, &swapchain_tex, &w, &h)) {
        SDL_Log("SDL_WaitForGPUSwapchain failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUColorTargetInfo color_target;
    SDL_zero(color_target);
    color_target.texture    = swapchain_tex;
    color_target.load_op    = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op   = SDL_GPU_STOREOP_STORE;
    color_target.clear_color = (SDL_FColor){ 0.1f, 0.1f, 0.1f, 1.0f };

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, NULL);
    /* Scene rendering will go here */
    SDL_EndGPURenderPass(pass);

    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;
    AppState *state = appstate;
    if (!state) return;

    SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
    SDL_DestroyWindow(state->window);
    SDL_DestroyGPUDevice(state->device);
    SDL_free(state);
}
```

**Adapt the scaffold as needed:**

- If the scene clearly needs the math library, include `"math/forge_math.h"`
- If it needs capture support, include `"capture/forge_capture.h"`
- Keep it minimal — the goal is a buildable starting point, not a full scene

### 6. Create `CMakeLists.txt`

```cmake
add_executable(NN-name WIN32 main.c)
target_include_directories(NN-name PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(NN-name PRIVATE SDL3::SDL3)

add_custom_command(TARGET NN-name POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:NN-name>
)
```

### 7. Create `PLAN.md`

This file lives inside the lesson directory and tracks the scene-building
progress. It describes what needs to be built before the new concept can be
added.

```markdown
# Lesson NN — Title

## Concept

[What this lesson teaches — the ONE new concept being introduced]

## Scene requirements

The scene needs to be built before introducing the new concept. Check off
each feature as it is implemented and working:

- [ ] Camera: [type and controls]
- [ ] Lighting: [type, shadows, etc.]
- [ ] Materials: [shading model, maps, etc.]
- [ ] Post-processing: [HDR, bloom, tone mapping, etc.]
- [ ] Models: [specific models needed]
- [ ] Other: [any lesson-specific needs]

Remove any lines that don't apply — not every lesson needs all of these.

## Camera

[Starting position and orientation. What should be visible from the default
viewpoint.]

## New concept integration

[How the new concept fits into the scene — where in the code and/or shaders
it will appear. Which existing features it builds on or modifies.]

## Status

- [ ] Directory scaffolded (`/start-lesson`)
- [ ] Scene building (main.c, shaders)
- [ ] New concept implemented
- [ ] README written (`/create-lesson`)
- [ ] Skill file created (`/create-lesson`)
- [ ] Screenshot captured
- [ ] Final pass (`/final-pass`)
- [ ] Published (`/publish-lesson`)
```

**Fill in the bracketed sections** with the actual scene requirements gathered
in step 2. Remove requirement lines that don't apply.

### 8. Create `shaders/` directory

Create an empty `shaders/` directory. Shaders will be added as the scene is
built.

```bash
mkdir -p lessons/gpu/NN-name/shaders
```

### 9. Update root `CMakeLists.txt`

Add `add_subdirectory(lessons/gpu/NN-name)` under the "GPU Lessons" section,
in numerical order.

### 10. Build the scaffold

Use a Task agent (`model: "haiku"`) to build and verify the minimal program
compiles and runs:

```bash
cmake -B build
cmake --build build --config Debug --target NN-name
```

The window should open and show a dark gray clear color. If it doesn't
compile, fix the issue before proceeding.

### 11. Mark the first status checkbox

Update the PLAN.md to check off "Directory scaffolded":

```markdown
- [x] Directory scaffolded (`/start-lesson`)
```

### 12. Report to the user

Tell the user:

- The lesson directory is ready at `lessons/gpu/NN-name/`
- The minimal main.c compiles and shows a window
- The PLAN.md describes what needs to be built next
- Suggest the next step: start building the scene (camera, lighting, etc.)
  working through the PLAN.md checklist

## What this skill does NOT do

- Does **not** create README.md (that's `/create-lesson`)
- Does **not** create a skill file (that's `/create-lesson`)
- Does **not** capture screenshots (that's `/create-lesson` or `/add-screenshot`)
- Does **not** compile shaders (the scene isn't ready yet)
- Does **not** update root README.md gallery (that's `/create-lesson`)
- Does **not** create a branch or PR (that's `/publish-lesson`)

## Code style reminders

- C99 matching SDL's style
- `PascalCase` for typedefs (`AppState`, `SceneVertex`)
- `lowercase_snake_case` for locals and functions
- `UPPER_SNAKE_CASE` for `#define` constants
- `SDL_calloc` / `SDL_free` for app_state (not malloc/free)
- Check every SDL bool return value with descriptive error messages
- Use `SDL_zero()` on all create-info structs before filling them
- Extensive comments explaining *why*, not just *what*
