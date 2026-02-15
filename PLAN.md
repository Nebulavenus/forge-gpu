# forge-gpu — Lesson Plan

## Foundation

Infrastructure that supports all lessons:

- [x] **Project scaffolding** — CMakeLists.txt, FetchContent for SDL3, common/ header, directory structure
- [x] **zlib LICENSE**
- [x] **Math library** — `common/math/forge_math.h` (vectors, matrices, documented, tested)
- [x] **Math lessons** — `lessons/math/` with standalone programs teaching concepts
- [x] **Test suite** — Automated tests for math library (CTest integration)
- [x] **Skills** — `/math-lesson`, `/new-lesson`, etc. for AI-assisted development

## GPU Lessons — Completed

- [x] **Lesson 01 — Hello Window** — SDL callbacks, GPU device creation, swapchain, clear screen via render pass
- [x] **Lesson 02 — First Triangle** — Vertex buffers, shaders (SPIRV/DXIL), graphics pipeline, sRGB swapchain
- [x] **Lesson 03 — Uniforms & Motion** — Uniform buffers, passing time to shaders, spinning triangle

## Math Lessons — Completed

- [x] **Math Lesson 01 — Vectors** — vec2/vec3/vec4, dot product, cross product, normalization, lerp
- [x] **Math Lesson 02 — Coordinate Spaces** — Model, world, view, projection transforms
- [x] **Math Lesson 03 — Orthographic Projection** — mat4_orthographic, 2D rendering, perspective comparison

## GPU Lessons — Up Next

- [x] **Lesson 04 — Textures & Samplers** — Loading images, texture sampling, UV coordinates
- [ ] **Lesson 05 — Mipmaps** — Mip chain generation, trilinear filtering, why mipmaps fix aliasing; fragment derivatives (ddx/ddy) and how the GPU picks the mip level; helper invocations and 2x2 quad execution
- [ ] **Lesson 06 — Depth Buffer & 3D Transforms** — MVP matrices, perspective projection, depth testing, window resize handling
- [ ] **Lesson 07 — Loading a Mesh** — OBJ or glTF parsing, indexed rendering
- [ ] **Lesson 08 — Basic Lighting** — Diffuse + specular, normal vectors, Phong model
- [ ] **Lesson 09 — Gamma Correction & sRGB** — Linear color space, why sRGB exists, how `SDR_LINEAR` works
- [ ] **Lesson 10 — Render-to-Texture** — Offscreen passes, framebuffers, post-processing
- [ ] **Lesson 11 — Compute Shaders** — General-purpose GPU computation
- [ ] **Lesson 12 — Instanced Rendering** — Drawing many objects efficiently

## Math Lessons — Up Next

- [x] **Math Lesson 04 — Bilinear Interpolation** — How LINEAR texture filtering works: two nested lerps blending the 4 nearest texels; comparison with NEAREST; builds on lerp from Math Lesson 01
- [ ] **Math Lesson 05 — Mipmaps & LOD** — Mip chain as power-of-two downsampling, log2 for level selection, how screen-space derivatives map to mip levels; connects bilinear interpolation (Math Lesson 04) to trilinear filtering
- [ ] **Math Lesson 06 — Matrices** — mat4x4, transformations, rotations, composition
- [ ] **Math Lesson 07 — Quaternions** — Rotation representation, slerp, avoiding gimbal lock
- [ ] **Math Lesson 08 — Projections** — Perspective, orthographic, NDC mapping

## Developer Experience / Tooling

Improvements to make development easier:

- [x] **Run script/utility** — `python scripts/run.py <name-or-number>` runs lessons easily
  - Example: `python scripts/run.py 02` or `python scripts/run.py first-triangle`
  - Supports `math/01` prefix, substring matching, lists all lessons when run with no args

- [x] **Shader compilation helper** — `python scripts/compile_shaders.py` compiles all HLSL shaders
  - Auto-detects dxc from VULKAN_SDK or PATH
  - Compiles to SPIRV and DXIL, generates C byte-array headers
  - Can target a specific lesson: `python scripts/compile_shaders.py 02`

- [x] **Setup script** — `python scripts/setup.py` verifies the full environment
  - Checks git, CMake, C compiler (MSVC/GCC/Clang), Python, Pillow
  - Detects Vulkan SDK, dxc shader compiler, GPU via vulkaninfo
  - `--fix` installs missing Python packages, `--build` configures + builds

- [x] **Screenshot generation** — `scripts/capture_lesson.py` + `common/capture/forge_capture.h`
  - Programmatic capture via `--screenshot` / `--capture-dir` CLI flags
  - BMP→PNG/GIF conversion, README auto-update

## Open Questions

- ~~Shader workflow~~ **Resolved:** ship HLSL source + pre-compiled SPIRV & DXIL headers; compile with `dxc` from Vulkan SDK
