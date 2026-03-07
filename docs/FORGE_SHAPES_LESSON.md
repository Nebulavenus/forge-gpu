# Prompt: Geometry Generation Library — `forge_shapes.h`

## Context

In early GPU lessons (02, 06, etc.) geometric objects like triangles, quads,
and cubes were hand-coded as one-off static arrays. This works for single
lessons but does not scale — later lessons need spheres, cylinders, tori, and
other shapes without re-authoring vertices each time. Physics lessons, particle
systems, and debug visualisation all face the same problem.

This lesson creates `common/shapes/forge_shapes.h` — a header-only, zero-
dependency procedural geometry library that lives alongside `forge_math.h` and
follows every convention already established in forge-gpu. It is **not** a
port of `par_shapes.h`. Every line of code is written from scratch using our
`vec3`/`vec2` math types, our right-handed Y-up coordinate system, our
CCW winding order, and our documentation style.

---

## Goal

1. Create a well-tested, well-documented header-only library at
   `common/shapes/forge_shapes.h` that generates parametric 3D geometry.
2. Create a new GPU lesson at `lessons/gpu/35-procedural-geometry/` that
   teaches how the library works by building it step-by-step, then renders
   a scene that would be impossible without it.
3. Create a matching test suite at `tests/test_shapes.c`.
4. Write a matching Claude Code skill at
   `.claude/skills/forge-procedural-geometry/SKILL.md`.

---

## Library: `common/shapes/forge_shapes.h`

### File header

```c
/*
 * forge_shapes.h — Procedural geometry library for forge-gpu
 *
 * Generates 3D meshes (positions, normals, UVs, indices) from mathematical
 * descriptions.  Every shape is unit-scale and centred at the origin —
 * use mat4_translate / mat4_scale to place and size it in your scene.
 *
 * Coordinate system: Right-handed, Y-up  (+X right, +Y up, +Z toward camera)
 * Winding order:     Counter-clockwise front faces (matches forge-gpu default)
 * UV origin:         Bottom-left  (U right, V up — matches HLSL's UV space)
 * Index type:        uint32_t  (supports meshes up to 4 billion vertices)
 *
 * Usage pattern:
 *   ForgeShape sphere = forge_shapes_sphere(32, 16);
 *   // ... upload positions, normals, uvs, indices to GPU ...
 *   forge_shapes_free(&sphere);
 *
 * All generator functions produce smooth (per-vertex) normals and texture
 * coordinates unless the shape is inherently faceted (e.g. a cube).
 * Call forge_shapes_compute_flat_normals() to override with face normals.
 *
 * Memory: heap-allocated via SDL_malloc/SDL_free.  The caller owns the
 * ForgeShape and must call forge_shapes_free() when done.
 *
 * Dependencies: forge_math.h, SDL3/SDL.h (for SDL_malloc/SDL_free/SDL_assert)
 *
 * See common/shapes/README.md for full API reference.
 * See lessons/gpu/35-procedural-geometry/ for a full walkthrough.
 *
 * SPDX-License-Identifier: Zlib
 */
```

### Directory structure

```text
common/shapes/
├── forge_shapes.h    (the library — header-only, implementation in #ifdef)
└── README.md         (API reference and usage guide)
```

### `ForgeShape` struct

```c
/*
 * ForgeShape — a generated mesh ready for GPU upload.
 *
 * All arrays are flat (struct-of-arrays layout):
 *   positions[i]  — world-space position of vertex i
 *   normals[i]    — outward-facing unit normal at vertex i  (may be NULL)
 *   uvs[i]        — texture coordinates at vertex i        (may be NULL)
 *   indices[j]    — vertex index for triangle j/3, corner j%3
 *
 * Counts:
 *   vertex_count  — length of positions[], normals[], uvs[]
 *   index_count   — length of indices[] (always a multiple of 3)
 *   triangle_count == index_count / 3
 */
typedef struct {
    vec3    *positions;      /* [vertex_count]  XYZ positions            */
    vec3    *normals;        /* [vertex_count]  unit normals (may be NULL)*/
    vec2    *uvs;            /* [vertex_count]  texture coords (may be NULL)*/
    uint32_t *indices;       /* [index_count]   triangle list             */
    int      vertex_count;
    int      index_count;
} ForgeShape;
```

