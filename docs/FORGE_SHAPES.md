---
name: dev-forge-shapes
description: Build forge_shapes.h — a header-only procedural geometry library — and GPU Lesson 35 that teaches parametric surface generation by building the library step by step
---

Build the `forge_shapes.h` procedural geometry library and the GPU lesson that
teaches how it works.  The library lives in `common/shapes/` alongside
`forge_math.h` and follows every forge-gpu convention: right-handed Y-up,
CCW winding, `vec3`/`vec2` math types, `SDL_malloc`/`SDL_free`, and
thorough inline documentation.  The lesson renders a five-shape showcase
scene using the library.

**This is a one-shot deliverable.** All three outputs are created together:

1. `common/shapes/forge_shapes.h` — header-only library
2. `lessons/gpu/35-procedural-geometry/` — GPU lesson
3. `tests/test_shapes.c` — comprehensive test suite

---

## Step 1 — Read existing patterns before writing any code

Read all of the following before writing a single line:

- `common/math/forge_math.h` — documentation style, `static inline`, naming
- `common/math/DESIGN.md` — coordinate system, matrix layout, design rationale
- `CLAUDE.md` — code style, tone, error handling rules
- `lessons/gpu/06-depth-and-3d/main.c` — early hand-coded cube geometry to
  replace
- `.claude/large-file-strategy.md` — chunked write rules (mandatory)
- `tests/test_math.c` — test harness pattern to follow

---

## Step 2 — Create `common/shapes/forge_shapes.h`

### File header (copy verbatim, fill bracketed items)

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
 * UV origin:         Bottom-left  (U right, V up — matches SDL GPU UV space)
 * Index type:        uint32_t  (supports meshes up to 4 billion vertices)
 *
 * Usage pattern:
 *   ForgeShape sphere = forge_shapes_sphere(32, 16);
 *   // ... upload positions, normals, uvs, indices to GPU ...
 *   forge_shapes_free(&sphere);
 *
 * Memory: heap-allocated via SDL_malloc / SDL_free.  The caller owns the
 * ForgeShape and must call forge_shapes_free() when done.
 *
 * Dependencies: math/forge_math.h, SDL3/SDL.h (SDL_malloc, SDL_free,
 *               SDL_assert only — no GPU API calls)
 *
 * See common/shapes/README.md for full API reference.
 * See lessons/gpu/35-procedural-geometry/ for a full walkthrough.
 *
 * SPDX-License-Identifier: Zlib
 */
```

### `ForgeShape` struct

```c
/*
 * ForgeShape — a generated mesh ready for GPU upload.
 *
 * Struct-of-arrays layout: each field maps to one SDL_GPUBuffer with no
 * interleaving.  Upload positions to the position buffer, normals to the
 * normal buffer, etc.  This avoids stride arithmetic at upload time and
 * lets shaders that don't need UVs skip that buffer entirely.
 *
 * All arrays are flat and parallel — positions[i], normals[i], and uvs[i]
 * all describe vertex i.  indices[] contains triangle_count * 3 vertex
 * indices in CCW order.
 *
 *   vertex_count   — length of positions[], normals[], uvs[]
 *   index_count    — length of indices[] (always a multiple of 3)
 *   triangle_count — index_count / 3
 *
 * normals and uvs may be NULL for shapes that were not generated with them
 * (currently all generators produce both, but future shapes may not).
 */
