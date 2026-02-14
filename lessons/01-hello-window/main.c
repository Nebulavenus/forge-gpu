/*
 * Lesson 01 — Hello Window
 *
 * The simplest possible SDL GPU program: create a window, claim a GPU device,
 * clear the screen to a color, and present.  No shaders, no geometry — just
 * the core frame loop that every later lesson builds on.
 *
 * Concepts introduced:
 *   - SDL_GPUDevice       — handle to the GPU backend (Vulkan / D3D12 / Metal)
 *   - SDL callbacks        — SDL drives the main loop, you fill in the blanks
 *   - Command buffers      — batches of GPU work submitted per frame
 *   - Swapchain textures   — the images the window displays
 *   - Render passes        — a scope in which draw (or clear) operations happen
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - 01 Hello Window"
#define WINDOW_WIDTH  800
#define WINDOW_HEIGHT 600

/* The color we clear the screen to each frame (dark blue-grey). */
#define CLEAR_R 0.15f
#define CLEAR_G 0.15f
#define CLEAR_B 0.20f
#define CLEAR_A 1.0f

/* ── Application state ────────────────────────────────────────────────────── */
/* Everything the app needs across callbacks lives here.
 * SDL passes this pointer to every callback after init. */

typedef struct app_state {
    SDL_Window    *window;
    SDL_GPUDevice *device;
} app_state;

/* ── SDL_AppInit ──────────────────────────────────────────────────────────── */
/* Called once at startup.  Create the window, GPU device, and swapchain.
 * Store everything in appstate so the other callbacks can reach it. */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* ── 1. Initialise SDL ───────────────────────────────────────────────
     * We only need the video subsystem.  SDL_Init returns true on success
     * (this changed from SDL2, which returned 0). */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 2. Create a GPU device ──────────────────────────────────────────
     * The shader format flags tell SDL which shader bytecode formats our
     * application can provide.  SDL picks the best available backend that
     * supports at least one of them.
     *
     *   SPIRV → Vulkan
     *   DXIL  → Direct3D 12
     *   MSL   → Metal
     *
     * We list all three so the program runs on any platform.
     * The second parameter enables validation/debug layers — always use
     * this during development to catch API misuse early. */
    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV |   /* Vulkan    */
        SDL_GPU_SHADERFORMAT_DXIL  |   /* D3D12     */
        SDL_GPU_SHADERFORMAT_MSL,      /* Metal     */
        true,                          /* debug on  */
        NULL                           /* no backend preference */
    );
    if (!device) {
        SDL_Log("Failed to create GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Log which backend SDL chose — helpful for troubleshooting. */
    SDL_Log("GPU backend: %s", SDL_GetGPUDeviceDriver(device));

    /* ── 3. Create a window ──────────────────────────────────────────────
     * Plain window, no special flags.  SDL3 takes (title, w, h, flags). */
    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        0   /* flags — none needed */
    );
    if (!window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 4. Claim the window for GPU presentation ────────────────────────
     * This binds the window's surface to our GPU device, creating the
     * swapchain (a ring of textures the OS composites to the screen). */
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("Failed to claim window: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 5. Store state for other callbacks ──────────────────────────── */
    app_state *state = SDL_calloc(1, sizeof(app_state));
    if (!state) {
        SDL_Log("Failed to allocate app state");
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window = window;
    state->device = device;
    *appstate = state;

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ─────────────────────────────────────────────────────────── */
/* Called once per event.  We only care about the quit event for now. */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ───────────────────────────────────────────────────────── */
/* Called once per frame.  Acquire a command buffer, clear the screen,
 * and submit — the heartbeat of every GPU application. */

SDL_AppResult SDL_AppIterate(void *appstate)
{
    app_state *state = (app_state *)appstate;

    /* Acquire a command buffer — a recording of GPU work. */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Get the next swapchain texture to render into.
     * This may return NULL if the window is minimised — that's fine,
     * we just skip the render pass and submit an empty command buffer. */
    SDL_GPUTexture *swapchain = NULL;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window, &swapchain, NULL, NULL)) {
        SDL_Log("Failed to acquire swapchain: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (swapchain) {
        /* Describe the color target for this render pass.
         *
         * load_op  = CLEAR → fill with clear_color before any drawing
         * store_op = STORE → keep the results when the pass ends
         *
         * Since we're only clearing (no draw calls), the render pass
         * is effectively a full-screen fill. */
        SDL_GPUColorTargetInfo color_target = { 0 };
        color_target.texture     = swapchain;
        color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op    = SDL_GPU_STOREOP_STORE;
        color_target.clear_color = (SDL_FColor){ CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A };

        /* Begin the render pass with one color target, no depth. */
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd,
            &color_target, 1,   /* one color target */
            NULL                /* no depth/stencil */
        );

        /* Nothing to draw yet — just end the pass. */
        SDL_EndGPURenderPass(pass);
    }

    /* Submit the command buffer.
     * The GPU executes all recorded work and presents the
     * swapchain texture to the window. */
    SDL_SubmitGPUCommandBuffer(cmd);

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ──────────────────────────────────────────────────────────── */
/* Called once when the app is shutting down.  Clean up in reverse order.
 * SDL calls SDL_Quit() for us after this returns. */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    app_state *state = (app_state *)appstate;
    if (state) {
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
    }
}
