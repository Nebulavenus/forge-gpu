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

### Documentation and public-facing tone

READMEs, descriptions, and any text a visitor reads first. The core audience
is two groups: beginners new to graphics/game dev, and experienced graphics
programmers who evaluate projects critically. Expert endorsement is what drives
adoption — if credible sources recommend forge-gpu, beginners follow.

**Say what it is. Don't sell it.**

- **No rhetorical flourishes.** "The same license as SDL" not "the same license
  as SDL itself." Every unnecessary word that exists to make something sound
  more impressive undermines credibility.
- **No AI-generated hype.** Words like *powerful*, *unlock*, *robust*,
  *grounded*, *revolutionary* are instant dismissal for the expert audience.
  They pattern-match to LLM output.
- **No selling against others.** "Most tutorials only do X" is insecure.
  Present the project's own value and let people compare.
- **No performing.** "Designed for exploration with an AI assistant" is
  performing. "The lessons are structured so Claude Code can navigate them" is
  informing. The difference is whether the sentence creates an impression or
  communicates a fact.
- **Earned confidence over enthusiasm.** The tone of someone who built
  something substantial and can describe it plainly — not someone trying to
  convince you it's substantial.
- **Stable facts.** If a number will go stale next week (lesson counts), don't
  feature it prominently. Describe scope through arcs and breadth instead.
- **Trust the reader.** Short is fine. Sparse is fine. Over-explaining reads
  as selling. If the information is there, people will find it.

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

### FPS camera pattern (MANDATORY)

Every 3D lesson with a fly camera **must** use the math library's quaternion
camera — never hand-roll sin/cos. This is the canonical pattern:

```c
/* ── Mouse look (in SDL_AppEvent) ────────────────────────────────── */
case SDL_EVENT_MOUSE_MOTION:
    if (state->mouse_captured) {
        state->cam_yaw   -= event->motion.xrel * MOUSE_SENSITIVITY;
        state->cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;
        if (state->cam_pitch >  PITCH_CLAMP) state->cam_pitch =  PITCH_CLAMP;
        if (state->cam_pitch < -PITCH_CLAMP) state->cam_pitch = -PITCH_CLAMP;
    }
    break;

/* ── Camera update (in SDL_AppIterate) ───────────────────────────── */
quat cam_orient = quat_from_euler(state->cam_yaw, state->cam_pitch, 0.0f);
vec3 forward = quat_forward(cam_orient);
vec3 right   = quat_right(cam_orient);

vec3 move = vec3_create(0.0f, 0.0f, 0.0f);
if (keys[SDL_SCANCODE_W]) move = vec3_add(move, forward);
if (keys[SDL_SCANCODE_S]) move = vec3_sub(move, forward);
if (keys[SDL_SCANCODE_D]) move = vec3_add(move, right);
if (keys[SDL_SCANCODE_A]) move = vec3_sub(move, right);
if (keys[SDL_SCANCODE_SPACE])  move.y += 1.0f;
if (keys[SDL_SCANCODE_LSHIFT]) move.y -= 1.0f;

if (vec3_length(move) > 0.001f) {
    move = vec3_scale(vec3_normalize(move), MOVE_SPEED * dt);
    state->cam_position = vec3_add(state->cam_position, move);
}

mat4 view = mat4_view_from_quat(state->cam_position, cam_orient);
mat4 proj = mat4_perspective(FOV_DEG * FORGE_DEG2RAD, aspect,
                             NEAR_PLANE, FAR_PLANE);
mat4 cam_vp = mat4_multiply(proj, view);
```

Key rules — violating any of these produces a backwards or broken camera:

- **Yaw decrements** on positive xrel (`-=`, not `+=`)
- **Use `quat_from_euler` / `quat_forward` / `quat_right`** — never `sinf`/`cosf`
- **Use `mat4_view_from_quat`** — never `mat4_look_at` with a manual target
- **Use `FORGE_DEG2RAD`** — never `* FORGE_PI / 180.0f`

## Git workflow

- **Run `/dev-local-review` before every commit.** No exceptions — this catches
  issues before they reach the PR. Do not `git commit` until the local review
  completes with zero findings.
- **Never commit directly to `main`.** All changes go through pull requests.
- Create a feature branch, commit there, push, and open a PR with `gh pr create`.
- Use the **/dev-publish-lesson** skill for lesson PRs — it handles branch + PR creation.
- **Batch all commits into a single push.** Never push commits piecewise (e.g.
  pushing a fix, then pushing a lint fix separately). Multiple rapid pushes
  cause CodeRabbit to freeze and pause reviews, delaying the entire PR. Make
  all local commits first, verify lint and builds pass locally, then push once.

## Project Structure