typedef struct {
    vec3     *positions;    /* [vertex_count]  XYZ positions            */
    vec3     *normals;      /* [vertex_count]  unit normals (may be NULL)*/
    vec2     *uvs;          /* [vertex_count]  texture coords (may be NULL)*/
    uint32_t *indices;      /* [index_count]   CCW triangle list         */
    int       vertex_count;
    int       index_count;
} ForgeShape;
```

### Generators to implement

Document every generator with the same depth as `forge_math.h` — coordinate
conventions, UV range, normal method, vertex/index count formula, and cross-
references to the lesson.  Show the parametric equations in the comment.

#### `forge_shapes_sphere(int slices, int stacks)`

UV-parameterised sphere of radius 1, centred at origin.

```text
x = cos(phi) * sin(theta)    phi   in [0, TAU]  (longitude)
y = cos(theta)               theta in [0, PI]   (latitude, Y-up)
z = sin(phi) * sin(theta)
```

UV: `U = phi / TAU`, `V = 1 - theta / PI` (V=1 at north pole).

Seam duplication: duplicate vertices at `phi=0`/`phi=TAU` so each gets the
correct U value (0 or 1).  Explain this in the comment — it is non-obvious.

Vertex count: `(slices + 1) * (stacks + 1)`
Index count: `slices * stacks * 6`

#### `forge_shapes_icosphere(int subdivisions)`

Start from the 12-vertex icosahedron; subdivide every triangle into 4 smaller
triangles and normalise new vertices back onto the unit sphere.

Subdivision 0 = 20 faces / 12 vertices / 60 indices.
Subdivision 1 = 80 faces / 42 vertices / 240 indices.

Normal = normalised position (for a unit sphere).  Explain this in the
comment.

#### `forge_shapes_cylinder(int slices, int stacks)`

Open-ended cylinder, radius 1, height 2 (Y from -1 to +1), no caps.
Normal: radially outward `(cos(phi), 0, sin(phi))`.
UV: `U = phi / TAU`, `V = (y + 1) / 2`.

#### `forge_shapes_cone(int slices, int stacks)`

Apex at Y=+1, base radius 1 at Y=-1, no base cap.
Explain the slant-normal formula in the comment — it requires the half-angle.

#### `forge_shapes_torus(int slices, int stacks, float major_radius, float tube_radius)`

Torus in the XZ plane (hole along Y axis).

```text
phi   = angle around the ring  [0, TAU]
theta = angle around the tube  [0, TAU]

x = (major_radius + tube_radius * cos(theta)) * cos(phi)
y =  tube_radius * sin(theta)
z = (major_radius + tube_radius * cos(theta)) * sin(phi)
```

Normal direction: `(cos(theta)*cos(phi), sin(theta), cos(theta)*sin(phi))`.
Explain why the tube-centre point (before adding `tube_radius`) is subtracted
to get the outward direction.

UV: `U = phi / TAU`, `V = theta / TAU`.
Vertex count: `(slices + 1) * (stacks + 1)`.

#### `forge_shapes_plane(int slices, int stacks)`

Flat XZ-plane quad, Y=0, extents [-1,+1] in X and Z, normal = +Y.
UV: `(0,0)` at (-X,-Z) corner, `(1,1)` at (+X,+Z) corner.
Higher tessellation enables vertex-shader displacement.

#### `forge_shapes_cube(int slices, int stacks)`

Six faces, each a tessellated quad with its own flat normal.  Vertices are
NOT shared across faces.  Extents [-1,+1] on all axes.

Document the face order and UV orientation per face — it matters for cube-map
textures.

Vertex count: `6 * (slices + 1) * (stacks + 1)`.

#### `forge_shapes_capsule(int slices, int stacks, int cap_stacks, float half_height)`

Cylinder of radius 1, length `2 * half_height`, with a hemisphere cap at each
end.  Total height = `2 * half_height + 2`.

UV: treat as a single cylindrical projection; V runs from 0 (south pole) to
1 (north pole) continuously through the cylinder body.  Explain the V
continuity calculation at the cap/body seam in the comment.

### Utility functions

```c
void forge_shapes_free(ForgeShape *shape);
/* Sets all pointers to NULL and counts to 0 after freeing.
 * Passing a zeroed or already-freed ForgeShape is safe (no-op). */

void forge_shapes_compute_flat_normals(ForgeShape *shape);
/* Unwelds the mesh (every triangle gets 3 unique vertices) and replaces
 * normals with the geometric face normal.  vertex_count becomes
 * index_count after unwelding; the index buffer becomes 0,1,2,...
 * Re-allocates positions, normals, uvs, indices in place.  Old memory
 * is freed. */
```

### Single-header implementation guard

```c
#ifdef FORGE_SHAPES_IMPLEMENTATION
/* ... all function bodies ... */
#endif /* FORGE_SHAPES_IMPLEMENTATION */
```

One `.c` file in the project defines `FORGE_SHAPES_IMPLEMENTATION` before
including the header.  All other files just get the declarations.  Explain
this pattern in the README — it is the same pattern used by stb libraries.

### Chunked write rule

`forge_shapes.h` will exceed 800 lines.  Write it in parts to `/tmp/`, then
concatenate:

- **Part A** — file header, `ForgeShape` struct, declarations
- **Part B** — `forge_shapes_sphere`, `forge_shapes_icosphere`
- **Part C** — `forge_shapes_cylinder`, `forge_shapes_cone`,
  `forge_shapes_torus`
- **Part D** — `forge_shapes_plane`, `forge_shapes_cube`,
  `forge_shapes_capsule`, `forge_shapes_free`,
  `forge_shapes_compute_flat_normals`

```bash
cat /tmp/shapes_a.h /tmp/shapes_b.h /tmp/shapes_c.h /tmp/shapes_d.h \
    > common/shapes/forge_shapes.h