**Why struct-of-arrays?**
Upload positions, normals, and UVs into separate SDL GPU buffers without any
interleaving or stride work — each field maps directly to one
`SDL_GPUBuffer`. The lesson should explain this choice explicitly.

### Generator functions — shapes to implement

All shapes are unit-scale (radius 1 or side length 1) and centred at the
origin unless otherwise noted. Parameters follow the same naming convention
as the rest of forge-gpu — no abbreviations.

#### Parametric sphere

```c
/*
 * forge_shapes_sphere — UV-parameterised sphere
 *
 * Generates a sphere of radius 1 centred at the origin using the standard
 * spherical parameterisation:
 *   x = cos(longitude) * sin(latitude)
 *   y = cos(latitude)                     (Y-up)
 *   z = sin(longitude) * sin(latitude)
 *
 * slices  — number of longitudinal divisions (columns, min 3)
 * stacks  — number of latitudinal divisions  (rows,    min 2)
 *
 * A 32-slice, 16-stack sphere is a good default for most scenes.
 * More slices/stacks = smoother but more vertices.
 *
 * UV mapping: U = longitude / TAU (0..1 west-to-east)
 *             V = latitude  / PI  (0 = south pole, 1 = north pole)
 *
 * Produces smooth normals.  Texture seam at U=0/U=1 is handled by
 * duplicating vertices along the seam so each gets the correct U coordinate.
 *
 * Vertex count:  (slices + 1) * (stacks + 1)
 * Index count:   slices * stacks * 6
 *
 * See: lessons/gpu/35-procedural-geometry
 * See: https://en.wikipedia.org/wiki/Spherical_coordinate_system
 */
ForgeShape forge_shapes_sphere(int slices, int stacks);
```

#### Icosphere (subdivided icosahedron)

```c
/*
 * forge_shapes_icosphere — subdivided icosahedron
 *
 * Starts from the 12-vertex, 20-face icosahedron and repeatedly subdivides
 * every triangle into 4 smaller triangles, normalising new vertices back
 * onto the unit sphere.  This produces a much more even triangle
 * distribution than a UV sphere — no dense poles, no sparse equator.
 *
 * subdivisions  — number of subdivision passes (0 = raw icosahedron = 20
 *                 triangles, 1 = 80, 2 = 320, 3 = 1280, ...)
 *
 * Trade-off vs UV sphere:
 *   + Even triangle distribution (better for ray tracing, physics proxies)
 *   - No natural UV parameterisation (UVs are generated from position)
 *   - No poles — can't directly map cylindrical textures
 *
 * Normals: the normalised position IS the normal for a unit sphere, so
 * normals are computed as position (no extra work).
 *
 * UV mapping: spherical projection from normalised position — same formula
 * as UV sphere but without a seam-duplication pass.  Seam artefacts may
 * appear at the anti-meridian when using tiling textures.
 *
 * Vertex count: grows as 10 * 4^subdivisions + 2 (approx)
 *
 * See: lessons/gpu/35-procedural-geometry
 */
ForgeShape forge_shapes_icosphere(int subdivisions);
```

#### Cylinder

```c
/*
 * forge_shapes_cylinder — open-ended cylinder along the Y axis
 *
 * Generates the lateral surface of a cylinder of radius 1 and height 2,
 * centred at the origin.  The top cap is at Y=+1, the bottom cap at Y=-1.
 * Caps are NOT included — use forge_shapes_disk() to cap the ends.
 *
 * slices  — number of longitudinal divisions (min 3)
 * stacks  — number of height divisions       (min 1)
 *
 * UV mapping: U = longitude / TAU (wraps around)
 *             V = (y + 1) / 2     (0 = bottom, 1 = top)
 *
 * Normals: radially outward — (cos(a), 0, sin(a)) at angle a.
 *
 * See: lessons/gpu/35-procedural-geometry
 */
ForgeShape forge_shapes_cylinder(int slices, int stacks);
```

