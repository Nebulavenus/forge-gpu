# forge-gpu

A learning platform and building tool for real-time graphics with SDL's GPU API.

**Dual purpose:**

1. **Learn** — Guided lessons teaching GPU programming, math, and game techniques
2. **Forge** — Skills and libraries enabling humans + AI to build real projects

Repository: https://github.com/RosyGameStudio/forge-gpu
License: zlib (matching SDL)

## Tone

Many users are learning graphics or game programming for the first time. When
answering questions or writing code:

- Be encouraging and patient — there are no dumb questions about GPU APIs or math
- Explain *why*, not just *what* — connect each concept to the bigger picture
- Use plain language before jargon; when jargon is unavoidable, define it
- Suggest alternatives gently rather than just correcting
- Reference specific lessons when relevant
- When using the math library, link to both the library docs AND the math lesson

### Lesson writing tone

Lessons teach real techniques backed by math and engineering — treat every
concept with the respect it deserves.

- **Banned words:** *trick*, *hack*, *magic*, *clever*, *neat* — these cheapen
  the material. Use instead: *technique*, *method*, *approach*, *insight*,
  *key idea*, *shortcut*, *property*, *observation*.
- **Be direct and precise.** State what a technique does and why it works.
  Avoid hedging phrases ("it turns out that", "there's a neat way to").
- **Credit named techniques** (Blinn-Phong, Gram-Schmidt) — they carry weight
  and help readers find more resources.
- **Explain reasoning, not magic.** "We use the transpose because rotation
  matrices are orthonormal" beats "there's a neat trick with the transpose."

## Code conventions

- **Language:** C99, matching SDL's own style conventions
- **Naming:** `Prefix_PascalCase` for public API (e.g. `forge_capture_init`,
  `ForgeCapture`), `lowercase_snake` for local/internal. The prefix is the
  module name (`forge_capture_`, `forge_math_`, etc.) — NOT `SDL_`, which is
  reserved for SDL's own symbols
- **Constants:** No magic numbers — #define or enum everything
- **Comments:** Explain *why* and *purpose* (every uniform, pipeline state, etc.)
- **Errors:** Handle every SDL GPU call with descriptive messages. **Every SDL
  function that returns `bool` must be checked** — log the function name and
  `SDL_GetError()` on failure, then clean up resources and early-return. This
  includes `SDL_SubmitGPUCommandBuffer`, `SDL_SetGPUSwapchainParameters`,
  `SDL_Init`, and others. Never ignore a bool return value.
- **Line endings:** Always use Unix-style (LF) line endings — never CRLF.
  The repository enforces this via `.gitattributes`.
- **Readability:** This code is meant to be learned from — clarity over cleverness
- **glTF assets:** Load entire models via `forge_gltf_load()` — never extract
  individual textures or meshes à la carte. Copy the complete model (`.gltf`,
  `.bin`, textures) into the lesson's `assets/` directory.
- **Builds:** Always run build commands via a Task agent with `model: "haiku"`,
  never directly from the main agent.

## Git workflow

- **Never commit directly to `main`.** All changes go through pull requests.
- Create a feature branch, commit there, push, and open a PR with `gh pr create`.
- Use the **/publish-lesson** skill for lesson PRs — it handles branch + PR creation.

## Project Structure

```text
forge-gpu/
├── lessons/
│   ├── math/              # Math fundamentals (vectors, matrices, etc.)
│   ├── engine/            # Engine fundamentals (CMake, C, debugging)
│   ├── ui/                # UI fundamentals (fonts, text, atlas, controls)
│   └── gpu/               # SDL GPU lessons (rendering, pipelines, etc.)
├── common/
│   ├── math/              # Math library (header-only, documented)
│   ├── obj/               # OBJ parser (Wavefront .obj files)
│   ├── gltf/              # glTF 2.0 parser (scenes, materials, hierarchy)
│   ├── ui/                # UI library (TTF parsing, rasterization, atlas)
│   ├── raster/            # CPU triangle rasterizer (edge function method)
│   ├── capture/           # Screenshot/GIF capture utility
│   └── forge.h            # Shared utilities for lessons
├── tests/                 # Tests per module (math, obj, gltf, raster)
├── .claude/skills/        # Claude Code skills (AI-invokable patterns)
└── third_party/           # Dependencies (SDL3, etc.)
```

