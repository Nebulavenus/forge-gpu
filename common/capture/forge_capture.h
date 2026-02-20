/*
 * forge_capture.h — Frame capture utility for forge-gpu lessons
 *
 * Captures a rendered frame to a BMP file for screenshot generation.
 * Header-only — #include behind an #ifdef FORGE_CAPTURE guard.
 *
 * How it works:
 *   After the lesson renders to the swapchain as normal, a copy pass
 *   downloads the swapchain texture into a transfer buffer.  The pixels
 *   are then saved as a BMP file using SDL_SaveBMP.  The lesson's render
 *   code is completely unchanged — capture is purely additive.
 *
 * Build:
 *   Enable with cmake -DFORGE_CAPTURE=ON.  Without that flag, none of
 *   this code is compiled — lessons build and run exactly as before.
 *
 * Command-line flags (when compiled with FORGE_CAPTURE):
 *   --screenshot <file.bmp>          Capture one frame and save
 *   --capture-frame N                Frame to start capturing (default: 5)
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_CAPTURE_H
#define FORGE_CAPTURE_H

#include <SDL3/SDL.h>
#include <stdio.h>     /* snprintf */

/* ── Constants ────────────────────────────────────────────────────────────── */

/* Wait a few frames before capturing so the GPU pipeline is warmed up
 * and any first-frame artifacts are gone. */
#define FORGE_CAPTURE_DEFAULT_START_FRAME  5

/* Bytes per pixel for RGBA/BGRA formats (used for transfer buffer sizing). */
#define FORGE_CAPTURE_BYTES_PER_PIXEL  4

/* Maximum path length for output filenames. */
#define FORGE_CAPTURE_MAX_PATH  512

/* ── Capture mode ─────────────────────────────────────────────────────────── */

typedef enum ForgeCaptureMode {
    FORGE_CAPTURE_NONE,        /* Normal operation — no capture            */
    FORGE_CAPTURE_SCREENSHOT   /* Capture a single frame as one BMP file   */
} ForgeCaptureMode;

/* ── Capture state ────────────────────────────────────────────────────────── */

typedef struct ForgeCapture {
    /* Configuration (set by forge_capture_parse_args) */
    ForgeCaptureMode mode;
    char             output_path[FORGE_CAPTURE_MAX_PATH];
    int              start_frame;

    /* Runtime counters */
    int              current_frame;
    bool             saved;

    /* GPU resources (created by forge_capture_init) */
    SDL_GPUDevice         *device;
    SDL_GPUTransferBuffer *buffer;   /* download transfer buffer */
    Uint32                 width;
    Uint32                 height;
    SDL_GPUTextureFormat   format;
} ForgeCapture;

/* ── Forward declarations ─────────────────────────────────────────────────── */

static inline bool forge_capture_parse_args(
    ForgeCapture *cap, int argc, char *argv[]);

static inline bool forge_capture_init(
    ForgeCapture *cap, SDL_GPUDevice *device, SDL_Window *window);

static inline bool forge_capture_finish_frame(
    ForgeCapture *cap, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain);

static inline bool forge_capture_should_quit(const ForgeCapture *cap);

static inline void forge_capture_destroy(
    ForgeCapture *cap, SDL_GPUDevice *device);

/* ── Implementation ───────────────────────────────────────────────────────── */

/*
 * Parse command-line arguments for capture flags.
 * Returns true if capture mode was activated.
 */
static inline bool forge_capture_parse_args(
    ForgeCapture *cap, int argc, char *argv[])
{
    SDL_memset(cap, 0, sizeof(*cap));
    cap->start_frame = FORGE_CAPTURE_DEFAULT_START_FRAME;

    for (int i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            cap->mode = FORGE_CAPTURE_SCREENSHOT;
            SDL_strlcpy(cap->output_path, argv[i + 1],
                        FORGE_CAPTURE_MAX_PATH);
            i++;
        } else if (SDL_strcmp(argv[i], "--capture-frame") == 0 && i + 1 < argc) {
            cap->start_frame = SDL_atoi(argv[i + 1]);
            if (cap->start_frame < 0) cap->start_frame = 0;
            i++;
        }
    }

    return cap->mode != FORGE_CAPTURE_NONE;
}

/*
 * Create the download transfer buffer.
 *
 * We only need a transfer buffer sized for one frame — the swapchain
 * texture itself is the render target (the lesson renders to it normally).
 * We use SDL_GetWindowSizeInPixels for HiDPI correctness.
 *
 * Returns true on success.
 */
static inline bool forge_capture_init(
    ForgeCapture *cap, SDL_GPUDevice *device, SDL_Window *window)
{
    cap->device = device;

    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(window, &w, &h)) {
        SDL_Log("Capture: failed to get window size: %s", SDL_GetError());
        return false;
    }
    cap->width  = (Uint32)w;
    cap->height = (Uint32)h;
    cap->format = SDL_GetGPUSwapchainTextureFormat(device, window);

    SDL_GPUTransferBufferCreateInfo buf_info;
    SDL_zero(buf_info);
    buf_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    buf_info.size  = cap->width * cap->height * FORGE_CAPTURE_BYTES_PER_PIXEL;

    cap->buffer = SDL_CreateGPUTransferBuffer(device, &buf_info);
    if (!cap->buffer) {
        SDL_Log("Capture: failed to create transfer buffer: %s",
                SDL_GetError());
        return false;
    }

    SDL_Log("Capture: ready (%ux%u)", cap->width, cap->height);
    return true;
}

