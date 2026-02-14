# forge-gpu

An educational project teaching real-time graphics with SDL's GPU API, written in C.

Repository: https://github.com/RosyGameStudio/forge-gpu
License: zlib (matching SDL)

## Philosophy
- Every line of code should be understandable by someone learning graphics programming
- Extensive comments explaining *why*, not just *what*
- Each lesson builds on the previous, introducing one concept at a time
- C99, matching SDL's own style conventions
- **Dual audience: humans and AI** — every lesson teaches a person, and every
  lesson produces a reusable *skill* that teaches an AI agent the same pattern.
  The goal is to enable people to use Claude (or any AI) to build games and
  renderers with SDL GPU confidently.

## Code Style
- SDL naming conventions (SDL_PrefixedNames for public, lowercase_snake for local)
- No magic numbers — #define or enum everything
- Every shader uniform, every pipeline state: comment explaining its purpose
- Error handling on every SDL GPU call with descriptive messages

## Structure
- Each lesson in lessons/NN-name/ is a standalone buildable project
- Lessons share common utility code via a small header-only lib in common/
- Skills in .claude/skills/ are Claude Code skills that teach AI agents the
  patterns from each lesson — invocable with /skill-name or automatically
  when Claude determines they're relevant

## When writing lessons
- Start each README.md with what the reader will learn
- Show the result (screenshot) before diving into code
- Introduce API calls one at a time with context
- End with exercises that extend the lesson
- **Also write a matching skill** in .claude/skills/<name>/SKILL.md that
  distills the lesson into a reusable Claude Code skill: YAML frontmatter
  with name and description, then the key API calls, correct order, common
  mistakes, and a ready-to-use code template

## Dependencies
- SDL3 (with GPU API)
- CMake 3.24+
- A Vulkan/Metal/D3D12-capable GPU