#### Cone

```c
/*
 * forge_shapes_cone — open-ended cone along the Y axis
 *
 * Apex at Y=+1, base circle of radius 1 at Y=-1, centred at origin.
 * Base cap is NOT included — use forge_shapes_disk() if you need it.
 *
 * slices  — number of longitudinal divisions around the base (min 3)
 * stacks  — number of height divisions along the slope       (min 1)
 *
 * Normals: the cone's slant normal at angle a is:
 *   n = normalize(cos(a) * sin(half_angle), cos(half_angle),
 *                 sin(a) * sin(half_angle))
 * where half_angle = atan2(1, 2) for a unit cone.  Explain this in the
 * README — it is non-obvious and a good teaching moment.
 *
 * UV mapping: U = longitude / TAU, V = height fraction
 *
 * See: lessons/gpu/35-procedural-geometry
 */
ForgeShape forge_shapes_cone(int slices, int stacks);
```

#### Torus

```c
/*
 * forge_shapes_torus — donut shape in the XZ plane
 *
 * A torus is the surface of revolution of a circle.  Two radii:
 *   major_radius  — distance from the torus centre to the tube centre
 *   tube_radius   — radius of the tube itself
 *
 * For a "fat donut": major_radius=1.0, tube_radius=0.4
 * For a "thin ring": major_radius=1.0, tube_radius=0.1
 *
 * The torus lies in the XZ plane — the hole is along the Y axis.
 *
 * slices  — divisions around the tube   (min 3, controls tube smoothness)
 * stacks  — divisions around the ring   (min 3, controls ring smoothness)
 *
 * Parameterisation:
 *   phi   = angle around the ring (0..TAU)
 *   theta = angle around the tube (0..TAU)
 *   x = (major_radius + tube_radius * cos(theta)) * cos(phi)
 *   y =  tube_radius * sin(theta)
 *   z = (major_radius + tube_radius * cos(theta)) * sin(phi)
 *
 * UV: U = phi / TAU, V = theta / TAU
 *
 * Normals: tube_radius * (cos(theta)*cos(phi), sin(theta), cos(theta)*sin(phi))
 * (point radially outward from the tube centreline — explain the derivation
 * in the README because it is a good test of understanding the two angles)
 *
 * See: lessons/gpu/35-procedural-geometry
 */
ForgeShape forge_shapes_torus(int slices, int stacks,
                               float major_radius, float tube_radius);
```

#### Plane (quad)

```c
/*
 * forge_shapes_plane — flat XZ-plane quad
 *
 * Generates a flat mesh lying in the XZ plane, centred at the origin,
 * spanning from (-1,-1) to (+1,+1) in XZ (Y=0).
 * Normal is +Y (pointing up).
 *
 * slices  — divisions along X (min 1)
 * stacks  — divisions along Z (min 1)
 *
 * UV: (0,0) at (-X, -Z) corner, (1,1) at (+X, +Z) corner.
 *
 * A 1x1 plane is two triangles; a 4x4 plane is 32 triangles.  Higher
 * tessellation enables vertex-shader displacement (terrain, water, etc.)
 *
 * See: lessons/gpu/35-procedural-geometry
 */
ForgeShape forge_shapes_plane(int slices, int stacks);
```

#### Cube / box

