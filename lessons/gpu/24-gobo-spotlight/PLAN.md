# Lesson 24 — Gobo Spotlight

## Concept

Projected-texture (cookie/gobo) spotlight. This lesson introduces:

1. **Spotlight cone** with inner/outer angles and smooth falloff
2. **Gobo texture projection** — projecting a pattern through the light like a
   theatrical gobo or cookie
3. **Spotlight shadow map** — a single perspective shadow map from the
   spotlight's frustum (simpler than the cube maps from Lesson 23)
4. **Theatrical/cinematic lighting** — combining these techniques for dramatic
   scene illumination

The key insight is that a spotlight's frustum is identical to a standard
perspective camera frustum, so the same view-projection matrix that defines
the light cone also maps fragments into the gobo texture's UV space.

## Scene requirements

The scene needs a theatrical/stage feel to showcase the spotlight and gobo
projection effectively. Check off each feature as it is implemented:

- [ ] Camera: FPS fly camera (WASD + mouse look, from Lesson 07 pattern)
- [ ] Lighting: One spotlight with inner/outer cone angles, plus dim ambient
- [ ] Shadow map: Single 2D depth texture from the spotlight's perspective
- [ ] Gobo projection: Cookie texture projected through the spotlight cone
- [ ] Materials: Blinn-Phong shading (diffuse + specular)
- [ ] Models: CesiumMilkTruck + BoxTextured crates on a ground plane
- [ ] Grid floor: Procedural grid (from Lesson 12 pattern) to show gobo pattern
- [ ] HDR + bloom: R16G16B16A16_FLOAT render target, bloom, tone mapping
- [ ] sRGB swapchain: SDR_LINEAR for correct gamma output
- [ ] Gobo asset: A gobo pattern texture (procedural or loaded PNG)

## Camera

Start position: `(0, 4, 8)` looking toward the origin. The scene should show
the CesiumMilkTruck parked near center, a few BoxTextured crates scattered
around, and the spotlight illuminating them from above-left. The gobo pattern
should be clearly visible on the ground plane.

## Spotlight definition

```c
typedef struct Spotlight {
    vec3  position;      /* world-space light position */
    vec3  direction;      /* unit vector: where the spotlight points */
    vec3  color;          /* light color (HDR, can exceed 1.0) */
    float inner_angle;    /* inner cone half-angle in radians (full intensity) */
    float outer_angle;    /* outer cone half-angle in radians (falloff to zero) */
    float range;          /* attenuation distance */
} Spotlight;
```

The spotlight view matrix is `mat4_look_at(position, position + direction, up)`.
The projection matrix is `mat4_perspective(2 * outer_angle, 1.0, near, far)` —
the FOV covers the full outer cone.

## Shadow mapping approach

Unlike Lesson 23's cube maps (6 faces for omnidirectional), a spotlight only
needs a **single 2D depth texture**:

- Render the scene from the spotlight's perspective into a D32_FLOAT depth texture
- In the main pass, transform fragments into light clip space
- Perform perspective divide to get shadow map UVs
- Compare depth with stored depth (+ bias) for shadow test
- Apply PCF (percentage-closer filtering) for soft shadow edges

This is conceptually simpler than both cascaded shadows (Lesson 15) and point
light shadows (Lesson 23).

## Gobo texture projection

The gobo (or "cookie") is a texture projected through the spotlight, like a
physical gobo disc in a theatrical light:

1. Transform the world-space fragment position into light clip space
2. Perform perspective divide → NDC
3. Remap NDC `[-1,1]` → UV `[0,1]`
4. Sample the gobo texture at those UVs
5. Multiply the light contribution by the gobo sample

The same light view-projection matrix serves double duty: shadow mapping AND
gobo projection. Fragments outside the spotlight cone (UV outside `[0,1]`)
get zero light contribution.

## Shader plan

| File | Purpose |
|------|---------|
| `shadow.vert.hlsl` | Transform vertices into light clip space |
| `shadow.frag.hlsl` | Empty or writes depth only (hardware depth) |
| `scene.vert.hlsl` | Main pass: transform + pass world pos, normal, light-space pos |
| `scene.frag.hlsl` | Blinn-Phong + spotlight cone + gobo sample + shadow test |
| `grid.vert.hlsl` | Grid floor vertex shader |
| `grid.frag.hlsl` | Procedural grid + spotlight + gobo + shadow |
| `bloom_downsample.frag.hlsl` | Bloom downsample (13-tap from Lesson 22) |
| `bloom_upsample.frag.hlsl` | Bloom upsample (9-tap tent from Lesson 22) |
| `tonemap.frag.hlsl` | ACES tone mapping (from Lesson 21) |
| `fullscreen.vert.hlsl` | Fullscreen triangle for post-processing |

## New concept integration

The spotlight cone + gobo projection is the new concept. It integrates into
the scene fragment shader after lighting calculation:

```hlsl
// 1. Compute light-space position
float4 light_clip = mul(light_vp, float4(world_pos, 1.0));
float3 light_ndc  = light_clip.xyz / light_clip.w;

// 2. Remap to UV space [0,1]
float2 gobo_uv = light_ndc.xy * 0.5 + 0.5;
gobo_uv.y = 1.0 - gobo_uv.y;  // flip Y for texture coords

// 3. Spotlight cone falloff
float3 to_frag = normalize(world_pos - light_pos);
float  cos_angle = dot(to_frag, light_dir);
float  falloff = smoothstep(cos_outer, cos_inner, cos_angle);

// 4. Sample gobo pattern
float gobo = gobo_tex.Sample(gobo_sampler, gobo_uv).r;

// 5. Shadow test
float shadow = sample_shadow(light_ndc, shadow_depth);

// 6. Final spotlight contribution
float3 spotlight = light_color * falloff * gobo * shadow * blinn_phong;
```

## Gobo texture options

For the lesson, we'll generate a simple gobo pattern procedurally (a window
frame pattern or radial starburst). This avoids external asset dependencies
while clearly demonstrating the projection technique. The exercises can suggest
loading custom PNG gobos.

## Status

- [x] Directory scaffolded (`/start-lesson`)
- [x] Scene building (main.c, shaders)
- [x] New concept implemented
- [x] README written (`/create-lesson`)
- [x] Skill file created (`/create-lesson`)
- [x] Screenshot captured
- [ ] Final pass (`/final-pass`)
- [ ] Published (`/publish-lesson`)
