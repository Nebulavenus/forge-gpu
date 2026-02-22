/*
 * Engine Lesson 08 — Debugging Graphics with RenderDoc
 *
 * Demonstrates: GPU debug annotations (debug groups and labels) and
 * runtime detection of RenderDoc via its in-application API.
 *
 * This program creates a window, clears it to a color, and annotates every
 * GPU operation with debug groups.  When launched through RenderDoc, these
 * annotations appear in the Event Browser, making it easy to identify each
 * phase of rendering.
 *
 * Why this lesson exists:
 *   Engine Lesson 07 taught CPU-level debugging with GDB, LLDB, and
 *   Visual Studio.  But GPU rendering happens on a separate processor —
 *   you cannot set a breakpoint inside a shader or inspect a vertex buffer
 *   from GDB.  RenderDoc fills this gap by capturing an entire frame of
 *   GPU work and letting you inspect every draw call, texture, buffer,
 *   and shader after the fact.
 *
 * Key concepts:
 *   - Debug mode GPU device     — enables validation and debug utilities
 *   - SDL_PushGPUDebugGroup     — begin a named group of GPU operations
 *   - SDL_PopGPUDebugGroup      — end the current debug group
 *   - SDL_InsertGPUDebugLabel   — mark a specific point in the command stream
 *   - RenderDoc in-application API — detect and trigger captures from code
 *
 * SPDX-License-Identifier: Zlib
 */

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* ── Constants ────────────────────────────────────────────────────────────── */

#define WINDOW_TITLE  "Forge GPU - Engine 08 RenderDoc"
#define WINDOW_WIDTH  1280
#define WINDOW_HEIGHT 720

/* Linear-space clear color — cornflower blue, a classic debug background.
 * In linear space these values are moderate; the sRGB swapchain makes them
 * appear as the familiar bright blue on screen. */
#define CLEAR_R 0.39f
#define CLEAR_G 0.58f
#define CLEAR_B 0.93f
#define CLEAR_A 1.0f

/* Frame on which to trigger a programmatic RenderDoc capture (if connected).
 * Frame 0 is often a good choice — it captures the very first rendered frame
 * including all resource creation that happened before it. */
#define CAPTURE_TARGET_FRAME 60

/* How often to print a status line (in frames).  At 60 fps this logs
 * roughly every 5 seconds — frequent enough to confirm the program is
 * running, infrequent enough to avoid flooding the console. */
#define FRAME_LOG_INTERVAL 300

/* ── RenderDoc in-application API (minimal definitions) ───────────────────
 *
 * The full RenderDoc API is defined in renderdoc_app.h, available at:
 *   https://github.com/baldurk/renderdoc/blob/stable/renderdoc/api/app/renderdoc_app.h
 *
 * We define only what we need here so the lesson compiles without external
 * headers.  The approach:
 *   1. Check if RenderDoc injected its shared library into the process
 *   2. Call RENDERDOC_GetAPI to obtain the API function table
 *   3. Use StartFrameCapture / EndFrameCapture to capture programmatically
 *
 * When RenderDoc launches your application, it injects renderdoc.dll (Windows)
 * or librenderdoc.so (Linux) into the process BEFORE main() runs.  We detect
 * this by trying to load the already-injected library.
 */

/* RenderDoc API version we request — 1.1.2 gives us frame capture control. */
#define RENDERDOC_API_VERSION 10102  /* eRENDERDOC_API_Version_1_1_2 */

/* Function pointer type for RENDERDOC_GetAPI. */
typedef int (*pRENDERDOC_GetAPI)(int version, void **out_api);

/* Minimal subset of the RENDERDOC_API_1_1_2 struct.
 *
 * The real struct has ~30 function pointers.  We only need three, but we
 * must preserve the struct layout so our pointers land at the correct
 * offsets.  Each 'void *' below stands in for a function pointer we do
 * not use.  The offsets come directly from renderdoc_app.h.
 *
 * If you need more API functions (overlay control, file paths, etc.),
 * include the real renderdoc_app.h instead of this minimal definition. */