```c
/*
 * forge_shapes_cube — axis-aligned unit cube
 *
 * Six faces, each tessellated into a grid.  Each face has its own normal
 * and UV set — vertices are NOT shared across faces (sharing is impossible
 * because adjacent faces need different normals).
 *
 * Centred at the origin, side length 2 (extends from -1 to +1 on each axis).
 *
 * slices  — divisions per face along U (min 1)
 * stacks  — divisions per face along V (min 1)
 *
 * Face layout (UV 0,0 is bottom-left of each face):
 *   +Y face: top     — UV aligns with world X (right) and world Z (forward)
 *   -Y face: bottom  — mirrored
 *   +X face: right
 *   -X face: left
 *   +Z face: front
 *   -Z face: back
 *
 * The document the face ordering and UV orientation explicitly — it matters
 * when applying cube-map textures or checking specific face UV directions.
 *
 * See: lessons/gpu/35-procedural-geometry
 */
ForgeShape forge_shapes_cube(int slices, int stacks);
```

#### Capsule

```c
/*
 * forge_shapes_capsule — cylinder with hemispherical end-caps
 *
 * A cylinder of radius 1 and half-height `half_height` along the Y axis,
 * with a hemisphere of radius 1 at each end.  Total height = 2*half_height + 2.
 *
 * Commonly used as a physics proxy for characters and other tall objects.
 *
 * slices      — divisions around the circumference (min 3)
 * stacks      — divisions along the cylinder body  (min 1)
 * cap_stacks  — latitudinal divisions in each hemisphere cap (min 1)
 * half_height — half the length of the cylindrical section (default 1.0)
 *
 * UV: treat the capsule as a single cylinder-projected texture, where V
 * runs from 0 at the bottom pole to 1 at the top pole, passing through
 * the cylinder body linearly.  Explain the UV continuity challenge at the
 * cap/body seam — this is a good teaching point.
 *
 * See: lessons/gpu/35-procedural-geometry
 */
ForgeShape forge_shapes_capsule(int slices, int stacks, int cap_stacks,
                                 float half_height);
```

### Utility functions

```c
/*
 * forge_shapes_free — release all memory owned by a ForgeShape.
 *
 * Sets all pointers to NULL and counts to 0 after freeing.
 * Passing a zeroed or already-freed ForgeShape is safe (no-op).
 */
void forge_shapes_free(ForgeShape *shape);

/*
 * forge_shapes_compute_flat_normals — replace per-vertex normals with face normals.
 *
 * Unwelds the mesh (every triangle gets its own 3 vertices) and sets each
 * vertex normal to the face's geometric normal.  Vertex count becomes
 * index_count after unwelding; the index buffer becomes trivial (0,1,2,...).
 *
 * Use this when you want hard edges — e.g. a faceted cube or low-poly style.
 *
 * Note: this re-allocates positions, normals, uvs, and indices internally.
 * The ForgeShape is modified in place.  Old pointers are freed.
 */
void forge_shapes_compute_flat_normals(ForgeShape *shape);
```

### Implementation guard pattern

Follow the same single-header pattern established by `forge_math.h`:

```c
/* At the bottom of the file, behind a guard: */
#ifdef FORGE_SHAPES_IMPLEMENTATION
/* ... all function bodies here ... */
#endif /* FORGE_SHAPES_IMPLEMENTATION */
```

One translation unit per project defines `FORGE_SHAPES_IMPLEMENTATION` before
including the header to get the function bodies. All other includes just get
the declarations. Explain this pattern clearly in the README — it is the same
pattern used by stb libraries.

---

## Lesson: `lessons/gpu/35-procedural-geometry/`

### What the lesson teaches

The lesson demonstrates the library by rendering a **showcase scene** with:

- A UV sphere (centre, large, textured with a grid/checker texture)
- An icosphere (left, smaller, rotating, wire-frame overlay optional)
- A torus (right, rotating around its Y axis)
- A tessellated plane as the floor (4×4 grid, normal-mapped or flat-shaded)
- A capsule (back, demonstrating a compound shape)

All rendered with Blinn-Phong lighting (from Lesson 18), a camera (Lesson 07),
and a shadow map (Lesson 15) — reference those lessons rather than re-explaining
them.

