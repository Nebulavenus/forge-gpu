# forge-gpu — Lesson Plan

## Completed

- [x] **Project scaffolding** — CMakeLists.txt, FetchContent for SDL3, common/ header, directory structure
- [x] **zlib LICENSE**
- [x] **Lesson 01 — Hello Window** — SDL callbacks, GPU device creation, swapchain, clear screen via render pass

## Up Next

- [ ] **Lesson 02 — First Triangle** — Vertex buffers, shaders (SPIRV/DXIL/MSL), graphics pipeline, the classic colored triangle
- [ ] **Lesson 03 — Uniforms & Motion** — Uniform buffers, passing time to shaders, spinning triangle

## Future Ideas

- [ ] Textures & samplers
- [ ] Depth buffer & 3D transforms (MVP matrices)
- [ ] Loading a mesh (OBJ or glTF)
- [ ] Basic lighting (diffuse + specular)
- [ ] Render-to-texture / offscreen passes
- [ ] Compute shaders
- [ ] Instanced rendering

## Open Questions

- Shader workflow: ship pre-compiled SPIRV, or integrate SDL_shadercross at build time?
