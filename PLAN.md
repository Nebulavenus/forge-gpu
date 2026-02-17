# forge-gpu — Lesson Plan

## Foundation

Infrastructure that supports all lessons:

- [x] **Project scaffolding** — CMakeLists.txt, FetchContent for SDL3, common/ header, directory structure
- [x] **zlib LICENSE**
- [x] **Math library** — `common/math/forge_math.h` (vectors, matrices, documented, tested)
- [x] **Math lessons** — `lessons/math/` with standalone programs teaching concepts
- [x] **Test suite** — Automated tests for math library (CTest integration)
- [x] **Skills** — `/math-lesson`, `/new-lesson`, etc. for AI-assisted development

## GPU Lessons

- [x] **Lesson 01 — Hello Window** — SDL callbacks, GPU device creation, swapchain, clear screen via render pass
- [x] **Lesson 02 — First Triangle** — Vertex buffers, shaders (SPIRV/DXIL), graphics pipeline, sRGB swapchain
- [x] **Lesson 03 — Uniforms & Motion** — Uniform buffers, passing time to shaders, spinning triangle
- [x] **Lesson 04 — Textures & Samplers** — Loading images, texture sampling, UV coordinates
- [x] **Lesson 05 — Mipmaps** — Mip chain generation, trilinear filtering, why mipmaps fix aliasing; fragment derivatives (ddx/ddy) and how the GPU picks the mip level; helper invocations and 2x2 quad execution
- [x] **Lesson 06 — Depth Buffer & 3D Transforms** — MVP matrices, perspective projection, depth testing, window resize handling
- [x] **Lesson 07 — Camera & Input** — First-person camera with quaternion orientation (references Math Lesson 08 — Orientation and Math Lesson 09 — View Matrix); SDL event handling for keyboard and mouse input; delta time to decouple movement speed from frame rate; small scene with several objects to make navigation interesting
- [x] **Lesson 08 — Loading a Mesh (OBJ)** — Wavefront OBJ format; parsing vertices, normals, UVs, and faces; de-indexed rendering; quad triangulation; file-based texture loading with mipmaps; reusable OBJ parser library (`common/obj/forge_obj.h`)
- [x] **Lesson 09 — Loading a Scene (glTF)** — glTF 2.0 format; JSON scene description + binary buffers; node hierarchy, meshes, accessors, and buffer views; PBR materials; multi-material indexed rendering; reusable glTF parser library (`common/gltf/forge_gltf.h`)
- [x] **Lesson 10 — Basic Lighting** — Blinn-Phong shading (ambient + diffuse + specular), world-space normal transformation, half-vector specular, lighting uniforms (light direction, camera position, shininess)
- [x] **Lesson 11 — Compute Shaders** — Compute pipelines, storage textures, dispatch groups, fullscreen triangle, compute-then-render pattern
- [x] **Lesson 12 — Shader Grid** — Procedural grid rendering with fwidth()/smoothstep() anti-aliasing, distance fade, multiple pipelines in one render pass, combining procedural geometry with 3D models
- [ ] **Lesson 13 — Instanced Rendering** — Drawing many objects efficiently
- [ ] **Lesson 14 — HDR & Tone Mapping** — Gamma/sRGB, render-to-texture, floating-point framebuffers, tone mapping operators (depends on compute for post-processing)
- [ ] **Lesson ?? — Point Lights & Shadows** — Multiple point light sources, shadow mapping
- [ ] **Lesson ?? — Cascaded Shadow Maps (CSM)** — View-dependent shadow partitioning for large outdoor scenes
- [ ] **Lesson ?? — Height Map Terrain** — GPU terrain from height map, LOD, normal computation

## Math Lessons

- [x] **Math Lesson 01 — Vectors** — vec2/vec3/vec4, dot product, cross product, normalization, lerp
- [x] **Math Lesson 02 — Coordinate Spaces** — Model, world, view, projection transforms
- [x] **Math Lesson 03 — Bilinear Interpolation** — How LINEAR texture filtering works: two nested lerps blending the 4 nearest texels; comparison with NEAREST; builds on lerp from Math Lesson 01
- [x] **Math Lesson 04 — Mipmaps & LOD** — Mip chain as power-of-two downsampling, log2 for level selection, how screen-space derivatives map to mip levels; connects bilinear interpolation (Math Lesson 03) to trilinear filtering
- [x] **Math Lesson 05 — Matrices** — mat4x4, transformations, rotations, composition
- [x] **Math Lesson 06 — Projections** — Perspective, orthographic, frustums, clip space, NDC, perspective-correct interpolation
- [x] **Math Lesson 07 — Floating Point** — Brief intro to fixed-point as motivation; IEEE 754 floating-point representation (sign, exponent, mantissa); how precision varies across the number line; epsilon and testing for equality (absolute vs relative tolerance); precision pitfalls in graphics: z-fighting and why depth buffer precision is non-linear; 32-bit vs 64-bit (float vs double) trade-offs — when each is appropriate and why GPUs favor 32-bit
- [x] **Math Lesson 08 — Orientation** — *(Larger lesson, like Projections.)* Four representations of 3D rotation and how to convert between them. **Euler angles:** pitch, yaw, and roll diagram; rotation order conventions; gimbal lock explained and visualized. **Rotation matrices:** how each basis-axis rotation matrix (Rx, Ry, Rz) is constructed; rotation around an arbitrary axis (Rodrigues' formula). **Axis-angle:** compact representation (axis + angle); why it is commonly used as an input/interface format even when quaternions are the storage format. **Quaternions:** the imaginary-number basis (i, j, k) and their properties; representation as (w, x, y, z); identity quaternion; multiplication and how it composes rotations; conjugate and inverse; rotating a vector by a quaternion; unit quaternion constraint; slerp for smooth interpolation. Conversions: Euler↔matrix, axis-angle↔quaternion, quaternion↔matrix, and the full round-trip
- [x] **Math Lesson 09 — View Matrix & Virtual Camera** — Building a view matrix from scratch; the camera as an inverse transform (world-to-view); constructing the view matrix from position + quaternion orientation; extracting forward/right/up vectors from a quaternion; look-at as a special case; how the view matrix feeds into the MVP pipeline (connects to Math Lesson 02 — Coordinate Spaces and Math Lesson 06 — Projections)
- [x] **Math Lesson 10 — Anisotropy vs Isotropy** — Direction-dependent vs direction-independent behavior; isotropic sampling (equal in all directions) vs anisotropic filtering (stretches samples along the axis of greatest compression); anisotropic noise (Perlin/simplex stretched along a direction, e.g. wood grain, brushed metal); anisotropic friction in rigid body physics (ice rink, grooved surfaces, tire grip); eigenvalues of the screen-space Jacobian and how they drive the GPU's anisotropic sampler; practical demo comparing isotropic vs anisotropic texture filtering on a tilted plane

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
