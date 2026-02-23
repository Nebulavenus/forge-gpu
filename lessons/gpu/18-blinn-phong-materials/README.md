# Lesson 18 — Blinn-Phong with Materials

## What you'll learn

- How to define per-object **material properties** (ambient, diffuse, specular
  colors and shininess)
- The difference between **metallic** and **dielectric** specular behavior
- How to push **different uniform data** per draw call to render multiple
  objects with distinct appearances
- The classic **OpenGL material property tables** and why they work
- How material parameters map to the three terms of Blinn-Phong lighting

## Result

![Lesson 18 screenshot](assets/screenshot.png)

Five Suzanne heads rendered side by side on a grid floor, each with a different
material: Gold, Red Plastic, Jade, Pearl, and Chrome.  The same model, the same
light, the same shader — only the material uniforms change.  Notice how Gold and
Chrome have colored specular highlights (metallic), while Red Plastic has
near-white highlights (dielectric).

## Key concepts

### The Material struct

Lesson 10 used global constants for lighting parameters — one shininess value,
one ambient strength, one specular strength, and always-white specular
highlights.  Every surface in the scene looked the same.

This lesson introduces a `Material` struct that groups the three reflectance
colors of Blinn-Phong lighting:

```c
typedef struct Material {
    float ambient[4];    /* ambient reflectance (rgb)             */
    float diffuse[4];    /* diffuse reflectance (rgb)             */
    float specular[4];   /* specular reflectance (rgb) + shininess (w) */
} Material;
```

Each color describes how the surface interacts with a component of the lighting
equation:

- **Ambient** — the color reflected in shadow (indirect light approximation)
- **Diffuse** — the main surface color under direct illumination
- **Specular** — the highlight color and intensity
- **Shininess** — how tight the specular highlight is (packed in `specular[3]`)

### Metallic vs dielectric specular

The most important insight this lesson demonstrates is the difference between
**metallic** and **dielectric** materials:

**Dielectrics** (plastics, stone, wood) reflect all wavelengths roughly equally
at specular angles.  Their highlights are near-white regardless of the surface
color.  Red Plastic has a red diffuse color but white-ish specular highlights.

**Metals** (gold, copper, chrome) absorb and reflect light wavelength-selectively
even at specular angles.  Gold has golden highlights because it reflects more red
and green than blue at all angles.  This is why `mat_specular` is a full RGB
color, not a scalar — it encodes this wavelength-dependent reflectance.

### Classic material tables

The five materials in this lesson are adapted from the OpenGL Programming Guide
(commonly known as the "Devernay tables").  These values are physically motivated
approximations that capture the essential character of each material:

| Material | Shininess | Character |
|----------|-----------|-----------|
| Gold | 51.2 | Warm metallic — colored specular highlights |
| Red Plastic | 32.0 | Bright dielectric — near-white highlights |
| Jade | 12.8 | Soft stone — wide, subtle highlights |
| Pearl | 11.3 | Warm white — gentle iridescent highlights |
| Chrome | 76.8 | Mirror-like — tight, bright highlights |

### Per-object uniforms

The rendering loop draws the same Suzanne model five times.  Before each draw
call, different material uniforms are pushed to the fragment shader:

```c
for (obj_i = 0; obj_i < NUM_OBJECTS; obj_i++) {
    const SceneObject *obj = &scene_objects[obj_i];
    const Material *mat = obj->material;

    /* Build model matrix: place at object position, then apply
     * the glTF node's own transform (rotation, scale, etc.) */
    const ForgeGltfNode *node = &scene.nodes[0];
    mat4 translate = mat4_translate(obj->position);
    mat4 model = mat4_multiply(translate, node->world_transform);

    /* Push material colors to the fragment shader */
    FragUniforms fu;
    SDL_memcpy(fu.mat_ambient,  mat->ambient,  sizeof(fu.mat_ambient));
    SDL_memcpy(fu.mat_diffuse,  mat->diffuse,  sizeof(fu.mat_diffuse));
    SDL_memcpy(fu.mat_specular, mat->specular, sizeof(fu.mat_specular));
    /* ... lighting parameters ... */
    SDL_PushGPUFragmentUniformData(cmd, 0, &fu, sizeof(fu));

    /* Draw the model */
    SDL_DrawGPUIndexedPrimitives(pass, prim->index_count, 1, 0, 0, 0);
}
```

This is the same push-uniform pattern from Lesson 03, but now each draw call
gets different data.  The GPU pipeline doesn't change — only the uniforms do.

### The lighting equation

The fragment shader implements the full per-material Blinn-Phong equation:

```text
ambient  = mat_ambient.rgb
diffuse  = max(dot(N, L), 0) * mat_diffuse.rgb
specular = pow(max(dot(N, H), 0), shininess) * mat_specular.rgb
result   = ambient + diffuse + specular
```

Compared to Lesson 10:

- `ambient` is now a full RGB color (was `scalar * surface_color`)
- `diffuse` uses the material's diffuse color (was `NdotL * surface_color`)
- `specular` uses a full RGB color (was `scalar * white`)
- `shininess` is per-material (was a global constant)

## Math

This lesson uses:

- **Vectors** — [Math Lesson 01](../../math/01-vectors/) for positions,
  normals, and light directions
- **Matrices** — [Math Lesson 05](../../math/05-matrices/) for model/view/projection
  transforms
- **View matrix** — [Math Lesson 09](../../math/09-view-matrix/) for the
  first-person camera

## Shaders

| File | Purpose |
|------|---------|
| `material.vert.hlsl` | Transforms vertices to clip and world space with normal transformation via adjugate transpose |
| `material.frag.hlsl` | Per-material Blinn-Phong lighting with full RGB ambient, diffuse, and specular colors |
| `grid.vert.hlsl` | Transforms the procedural floor grid quad to clip space |
| `grid.frag.hlsl` | Anti-aliased procedural grid with Blinn-Phong lighting |

## Building

```bash
# Configure (first time only)
cmake -B build

# Build
cmake --build build --config Debug --target 18-blinn-phong-materials

# Run
./build/lessons/gpu/18-blinn-phong-materials/Debug/18-blinn-phong-materials
```

Or use the run script:

```bash
python scripts/run.py 18
```

## AI skill

This lesson has a matching Claude Code skill at
[`.claude/skills/blinn-phong-materials/SKILL.md`](../../../.claude/skills/blinn-phong-materials/SKILL.md).
Invoke it with `/blinn-phong-materials` to add per-material Blinn-Phong lighting
to any SDL GPU project.  Copy the skill into your own project to use it with
Claude.

## Exercises

1. **Add more materials.** Look up the full OpenGL material property tables
   (search for "OpenGL material properties Devernay") and add Copper, Emerald,
   Ruby, or Obsidian.  Pay attention to how the ambient/diffuse/specular ratios
   differ between metals and gemstones.

2. **Texture-modulated materials.** Set `has_texture = 1` for one of the objects
   and observe how the Suzanne base color texture modulates the material's
   diffuse color.  Gold-tinted Suzanne with the original texture pattern is an
   interesting effect.

3. **Animate material transitions.** Use `forge_lerpf()` (from the math library)
   to smoothly interpolate between two materials over time.  Lerp each component
   (ambient, diffuse, specular, shininess) independently.  This is the foundation
   for material animation effects.

4. **Multiple lights.** Extend the shader to accept two directional lights (add a
   second `light_dir` to the uniform buffer).  Compute the Blinn-Phong equation
   for each light and sum the results.  Observe how the second light reveals
   different aspects of each material's specular response.