typedef struct RenderdocAPI {
    /* Slots 0-5: GetAPIVersion, SetCaptureOptionU32, SetCaptureOptionF32,
     *            GetCaptureOptionU32, GetCaptureOptionF32, SetFocusToggleKeys */
    void *_unused_0[6];

    /* Slot 6: SetCaptureKeys */
    void *_unused_6;

    /* Slot 7: GetOverlayBits */
    void *_unused_7;

    /* Slot 8: MaskOverlayBits */
    void *_unused_8;

    /* Slot 9: RemoveHooks (deprecated) */
    void *_unused_9;

    /* Slot 10: UnloadCrashHandler (deprecated) */
    void *_unused_10;

    /* Slot 11: SetCaptureFilePathTemplate */
    void *_unused_11;

    /* Slot 12: GetCaptureFilePathTemplate */
    void *_unused_12;

    /* Slot 13: GetNumCaptures */
    void *_unused_13;

    /* Slot 14: GetCapture */
    void *_unused_14;

    /* Slot 15: TriggerCapture (captures the next frame presented) */
    void (*TriggerCapture)(void);

    /* Slot 16: IsTargetControlConnected */
    void *_unused_16;

    /* Slot 17: LaunchReplayUI */
    void *_unused_17;

    /* Slot 18: SetActiveWindow */
    void *_unused_18;

    /* Slot 19: StartFrameCapture */
    void (*StartFrameCapture)(void *device_pointer, void *window_handle);

    /* Slot 20: IsFrameCapturing */
    int (*IsFrameCapturing)(void);

    /* Slot 21: EndFrameCapture */
    void (*EndFrameCapture)(void *device_pointer, void *window_handle);
} RenderdocAPI;

/* ── Application state ────────────────────────────────────────────────────── */

typedef struct AppState {
    SDL_Window    *window;
    SDL_GPUDevice *device;
    int            frame_number;

    /* RenderDoc API — NULL if RenderDoc is not attached. */
    RenderdocAPI  *rdoc;
    bool           capture_triggered;
} AppState;

/* ── RenderDoc detection ──────────────────────────────────────────────────── */
/*
 * Detect whether RenderDoc is attached to this process and obtain its API.
 *
 * When you launch a program through RenderDoc (File -> Launch Application),
 * RenderDoc injects its shared library before main() runs.  We detect this
 * by loading the library (which returns the already-loaded instance) and
 * looking up RENDERDOC_GetAPI.
 *
 * Returns the API pointer on success, NULL if RenderDoc is not present.
 */
static RenderdocAPI *detect_renderdoc(void)
{
    SDL_SharedObject *rdoc_lib = NULL;

    /* Try platform-specific library names.
     *
     * SDL_LoadObject uses LoadLibraryA on Windows and dlopen on Linux.
     * If RenderDoc already injected the library, this returns a handle to
     * the existing instance rather than loading a new copy. */
#ifdef _WIN32
    rdoc_lib = SDL_LoadObject("renderdoc.dll");
#elif defined(__linux__)
    rdoc_lib = SDL_LoadObject("librenderdoc.so");
#elif defined(__APPLE__)
    /* RenderDoc does not support Metal (macOS).  On macOS, use Xcode's
     * GPU debugger or the Metal Debugger instead.  We still try loading
     * in case a future version adds support. */
    rdoc_lib = SDL_LoadObject("librenderdoc.dylib");
#endif

    if (!rdoc_lib) {
        return NULL;
    }

    /* Look up the RENDERDOC_GetAPI entry point. */
    pRENDERDOC_GetAPI get_api = (pRENDERDOC_GetAPI)SDL_LoadFunction(
        rdoc_lib, "RENDERDOC_GetAPI"
    );
    if (!get_api) {
        SDL_Log("Found RenderDoc library but RENDERDOC_GetAPI not found");
        return NULL;
    }

    /* Request the API.  RENDERDOC_GetAPI returns 1 on success, 0 on failure.
     * We request version 1.1.2, which provides frame capture control. */
    RenderdocAPI *api = NULL;
    int result = get_api(RENDERDOC_API_VERSION, (void **)&api);
    if (result != 1 || !api) {
        SDL_Log("RENDERDOC_GetAPI failed (requested version 1.1.2)");
        return NULL;
    }

    return api;
}

