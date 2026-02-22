# Lesson 23 — Point Light Shadows

Omnidirectional shadow mapping with cube maps — how point lights cast shadows
in every direction.

## What you'll learn

- Omnidirectional shadow mapping using cube map textures
- Rendering to individual cube map faces with per-face view-projection matrices
- Storing linear depth in R32_FLOAT color targets instead of hardware depth
- Sampling a `TextureCube` with a world-space direction vector for shadow lookup
- Shadow bias to prevent self-shadowing artifacts (shadow acne)
- Integrating point light shadows with HDR rendering and bloom

## Result

![Lesson 23 screenshot](assets/screenshot.png)

Four colored point lights illuminate a scene with the CesiumMilkTruck, textured
boxes, and a procedural grid floor. Each light casts omnidirectional shadows —
the truck and boxes block light from all directions, producing shadows on the
grid and on each other. Toggle individual lights with the 1/2/3/4 keys to see
how each light's shadows contribute to the scene.

## Key concepts

### Why cube maps for point light shadows?

Directional lights (Lesson 15) use a single shadow map because all light rays
travel in the same direction. Point lights radiate in every direction, so a
single 2D shadow map cannot cover the full sphere of possible shadow directions.

The solution is a **cube map** — six square textures arranged as the faces of a
cube, one for each axis direction (+X, -X, +Y, -Y, +Z, -Z). The GPU's
`TextureCube` sampler accepts a 3D direction vector and automatically selects
the correct face and texel.

![Cube map face layout](assets/cube_face_layout.png)

### Shadow pass structure

For each active point light, the shadow pass renders the scene into a 6-face
cube map. Each face uses a 90-degree field-of-view perspective projection and
a view matrix looking from the light position along one axis:

```text
Face 0 (+X): look = ( 1, 0, 0),  up = (0,-1, 0)
Face 1 (-X): look = (-1, 0, 0),  up = (0,-1, 0)
Face 2 (+Y): look = ( 0, 1, 0),  up = (0, 0, 1)
Face 3 (-Y): look = ( 0,-1, 0),  up = (0, 0,-1)
Face 4 (+Z): look = ( 0, 0, 1),  up = (0,-1, 0)
Face 5 (-Z): look = ( 0, 0,-1),  up = (0,-1, 0)
```

The 90-degree FOV with a 1:1 aspect ratio ensures the six faces tile the full
sphere without gaps or overlap. In `main.c`, the `build_cube_face_vp()` function
constructs the six view-projection matrices.

### Linear depth storage

Instead of using hardware depth (z/w), the shadow fragment shader stores the
**linear distance** from the light to the fragment, normalized by the far plane:

```hlsl
float dist = length(world_pos - light_pos);
return float4(dist / far_plane, 0.0, 0.0, 1.0);
```

Hardware depth is non-linear — precision concentrates near the near plane and
becomes coarse far away. Linear depth gives uniform precision across the entire
shadow range, which produces more consistent shadow comparisons. The shadow maps
use `R32_FLOAT` as the color target format, with a separate `D32_FLOAT` depth
buffer for rasterization.

![Linear vs hardware depth](assets/linear_vs_hardware_depth.png)

### Shadow lookup in the fragment shader

To test whether a surface point is in shadow, the scene fragment shader:

1. Computes the vector from the light to the fragment: `light_to_frag = world_pos - light_pos`
2. Samples the cube map using that direction vector (the GPU selects the correct face)
3. Compares the stored depth against the actual distance

```hlsl
float sample_shadow(int light_index, float3 light_to_frag)
{
    float current_depth = length(light_to_frag) / shadow_far_plane;
    float bias = 0.002;
    float stored_depth = shadow_cube.Sample(shadow_smp, light_to_frag).r;
    return (current_depth - bias > stored_depth) ? 0.0 : 1.0;
}
```

The shadow factor (0.0 or 1.0) multiplies each light's diffuse and specular
contribution, leaving the ambient term unaffected.

![Shadow lookup](assets/shadow_lookup.png)

### Shadow bias

Without bias, surfaces facing a light would shadow themselves — a pattern of
alternating light and dark bands called **shadow acne**. This happens because
the shadow map has finite resolution, so neighboring texels may store slightly
different depths than the fragment being tested.

A small bias (0.002 in normalized depth) shifts the comparison threshold so
that surfaces at approximately the correct depth are considered lit. Too much
bias causes shadows to detach from their casters — an artifact called **Peter
Panning**. The value 0.002 was tuned for this scene's geometry and shadow map
resolution (512x512).

### SDL3 GPU viewport Y-flip

When rendering cube map faces, the projection matrix negates its Y component:

```c
mat4 shadow_proj = mat4_perspective(PI/2, 1.0, near, far);
shadow_proj.m[5] = -shadow_proj.m[5];
```