```text
forge-gpu/
├── lessons/
│   ├── math/              # Math fundamentals (vectors, matrices, etc.)
│   ├── engine/            # Engine fundamentals (CMake, C, debugging)
│   ├── ui/                # UI fundamentals (fonts, text, atlas, controls)
│   ├── physics/           # Physics simulation (particles, rigid bodies, collisions)
│   ├── audio/             # Audio programming (playback, mixing, spatial audio, DSP)
│   ├── assets/            # Asset pipeline lessons (walkthroughs + exercises)
│   └── gpu/               # SDL GPU lessons (rendering, pipelines, etc.)
├── pipeline/              # Asset pipeline library (Python, pip-installable)
│   ├── __init__.py        # Package marker
│   ├── __main__.py        # CLI entry point
│   ├── config.py          # TOML configuration loader
│   ├── plugin.py          # Plugin base class, registry, discovery
│   ├── scanner.py         # File scanning and fingerprinting
│   ├── bundler.py         # Asset bundle packing and compression
│   └── plugins/           # Built-in asset type plugins
├── common/
│   ├── arena/             # Arena (bump) allocator (header-only)
│   ├── math/              # Math library (header-only, documented)
│   ├── obj/               # OBJ parser (Wavefront .obj files)
│   ├── gltf/              # glTF 2.0 parser (scenes, materials, hierarchy, arena-allocated)
│   ├── ui/                # UI library (TTF parsing, atlas, immediate-mode controls, layout, panels, windows)
│   ├── physics/           # Physics library (particles, rigid bodies, collisions)
│   ├── audio/             # Audio library (playback, mixing, spatial audio, DSP)
│   ├── pipeline/          # Pipeline runtime library (.fmesh + texture loader)
│   ├── shapes/            # Procedural geometry (sphere, torus, capsule, etc.)
│   ├── raster/            # CPU triangle rasterizer (edge function method)
│   ├── capture/           # Screenshot/GIF capture utility
│   ├── scene/             # Scene renderer library (shadow map, Blinn-Phong, grid, sky, camera, UI)
│   └── forge.h            # Shared utilities for lessons
├── tests/                 # Tests per module (arena, math, obj, gltf, raster, ui, physics, shapes, scene, pipeline)
├── scripts/
│   ├── compile_shaders.py # HLSL → SPIRV/DXIL shader compiler
│   ├── compile_scene_shaders.py # Scene renderer shader compiler
│   ├── dump_fmesh.py      # .fmesh binary inspector
│   ├── dump_fscene.py     # .fscene binary inspector
│   └── forge_diagrams/    # Matplotlib diagram generator (per-lesson modules)
│       ├── gpu/           # GPU lesson diagrams (lesson_03.py … lesson_43.py)
│       ├── math/          # Math lesson diagrams
│       ├── ui/            # UI lesson diagrams
│       ├── engine/        # Engine lesson diagrams
│       ├── audio/         # Audio lesson diagrams
│       ├── assets/        # Asset pipeline lesson diagrams
│       └── physics/       # Physics lesson diagrams
├── tools/
│   ├── common/            # Shared binary I/O helpers (header-only)
│   ├── mesh/              # C mesh processing tool (meshoptimizer, MikkTSpace)
│   ├── anim/              # C animation processing tool (glTF animation extraction)
│   ├── scene/             # C scene hierarchy extraction tool (glTF to .fscene)
│   └── texture/           # C texture processing tool (BC7/BC5 compression via basisu)
├── assets/                # Shared assets (fonts, models, skyboxes)
├── docs/                  # Project documentation and plans
├── .claude/skills/        # Claude Code skills (AI-invokable patterns)
└── third_party/           # Dependencies (SDL3, etc.)
```

## Testing

```bash
cmake --build build --target test_gltf   # build one C test
ctest --test-dir build -R gltf           # run one C test
ctest --test-dir build                   # run all C tests
pytest tests/pipeline/ -v                # run pipeline (Python) tests
```

When modifying parsers or libraries in `common/`, always build and run the
corresponding tests. When modifying the pipeline library in `pipeline/`,
run `pytest tests/pipeline/`. When adding a new module, add a matching test
under `tests/` and register it in the root `CMakeLists.txt` (C) or
`pyproject.toml` (Python).

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
- **Use diagrams** — use **/dev-create-diagram** to generate matplotlib visuals
  for geometric, mathematical, or data-flow concepts. Never use ASCII art
  diagrams in lesson READMEs; proper diagrams are more readable and match the
  project's visual identity.
- End with exercises that extend the lesson
- Write a matching skill in `.claude/skills/<name>/SKILL.md`

### Engine lessons (lessons/engine/)

- Use **/dev-engine-lesson** skill to scaffold
- Focus on practical engineering: build systems, C, debugging, project structure
- Show common errors and how to diagnose them
- Cross-reference GPU and math lessons where relevant

### Math lessons (lessons/math/)

- Use **/dev-math-lesson** skill to scaffold + update library
- Small, focused program demonstrating one concept
- Add implementation to `common/math/` with extensive inline docs
- Cross-reference GPU lessons that use this math

### UI lessons (lessons/ui/)

- Use **/dev-ui-lesson** skill to scaffold
- Build an immediate-mode UI system — fonts, text, layout, controls
- **No GPU code** — lessons produce textures, vertices, indices, and UVs
- [GPU Lesson 28](lessons/gpu/28-ui-rendering/) renders the UI data on the GPU
- Add reusable types and functions to `common/ui/` as the track grows
- Cross-reference math lessons (vectors, rects) and engine lessons (memory,
  structs) where relevant