### One new concept per lesson rule

The **one new concept** in this lesson is **parametric geometry generation**:
how to turn a mathematical surface description (a function of two parameters
u and v) into a triangle mesh. Everything else (lighting, camera, shadows)
is scaffolded from previous lessons.

### Key concepts to explain in README

**Parametric surfaces** — A surface is parameterised if every point on it is
described by two parameters (u, v), typically in [0,1]. A sphere is:
`P(u,v) = (cos(u*TAU)*sin(v*PI), cos(v*PI), sin(u*TAU)*sin(v*PI))`.
This maps a 2D grid of (u,v) samples onto a 3D surface.

**Slices and stacks** — The resolution of the grid.  Slices divide the
longitude (u); stacks divide the latitude (v). Explain how the number of
triangles relates to slices and stacks: `triangles = slices * stacks * 2`.

**Seam duplication** — A UV sphere needs to duplicate vertices along the seam
at u=0/u=1 because the same 3D position maps to two different U values (0 and
1). Explain why this cannot be avoided for correct UV mapping, and how it
manifests as `(slices + 1)` rather than `slices` columns.

**Normal computation on a sphere** — The outward normal at any point on a unit
sphere is the normalised position vector itself. This is unique to spheres
and worth stating explicitly.

**Smooth vs flat normals** — Smooth normals come from interpolating the
mathematical normal across a triangle; flat normals come from the triangle's
geometric normal. Show both visually (a smooth sphere next to a faceted cube).

