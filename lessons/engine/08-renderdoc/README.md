# Engine Lesson 08 — Debugging Graphics with RenderDoc

GPU debugging requires different tools than CPU debugging — RenderDoc lets you
capture a frame of GPU work and inspect every draw call, texture, buffer, and
shader after the fact.

## What you'll learn

- Why CPU debuggers cannot debug GPU rendering problems
- How to install RenderDoc and launch your application through it
- How to capture a frame and navigate RenderDoc's interface
- GPU debug groups and labels (`SDL_PushGPUDebugGroup`, `SDL_PopGPUDebugGroup`,
  `SDL_InsertGPUDebugLabel`) for organizing captures
- How to inspect render targets, pipeline state, and draw calls
- The RenderDoc in-application API for triggering captures from code
- Diagnosing common visual bugs with RenderDoc

## Why this matters

[Engine Lesson 07](../07-using-a-debugger/) taught you to debug C code with GDB,
LLDB, and Visual Studio. Those tools work because your C code runs on the CPU —
you can set breakpoints, step through lines, and inspect variables.

GPU rendering is fundamentally different. Your shaders run on a separate
processor (the GPU) with its own memory. You cannot set a breakpoint inside a
fragment shader from GDB. You cannot `print` a vertex buffer's contents from
LLDB. When a triangle appears in the wrong position, the wrong color, or not at
all, a CPU debugger cannot tell you why.

RenderDoc solves this by capturing an entire frame of GPU work — every API call,
every buffer upload, every draw call, every shader execution — and letting you
replay and inspect it offline. It is to GPU programming what GDB is to C
programming: the tool you reach for when something looks wrong and you need to
understand why.

## Result

The example program creates a window, clears it to cornflower blue, and
annotates every GPU operation with debug groups. When launched through
RenderDoc, the debug groups appear in the Event Browser.

The program is intentionally minimal — it clears the screen without drawing
geometry. This keeps the lesson focused on the RenderDoc workflow and debug
annotations. To practice inspecting pipeline state, vertex data, and draw
calls, launch any GPU lesson through RenderDoc (Exercise 1 walks through
this with [GPU Lesson 02](../../gpu/02-first-triangle/)).

If RenderDoc is attached, the program also detects the in-application API and
triggers a programmatic capture on frame 60.

**Example output (without RenderDoc):**

```text
INFO: GPU backend: vulkan
INFO: Debug mode: enabled (required for debug groups and validation)
INFO:
INFO: RenderDoc not detected.
INFO: To use RenderDoc: launch this program from RenderDoc's
INFO:   File -> Launch Application dialog.
INFO:   See the lesson README for step-by-step instructions.
INFO:
INFO: === Engine Lesson 08: Debugging Graphics with RenderDoc ===
INFO: Close the window or press Escape to exit.
INFO: Frame 300 — press F12 in RenderDoc to capture
```

**Example output (with RenderDoc):**

```text
INFO: GPU backend: vulkan
INFO: Debug mode: enabled (required for debug groups and validation)
INFO:
INFO: ==========================================================
INFO:   RenderDoc detected!  In-application API connected.
INFO:   A capture will be triggered on frame 60.
INFO:   You can also press F12 (or PrintScreen) to capture
INFO:   any frame manually.
INFO: ==========================================================
INFO:
INFO: === Engine Lesson 08: Debugging Graphics with RenderDoc ===
INFO: Close the window or press Escape to exit.
INFO: Triggering RenderDoc capture on frame 60...
INFO: Capture complete!  Open RenderDoc to inspect the frame.
INFO: The capture file is saved in RenderDoc's capture directory.
```

## Key concepts

- **GPU debugger** — A tool that captures and replays GPU commands, letting you
  inspect the state at any point in a frame. RenderDoc is the most widely used
  open-source GPU debugger.
- **Frame capture** — A snapshot of all GPU work for one frame: command buffers,
  draw calls, resource uploads, and their results. You capture first, then
  inspect offline.
- **Debug group** — A named scope in the GPU command stream
  (`SDL_PushGPUDebugGroup` / `SDL_PopGPUDebugGroup`). Groups nest to form a
  tree that appears in RenderDoc's Event Browser.
- **Debug label** — A named point in the command stream
  (`SDL_InsertGPUDebugLabel`). Unlike groups, labels mark a single location
  rather than wrapping a range.
