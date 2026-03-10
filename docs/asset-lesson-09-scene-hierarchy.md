# Asset Lesson 09 — Scene Hierarchy

## Rationale

The asset pipeline's mesh tool flattens all glTF primitives into a single
`.fmesh` file with no record of which node each primitive belongs to. This
means the application has no way to position parts of a multi-node model
correctly — everything renders at the origin.

This surfaced during GPU Lesson 40 (Scene Renderer), where an agent failed to
render the CesiumMilkTruck. The truck has separate nodes for the body and two
wheels, each with their own transforms. When the mesh tool discarded the node
tree, all parts collapsed to the same position. The agent then tried to fix it
by baking world transforms directly into vertex positions in the mesh tool —
destroying instancing (two wheel nodes share one mesh) and making animation
impossible (animation channels target node transforms, not vertices). When
that was pointed out, the agent wanted to flatten the hierarchy entirely.

These are the wrong solutions. An asset pipeline needs to describe the scene.
The node hierarchy is composable: nodes can reference the same mesh, apply
different transforms, and have animations applied at the transform level. All
of this must be available to the application at runtime.

The asset pipeline track plans a scene editor (Lesson 12). A scene editor is
impossible without scene information in the pipeline output. This lesson
builds the foundation: a production-ready scene extraction tool that
preserves the full glTF node tree for rendering, instancing, and animation.

The tool is written in C (like the mesh and animation tools) because the glTF
parser and all extraction logic are already in C. The Python pipeline invokes
it as a subprocess, consistent with the existing architecture.

## Plan

### New binary format: `.fscene`

A binary file describing the glTF node hierarchy. Three sections after the
header:

1. **Root indices** — which nodes are scene roots (a glTF scene can have
   multiple root nodes)
2. **Mesh table** — maps each glTF mesh index to a range of submeshes in the
   `.fmesh` file (`first_submesh`, `submesh_count`), so the renderer knows
   which geometry a node's `mesh_index` refers to
3. **Node table** — fixed-size entries with name, parent index, mesh index,
   skin index, TRS decomposition, and local transform matrix
4. **Children array** — flat array of child node indices, referenced by
   `first_child` and `child_count` in each node entry

World transforms are not stored in the binary — they are computed at load
time by walking the hierarchy (parent's world × child's local). This keeps
the file minimal and means the loader always produces correct world transforms
even if the source tool changes.

Format details:

```text
Header (24 bytes):
  magic          "FSCN"
  version        u32 (1)
  node_count     u32
  mesh_count     u32
  root_count     u32
  reserved       u32

Root indices:    root_count × u32
Mesh table:      mesh_count × 8 bytes (first_submesh u32, submesh_count u32)
Node table:      node_count × 192 bytes (fixed-size entries)
Children array:  total_children × u32
```

Node entry (192 bytes):

```text
name               64 bytes (null-terminated, zero-padded)
parent             i32 (-1 = root)
mesh_index         i32 (-1 = no mesh)
skin_index         i32 (-1 = no skin)
first_child        u32 (index into children array)
child_count        u32
has_trs            u32 (1 = TRS valid, 0 = raw matrix)
translation        float[3]
rotation           float[4] (x, y, z, w — glTF quaternion order)
scale              float[3]
local_transform    float[16] (column-major 4×4)
```

### Deliverables

1. **C tool** (`tools/scene/main.c`) — parses glTF via `forge_gltf_load()`,
   writes `.fscene` binary and `.meta.json` sidecar. Follows the same
   patterns as `tools/anim/main.c`: argument parsing, binary helpers,
   SDL I/O, metadata sidecar.

2. **Python plugin** (`pipeline/plugins/scene.py`) — subprocess wrapper
   matching the animation plugin pattern. Registers for `.gltf`/`.glb`
   extensions, coexists with mesh and animation plugins on the same
   file types.

3. **Runtime loader** (`common/pipeline/forge_pipeline.h`) — adds
   `ForgePipelineScene`, `ForgePipelineSceneNode`, `ForgePipelineSceneMesh`
   types and `forge_pipeline_load_scene()`/`forge_pipeline_free_scene()`
   functions. Computes world transforms at load time by walking the tree.
   No dependency on `forge_math.h` — matrices stored as `float[16]`,
   quaternions as `float[4]`.

4. **Python tests** (`tests/pipeline/test_scene.py`) — 16 tests covering
   plugin registration, tool invocation, error handling, metadata, and
   multi-plugin coexistence.

5. **Build integration** — `tools/scene/CMakeLists.txt` added to root
   CMakeLists under the Asset Pipeline Tools section. Includes `POST_BUILD`
   copy of `SDL3.dll` for Windows.

6. **Lesson README** (`lessons/assets/09-scene-hierarchy/README.md`)

### Usage at render time

```c
ForgePipelineScene scene;
ForgePipelineMesh  mesh;
ForgePipelineMaterialSet materials;

forge_pipeline_load_scene("model.fscene", &scene);
forge_pipeline_load_mesh("model.fmesh", &mesh);
forge_pipeline_load_materials("model.fmat", &materials);

for (uint32_t i = 0; i < scene.node_count; i++) {
    ForgePipelineSceneNode *node = &scene.nodes[i];
    if (node->mesh_index < 0) continue;  /* transform-only node */

    /* Look up which submeshes this mesh uses */
    const ForgePipelineSceneMesh *sm =
        forge_pipeline_scene_mesh_submeshes(&scene, node->mesh_index);

    /* Draw each submesh with this node's world transform */
    for (uint32_t s = 0; s < sm->submesh_count; s++) {
        uint32_t submesh_idx = sm->first_submesh + s;
        const ForgePipelineSubmesh *sub =
            forge_pipeline_lod_submesh(&mesh, 0, submesh_idx);

        /* Push node->world_transform as the model matrix */
        /* Draw sub->index_count indices at sub->index_offset */
    }
}
```

### CesiumMilkTruck verification

The tool produces this hierarchy, which matches the glTF source:

```text
[5] "Yup2Zup"            root, transform-only
 └─[4] "Cesium_Milk_Truck"  mesh 1 (body, 3 submeshes)
    ├─[1] "Node"             transform-only, T=(1.43, 0, -0.43)
    │  └─[0] "Wheels"        mesh 0 (wheel, 1 submesh)
    └─[3] "Node.001"         transform-only, T=(-1.35, 0, -0.43)
       └─[2] "Wheels.001"    mesh 0 (same wheel mesh, different position)
```

Mesh 0 is shared by nodes 0 and 2 — instancing preserved. The wheel
transforms position them correctly relative to the truck body. Animation
channels from `.fanim` target these nodes by index to spin the wheels.

### Open questions

- **Naming**: `ForgePipelineSceneMesh` maps glTF meshes to `.fmesh` submesh
  ranges. The name could be clearer — `SubmeshRange`, `MeshMapping`, or
  similar. "SceneMesh" is ambiguous alongside `ForgePipelineMesh`.

- **ForgeGltfScene stack size**: The glTF parser struct uses fixed-size arrays
  (512 nodes × 256 children, 1024 primitives, etc.) making it too large for
  the stack. The scene tool heap-allocates it, but this affects every tool and
  lesson. An arena allocator or switching to heap-allocated dynamic arrays
  would fix it project-wide.

- **PLAN.md numbering**: The current plan has Lesson 09 as "Web UI Scaffold."
  This lesson takes that slot. The web frontend lessons (09–12) shift to
  10–13, or the plan is renumbered.
