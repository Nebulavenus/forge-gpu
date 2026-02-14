# forge-gpu — Lesson Plan

## Completed

- [x] **Project scaffolding** — CMakeLists.txt, FetchContent for SDL3, common/ header, directory structure
- [x] **zlib LICENSE**
- [x] **Lesson 01 — Hello Window** — SDL callbacks, GPU device creation, swapchain, clear screen via render pass
- [x] **Lesson 02 — First Triangle** — Vertex buffers, shaders (SPIRV/DXIL), graphics pipeline, sRGB swapchain

## Up Next

- [ ] **Lesson 03 — Uniforms & Motion** — Uniform buffers, passing time to shaders, spinning triangle

## Future Ideas

- [ ] Textures & samplers
- [ ] Depth buffer & 3D transforms (MVP matrices)
- [ ] Loading a mesh (OBJ or glTF)
- [ ] Basic lighting (diffuse + specular)
- [ ] Gamma correction & sRGB — what linear colour space means, why sRGB exists, how `SDR_LINEAR` works under the hood.  Note: explain the naming — `SDR_LINEAR` refers to the shader's working space (linear), not the framebuffer format (which is sRGB).  `SDR` vs `SDR_LINEAR` names the input contract, not the output encoding.
- [ ] Render-to-texture / offscreen passes
- [ ] Compute shaders
- [ ] Instanced rendering

## Open Questions

- ~~Shader workflow~~ **Resolved:** ship HLSL source + pre-compiled SPIRV & DXIL headers; compile with `dxc` from Vulkan SDK