- **Event Browser** — RenderDoc's panel listing every GPU operation in
  chronological order. Debug groups create collapsible sections.
- **Pipeline State** — The full GPU configuration for a draw call: shaders,
  vertex layout, blend mode, depth test, rasterizer settings.
- **Texture Viewer** — RenderDoc's panel for inspecting render targets and
  textures. Shows individual channels (R, G, B, A), mip levels, and pixel
  values.
- **Mesh Viewer** — RenderDoc's panel for inspecting vertex data. Shows vertex
  positions in a 3D preview and lists attribute values in a table.
- **In-application API** — A C API provided by RenderDoc that lets your program
  detect RenderDoc at runtime and trigger captures programmatically.

## The Details

### Why CPU debuggers cannot debug GPU problems

A CPU debugger (GDB, LLDB, Visual Studio) operates on the CPU side of your
program. It can inspect:

- C variables and struct fields
- Function call stacks
- Memory addresses and pointer values

But it **cannot** inspect:

- The contents of GPU buffers (vertex data, uniform data)
- What a shader computed for a specific pixel
- Why a triangle is the wrong color or missing entirely
- The actual render target after each draw call
- Pipeline state (blend mode, depth test, cull mode)

This is because GPU work is *deferred*. When you call
`SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0)`, the CPU records the command into a
command buffer. The GPU executes it later, possibly after your C function has
already returned. There is no point in time where GDB can "pause" the GPU
mid-draw.

RenderDoc intercepts the GPU API (Vulkan, D3D12) at the driver level, records
every command, and then replays them to reconstruct the state at any point in
the frame.

### Installing RenderDoc

RenderDoc is free, open-source, and available for Windows and Linux. It supports
Vulkan and D3D12 — the two backends SDL GPU uses on these platforms.

**Windows:**