### Physics lessons (lessons/physics/)

- Use **/dev-physics-lesson** skill to scaffold
- Interactive 3D programs — simulation rendered in real time with SDL GPU
- Every lesson includes Blinn-Phong lighting, grid floor, shadow map, and
  camera controls as a rendering baseline
- Use simple shapes (spheres, cubes, capsules) — the physics is the focus
- Fixed timestep with accumulator pattern — physics must be frame-rate independent
- Support pause (P), reset (R), and slow motion (T) in every lesson
- Add reusable physics code to `common/physics/` as the track grows
- Capture both screenshots AND animated GIFs (physics is dynamic)
- Cross-reference math lessons for vectors, quaternions, and integration theory

### Audio lessons (lessons/audio/)

- Use **/dev-audio-lesson** skill to scaffold
- Interactive programs with SDL GPU scenes and forge UI control panels
- Every lesson includes Blinn-Phong lighting, grid floor, shadow map, camera
  controls, and a forge UI panel as a rendering/UI baseline
- Camera position doubles as the audio listener position for spatial lessons
- All audio output goes through SDL3 audio streams — no third-party audio libs
- `forge_audio.h` is a header-only library that grows lesson by lesson, with
  the same quality bar as `forge_physics.h` (documented, tested, numerically safe)
- UI widgets for audio controls (volume sliders, waveform displays, VU meters);
  if a needed widget does not exist in `common/ui/`, add it as part of the lesson
- Add reusable audio code to `common/audio/` as the track grows
- Cross-reference math lessons for vectors (spatial audio) and signal processing

### Asset pipeline lessons (lessons/assets/)

- Use **/dev-asset-lesson** skill to scaffold
- **Hybrid Python + C track** — Python orchestrates, C handles performance-
  critical processing (meshoptimizer, MikkTSpace) and procedural geometry
- **The pipeline is a reusable library** at `pipeline/` in the repo root,
  pip-installable via `pip install -e ".[dev]"`. Lessons teach how each
  piece works; the code lives in the shared library, not in lesson directories.
- Each lesson adds real functionality to `pipeline/` — config, plugins,
  scanning, processing. Tests live in `tests/pipeline/`.
- C tool/library lessons are added to CMakeLists.txt for building
- Plugin architecture — each asset type (texture, mesh, scene) is a plugin;
  C tools are invoked as subprocesses by the Python pipeline
- Procedural geometry lives in `common/shapes/forge_shapes.h` (header-only)
- Incremental builds with content-hash fingerprinting
- Later lessons add a web frontend (Flask/FastAPI + static HTML/JS)
- Cross-reference GPU lessons that consume the processed assets
- Cross-reference engine lessons for build system and dependency concepts

### Math library usage

GPU lessons and real projects should always use `common/math/`. When you need
a function that doesn't exist yet, use **/dev-math-lesson** to add it (creates
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

All Python code is linted with [Ruff](https://docs.astral.sh/ruff/) (config: `pyproject.toml`).
Test locally: `ruff check && ruff format --check`
Auto-fix: `ruff check --fix && ruff format`

### CRITICAL: Never circumvent quality checks

Never disable lint rules, remove CI workflows, add ignore comments, or relax
thresholds to make errors pass. Always fix the underlying issue. If a rule
seems problematic, ask the user — don't bypass it yourself.

## Large file writes (MANDATORY — Task agent token limit)

Task agents hit a **32K output token limit** on Write calls. A single Write
producing ~800+ lines of C will fail silently — the file is never created and
all agent work is lost. This is a **fatal, unrecoverable error** that wastes
hours of planning and coding.

**MANDATORY rules — these are non-negotiable:**

1. **ALL GPU lesson `main.c` files MUST use the chunked-write pattern.** Do not
   attempt to write a full lesson `main.c` in a single Write call — ever. Split
   into 3-4 parts (~400-600 lines each), write each to `/tmp/`, then
   concatenate with `cat`.
2. **Every lesson PLAN.md MUST include a "main.c Decomposition" section**
   specifying what goes in each chunk before any coding agent starts writing.
3. **If a coding agent fails with a token limit error, NEVER write a fallback
   or simplified replacement.** STOP immediately and report the failure to the
   user. Writing a dumbed-down `main.c` destroys all the planning and coding
   work and forces the user to redo everything from scratch.
4. **Any file over ~800 lines** (README, `.c`, `.h`) must be written in chunks.

See [`.claude/large-file-strategy.md`](.claude/large-file-strategy.md) for the
full strategy, agent decomposition template, contract-sharing rules, and
recovery steps.

## Dependencies

- SDL3 (with GPU API)
- CMake 3.24+
- A Vulkan/Metal/D3D12-capable GPU
- Git LFS (`git lfs install` after cloning — GIFs are LFS-tracked)
- Python packages for diagram generation: `pip install numpy matplotlib`