```

---

## Step 3 — Create `common/shapes/README.md`

Include:

- Quick-start code snippet
- Full API table (function, parameters, return, notes)
- Shape gallery — vertex/index counts at common tessellation levels
- "How parametric generation works" explanation with UV-grid diagram
- "Smooth vs flat normals" section
- "Struct-of-arrays layout" explanation
- "Adding a new shape" guide for contributors

---

## Step 4 — Create `tests/test_shapes.c`

Follow the test harness in `tests/test_math.c` exactly.  Write a `CHECK`
macro and a `run_test` wrapper consistent with existing tests.

### Required tests — implement all 28

**Sphere:**

| # | Test | What to check |
|---|---|---|
| 1 | `test_sphere_vertex_count` | `(slices+1)*(stacks+1)` vertices for `sphere(32,16)` |
| 2 | `test_sphere_index_count` | `slices * stacks * 6` indices |
| 3 | `test_sphere_north_pole` | Designated north-pole vertex at `(0, 1, 0)` |
| 4 | `test_sphere_south_pole` | Designated south-pole vertex at `(0, -1, 0)` |
| 5 | `test_sphere_normals_unit_length` | Every normal has length `1.0 ± 1e-5` |
| 6 | `test_sphere_positions_on_unit_sphere` | Every position has length `1.0 ± 1e-5` |
| 7 | `test_sphere_uv_range` | All U in `[0,1]`, all V in `[0,1]` |
| 8 | `test_sphere_winding_order` | For each triangle, `(v1-v0) × (v2-v0)` dotted with centroid direction is positive |

**Icosphere:**

| # | Test | What to check |
|---|---|---|
| 9 | `test_icosphere_sub0_counts` | 12 vertices, 60 indices |
| 10 | `test_icosphere_sub1_counts` | 42 vertices, 240 indices |
| 11 | `test_icosphere_positions_on_unit_sphere` | Every position has length `1.0 ± 1e-5` |
| 12 | `test_icosphere_normals_match_positions` | `normal[i] == normalize(position[i])` |

**Torus:**

| # | Test | What to check |
|---|---|---|
| 13 | `test_torus_vertex_count` | `(slices+1) * (stacks+1)` |
| 14 | `test_torus_inner_radius` | Min XZ distance from origin ≈ `major - tube` |
| 15 | `test_torus_outer_radius` | Max XZ distance from origin ≈ `major + tube` |
| 16 | `test_torus_normals_unit_length` | Every normal length `1.0 ± 1e-5` |
| 17 | `test_torus_y_extent` | All Y in `[-tube_radius, +tube_radius]` |

**Plane:**

| # | Test | What to check |
|---|---|---|
| 18 | `test_plane_vertex_count` | `(slices+1) * (stacks+1)` |
| 19 | `test_plane_index_count` | `slices * stacks * 6` |
| 20 | `test_plane_all_normals_up` | Every normal == `(0, 1, 0)` |
| 21 | `test_plane_y_is_zero` | Every position Y == 0 |
| 22 | `test_plane_uv_corners` | Corners have UVs `(0,0)` `(1,0)` `(0,1)` `(1,1)` |

**Cube:**

| # | Test | What to check |
|---|---|---|
| 23 | `test_cube_vertex_count` | `6 * (slices+1) * (stacks+1)` for `cube(1,1)` |
| 24 | `test_cube_face_normals_axis_aligned` | Every normal is one of the 6 axis directions |
| 25 | `test_cube_positions_in_unit_box` | Every position component in `[-1, 1]` |

**Flat normals:**

| # | Test | What to check |
|---|---|---|
| 26 | `test_flat_normals_constant_per_triangle` | After `compute_flat_normals()`, all 3 vertices of every triangle share an identical normal |

**Memory:**

| # | Test | What to check |
|---|---|---|
| 27 | `test_free_zeroes_pointers` | After `forge_shapes_free()`, all pointers NULL, counts 0 |
| 28 | `test_free_safe_on_zeroed_shape` | `forge_shapes_free()` on a zero-initialised `ForgeShape` does not crash |

### Register the test target

In root `CMakeLists.txt`, add alongside `test_math`, `test_gltf`, etc.:

```cmake
add_executable(test_shapes tests/test_shapes.c)
target_include_directories(test_shapes PRIVATE ${FORGE_COMMON_DIR})
target_link_libraries(test_shapes PRIVATE SDL3::SDL3)
add_test(NAME shapes COMMAND test_shapes)
```

---

## Step 5 — Create `lessons/gpu/35-procedural-geometry/`

### Scene

Render five shapes simultaneously with Blinn-Phong lighting (pattern from
Lesson 18) and a first-person camera (Lesson 07).  Shadows are optional —
reference Lesson 15 if included.  Focus is on the geometry, not new
rendering techniques.

| Shape | Position | Material | Motion |
|---|---|---|---|
| UV sphere | centre, large | white/grey | stationary |
| Icosphere | left | jade green | slow Y-axis rotation |
| Torus | right | gold | Y-axis rotation |
| Tessellated plane | floor (4×4) | dark stone | stationary |
| Capsule | back-centre | red plastic | stationary |

### One new concept

The single new concept is **parametric geometry generation** — how a function
of two parameters (u, v) in [0,1] becomes a triangle mesh.  Everything else
(lighting, camera) is scaffolded from previous lessons and cross-referenced,
not re-explained.

### Key concepts for the README

These must all be explained with depth:

- **Parametric surfaces** — every point described by two parameters; show the
  sphere formula explicitly
- **Slices and stacks** — the grid resolution; `triangles = slices * stacks * 2`
- **Seam duplication** — why `(slices+1)` columns exist; what happens without it
- **Normal computation on a sphere** — the outward normal equals the normalised
  position; why this is unique to spheres
- **Smooth vs flat normals** — show both visually (smooth sphere, faceted cube)
- **Struct-of-arrays vs interleaved** — why separate buffers simplify GPU upload

### `PLAN.md` in the lesson directory

Create `lessons/gpu/35-procedural-geometry/PLAN.md` with a **"main.c
Decomposition"** section before writing any code:

- **Chunk A** — includes, constants, `Vertex` struct (position + normal),
  `AppState`, `create_pipeline()`, `upload_shape()`
- **Chunk B** — `SDL_AppInit()`: device, window, swapchain, depth texture,
  all five `forge_shapes_*()` calls, GPU buffer uploads
- **Chunk C** — `SDL_AppIterate()`: uniform updates, render pass with five
  draw calls
- **Chunk D** — `SDL_AppQuit()`, `SDL_AppEvent()`, all cleanup

Write each chunk to `/tmp/` then concatenate before the build step.

### Shaders

Reuse Blinn-Phong vertex + fragment shaders from Lesson 18, or write thin
variants.  Receive position and normal as separate vertex buffers (matching
the struct-of-arrays layout of `ForgeShape`).  Push uniforms per draw call:
MVP, model matrix, normal matrix.

Compile:

```bash
python scripts/compile_shaders.py 35
```

### Vertex attribute binding for struct-of-arrays

Bind two vertex buffers per draw — positions at slot 0, normals at slot 1.
The lesson README must explain why two bindings are used:

```c
/* Bind position buffer at slot 0 */
SDL_GPUBufferBinding pos_bind = { .buffer = shape_pos_buf, .offset = 0 };
/* Bind normal buffer at slot 1 */
SDL_GPUBufferBinding nor_bind = { .buffer = shape_nor_buf, .offset = 0 };
SDL_BindGPUVertexBuffers(rpass, 0, &pos_bind, 1);
SDL_BindGPUVertexBuffers(rpass, 1, &nor_bind, 1);
```

---

## Step 6 — Diagrams

Use `/dev-create-diagram` to generate four diagrams.  Place all output in
`lessons/gpu/35-procedural-geometry/assets/`.

| Diagram | Description |
|---|---|
| `uv_grid_to_sphere.png` | 2D (u,v) grid on left, arrows mapping samples to a 3D sphere on right; highlight the seam duplication column |
| `smooth_vs_flat.png` | Two icospheres side by side: one with per-vertex normals as arrows, one with per-face normals; label the difference |
| `sphere_vs_icosphere.png` | Top-down view: UV sphere (dense poles, sparse equator) vs icosphere (even distribution); annotate slice/stack lines |
| `torus_parameters.png` | Cross-section showing `major_radius` and `tube_radius`; label `phi` (around ring) and `theta` (around tube) |

---

## Step 7 — `common/shapes/README.md`

Full API reference.  Must include every function, a shape gallery table with
vertex/index counts, and the "struct-of-arrays layout" explanation.

---

## Step 8 — Root file updates

| File | Change |
|---|---|
| Root `CMakeLists.txt` | Add `add_subdirectory(lessons/gpu/35-procedural-geometry)` under GPU Lessons; add `test_shapes` under Tests |
| Root `README.md` | Add row to GPU Lessons table; add row to Common Libraries table for `common/shapes/` |
| `PLAN.md` | Mark Lesson 35 complete |
| `lessons/gpu/README.md` | Add row for Lesson 35 |

---

## Step 9 — Skill file

Create `.claude/skills/forge-procedural-geometry/SKILL.md` teaching how to
generate and render a shape.  Cover:

- `#define FORGE_SHAPES_IMPLEMENTATION` pattern (one `.c` file only)
- Generate a shape, upload to two SDL GPU buffers (positions, normals), bind,
  draw
