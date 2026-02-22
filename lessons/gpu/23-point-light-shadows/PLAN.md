# Lesson 23 — Point Lights & Shadows

## Concept

Omnidirectional shadow mapping for point lights using cube map depth textures.
Each point light renders the scene into a 6-face cube depth map, then the
fragment shader samples the cube map with a world-space direction vector to
determine shadow coverage. Covers shadow bias, Peter Panning, and
distance-based attenuation falloff.

## Scene requirements

The scene needs to be built before introducing the new concept. Check off
each feature as it is implemented and working:

- [x] Camera: fly camera with WASD + mouse look (from Lesson 07)
- [x] Lighting: 3 point lights with Blinn-Phong shading (from Lessons 10/18)
- [x] Materials: per-material Blinn-Phong (ambient, diffuse, specular, shininess)
- [x] Post-processing: HDR render target + Jimenez bloom + ACES tone mapping (from Lessons 21/22)
- [x] Models: CesiumMilkTruck + BoxTextured ring (from Lessons 15/22)
- [x] Grid: procedural anti-aliased floor grid (from Lesson 12)

## Camera

Starting position: `(8, 6, 12)` looking toward origin. The default viewpoint
should show the truck, surrounding boxes, and all point lights with visible
shadow casting. Yaw/pitch set so the scene is centered.

## Point lights

2–3 point lights at different positions and colors:

- Light 0: warm white `(1.0, 0.9, 0.7)` — positioned above and to the left
- Light 1: cool blue `(0.4, 0.6, 1.0)` — positioned to the right
- Light 2 (optional): soft red `(1.0, 0.3, 0.2)` — positioned behind/above

Each light has:

- World-space position (`float3`)
- Color (`float3`)
- Intensity (HDR scalar, e.g. 5.0–15.0)
- Shadow cube map (R32_FLOAT color, `SDL_GPU_TEXTURETYPE_CUBE`) storing linear depth, with a separate D32_FLOAT depth buffer for rasterization
- Attenuation: quadratic falloff `1 / (1 + 0.09*d + 0.032*d²)`

## New concept integration

### Shadow pass (6 faces per light)

For each point light, render the scene into a cube depth map:

1. Create one `SDL_GPU_TEXTURETYPE_CUBE` color texture per light
   (R32_FLOAT, `COLOR_TARGET | SAMPLER`, e.g. 1024x1024) to store linear
   depth, plus a shared D32_FLOAT depth buffer for rasterization
2. Build 6 view matrices using `mat4_look_at()` with the standard
   cube face directions (+X, -X, +Y, -Y, +Z, -Z) and their up vectors
3. Use `mat4_perspective(PI/2, 1.0, near, far)` for the 90-degree projection
4. For each face: set `SDL_GPUDepthStencilTargetInfo.layer = face`,
   push the `light_vp` uniform, draw all shadow casters
5. Use no culling (glTF meshes may not be watertight) and a small
   fragment-shader bias to prevent shadow acne

### Fragment shader shadow lookup

Instead of projecting into 2D shadow UV (like cascaded maps), use the
world-space direction from light to fragment:

```hlsl
float3 light_to_frag = world_pos - light_pos;
float current_depth = length(light_to_frag);
float closest_depth = shadow_cube.Sample(smp, light_to_frag).r * far_plane;
float shadow = (current_depth - bias <= closest_depth) ? 1.0 : 0.0;
```

Store linearized depth (distance / far_plane) in the shadow pass fragment
shader so all 6 faces produce consistent depth values regardless of
projection direction.

### Shadow pipeline

- Vertex shader: transform position by light MVP
- Fragment shader: output `length(world_pos - light_pos) / far_plane` into the
  R32_FLOAT color channel via `SV_Target` (linear depth stored as color, not
  hardware depth — the separate D32_FLOAT buffer handles rasterization)
- Rasterizer: no culling (open glTF meshes), fragment-shader bias for shadow acne

### Render pass order

1. Shadow passes (6 faces × N lights = 12–18 depth-only passes)
2. Scene pass → HDR render target (lit geometry + shadows + grid)
3. Bloom downsample chain (5 passes)
4. Bloom upsample chain (4 passes)
5. Tone map → swapchain

### Shader bindings (scene fragment)

- `t0, space2`: diffuse texture (model albedo)
- `s0, space2`: diffuse sampler
- `t1, space2`: shadow cube map 0
- `s1, space2`: shadow sampler 0
- `t2, space2`: shadow cube map 1
- `s2, space2`: shadow sampler 1
- (if 3 lights: `t3/s3` for shadow cube map 2)

## Status

- [x] Directory scaffolded (`/start-lesson`)
- [x] Scene building (main.c, shaders)
- [x] New concept implemented
- [x] README written (`/create-lesson`)
- [x] Skill file created (`/create-lesson`)
- [x] Screenshot captured
- [ ] Final pass (`/final-pass`)
- [ ] Published (`/publish-lesson`)
