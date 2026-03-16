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
| `forge_scene_draw_mesh_ex(scene, pipeline, vb, ib, count, model, color)` | Draw a lit mesh with a caller-provided pipeline |
| `forge_scene_create_pipeline(scene, cull_mode, fill_mode)` | Create a pipeline with custom rasterizer state |
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
| `forge_scene_ui(s)` | `ForgeUiContext *` (NULL if UI disabled, valid between begin_ui/end_ui) |
| `forge_scene_window_ui(s)` | `ForgeUiWindowContext *` (NULL if UI disabled, valid between begin_ui/end_ui) |

### Custom pipelines

`forge_scene_create_pipeline()` creates a Blinn-Phong pipeline with custom
rasterizer state. The pipeline shares the scene's shaders, vertex layout,
depth test, and shadow map binding — only the cull mode and fill mode differ.

```c
/* Create a wireframe pipeline (for debug overlays like AABBs) */
SDL_GPUGraphicsPipeline *wireframe = forge_scene_create_pipeline(
    &scene, SDL_GPU_CULLMODE_NONE, SDL_GPU_FILLMODE_LINE);

/* Create a double-sided pipeline (for open geometry like uncapped cylinders) */
SDL_GPUGraphicsPipeline *double_sided = forge_scene_create_pipeline(
    &scene, SDL_GPU_CULLMODE_NONE, SDL_GPU_FILLMODE_FILL);

/* Draw with the custom pipeline */
forge_scene_draw_mesh_ex(&scene, wireframe, vb, ib, count, model, color);

/* The caller owns the pipeline — release it before destroying the scene */
SDL_ReleaseGPUGraphicsPipeline(forge_scene_device(&scene), wireframe);
SDL_ReleaseGPUGraphicsPipeline(forge_scene_device(&scene), double_sided);
```

Returns `NULL` if the scene is not initialized or if pipeline creation fails
(e.g. GPU resource allocation error). The caller must release the pipeline
with `SDL_ReleaseGPUGraphicsPipeline()` before calling `forge_scene_destroy()`.

### Model loading (`FORGE_SCENE_MODEL_SUPPORT`)

Define `FORGE_SCENE_MODEL_SUPPORT` before including `forge_scene.h` to enable
pipeline model rendering. Three model types are supported:

| Type | Struct | Loader | Vertex stride |
|------|--------|--------|---------------|
| Static model | `ForgeSceneModel` | `forge_scene_load_model()` | 48 bytes |
| Skinned model | `ForgeSceneSkinnedModel` | `forge_scene_load_skinned_model()` | 72 bytes |
| Morph model | `ForgeSceneMorphModel` | `forge_scene_load_morph_model()` | 48 bytes + storage buffers |

Each type has matching `_draw`, `_draw_shadows`, and `_free` functions.

### Coordinate system and model placement

forge-gpu uses a **Y-up, right-handed** coordinate system (matching glTF and
SDL GPU). All scene rendering — camera, lighting, shadow maps, grid — assumes
this convention.

**The placement matrix controls world-space positioning.** Every draw call
takes a `mat4 placement` parameter that positions the model in the scene:

```c
mat4 placement = mat4_multiply(
    mat4_translate(vec3_create(x, y, z)),
    mat4_scale_uniform(scale));
forge_scene_draw_morph_model(&scene, &model, placement);
```

**Node transforms are part of the model, not the scene.** Pipeline models
(`.fscene`) carry per-node transforms from the original glTF. These often
include coordinate-system conversions (e.g. Blender's Z-up → glTF's Y-up
rotation) and authoring-time scale. The scene renderer applies these
transforms as part of the model's internal hierarchy — they are correct data,
not artifacts to be stripped.

**To determine a model's world-space size,** check the `.fscene` node
transforms, not just the `.fmesh` vertex positions. A mesh with positions
ranging [-0.01, 0.01] may have a 100x scale in its scene node, making it
~2 units in world space. Use `dump_fscene.py` to inspect:

```bash
python scripts/dump_fscene.py path/to/model.fscene
```

**Common mistakes:**

- Scaling the placement by 100x because the mesh vertices looked tiny — check
  the node transform first, it may already include the intended scale
- Trying to "fix" a rotated model by counter-rotating the placement — the
  rotation is likely a Z-up → Y-up conversion that puts the model in the
  correct coordinate system
- Resetting node transforms to identity in the loader — this breaks animation
  keyframes and joint hierarchies that are authored relative to the node space

### Storage buffer alignment (StructuredBuffer)

**Always declare `StructuredBuffer<float4>` in HLSL, never `float3`.**
SPIRV and DXIL disagree on the stride of `StructuredBuffer<float3>`:

| Backend | `StructuredBuffer<float3>` stride | `StructuredBuffer<float4>` stride |
|---------|-----------------------------------|-----------------------------------|
| SPIRV (Vulkan) | **16 bytes** (vec4 alignment) | 16 bytes |
| DXIL (D3D12) | **12 bytes** (natural size) | 16 bytes |
| MSL (Metal) | **16 bytes** (vec4 alignment) | 16 bytes |

If the CPU uploads 16-byte-stride data (4 floats per element, 4th as
padding) and the shader declares `StructuredBuffer<float3>`, Vulkan reads
correctly but D3D12 reads every element after index 0 at the wrong offset.
The result is visible mesh corruption — vertices splitting at seams, faces
skewing asymmetrically.

**The fix:** declare `StructuredBuffer<float4>` and read `.xyz`:

```hlsl
/* Wrong — stride mismatch on D3D12 */
StructuredBuffer<float3> deltas : register(t0, space0);
float3 d = deltas[vertex_id];

/* Correct — 16-byte stride on all backends */
StructuredBuffer<float4> deltas : register(t0, space0);
float3 d = deltas[vertex_id].xyz;
```

CPU-side arrays must use 4 floats per element (16-byte stride) with the
4th float as padding. The morph model code uses this layout — see
`forge_scene_update_morph_animation`.

This applies to any `StructuredBuffer` carrying 3-component data (positions,
normals, deltas). 2-component and 4-component types do not have this
mismatch.

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