- Combining multiple shapes in one scene (loop over shapes, bind their buffers,
  draw)
- Common mistakes: forgetting to free; uploading normals to wrong slot;
  incorrect vertex attribute offsets for struct-of-arrays
- Minimal code template: load a sphere and render it

---

## Step 10 — Build, run tests, capture screenshot

```bash
# Build all
cmake -B build
cmake --build build --config Debug

# Run tests
ctest --test-dir build -R shapes

# Run lesson
./build/lessons/gpu/35-procedural-geometry/35-procedural-geometry

# Screenshot
python scripts/capture_lesson.py lessons/gpu/35-procedural-geometry
```

Use Task agents with `model: "haiku"` for all build and test commands.

---

## Step 11 — Verify and lint

Launch a verification agent on:

- `common/shapes/forge_shapes.h`
- `lessons/gpu/35-procedural-geometry/README.md`
- `tests/test_shapes.c`

The agent must confirm:

1. Every public function has a comment that includes: shape description,
   origin, scale, UV convention, normal method, vertex/index count formula
2. Every "What you'll learn" bullet has a matching section in the README
3. Every section in the README has corresponding code in `main.c`
4. All 28 tests exist in `test_shapes.c` and are registered in the test runner

Then lint:

```bash
npx markdownlint-cli2 "**/*.md"
ruff check scripts/
```