/* ── SDL_AppInit ──────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    /* ── 1. Initialise SDL ────────────────────────────────────────────── */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* ── 2. Create GPU device with debug mode ─────────────────────────
     *
     * The second parameter (true) enables the GPU validation layer.
     * This is critical for RenderDoc:
     *
     *   - It enables debug markers (SDL_PushGPUDebugGroup, etc.) so
     *     your annotations appear in RenderDoc's Event Browser
     *   - It enables GPU validation, catching API misuse that would
     *     otherwise be silent corruption
     *   - It adds a small performance overhead — always disable in
     *     release builds
     *
     * We request all three shader formats so the program runs on any
     * platform.  RenderDoc supports both Vulkan (SPIRV) and D3D12 (DXIL).
     * On macOS, neither RenderDoc nor debug markers are supported — use
     * Xcode's Metal debugger instead. */
    SDL_GPUDevice *device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV |   /* Vulkan    */
        SDL_GPU_SHADERFORMAT_DXIL  |   /* D3D12     */
        SDL_GPU_SHADERFORMAT_MSL,      /* Metal     */
        true,                          /* debug mode ON */
        NULL                           /* no backend preference */
    );
    if (!device) {
        SDL_Log("Failed to create GPU device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    const char *backend = SDL_GetGPUDeviceDriver(device);
    SDL_Log("GPU backend: %s", backend);
    SDL_Log("Debug mode: enabled (required for debug groups and validation)");

    /* ── 3. Create window ─────────────────────────────────────────────── */
    SDL_Window *window = SDL_CreateWindow(
        WINDOW_TITLE, WINDOW_WIDTH, WINDOW_HEIGHT, 0
    );
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 4. Claim window for GPU presentation ─────────────────────────── */
    if (!SDL_ClaimWindowForGPUDevice(device, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }

    /* ── 5. Request sRGB swapchain ────────────────────────────────────── */
    if (SDL_WindowSupportsGPUSwapchainComposition(
            device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR)) {
        if (!SDL_SetGPUSwapchainParameters(
                device, window,
                SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR,
                SDL_GPU_PRESENTMODE_VSYNC)) {
            SDL_Log("SDL_SetGPUSwapchainParameters failed: %s",
                    SDL_GetError());
            SDL_ReleaseWindowFromGPUDevice(device, window);
            SDL_DestroyWindow(window);
            SDL_DestroyGPUDevice(device);
            return SDL_APP_FAILURE;
        }
    }

    /* ── 6. Detect RenderDoc ──────────────────────────────────────────── */
    RenderdocAPI *rdoc = detect_renderdoc();
    if (rdoc) {
        SDL_Log(" ");
        SDL_Log("==========================================================");
        SDL_Log("  RenderDoc detected!  In-application API connected.");
        SDL_Log("  A capture will be triggered on frame %d.",
                CAPTURE_TARGET_FRAME);
        SDL_Log("  You can also press F12 (or PrintScreen) to capture");
        SDL_Log("  any frame manually.");
        SDL_Log("==========================================================");
        SDL_Log(" ");
    } else {
        SDL_Log(" ");
        SDL_Log("RenderDoc not detected.");
        SDL_Log("To use RenderDoc: launch this program from RenderDoc's");
        SDL_Log("  File -> Launch Application dialog.");
        SDL_Log("  See the lesson README for step-by-step instructions.");
        SDL_Log(" ");
    }

    /* ── 7. Store state ───────────────────────────────────────────────── */
    AppState *state = SDL_calloc(1, sizeof(AppState));
    if (!state) {
        SDL_Log("Failed to allocate application state");
        SDL_ReleaseWindowFromGPUDevice(device, window);
        SDL_DestroyWindow(window);
        SDL_DestroyGPUDevice(device);
        return SDL_APP_FAILURE;
    }
    state->window           = window;
    state->device           = device;
    state->frame_number     = 0;
    state->rdoc             = rdoc;
    state->capture_triggered = false;

    *appstate = state;

    SDL_Log("=== Engine Lesson 08: Debugging Graphics with RenderDoc ===");
    SDL_Log("Close the window or press Escape to exit.");

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppEvent ─────────────────────────────────────────────────────────── */

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    (void)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }
    if (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE) {
        return SDL_APP_SUCCESS;
    }

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppIterate ───────────────────────────────────────────────────────── */
/*
 * Each frame demonstrates GPU debug annotations:
 *
 * In RenderDoc's Event Browser, you will see a hierarchy like:
 *
 *   + Frame 60
 *     + Render Scene
 *       > Clear background to cornflower blue
 *       > End render pass
 *
 * Without debug groups, RenderDoc shows raw API calls:
 *
 *   vkCmdBeginRenderPass
 *   vkCmdEndRenderPass
 *
 * Debug groups transform this into readable, organized structure that
 * matches your application's logic rather than the underlying API.
 */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    AppState *state = (AppState *)appstate;

    /* ── Programmatic capture: begin ──────────────────────────────────
     *
     * StartFrameCapture tells RenderDoc to record all GPU commands
     * until EndFrameCapture is called.  Passing NULL for both parameters
     * captures all devices and windows.
     *
     * This is useful for:
     *   - Automated testing (capture frame N and verify results)
     *   - Bug reports (capture the exact frame that shows the bug)
     *   - CI pipelines (capture and archive frames for regression) */
    bool capturing = false;
    if (state->rdoc && !state->capture_triggered
        && state->frame_number == CAPTURE_TARGET_FRAME) {
        SDL_Log("Triggering RenderDoc capture on frame %d...",
                state->frame_number);
        state->rdoc->StartFrameCapture(NULL, NULL);
        capturing = true;
    }

    /* ── Acquire command buffer ───────────────────────────────────────── */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(state->device);
    if (!cmd) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        if (capturing) {
            state->rdoc->EndFrameCapture(NULL, NULL);
        }
        return SDL_APP_FAILURE;
    }

    /* ── Debug group: Frame ───────────────────────────────────────────
     *
     * SDL_PushGPUDebugGroup creates a named scope in the GPU command
     * stream.  In RenderDoc, nested groups appear as a collapsible tree
     * in the Event Browser.
     *
     * Best practices for debug groups:
     *   - Name groups after your application's logical phases
     *     ("Shadow Pass", "Lighting", "Post-Processing")
     *   - Nest groups to show structure (pass > sub-pass > draw call)
     *   - Keep names short but descriptive
     *   - Always match Push with Pop — unmatched pairs cause errors */
    SDL_PushGPUDebugGroup(cmd, "Frame");

    /* ── Acquire swapchain texture ────────────────────────────────────── */
    SDL_GPUTexture *swapchain = NULL;
    if (!SDL_AcquireGPUSwapchainTexture(cmd, state->window,
                                        &swapchain, NULL, NULL)) {
        SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        SDL_PopGPUDebugGroup(cmd);
        if (!SDL_SubmitGPUCommandBuffer(cmd)) {
            SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        }
        if (capturing) {
            state->rdoc->EndFrameCapture(NULL, NULL);
        }
        return SDL_APP_FAILURE;
    }

    if (swapchain) {
        /* ── Debug group: Render Scene ────────────────────────────────
         *
         * Nesting a second group inside "Frame" creates a tree:
         *   Frame
         *     Render Scene
         *       [GPU operations]
         *
         * In a real application you might have:
         *   Frame
         *     Shadow Map Pass
         *     Geometry Pass
         *     Lighting Pass
         *     Post-Processing
         *     UI Overlay */
        SDL_PushGPUDebugGroup(cmd, "Render Scene");

        /* ── Clear the screen ─────────────────────────────────────────
         *
         * A render pass that only clears is the simplest GPU operation.
         * In RenderDoc you can inspect:
         *   - The clear color value
         *   - The render target format and size
         *   - The load/store operations */
        SDL_GPUColorTargetInfo color_target = { 0 };
        color_target.texture     = swapchain;
        color_target.load_op     = SDL_GPU_LOADOP_CLEAR;
        color_target.store_op    = SDL_GPU_STOREOP_STORE;
        color_target.clear_color = (SDL_FColor){
            CLEAR_R, CLEAR_G, CLEAR_B, CLEAR_A
        };

        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(
            cmd, &color_target, 1, NULL
        );
        if (!pass) {
            SDL_Log("SDL_BeginGPURenderPass failed: %s", SDL_GetError());
            SDL_PopGPUDebugGroup(cmd);  /* Render Scene */
            SDL_PopGPUDebugGroup(cmd);  /* Frame */
            if (!SDL_SubmitGPUCommandBuffer(cmd)) {
                SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s",
                        SDL_GetError());
            }
            if (capturing) {
                state->rdoc->EndFrameCapture(NULL, NULL);
            }
            return SDL_APP_FAILURE;
        }

        /* ── Debug label: mark a specific point ───────────────────────
         *
         * SDL_InsertGPUDebugLabel is different from debug groups:
         *   - A group wraps a RANGE of operations (push...pop)
         *   - A label marks a single POINT in the command stream
         *
         * Labels appear as standalone entries in RenderDoc's Event
         * Browser.  Use them to annotate specific operations within
         * a larger group. */
        SDL_InsertGPUDebugLabel(cmd, "Clear background to cornflower blue");

        /* In a real lesson (e.g. GPU Lesson 02), you would bind a
         * pipeline and draw geometry here.  RenderDoc would then show:
         *
         *   + Render Scene
         *     > Clear background
         *     > Bind pipeline
         *     > Bind vertex buffer
         *     > Draw(3 vertices)
         *
         * Each entry is inspectable — click a draw call to see the
         * vertex data, pipeline state, shader source, and output. */

        SDL_EndGPURenderPass(pass);

        SDL_PopGPUDebugGroup(cmd);  /* Render Scene */
    }

    SDL_PopGPUDebugGroup(cmd);  /* Frame */

    /* ── Submit ────────────────────────────────────────────────────────── */
    if (!SDL_SubmitGPUCommandBuffer(cmd)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        if (capturing) {
            state->rdoc->EndFrameCapture(NULL, NULL);
        }
        return SDL_APP_FAILURE;
    }

    /* ── Programmatic capture: end ────────────────────────────────────── */
    if (capturing) {
        state->rdoc->EndFrameCapture(NULL, NULL);
        state->capture_triggered = true;
        SDL_Log("Capture complete!  Open RenderDoc to inspect the frame.");
        SDL_Log("The capture file is saved in RenderDoc's capture directory.");
    }

    /* Log periodically so the user knows the program is still running. */
    if (state->frame_number > 0
        && state->frame_number % FRAME_LOG_INTERVAL == 0) {
        SDL_Log("Frame %d — press F12 in RenderDoc to capture",
                state->frame_number);
    }

    state->frame_number++;

    return SDL_APP_CONTINUE;
}

/* ── SDL_AppQuit ──────────────────────────────────────────────────────────── */

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    (void)result;

    AppState *state = (AppState *)appstate;
    if (state) {
        SDL_Log("Exiting after %d frames.", state->frame_number);
        SDL_ReleaseWindowFromGPUDevice(state->device, state->window);
        SDL_DestroyWindow(state->window);
        SDL_DestroyGPUDevice(state->device);
        SDL_free(state);
    }
}