## Testing

```bash
cmake --build build --target test_gltf   # build one test
ctest --test-dir build -R gltf           # run one test
ctest --test-dir build                   # run all tests
```

When modifying parsers or libraries in `common/`, always build and run the
corresponding tests. When adding a new module, add a matching test under
`tests/` and register it in the root `CMakeLists.txt`.

## Shader compilation

Shaders are written in HLSL and compiled to SPIRV + DXIL using
`scripts/compile_shaders.py`:

```bash
python scripts/compile_shaders.py              # all lessons
python scripts/compile_shaders.py 16            # lesson 16 only
python scripts/compile_shaders.py blending      # by name fragment
```

Generated files (`.spv`, `.dxil`, C headers) go in `shaders/compiled/`.
Headers are included as `"shaders/compiled/scene_vert_spirv.h"`. Recompile
after any HLSL change — the C build does not auto-detect shader changes.

## Writing lessons

### GPU lessons (lessons/gpu/)

- Start README with what the reader will learn, show result before code
- Introduce API calls one at a time with context
- **Use the math library** — no bespoke math in GPU code
- Link to relevant math lessons for theory
- **Use diagrams** — use **/create-diagram** to generate matplotlib visuals
  for geometric, mathematical, or data-flow concepts. Never use ASCII art
  diagrams in lesson READMEs; proper diagrams are more readable and match the
  project's visual identity.
- End with exercises that extend the lesson
- Write a matching skill in `.claude/skills/<name>/SKILL.md`

### Engine lessons (lessons/engine/)

- Use **/engine-lesson** skill to scaffold
- Focus on practical engineering: build systems, C, debugging, project structure
- Show common errors and how to diagnose them
- Cross-reference GPU and math lessons where relevant

### Math lessons (lessons/math/)

- Use **/math-lesson** skill to scaffold + update library
- Small, focused program demonstrating one concept
- Add implementation to `common/math/` with extensive inline docs
- Cross-reference GPU lessons that use this math

### UI lessons (lessons/ui/)

- Use **/ui-lesson** skill to scaffold
- Build an immediate-mode UI system — fonts, text, layout, controls
- **No GPU code** — lessons produce textures, vertices, indices, and UVs
- A separate GPU lesson will render the UI data
- Add reusable types and functions to `common/ui/` as the track grows
- Cross-reference math lessons (vectors, rects) and engine lessons (memory,
  structs) where relevant

### Math library usage

GPU lessons and real projects should always use `common/math/`. When you need
a function that doesn't exist yet, use **/math-lesson** to add it (creates
lesson + updates library + documents the concept).

## Skills

Skills in `.claude/skills/` are Claude Code commands that automate lesson
creation and teach AI agents the patterns from lessons. Users invoke with
`/skill-name` in chat. Claude can also invoke them automatically when relevant.

## Quality Assurance

### Markdown linting

All markdown is linted with markdownlint-cli2 (config: `.markdownlint-cli2.jsonc`).
Key rule: all code blocks MUST have language tags (`` ```c ``, `` ```bash ``, `` ```text ``).
Test locally: `npx markdownlint-cli2 "**/*.md"`

### Python linting

Scripts are linted with [Ruff](https://docs.astral.sh/ruff/) (config: `pyproject.toml`).
Test locally: `ruff check scripts/ && ruff format --check scripts/`
Auto-fix: `ruff check --fix scripts/ && ruff format scripts/`

### CRITICAL: Never circumvent quality checks

Never disable lint rules, remove CI workflows, add ignore comments, or relax
thresholds to make errors pass. Always fix the underlying issue. If a rule
seems problematic, ask the user — don't bypass it yourself.

## Dependencies

- SDL3 (with GPU API)
- CMake 3.24+
- A Vulkan/Metal/D3D12-capable GPU
- Python packages for diagram generation: `pip install numpy matplotlib`
