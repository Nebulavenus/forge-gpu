# forge-gpu

An educational project teaching real-time graphics with SDL's GPU API, written in C.

Repository: https://github.com/RosyGameStudio/forge-gpu
License: zlib (matching SDL)

## Philosophy
- Every line of code should be understandable by someone learning graphics programming
- Extensive comments explaining *why*, not just *what*
- Each lesson builds on the previous, introducing one concept at a time
- C99, matching SDL's own style conventions

## Code Style
- SDL naming conventions (SDL_PrefixedNames for public, lowercase_snake for local)
- No magic numbers — #define or enum everything
- Every shader uniform, every pipeline state: comment explaining its purpose
- Error handling on every SDL GPU call with descriptive messages

## Structure
- Each lesson in lessons/NN-name/ is a standalone buildable project
- Lessons share common utility code via a small header-only lib in common/
- Skills in skills/ are Claude Code skill files (.md) that teach AI agents
  to work with SDL GPU effectively

## When writing lessons
- Start each README.md with what the reader will learn
- Show the result (screenshot) before diving into code
- Introduce API calls one at a time with context
- End with exercises that extend the lesson

## Dependencies
- SDL3 (with GPU API)
- CMake 3.24+
- A Vulkan/Metal/D3D12-capable GPU
