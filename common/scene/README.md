# forge_scene.h — Scene Renderer Library

Header-only library that packages the rendering stack from GPU lessons 01–39
into a single include: SDL GPU setup, shadow mapping with PCF, Blinn-Phong
lighting, procedural grid floor, quaternion FPS camera, sky gradient, shader
helpers, GPU buffer upload, and forge UI initialization with font atlas.

## Usage

```c
// In exactly one .c file:
#define FORGE_SCENE_IMPLEMENTATION
#include "scene/forge_scene.h"

ForgeSceneConfig cfg = forge_scene_default_config("My App");
ForgeScene scene;
if (!forge_scene_init(&scene, &cfg, argc, argv)) {
    forge_scene_destroy(&scene);
    return SDL_APP_FAILURE;
}
```

## API

### Lifecycle

| Function | Description |
|----------|-------------|
| `forge_scene_default_config(title)` | Return config with sensible defaults |
| `forge_scene_init(scene, config, argc, argv)` | Initialize the full rendering stack |
| `forge_scene_destroy(scene)` | Release all GPU resources and free memory |
| `forge_scene_handle_event(scene, event)` | Process SDL events (quit, mouse capture, camera) |

### Frame loop

| Function | Description |
|----------|-------------|
| `forge_scene_begin_frame(scene)` | Compute dt, update camera, acquire swapchain (false = skip frame) |
| `forge_scene_begin_shadow_pass(scene)` | Begin the depth-only shadow pass |
| `forge_scene_draw_shadow_mesh(scene, vb, ib, count, model)` | Draw a mesh into the shadow map |
| `forge_scene_end_shadow_pass(scene)` | End shadow pass |
| `forge_scene_begin_main_pass(scene)` | Begin main color+depth pass (draws sky) |
| `forge_scene_draw_mesh(scene, vb, ib, count, model, color)` | Draw a lit mesh with Blinn-Phong and shadow |
| `forge_scene_draw_grid(scene)` | Draw procedural grid floor |
| `forge_scene_end_main_pass(scene)` | End main pass |
| `forge_scene_begin_ui(scene, mx, my, down)` | Begin UI for this frame |
| `forge_scene_end_ui(scene)` | Finalize UI, upload draw data, render |
| `forge_scene_end_frame(scene)` | Submit command buffer and handle capture |

### Accessors

| Function | Returns |
|----------|---------|
| `forge_scene_device(s)` | `SDL_GPUDevice *` |
| `forge_scene_dt(s)` | Frame delta time (seconds) |
| `forge_scene_view_proj(s)` | Camera view-projection matrix |
| `forge_scene_light_vp_mat(s)` | Light view-projection for shadows |
| `forge_scene_cam_pos(s)` | Camera world position |
| `forge_scene_width(s)` / `height(s)` | Swapchain dimensions |
| `forge_scene_cmd(s)` | Current command buffer |
| `forge_scene_main_pass(s)` | Current main render pass |
| `forge_scene_swapchain_format(s)` | Swapchain texture format |
| `forge_scene_window(s)` | `SDL_Window *` |
| `forge_scene_ui(s)` | `ForgeUiContext *` (valid between begin_ui/end_ui) |

## Error handling

`forge_scene_init()` validates all config fields before creating any GPU
resources. It checks that sizes are positive, `fov_deg` is in (0, 180),
near/far planes are correctly ordered, and shadow parameters are valid. If
any check fails, it returns false with a descriptive `SDL_Log` message.

Call `forge_scene_destroy()` after a failed init to clean up any partially
created resources. `forge_scene_destroy()` is idempotent — it zeros the struct
after cleanup, so calling it multiple times is safe.

## Shader compilation

The scene shaders live in `common/scene/shaders/` as HLSL source files. After
editing any `.vert.hlsl`, `.frag.hlsl`, or `.comp.hlsl` file, recompile to regenerate the
SPIRV + DXIL bytecode and C headers in `shaders/compiled/`:

```bash
python scripts/compile_scene_shaders.py              # all scene shaders
python scripts/compile_scene_shaders.py scene_model   # by name fragment
python scripts/compile_scene_shaders.py -v            # verbose (show dxc commands)
```

The script auto-detects `dxc` from `VULKAN_SDK` or the system PATH. Override
with `--dxc PATH` if needed.

Pre-compiled headers are committed so that consumers of `forge_scene.h` do not
need a shader compiler. The C build does not auto-detect shader changes —
always recompile after editing HLSL source.

## Dependencies

- SDL3 (GPU API, windowing, input)
- `common/math/forge_math.h` — vectors, matrices, quaternions
- `common/ui/forge_ui.h` — TTF parsing, font atlas
- `common/ui/forge_ui_ctx.h` — immediate-mode UI context

## See also

- [GPU Lesson 40 — Scene Renderer](../../lessons/gpu/40-scene-renderer/)