Fix all errors before marking done.

---

## Acceptance criteria

The deliverable is complete when all of the following are true:

- [ ] `ctest --test-dir build -R shapes` — 28 tests, 0 failures
- [ ] `cmake --build build --target 35-procedural-geometry` — succeeds
- [ ] The lesson window shows all five shapes with correct lighting
- [ ] `npx markdownlint-cli2 "**/*.md"` — 0 errors
- [ ] Every public function in `forge_shapes.h` is covered by at least one test
- [ ] Seam duplication is explained in both the sphere comment and the README
- [ ] The struct-of-arrays layout choice is justified in the README
- [ ] Four matplotlib diagrams exist in `assets/`
- [ ] Screenshot exists in `lessons/gpu/35-procedural-geometry/assets/`

---

## Code conventions checklist

- [ ] All public names use `forge_shapes_` prefix
- [ ] `ForgeShape` uses `PascalCase`
- [ ] `SDL_malloc` / `SDL_free` — not `malloc` / `free`
- [ ] `SDL_assert` with a descriptive message guards `slices >= 3`,
  `stacks >= 1` (or `>= 2` for sphere stacks) on every generator
- [ ] `forge_shapes_free()` NULLs all pointers and zeroes all counts
- [ ] No SDL GPU calls in the library — only `SDL_malloc`, `SDL_free`,
  `SDL_assert`
- [ ] No `forge_math.h` functions called for trivial ops that can use
  plain float arithmetic — but DO use `vec3_normalize`, `vec3_cross`,
  `vec3_length` where appropriate
- [ ] SPDX-License-Identifier: Zlib on all new source files
- [ ] Unix line endings (LF) on all files
- [ ] ASCII-only console output in `test_shapes.c`
