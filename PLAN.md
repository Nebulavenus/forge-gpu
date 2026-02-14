# forge-gpu — Lesson Plan

## Foundation

Infrastructure that supports all lessons:

- [x] **Project scaffolding** — CMakeLists.txt, FetchContent for SDL3, common/ header, directory structure
- [x] **zlib LICENSE**
- [x] **Math library** — `common/math/forge_math.h` (vectors, matrices, documented, tested)
- [x] **Math lessons** — `lessons/math/` with standalone programs teaching concepts
- [x] **Test suite** — Automated tests for math library (26 tests, CTest integration)
- [x] **Skills** — `/math-lesson`, `/new-lesson`, etc. for AI-assisted development

## GPU Lessons — Completed

- [x] **Lesson 01 — Hello Window** — SDL callbacks, GPU device creation, swapchain, clear screen via render pass
- [x] **Lesson 02 — First Triangle** — Vertex buffers, shaders (SPIRV/DXIL), graphics pipeline, sRGB swapchain
- [x] **Lesson 03 — Uniforms & Motion** — Uniform buffers, passing time to shaders, spinning triangle

## Math Lessons — Completed

- [x] **Math Lesson 01 — Vectors** — vec2/vec3/vec4, dot product, cross product, normalization, lerp

## GPU Lessons — Up Next

- [ ] **Lesson 04 — Textures & Samplers** — Loading images, texture sampling, UV coordinates
- [ ] **Lesson 05 — Depth Buffer & 3D Transforms** — MVP matrices, perspective projection, depth testing, window resize handling
- [ ] **Lesson 06 — Loading a Mesh** — OBJ or glTF parsing, indexed rendering
- [ ] **Lesson 07 — Basic Lighting** — Diffuse + specular, normal vectors, Phong model
- [ ] **Lesson 08 — Gamma Correction & sRGB** — Linear color space, why sRGB exists, how `SDR_LINEAR` works
- [ ] **Lesson 09 — Render-to-Texture** — Offscreen passes, framebuffers, post-processing
- [ ] **Lesson 10 — Compute Shaders** — General-purpose GPU computation
- [ ] **Lesson 11 — Instanced Rendering** — Drawing many objects efficiently

## Math Lessons — Up Next

- [ ] **Math Lesson 02 — Matrices** — mat4x4, transformations, rotations, composition
- [ ] **Math Lesson 03 — Coordinate Spaces** — Model, world, view, projection transforms
- [ ] **Math Lesson 04 — Quaternions** — Rotation representation, slerp, avoiding gimbal lock
- [ ] **Math Lesson 05 — Projections** — Perspective, orthographic, NDC mapping

## Developer Experience / Tooling

Improvements to make development easier:

- [ ] **Run script/utility** — Easy way to run lessons by name/number instead of typing full build paths
  - Example: `./run 02` or `./run first-triangle` instead of `build\lessons\gpu\02-first-triangle\Debug\02-first-triangle.exe`
  - Could be a Python script, shell script, or `/run` skill

- [ ] **Shader compilation helper** — Simplify shader recompilation workflow
  - Currently requires full path to Vulkan SDK's dxc
  - Could auto-detect dxc location or use configured path
  - Maybe a `/compile-shaders` skill or `scripts/compile_shaders.py`

- [ ] **Setup script/skill** — Help users configure their environment
  - Set up Vulkan SDK path
  - Configure Python location
  - Verify dependencies
  - Could be a `/setup` skill or `scripts/setup.py`

- [ ] **Screenshot generation** — Programmatically capture lesson output
  - See GitHub issue for automated screenshot generation
  - Helps keep README screenshots up-to-date
  - Could run after each lesson builds

## Open Questions

- ~~Shader workflow~~ **Resolved:** ship HLSL source + pre-compiled SPIRV & DXIL headers; compile with `dxc` from Vulkan SDK