SDL3 GPU normalizes viewport behavior across Vulkan and D3D12 by using a
negative viewport height on Vulkan. This flips the rendered image relative to
the `TextureCube` sampler's expected face orientation. Negating Y in the
projection compensates, ensuring each face's depth values align with the
direction vectors used during shadow lookup.

### Render pass order

The complete frame renders in five stages:

```text
1. Shadow passes  — 4 lights x 6 faces = up to 24 render passes
2. Scene pass     — HDR buffer (grid + models + emissive spheres)
3. Bloom down     — 5 downsample passes (Jimenez 13-tap)
4. Bloom up       — 4 upsample passes (tent filter)
5. Tone map       — HDR + bloom -> swapchain (ACES)
```

Shadow passes run first so the cube maps are ready before the scene pass
samples them. Only the truck and boxes cast shadows — the grid and emissive
spheres are excluded to avoid unnecessary geometry.

## Code walkthrough

### Shadow resources (`SDL_AppInit`)

In `main.c`, the shadow resources are created alongside other GPU state:

- **`create_shadow_cube()`** — Creates one `SDL_GPU_TEXTURETYPE_CUBE` texture
  with `R32_FLOAT` format, 6 layers, and `COLOR_TARGET | SAMPLER` usage
- **`shadow_depth`** — A shared `D32_FLOAT` 2D texture for rasterization depth
  testing (reused across all faces and lights)
- **`shadow_sampler`** — `NEAREST` filter, `CLAMP_TO_EDGE` addressing

### Shadow pipeline

The shadow pipeline uses the same `ForgeGltfVertex` layout as the scene
pipeline (position + normal + UV), but a different vertex and fragment shader:

- **Vertex shader** (`shadow.vert.hlsl`): Transforms positions by
  `light_mvp` and passes `world_pos` to the fragment shader
- **Fragment shader** (`shadow.frag.hlsl`): Outputs `distance / far_plane`
  to the R32_FLOAT color target
- **Cull mode**: `NONE` (both front and back faces write depth)

### Scene shader bindings

The scene fragment shader binds 5 texture-sampler pairs:

| Slot | Texture | Purpose |
|------|---------|---------|
| 0 | Diffuse texture | Model albedo |
| 1 | Shadow cube 0 | Light 0 shadows |
| 2 | Shadow cube 1 | Light 1 shadows |
| 3 | Shadow cube 2 | Light 2 shadows |
| 4 | Shadow cube 3 | Light 3 shadows |

The grid fragment shader binds 4 shadow cube maps (slots 0-3, no diffuse
texture).

## Math

This lesson uses:

- [Math Lesson 05 — Matrices](../../math/05-matrices/) for view and projection
  matrix construction
- [Math Lesson 06 — Projections](../../math/06-projections/) for the 90-degree
  perspective projection
- [Math Lesson 09 — View Matrix](../../math/09-view-matrix/) for
  `mat4_look_at()` used in cube face view matrices
- `mat4_perspective`, `mat4_look_at`, `mat4_multiply` from
  [`common/math/forge_math.h`](../../../common/math/README.md)

## Building

```bash
cmake -B build
cmake --build build --config Debug

# Windows
build\lessons\gpu\23-point-light-shadows\Debug\23-point-light-shadows.exe

# Linux / macOS
./build/lessons/gpu/23-point-light-shadows/23-point-light-shadows
```

## Controls

| Key | Action |
|-----|--------|
| WASD / Space / LShift | Move camera |
| Mouse | Look around |
| 1 / 2 / 3 / 4 | Toggle individual lights |
| =/+ | Increase exposure |
| - | Decrease exposure |
| B | Toggle bloom |
| Up / Down | Bloom intensity |
| Left / Right | Bloom threshold |
| Escape | Release mouse / quit |

## AI skill

This lesson's pattern is available as a reusable Claude Code skill:

- **Skill file**: [`.claude/skills/point-light-shadows/SKILL.md`](../../../.claude/skills/point-light-shadows/SKILL.md)
- **Invoke**: `/point-light-shadows`

You can copy this skill into your own project's `.claude/skills/` directory
to use the same pattern with Claude Code.

## What's next

[Lesson 24 — Gobo Spotlight](../24-gobo-spotlight/) adds projected-texture
spotlights with cookie/gobo patterns, building on the shadow mapping
foundation from this lesson.

## Exercises

1. **Increase shadow map resolution** — Change `SHADOW_MAP_SIZE` from 512 to
   1024 and observe the improvement in shadow edge quality. What is the memory
   trade-off? (Each cube map is 6 faces x width x height x 4 bytes.)

2. **Add PCF soft shadows** — Replace the single shadow comparison with a 3x3
   grid of samples using small offsets around the lookup direction. Average the
   results for softer shadow edges (similar to the PCF in Lesson 15).

3. **Limit shadow range** — Modify `sample_shadow()` to return 1.0 (fully lit)
   when the fragment is beyond `SHADOW_FAR_PLANE`. This prevents distant
   fragments from incorrectly appearing shadowed.