1. Download from [renderdoc.org](https://renderdoc.org/)
2. Run the installer
3. RenderDoc appears in the Start Menu

**Linux (Ubuntu/Debian):**

```bash
sudo apt install renderdoc
```

**Linux (Flatpak — any distribution):**

```bash
flatpak install flathub org.renderdoc.RenderDoc
```

**macOS:** RenderDoc does not support Metal. Use
[Xcode's GPU debugger](https://developer.apple.com/documentation/xcode/optimizing-gpu-performance-with-the-gpu-profiler/)
instead. The concepts (frame capture, draw call inspection, shader debugging)
are the same — only the tool differs.

### Launching your application through RenderDoc

RenderDoc needs to intercept GPU API calls from the very start. The simplest
way is to launch your application from within RenderDoc:

1. Open RenderDoc
2. Go to **File > Launch Application** (or the launch tab on the home screen)
3. Set **Executable Path** to your compiled program:
   - Windows: `build\lessons\engine\08-renderdoc\Debug\08-renderdoc.exe`
   - Linux: `./build/lessons/engine/08-renderdoc/08-renderdoc`
4. Set **Working Directory** to your project root (where `assets/` lives)
5. Click **Launch**

Your program starts with RenderDoc's overlay visible in the top-left corner.
The overlay shows the frame number and capture status.

### Capturing a frame

Once your application is running through RenderDoc:

**Manual capture:**

- Press **F12** (or **PrintScreen**) to capture the current frame
- The overlay briefly flashes to confirm the capture
- The capture appears in RenderDoc's capture list

**Programmatic capture (from code):**

The example program demonstrates this using the RenderDoc in-application API.
On frame 60, it calls `StartFrameCapture` / `EndFrameCapture` to capture
without any user interaction. See the [In-application API](#in-application-api)
section below.

**Opening a capture:**

Double-click the capture in RenderDoc's list (or go to
**File > Open Capture**). RenderDoc replays the frame and populates all
inspection panels.

### The Event Browser

The Event Browser is the first panel you see after opening a capture. It lists
every GPU operation in the order they were submitted.

**Without debug groups**, the Event Browser shows raw API calls:

```text
vkCmdBeginRenderPass
vkCmdEndRenderPass
```

**With debug groups**, the same frame becomes readable:

```text
+ Frame
  + Render Scene
    > Clear background to cornflower blue
    > End render pass
```

Debug groups transform a flat list of API calls into a structured, collapsible
tree that matches your application's logic. This is why annotating your GPU
commands matters — without it, debugging a complex frame with hundreds of draw
calls becomes a search through a wall of cryptic API names.

**Navigating the Event Browser:**

- Click any event to select it — all other panels update to show state at that
  point
- Expand/collapse groups with the arrow keys or mouse
- Use the **Timeline Bar** at the top for a visual overview of the frame
- Right-click an event for options like "Go to resource" or "Debug this draw"

### Pipeline State panel

Select a draw call in the Event Browser and open the **Pipeline State** panel.
This shows the complete GPU configuration for that draw:

- **Vertex Input** — The vertex buffer bindings, attribute formats, and strides.
  Verify that your C struct layout matches what the pipeline expects.
- **Vertex Shader / Fragment Shader** — The compiled shader source. You can
  click into the shader to view or debug it.
- **Rasterizer** — Fill mode (solid/wireframe), cull mode (back/front/none),
  winding order (CW/CCW), depth bias.
- **Depth/Stencil** — Depth test enabled, compare function (LESS, LEQUAL),
  depth write mask.
- **Blend State** — Color blend equation, alpha blend equation, blend factors
  (SRC_ALPHA, ONE_MINUS_SRC_ALPHA, etc.).
- **Render Targets** — The textures being written to, their formats, and
  load/store operations.

This panel is invaluable when something renders incorrectly. For example, if
back faces are visible, check the cull mode. If transparency looks wrong, check
the blend state. If depth ordering is broken, check the depth test.

### Texture Viewer

The Texture Viewer shows render targets and textures at any point in the frame.

**What you can do:**

- **View individual channels** — Toggle R, G, B, A to isolate a single
  channel. Useful for inspecting alpha masks or depth buffers.
- **View mip levels** — Select a specific mip level to verify mipmap content.
- **Read pixel values** — Hover over any pixel to see its exact RGBA value.
  Essential for verifying clear colors, shader output, and blending results.
- **Compare before/after** — Select two events and compare the render target
  to see exactly what a draw call changed.
- **Overlay modes** — Highlight wireframe, overdraw, depth test failures, and
  more. The "Highlight Drawcall" overlay shows which pixels a specific draw
  affected.

**Practical use:** If a mesh appears invisible, select the draw call and check
the Texture Viewer. Toggle the "Highlight Drawcall" overlay — if no pixels
light up, the mesh might be behind the camera, clipped by the near plane, or
culled by back-face culling.

### Mesh Viewer

The Mesh Viewer shows vertex data for a specific draw call. It has two tabs:

**VS Input (Vertex Shader Input):**

- Table of all vertices with their attribute values (position, color, normal,
  UV, etc.)
- 3D preview showing vertex positions as points
- Verify that your CPU-side vertex data reached the GPU correctly

**VS Output (Vertex Shader Output):**

- Table of post-transform vertices (after the vertex shader ran)
- 3D preview in clip space
- Check that your MVP (Model-View-Projection) matrix transformed vertices to
  the expected positions

**Practical use:** If a triangle appears at the wrong position, open the Mesh
Viewer. Compare VS Input (your original vertex data) with VS Output (after
transformation). If the input is correct but the output is wrong, the bug is in
your vertex shader or matrices.

### Shader Debugger

RenderDoc can step through shader code line by line, similar to how GDB steps
through C code:

1. Select a draw call in the Event Browser
2. In the Texture Viewer, right-click a pixel and choose **Debug this Pixel**
3. RenderDoc runs the fragment shader for that pixel and lets you step through
   each line, inspecting intermediate values

This is the closest GPU equivalent to GDB's breakpoint-and-inspect workflow.
You can watch how each variable changes as the shader executes.

**For vertex shaders:** Right-click a vertex in the Mesh Viewer and choose
**Debug this Vertex** to step through the vertex shader for that specific
vertex.

### GPU debug groups in SDL3

SDL3's GPU API provides three functions for annotating command buffers:

#### `SDL_PushGPUDebugGroup`

```c
SDL_PushGPUDebugGroup(cmd, "Shadow Pass");
/* ... shadow rendering commands ... */
SDL_PopGPUDebugGroup(cmd);
```

Creates a named scope. Everything between Push and Pop appears as a
collapsible group in RenderDoc's Event Browser. Groups can nest:

```c
SDL_PushGPUDebugGroup(cmd, "Frame");
    SDL_PushGPUDebugGroup(cmd, "Geometry Pass");
        /* draw calls */
    SDL_PopGPUDebugGroup(cmd);
    SDL_PushGPUDebugGroup(cmd, "Lighting Pass");
        /* draw calls */
    SDL_PopGPUDebugGroup(cmd);
SDL_PopGPUDebugGroup(cmd);
```

#### `SDL_PopGPUDebugGroup`

Closes the most recent debug group. Every Push must have a matching Pop.
Unmatched pairs cause validation errors.

#### `SDL_InsertGPUDebugLabel`

```c
SDL_InsertGPUDebugLabel(cmd, "Upload vertex data");
```

Inserts a named marker at a single point in the command stream. Unlike groups,
labels do not wrap a range — they mark a specific location. Use labels for
individual operations within a group.

#### When debug groups appear

Debug groups require the GPU device to be created with debug mode enabled:

```c
SDL_GPUDevice *device = SDL_CreateGPUDevice(
    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
    true,    /* debug mode — enables validation AND debug groups */
    NULL
);
```

Without debug mode, the functions are no-ops and RenderDoc shows only raw API
calls. Always enable debug mode during development.

#### Recommended debug group structure

For a typical rendering frame, annotate like this:

```c
SDL_PushGPUDebugGroup(cmd, "Shadow Map");
    /* render shadow depth */
SDL_PopGPUDebugGroup(cmd);

SDL_PushGPUDebugGroup(cmd, "Geometry + Lighting");
    SDL_InsertGPUDebugLabel(cmd, "Bind scene pipeline");
    /* bind pipeline, set uniforms */
    SDL_InsertGPUDebugLabel(cmd, "Draw terrain");
    /* draw terrain */
    SDL_InsertGPUDebugLabel(cmd, "Draw characters");
    /* draw characters */
SDL_PopGPUDebugGroup(cmd);

SDL_PushGPUDebugGroup(cmd, "Post-Processing");
    SDL_InsertGPUDebugLabel(cmd, "Bloom downsample");
    /* bloom passes */
    SDL_InsertGPUDebugLabel(cmd, "Tone mapping");
    /* tone map to swapchain */
SDL_PopGPUDebugGroup(cmd);
```

### In-application API

RenderDoc provides a C API that lets your program interact with RenderDoc at
runtime. The API is optional — your program works the same whether RenderDoc
is attached or not.

#### Detecting RenderDoc

When RenderDoc launches your application, it injects its shared library before
`main()` runs. To detect it, try loading the already-injected library:

```c
/* Try to find RenderDoc's library (already loaded if injected). */
SDL_SharedObject *lib = NULL;
#ifdef _WIN32
lib = SDL_LoadObject("renderdoc.dll");
#elif defined(__linux__)
lib = SDL_LoadObject("librenderdoc.so");
#endif

if (lib) {
    /* RenderDoc is present — get the API entry point. */
    pRENDERDOC_GetAPI get_api = (pRENDERDOC_GetAPI)SDL_LoadFunction(
        lib, "RENDERDOC_GetAPI"
    );

    RenderdocAPI *api = NULL;
    if (get_api && get_api(RENDERDOC_API_VERSION, (void **)&api) == 1) {
        /* api is now valid — use it. */
    }
}
```

This detection is safe even when RenderDoc is not present — `SDL_LoadObject`
returns NULL and the code skips the API setup.

#### Triggering captures

The most common use of the in-application API is programmatic frame capture:

```c
/* Start capturing — all GPU commands after this are recorded. */
api->StartFrameCapture(NULL, NULL);

/* ... render your frame ... */

/* End capturing — the capture is saved to disk. */
api->EndFrameCapture(NULL, NULL);
```

Passing `NULL` for both parameters captures all devices and windows. You can
also pass specific device/window handles for targeted capture.

**Use cases:**

- **Automated testing** — Capture a specific frame and verify rendering output
- **Bug reproduction** — Capture the exact frame where a visual bug appears
- **CI integration** — Capture frames during builds for regression testing

#### The full API

The example program defines only the minimal API subset needed for frame
capture. The full RenderDoc API includes:

- **Overlay control** — Show/hide the RenderDoc overlay
- **Capture file paths** — Set where captures are saved
- **Capture count** — Query how many captures have been taken
- **Replay UI** — Launch RenderDoc's UI from your application

For the complete API definition, see
[renderdoc_app.h](https://github.com/baldurk/renderdoc/blob/stable/renderdoc/api/app/renderdoc_app.h)
in the RenderDoc repository.

### Debugging common visual bugs

Here are common GPU rendering problems and how RenderDoc helps diagnose them:

#### Nothing renders (black screen)

1. Open the capture and check the Event Browser — are there any draw calls?
2. If no draw calls: the issue is on the CPU side (pipeline not created, mesh
   not loaded). Use a CPU debugger.
3. If draw calls exist: select one and check the Texture Viewer with
   "Highlight Drawcall" overlay. If no pixels light up:
   - **Mesh Viewer:** Are vertex positions valid (not NaN, not all zero)?
   - **Pipeline State > Rasterizer:** Is cull mode accidentally culling your
     front faces?
   - **Pipeline State > Vertex Shader:** Does the Model-View-Projection
     transform produce valid clip-space positions?
   - **Pipeline State > Depth/Stencil:** Is depth testing enabled with
     impossible compare function?

#### Wrong colors

1. Select the draw call and check the Texture Viewer — hover over the
   incorrectly colored pixels to read exact RGBA values.
2. **Shader Debugger:** Right-click the pixel, choose "Debug this Pixel", and
   step through the fragment shader. Watch how the color is computed.
3. **Pipeline State > Blend State:** Is blending enabled when it should not be
   (or vice versa)?
4. **sRGB mismatch:** sRGB is a gamma curve that encodes colors for display
   (see [Math Lesson 11 — Color Spaces](../../math/11-color-spaces/)). Are you
   writing linear colors to a non-sRGB render target (colors too bright) or
   sRGB colors to an sRGB target (double gamma correction, colors too dark)?

#### Z-fighting (flickering surfaces)

1. In the Texture Viewer, switch to the **Depth** target and inspect depth
   values at the flickering area.
2. Two overlapping surfaces will have nearly identical depth values. The fix:
   - Increase the near plane distance
   - Use a reversed depth buffer (greater precision near the camera)
   - Offset one surface with a small depth bias

#### Missing or inverted faces

1. Select the draw call and check **Pipeline State > Rasterizer**.
2. **Cull mode** — `BACK` culls triangles whose vertices wind clockwise (from
   the viewer's perspective). If your mesh has the opposite winding, all faces
   are culled.
3. **Front face** — Verify your winding convention matches the pipeline. SDL
   GPU uses counter-clockwise (CCW) as front by default.
4. **Mesh Viewer:** Check that vertex positions form triangles with the
   expected winding.

#### Texture appears wrong (stretched, black, or solid color)

1. In the **Pipeline State**, check the texture binding — is the correct
   texture bound to the correct slot?
2. In the **Texture Viewer**, find the texture resource and verify its contents
   look correct.
3. Check UV coordinates in the **Mesh Viewer** — are they in the expected
   [0,1] range?
4. Check the sampler state — is the address mode (clamp/wrap/mirror) correct?

## Common errors

### "No compatible replay API found"

**What you see:**

RenderDoc shows this error when trying to open a capture.

**Why it happens:** The application used a graphics API that RenderDoc does not
support in the current configuration. This typically happens on macOS (Metal is
not supported) or when the Vulkan/D3D12 driver is not installed.

**How to fix it:** Ensure you have a Vulkan or D3D12 driver installed. On Linux,
install the Vulkan ICD (Installable Client Driver — the user-space GPU driver):

```bash
# AMD
sudo apt install mesa-vulkan-drivers

# NVIDIA
sudo apt install nvidia-vulkan-icd
```

### Debug groups do not appear in RenderDoc

**What you see:** The Event Browser shows raw API calls instead of your debug
group names.

**Why it happens:** The GPU device was created without debug mode.

**How to fix it:** Pass `true` for the debug parameter:

```c
SDL_GPUDevice *device = SDL_CreateGPUDevice(
    SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL,
    true,    /* <-- must be true for debug groups */
    NULL
);
```

### RenderDoc overlay does not appear

**What you see:** The application runs but there is no RenderDoc overlay in the
corner of the window.

**Why it happens:** The application was not launched through RenderDoc, or
RenderDoc failed to inject its library.

**How to fix it:**

1. Launch from **File > Launch Application** in RenderDoc (do not just run the
   executable directly)
2. On Linux, ensure you have the correct Vulkan layer. Check with:

   ```bash
   vulkaninfo | grep -i renderdoc
   ```

3. If using Wayland, try launching with `SDL_VIDEO_DRIVER=x11` — some RenderDoc
   versions have limited Wayland support

### Capture file is empty or corrupt

**What you see:** Opening a capture shows nothing or RenderDoc reports an error.

**Why it happens:** The application exited or crashed before the frame was
fully submitted. The command buffer was not submitted before the capture ended.

**How to fix it:** Ensure `SDL_SubmitGPUCommandBuffer` is called before the
frame ends. If using programmatic capture, call `EndFrameCapture` **after**
the submit:

```c
api->StartFrameCapture(NULL, NULL);
/* ... record and submit GPU commands ... */
SDL_SubmitGPUCommandBuffer(cmd);
api->EndFrameCapture(NULL, NULL);  /* after submit */
```

## Where it's used

RenderDoc is useful for debugging every GPU lesson in forge-gpu:

- [GPU Lesson 02 — First Triangle](../../gpu/02-first-triangle/) — Inspect
  vertex buffer contents, vertex shader output, and pipeline state
- [GPU Lesson 06 — Depth & 3D](../../gpu/06-depth-and-3d/) — Inspect the
  depth buffer to diagnose z-fighting
- [GPU Lesson 10 — Basic Lighting](../../gpu/10-basic-lighting/) — Step through
  the fragment shader to verify lighting calculations
- [GPU Lesson 16 — Blending](../../gpu/16-blending/) — Inspect blend state and
  verify transparency order
- [GPU Lesson 21 — HDR & Tone Mapping](../../gpu/21-hdr-tone-mapping/) —
  Inspect floating-point render targets and verify tone mapping output
- [GPU Lesson 22 — Bloom](../../gpu/22-bloom/) — Step through the downsample
  and upsample chain to verify intermediate results

In general, any time a GPU lesson produces unexpected visual output, RenderDoc
is the first tool to reach for.

## Building

```bash
cmake -B build
cmake --build build --config Debug

# Windows
build\lessons\engine\08-renderdoc\Debug\08-renderdoc.exe

# Linux / macOS
./build/lessons/engine/08-renderdoc/08-renderdoc
```

To debug with RenderDoc, build in **Debug** or **RelWithDebInfo** mode — this
preserves shader debug information and makes the Mesh Viewer and Pipeline State
panels more informative.

## Exercises

1. **Capture a GPU lesson frame** — Launch
   [GPU Lesson 02](../../gpu/02-first-triangle/) through RenderDoc. Capture a
   frame and find the draw call for the triangle. In the Mesh Viewer, verify
   that the three vertex positions match the values defined in `main.c`.

2. **Debug a pixel** — In the same capture, right-click a pixel on the triangle
   in the Texture Viewer and choose "Debug this Pixel". Step through the
   fragment shader and watch how the interpolated color is computed.

3. **Add debug groups to a GPU lesson** — Pick any GPU lesson and add
   `SDL_PushGPUDebugGroup` / `SDL_PopGPUDebugGroup` calls around each phase
   of rendering (resource upload, render pass, submit). Capture a frame in
   RenderDoc and verify your groups appear in the Event Browser.

4. **Diagnose a bug with RenderDoc** — In a GPU lesson, deliberately change
   the cull mode to `SDL_GPU_CULLMODE_FRONT` and rebuild. Run through
   RenderDoc, capture a frame, and use the Pipeline State panel to find the
   incorrect setting. Then fix it.

## Further reading

- [RenderDoc Documentation](https://renderdoc.org/docs/index.html) —
  Official documentation with in-depth guides for every panel
- [RenderDoc In-Application API](https://renderdoc.org/docs/in_application_api.html) —
  Full API reference for programmatic capture control
- [renderdoc_app.h](https://github.com/baldurk/renderdoc/blob/stable/renderdoc/api/app/renderdoc_app.h) —
  The actual C header defining the complete API
- [Engine Lesson 07 — Using a Debugger](../07-using-a-debugger/) — CPU-level
  debugging with GDB, LLDB, and Visual Studio
- [GPU Lesson 19 — Debug Lines](../../gpu/19-debug-lines/) — In-scene debug
  visualization (a complementary approach to RenderDoc)
- [Xcode GPU Debugger](https://developer.apple.com/documentation/xcode/optimizing-gpu-performance-with-the-gpu-profiler/) —
  macOS equivalent for Metal applications
