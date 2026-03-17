# Claude Code Skills

Skills are Claude Code commands that automate common workflows in forge-gpu.
Users invoke them with `/skill-name` in chat; Claude can also invoke them
automatically when they match the current task.

Each skill directory contains a `SKILL.md` file that defines the skill's
purpose, inputs, and step-by-step instructions.

## Development skills (`dev-*`)

| Skill                    | Purpose                                        |
|--------------------------|-------------------------------------------------|
| `dev-gpu-lesson`         | GPU lesson scaffolding with forge_scene.h      |
| `dev-math-lesson`        | Create a math lesson + update math library      |
| `dev-engine-lesson`      | Scaffold an engine lesson                      |
| `dev-ui-lesson`          | Scaffold a UI lesson                           |
| `dev-physics-lesson`     | Scaffold a physics lesson                      |
| `dev-asset-lesson`       | Scaffold an asset pipeline lesson              |
| `dev-create-pr`          | Create a pull request                          |
| `dev-add-screenshot`     | Capture and embed screenshots/GIFs             |
| `dev-create-diagram`     | Generate matplotlib diagrams for lessons       |
| `dev-review-diagrams`    | Review diagram quality and accuracy            |
| `dev-docs-review`        | Review documentation for completeness          |
| `dev-review-pr`          | Review a pull request                          |
| `dev-final-pass`         | Final quality pass before merging              |
| `dev-markdown-lint`      | Run markdownlint checks                        |
| `dev-audio-lesson`       | Scaffold an audio lesson                       |
| `dev-audio-review`       | Review audio library changes                   |
| `dev-physics-review`     | Review physics library changes                 |
| `dev-local-review`       | Run local code review before pushing           |
| `dev-ui-review`          | Review UI library changes                      |
| `dev-reset-workspace`    | Reset workspace to a clean state               |

## Topic skills (`forge-*`)

Topic skills teach Claude about specific lesson implementations so it can
answer questions and help debug code related to each lesson.

| Skill | Lesson |
|-------|--------|
| `forge-sdl-gpu-setup` | SDL3 GPU application scaffold |
| `forge-first-triangle` | Vertex buffers, HLSL shaders, first draw call |
| `forge-uniforms-and-motion` | Push uniforms, per-frame shader data |
| `forge-textures-and-samplers` | GPU textures, samplers, textured geometry |
| `forge-mipmaps` | Mip chains, trilinear filtering, LOD control |
| `forge-depth-and-3d` | Depth buffer, MVP pipeline, 3D rendering |
| `forge-camera-and-input` | First-person fly camera, keyboard/mouse, delta time |
| `forge-mesh-loading` | Load OBJ models, textured mesh rendering |
| `forge-scene-loading` | Load glTF scenes, multi-material rendering |
| `forge-basic-lighting` | Blinn-Phong ambient, diffuse, specular lighting |
| `forge-compute-shaders` | Compute pipelines, storage textures, dispatch groups |
| `forge-shader-grid` | Procedural anti-aliased grid, screen-space derivatives |
| `forge-instanced-rendering` | Per-instance vertex buffers, instanced draw calls |
| `forge-environment-mapping` | Cube map skybox and reflective surfaces |
| `forge-cascaded-shadow-maps` | Cascaded shadow maps with PCF soft shadows |
| `forge-blending` | Alpha blending, alpha testing, additive blending |
| `forge-normal-maps` | Tangent-space normal mapping, TBN matrix |
| `forge-blinn-phong-materials` | Per-material Blinn-Phong lighting |
| `forge-debug-lines` | Immediate-mode debug line drawing |
| `forge-linear-fog` | Depth-based distance fog (linear, exponential) |
| `forge-hdr-tone-mapping` | HDR rendering, Reinhard/ACES tone mapping |
| `forge-bloom` | Jimenez dual-filter bloom with Karis averaging |
| `forge-point-light-shadows` | Omnidirectional point light shadows |
| `forge-gobo-spotlight` | Projected-texture gobo spotlight |
| `forge-shader-noise` | GPU noise: hash, value, Perlin, fBm, domain warp |
| `forge-procedural-sky` | Physically-based atmospheric scattering |
| `forge-ssao` | Screen-space ambient occlusion |
| `forge-ui-rendering` | Immediate-mode UI on GPU with font atlas |
| `forge-screen-space-reflections` | SSR with depth-buffer ray marching |
| `forge-planar-reflections` | Mirror camera, oblique near-plane clipping |
| `forge-transform-animations` | Keyframe animation from glTF with slerp |
| `forge-skinning-animations` | Skeletal skinning with joint hierarchies |
| `forge-vertex-pulling` | Programmable vertex fetch via storage buffers |
| `forge-stencil-testing` | Stencil buffer for portals, outlines, masking |
| `forge-decals` | Deferred decal projection with depth reconstruction |
| `forge-edge-detection` | Sobel edge detection, Fresnel silhouettes |
| `forge-3d-picking` | GPU object selection with color-ID and stencil-ID |
| `forge-indirect-drawing` | GPU-driven indirect draws with compute culling |
| `forge-pipeline-assets` | Load `.fmesh` and pipeline-processed textures |
| `forge-scene-renderer` | Complete scene with `forge_scene.h` |
| `forge-scene-model-loading` | Pipeline-processed models with scene hierarchy |
| `forge-pipeline-texture-compression` | BC7/BC5 block-compressed textures |
| `forge-pipeline-skinned-animations` | Pipeline skeletal animation with `.fskin`/`.fanim` |
| `forge-pipeline-morph-animations` | Morph target animation with `.fmesh` deltas |
| `forge-asset-pipeline` | Python asset pipeline library |
| `forge-pipeline-library` | `forge_pipeline.h` C runtime loader |
| `forge-procedural-geometry` | `forge_shapes.h` procedural geometry |
| `forge-audio-pool` | Audio buffer pool and source management |
| `forge-auto-widget-layout` | Automatic UI widget layout |
| `forge-draggable-windows` | Draggable UI windows |
