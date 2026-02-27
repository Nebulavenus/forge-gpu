# forge-gpu Capture Utility

A header-only frame capture utility for saving rendered frames to BMP files.

## Quick Start

```c
#include "capture/forge_capture.h"

ForgeCapture cap = {0};
forge_capture_parse_args(&cap, argc, argv);
if (cap.mode != FORGE_CAPTURE_NONE) {
    forge_capture_init(&cap, device, window);
}

/* In the render loop, after SDL_EndGPURenderPass: */
if (!forge_capture_finish_frame(&cap, cmd, swapchain)) {
    SDL_SubmitGPUCommandBuffer(cmd);  /* only submit if capture didn't */
}
if (forge_capture_should_quit(&cap)) break;

/* Cleanup */
forge_capture_destroy(&cap, device);
```

## What's Included

### Types

- **`ForgeCaptureMode`** -- Enum: `FORGE_CAPTURE_NONE` (normal operation),
  `FORGE_CAPTURE_SCREENSHOT` (capture one frame)
- **`ForgeCapture`** -- Capture state: mode, output path, frame counters,
  and GPU download resources

### Functions

- **`forge_capture_parse_args(cap, argc, argv)`** -- Parse `--screenshot <file.bmp>`
  and `--capture-frame N` from the command line. Returns `true` if capture was
  activated
- **`forge_capture_init(cap, device, window)`** -- Create the GPU download
  transfer buffer. Uses `SDL_GetWindowSizeInPixels` for HiDPI correctness.
  Returns `true` on success
- **`forge_capture_finish_frame(cap, cmd, swapchain)`** -- Download the
  swapchain texture and save to disk. Call after `SDL_EndGPURenderPass` and
  before `SDL_SubmitGPUCommandBuffer`. Returns `true` if the command buffer
  was submitted (caller must NOT submit again)
- **`forge_capture_should_quit(cap)`** -- Returns `true` when all requested
  captures are complete (lesson should exit)
- **`forge_capture_destroy(cap, device)`** -- Release GPU resources. Safe to
  call even if never initialized

### Constants

| Constant | Default | Description |
|----------|---------|-------------|
| `FORGE_CAPTURE_DEFAULT_START_FRAME` | 5 | Wait frames before capturing (GPU warm-up) |
| `FORGE_CAPTURE_BYTES_PER_PIXEL` | 4 | RGBA/BGRA format |
| `FORGE_CAPTURE_MAX_PATH` | 512 | Output filename buffer size |

## How It Works

The capture system is purely additive -- lesson render code is completely
unchanged. After the lesson renders to the swapchain as normal, a copy pass
downloads the swapchain texture into a transfer buffer. The pixels are then
saved as a BMP file using `SDL_SaveBMP`.

1. The lesson renders normally to the swapchain
2. `forge_capture_finish_frame` opens a GPU copy pass
3. The swapchain texture is downloaded to a transfer buffer
4. The command buffer is submitted with a fence and waited on
5. Pixels are mapped, converted to an SDL surface, and saved as BMP

## Build

Compiled only when `-DFORGE_CAPTURE=ON` is passed to CMake. Without that
flag, no capture code is compiled and lessons build exactly as before.

## Command-Line Flags

```bash
./lesson --screenshot output.bmp          # Capture one frame
./lesson --screenshot output.bmp --capture-frame 10  # Wait 10 frames first
```

## Dependencies

- **SDL3** -- GPU API (transfer buffers, copy passes, fences), surface
  creation, and BMP writing
- No `forge_math.h` dependency

## Design Philosophy

1. **Non-invasive** -- Lesson code is unchanged; capture wraps around the
   existing render loop
2. **Header-only** -- Just include `forge_capture.h`, no build config needed
3. **HiDPI-aware** -- Uses `SDL_GetWindowSizeInPixels` for correct dimensions
4. **Pixel format detection** -- Handles both BGRA and RGBA swapchain formats

## License

[zlib](../../LICENSE) -- same as SDL and the rest of forge-gpu.