/*
 * Map the GPU texture format to the matching SDL pixel format.
 *
 * GPU formats name channels in byte order (B8G8R8A8 = bytes B,G,R,A).
 * SDL pixel formats name bits from MSB to LSB (ARGB8888 = A in bits 24-31,
 * which on little-endian gives byte order B,G,R,A — matching the GPU).
 */
static inline SDL_PixelFormat forge_capture__pixel_format(
    SDL_GPUTextureFormat gpu_format)
{
    switch (gpu_format) {
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
        return SDL_PIXELFORMAT_ARGB8888;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
        return SDL_PIXELFORMAT_ABGR8888;
    default:
        return SDL_PIXELFORMAT_ARGB8888;
    }
}

/*
 * Save raw pixel data to a BMP file.
 */
static inline bool forge_capture__save_bmp(
    ForgeCapture *cap, const void *pixels, const char *path)
{
    SDL_PixelFormat fmt = forge_capture__pixel_format(cap->format);
    int pitch = (int)(cap->width * FORGE_CAPTURE_BYTES_PER_PIXEL);

    SDL_Surface *surface = SDL_CreateSurfaceFrom(
        (int)cap->width, (int)cap->height, fmt, (void *)pixels, pitch);
    if (!surface) {
        SDL_Log("Capture: failed to create surface: %s", SDL_GetError());
        return false;
    }

    bool ok = SDL_SaveBMP(surface, path);
    if (!ok) {
        SDL_Log("Capture: failed to save %s: %s", path, SDL_GetError());
    } else {
        SDL_Log("Capture: saved %s", path);
    }

    SDL_DestroySurface(surface);
    return ok;
}

/*
 * Download the swapchain texture and save it to disk.
 *
 * Call this AFTER SDL_EndGPURenderPass and BEFORE SDL_SubmitGPUCommandBuffer.
 * If this function needs to save a frame, it opens a copy pass, downloads the
 * swapchain, submits with a fence, waits, and saves — then returns true so
 * the caller knows NOT to call SDL_SubmitGPUCommandBuffer again.
 *
 * Returns false on frames that don't need capture (caller submits normally).
 */
static inline bool forge_capture_finish_frame(
    ForgeCapture *cap, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain)
{
    if (!cap || cap->mode == FORGE_CAPTURE_NONE) {
        return false;
    }

    cap->current_frame++;

    /* Not yet at the frame where we start capturing. */
    if (cap->current_frame < cap->start_frame) {
        return false;
    }

    /* Already captured the screenshot. */
    if (cap->saved) {
        return false;
    }

    /* Can't download if the window is minimized (no swapchain this frame). */
    if (!swapchain) {
        return false;
    }

    /* ── Download swapchain → transfer buffer ─────────────────────────── */
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    if (!copy) {
        SDL_Log("Capture: failed to begin copy pass (%ux%u): %s",
                cap->width, cap->height, SDL_GetError());
        return false;  /* caller should still submit the command buffer */
    }

    SDL_GPUTextureRegion src_region;
    SDL_zero(src_region);
    src_region.texture = swapchain;
    src_region.w       = cap->width;
    src_region.h       = cap->height;
    src_region.d       = 1;

    SDL_GPUTextureTransferInfo dst_info;
    SDL_zero(dst_info);
    dst_info.transfer_buffer = cap->buffer;

    SDL_DownloadFromGPUTexture(copy, &src_region, &dst_info);
    SDL_EndGPUCopyPass(copy);

    /* ── Submit with fence and wait ───────────────────────────────────── */
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (!fence) {
        SDL_Log("Capture: failed to submit with fence: %s", SDL_GetError());
        return true;  /* cmd was consumed even on failure */
    }

    if (!SDL_WaitForGPUFences(cap->device, true, &fence, 1)) {
        SDL_Log("Capture: failed to wait for fence: %s", SDL_GetError());
        SDL_ReleaseGPUFence(cap->device, fence);
        return true;  /* cmd was consumed — caller must not submit */
    }

    /* ── Map and save ─────────────────────────────────────────────────── */
    void *pixels = SDL_MapGPUTransferBuffer(cap->device, cap->buffer, false);
    if (pixels) {
        if (forge_capture__save_bmp(cap, pixels, cap->output_path)) {
            cap->saved = true;
        }
        SDL_UnmapGPUTransferBuffer(cap->device, cap->buffer);
    } else {
        SDL_Log("Capture: failed to map transfer buffer: %s", SDL_GetError());
    }

    SDL_ReleaseGPUFence(cap->device, fence);

    return true;  /* command buffer was submitted — caller must not submit */
}

/*
 * Check if all requested frames have been captured.
 * When this returns true, the lesson should exit.
 */
static inline bool forge_capture_should_quit(const ForgeCapture *cap)
{
    return cap->mode == FORGE_CAPTURE_SCREENSHOT && cap->saved;
}

/*
 * Release GPU resources.  Safe to call even if never initialized.
 */
static inline void forge_capture_destroy(
    ForgeCapture *cap, SDL_GPUDevice *device)
{
    if (cap->buffer) {
        SDL_ReleaseGPUTransferBuffer(device, cap->buffer);
        cap->buffer = NULL;
    }
}

#endif /* FORGE_CAPTURE_H */