**Struct-of-arrays vs interleaved** — Why `ForgeShape` uses separate arrays
rather than an interleaved vertex struct. Benefits: simpler GPU upload code,
easier to skip unused attributes (e.g., if a shader doesn't need UVs).

### `PLAN.md` in the lesson directory

Create a `lessons/gpu/35-procedural-geometry/PLAN.md` following the same
pattern as other GPU lessons. It must include a **"main.c Decomposition"**
section because `main.c` will exceed 800 lines. Suggested split:

- **Chunk A** — Includes, constants, vertex struct, `AppState`, shader
  bytecode includes, `create_geometry_pipeline()`, `upload_shape()`
- **Chunk B** — `SDL_AppInit()`: device, window, swapchain, depth texture,
  shadow map (referencing Lesson 15 pattern), all shape uploads, lighting
  uniforms
- **Chunk C** — `SDL_AppIterate()`: shadow pass, main render pass with all
  shape draw calls, present
- **Chunk D** — `SDL_AppQuit()`, `SDL_AppEvent()`, cleanup

### Shader notes

The lesson reuses Blinn-Phong shaders from Lesson 18 (or creates thin
variants). Shaders receive:

- `positions` buffer (or interleaved vertex if simpler for the lesson)
- MVP matrix as push uniform
- Normal matrix (transpose-inverse of model) as push uniform
- Shadow map (from Lesson 15)

Keep the shader simple — the point is the geometry generation, not a new
shading technique.

---

## Tests: `tests/test_shapes.c`

Create a comprehensive test suite that validates the library's geometry is
mathematically correct. Tests must be specific and precise — they check
actual computed values, not just that the function returned something.

### Required tests

**Sphere tests:**

1. `test_sphere_vertex_count` — `(slices+1)*(stacks+1)` vertices for a
   `forge_shapes_sphere(32, 16)`
2. `test_sphere_index_count` — `slices * stacks * 6` indices
3. `test_sphere_north_pole` — the last vertex (or designated pole vertex)
   is at `(0, 1, 0)` (Y=+1 for north pole)
4. `test_sphere_south_pole` — pole vertex at `(0, -1, 0)`
5. `test_sphere_normals_unit_length` — every normal has length `1.0 ±
   FORGE_EPSILON`
6. `test_sphere_positions_on_unit_sphere` — every position has length `1.0 ±
   FORGE_EPSILON`
7. `test_sphere_uv_range` — all U in [0,1], all V in [0,1]
8. `test_sphere_winding_order` — for each triangle, the cross product of
   (v1-v0) × (v2-v0) points outward (dot with centroid direction is positive)

**Icosphere tests:**

1. `test_icosphere_subdivision_0` — 12 vertices, 60 indices (20 faces × 3)
2. `test_icosphere_subdivision_1` — 42 vertices, 240 indices
3. `test_icosphere_positions_on_unit_sphere` — every position has length 1.0
4. `test_icosphere_normals_match_positions` — for an icosphere, normal equals
    normalised position

**Torus tests:**

1. `test_torus_vertex_count` — `(slices+1) * (stacks+1)` vertices
2. `test_torus_inner_radius` — minimum distance from Y axis equals
    `major_radius - tube_radius`
3. `test_torus_outer_radius` — maximum distance from Y axis equals
    `major_radius + tube_radius`
4. `test_torus_normals_unit_length` — every normal has length 1.0
5. `test_torus_lies_in_xz_plane` — all Y values are in
    `[-tube_radius, +tube_radius]`

**Plane tests:**

1. `test_plane_vertex_count` — `(slices+1) * (stacks+1)` for an N×M plane
2. `test_plane_index_count` — `slices * stacks * 6`
3. `test_plane_all_normals_up` — every normal is `(0, 1, 0)`
4. `test_plane_y_is_zero` — every position has Y=0
5. `test_plane_uv_corners` — corner UVs are (0,0), (1,0), (0,1), (1,1)

**Cube tests:**

1. `test_cube_vertex_count` — 6 faces × `(slices+1)*(stacks+1)` for a
    1×1 cube
2. `test_cube_face_normals_axis_aligned` — every normal is one of the 6
    axis-aligned directions
3. `test_cube_positions_in_unit_box` — every position component is in [-1, 1]

**Flat normals test:**

1. `test_flat_normals_constant_per_triangle` — after calling
    `forge_shapes_compute_flat_normals()`, the 3 vertices of every triangle
    share an identical normal

**Memory tests:**

1. `test_free_zeroes_pointers` — after `forge_shapes_free()`, all pointers
    are NULL and counts are 0
2. `test_free_is_safe_on_null_shape` — calling `forge_shapes_free()` on a
    zeroed `ForgeShape` does not crash

### Test harness

Follow the existing test harness pattern in `tests/` (check
`tests/test_math.c` for reference). Use SDL for `SDL_assert` or write a
minimal `CHECK` macro consistent with existing tests. Register the test
target in the root `CMakeLists.txt` following the same pattern as
`test_math`, `test_gltf`, etc.:

```cmake
add_executable(test_shapes tests/test_shapes.c)
target_include_directories(test_shapes PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_shapes PRIVATE SDL3::SDL3)
add_test(NAME shapes COMMAND test_shapes)
```

---

## `common/shapes/README.md`

Create a comprehensive API reference and usage guide. Must include:

- Quick-start example (generate a sphere, upload to GPU, free)
- Full API table with every function, its parameters, and return type
- Shape gallery with vertex/index counts for common tessellation levels
- Coordinate system diagram (show Y-up, CCW winding, UV orientation)
- "How parametric generation works" section with a diagram showing the
  uv-grid to mesh mapping
- "Smooth vs flat normals" section with visual explanation
- "Struct-of-arrays layout" section explaining the memory model
- "Adding a new shape" guide for contributors

---

## Root file updates

### `common/` area — `CMakeLists.txt`

`forge_shapes.h` is header-only with no compiled output, so no new
`add_library` target is needed. However, add a comment in the root
`CMakeLists.txt` under the "Common Libraries" section noting the shapes
library exists and where it lives.

### `tests/` — `CMakeLists.txt`

Add `test_shapes` as described above.

### Root `README.md`

Add a row in the "Common Libraries" table:

```text
| `common/shapes/` | forge_shapes.h | Procedural geometry — sphere, cylinder, torus, and more |
```

### Root `PLAN.md`

Add the lesson to the GPU Lessons list and mark it completed after finishing.

---

## Code conventions checklist

Before finalising:

- [ ] All public names use `forge_shapes_` prefix
- [ ] `ForgeShape` uses `PascalCase` (it is a public type)
- [ ] Every generator function comment documents: what shape, origin,
      scale, UV convention, normal convention, vertex/index count formula
- [ ] No magic numbers — `#define` or `enum` for any constant that appears
      more than once
- [ ] `SDL_malloc` / `SDL_free` everywhere (NOT `malloc`/`free`)
- [ ] Every generator does bounds-checking on `slices`/`stacks` and calls
      `SDL_assert` with a descriptive message if values are below minimum
- [ ] `forge_shapes_free()` NULLs all pointers after freeing
- [ ] Test file uses only ASCII console output
- [ ] All markdown files pass `npx markdownlint-cli2`
- [ ] SPDX-License-Identifier: Zlib on all source files
- [ ] Unix line endings (LF) on all files
- [ ] Library has no dependency on SDL GPU — only `SDL_malloc`,
      `SDL_free`, `SDL_assert` (from `SDL3/SDL.h`) and `forge_math.h`

---

## Large file write reminder

`main.c` for the GPU lesson will exceed 800 lines. **Mandatory chunked-write
pattern:** write parts A, B, C, D to `/tmp/`, then concatenate with `cat`.
See `.claude/large-file-strategy.md` for the full strategy.

`forge_shapes.h` itself may also exceed 800 lines. Use the same chunked
approach — split at logical section boundaries (sphere, icosphere, torus,
cylinder, etc.).

---

## Diagrams required

Use `/dev-create-diagram` to generate the following:

1. **UV grid to sphere mesh** — a 2D grid of (u,v) samples on the left,
   arrows showing how each maps to a 3D sphere surface on the right.
   Show the seam duplication with a highlighted column.

2. **Smooth vs flat normals** — two identical icospheres side by side,
   one with smooth per-vertex normals shown as arrows, one with flat
   per-face normals. Labels identify the difference clearly.

3. **Sphere vs icosphere triangle distribution** — top-down view of a
   UV sphere (showing dense poles, sparse equator) and an icosphere
   (showing even distribution). Annotate slice/stack lines on the UV
   sphere.

4. **Torus parameterisation** — cross-section diagram showing `major_radius`
   and `tube_radius`, with `phi` (around the ring) and `theta` (around the
   tube) labelled.

Place all diagrams in `lessons/gpu/35-procedural-geometry/assets/`.

---

## Skill file: `.claude/skills/forge-procedural-geometry/SKILL.md`

Create a skill that teaches future agents (and humans using Claude Code) how
to generate and render a shape with `forge_shapes.h`. Should cover:

- Include pattern (`#define FORGE_SHAPES_IMPLEMENTATION` in one `.c` file)
- Generate a shape, upload it to SDL GPU buffer(s), bind, draw
- How to combine multiple shapes in one scene (upload loop, draw loop)
- Common mistakes: forgetting to free, uploading normals to wrong binding slot,
  incorrect vertex attribute offsets for struct-of-arrays layout
- Code template for a minimal "load and draw a sphere" scene

---

## Acceptance criteria

The lesson is complete when:

1. `cmake --build build --target test_shapes && ctest --test-dir build -R shapes`
   passes all 28 tests with zero failures.
2. `cmake --build build --target 35-procedural-geometry` succeeds on both
   Windows (MSVC) and Linux (GCC/Clang).
3. The lesson runs and displays all five shapes with correct lighting and
   shadows.
4. `npx markdownlint-cli2 "**/*.md"` passes with zero errors.
5. `ruff check scripts/` passes (if any Python diagram scripts were added).
6. Every public function in `forge_shapes.h` is tested by at least one
   test in `test_shapes.c`.
7. Every test name appears in the test output with PASS or FAIL.
