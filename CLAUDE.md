# forge-gpu

A learning platform and building tool for real-time graphics with SDL's GPU API.

**Dual purpose:**

1. **Learn** — Guided lessons teaching GPU programming, math, and game techniques
2. **Forge** — Skills and libraries enabling humans + AI to build real projects

Repository: https://github.com/RosyGameStudio/forge-gpu
License: zlib (matching SDL)

## Tone

Many users are learning graphics or game programming for the first time. Others
are building projects and hit a problem that requires deeper understanding.
Either way, when answering questions or writing code:

- Be encouraging and patient — there are no dumb questions about GPU APIs or math
- Explain *why*, not just *what* — connect each concept to the bigger picture
- Use plain language before jargon; when jargon is unavoidable, define it
- If a user's approach won't work, explain the reason gently and suggest an
  alternative rather than just correcting them
- Reference specific lessons when relevant ("this is the pattern from Lesson 02",
  "see lessons/math/01-vectors for the dot product explanation")
- Remember that getting a triangle on screen for the first time is a big deal
- When using the math library, link to both the library docs AND the math lesson
  explaining the concept

## Philosophy

**Learning enables building.** Understanding fundamentals makes working with AI
more productive. This project teaches graphics, math, and game techniques — then
provides reusable skills and libraries so humans and AI can build together.

### Learning (lessons/)

- Every line of code understandable by someone new to graphics/game dev
- Extensive comments explaining *why*, not just *what*
- Progressive curriculum: each lesson builds on the previous
- Topics: SDL GPU API, math fundamentals, rendering techniques, physics, etc.
- Math lessons are standalone (small programs demonstrating concepts)
- GPU lessons integrate math library (no bespoke math in GPU code)

### Forging (skills + libraries)

- **Skills** (.claude/skills/) — Reusable Claude Code skills distilled from lessons
- **Math library** (common/math/) — Documented, readable, learning-focused
- Every lesson produces a skill; AI agents learn the same patterns as humans
- Goal: Enable anyone to use Claude to build games and renderers confidently

### Code conventions

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
- **Readability:** This code is meant to be learned from — clarity over cleverness
- **IMPORTANT:** Always run build commands via a Task agent with `model: "haiku"`,
  never directly from the main agent. This keeps the main context clean, uses
  fewer tokens, and is faster.

## Project Structure

```text
forge-gpu/
├── lessons/
│   ├── math/              # Math fundamentals (vectors, matrices, etc.)
│   │   ├── README.md      # Overview of math lessons
│   │   └── NN-concept/    # Each concept: program + README + adds to math lib
│   └── gpu/               # SDL GPU lessons (rendering, pipelines, etc.)
│       └── NN-name/       # Each lesson: standalone buildable project
├── common/
│   ├── math/              # Math library (header-only, documented)
│   └── ...                # Other shared utilities
├── .claude/skills/        # Claude Code skills (AI-invokable patterns)
│   ├── math-lesson/       # Skill: add math concept + lesson + update lib
│   ├── new-lesson/        # Skill: create new GPU lesson
│   └── ...
└── third_party/           # Dependencies (SDL3, etc.)
```

### How it fits together

1. **Math lessons** teach concepts, add to `common/math/`
2. **GPU lessons** use math library, refer to math lessons for theory
3. **Skills** automate lesson creation and teach AI agents the patterns
4. **Math library** is reusable in any project (lessons or real builds)

## When writing GPU lessons (lessons/gpu/)

- Start each README.md with what the reader will learn
- Show the result (screenshot) before diving into code
- Introduce API calls one at a time with context
- **Use the math library** for all math operations — no bespoke math in GPU code
- Link to relevant math lessons when explaining concepts
- End with exercises that extend the lesson
- **Also write a matching skill** in .claude/skills/<name>/SKILL.md that
  distills the lesson into a reusable Claude Code skill: YAML frontmatter
  with name and description, then the key API calls, correct order, common
  mistakes, and a ready-to-use code template

## When writing math lessons (lessons/math/)

- Use the **/math-lesson** skill to scaffold lesson + update library
- Small, focused program demonstrating one concept clearly
- README explains the math, the intuition, and where it's used in graphics/games
- Add the implementation to `common/math/` with extensive inline docs
- Cross-reference GPU lessons that use this math
- Keep it readable — this code is meant to be learned from

## Using the math library (common/math/)

**In your code:**

```c
#include "math/forge_math.h"  // Or whatever we name it

vec3 position = vec3_create(0.0f, 1.0f, 0.0f);
mat4 rotation = mat4_rotate_z(angle);
```

**When to use existing math:**

- GPU lessons should always use the math library
- Real projects building with forge-gpu should use it too
- Reference the math library README and relevant lessons for documentation

**When to add new math:**

- You need a function that doesn't exist yet
- Use **/math-lesson** skill (invokable by humans or Claude automatically)
- This creates: math lesson + updates library + documents the concept
- Even simple operations deserve a lesson (dot product, normalize, etc.)

## Using skills (.claude/skills/)

Skills are Claude Code commands that teach AI agents patterns from lessons.

**Available skills:**

- **/math-lesson** — Add a math concept (lesson + library update)
- **/new-lesson** — Create a new GPU lesson
- **/publish-lesson** — Commit and PR a completed lesson
- **/sdl-gpu-setup** — Scaffold SDL3 GPU application
- And more — see `.claude/skills/` directory

**How they work:**

- Users invoke with `/skill-name` in the chat
- Claude can also invoke them automatically when relevant
- Each skill knows the project conventions and generates correct code
- Skills are themselves documented and maintainable

**For Claude:** When you need to add math functionality, use the /math-lesson
skill. When creating GPU lessons, ensure they use the math library and reference
math lessons for theory.

## Quality Assurance

This project maintains quality through automated linting and CI workflows. These
checks ensure consistency, readability, and maintainability for learners and
contributors.

### Markdown linting

All markdown files are linted with markdownlint-cli2 in CI. Configuration is in
`.markdownlint-cli2.jsonc`.

**Key rules:**

- **MD040** — All code blocks MUST have language tags (` ```c`, ` ```bash`, ` ```text`)
- **MD060** — Tables must have consistent column alignment
- Line length limits are disabled (lessons need flexibility)
- Blank lines around lists/code blocks are lenient

**Testing locally:**

```bash
npx markdownlint-cli2 "**/*.md"
```

**Common fixes:**

- Missing language tag: ` ``` ` → ` ```c` or ` ```text` or ` ```bash`
- Use `text` for plain output, diagrams, or examples without syntax

The linter runs automatically on PRs and will fail if issues are found.

### Python linting

All Python scripts in `scripts/` are linted and formatted with
[Ruff](https://docs.astral.sh/ruff/). Configuration is in `pyproject.toml`.

**Rules enabled:** pycodestyle, pyflakes, isort, pyupgrade, bugbear, simplify.

**Testing locally:**

```bash
ruff check scripts/
ruff format --check scripts/
```

**Auto-fix:**

```bash
ruff check --fix scripts/
ruff format scripts/
```

The linter runs automatically on PRs and will fail if issues are found.

### CRITICAL: Never circumvent quality checks

**NEVER do any of the following to "unblock" changes:**

- ❌ Disable or remove lint rules to make errors pass
- ❌ Relax lint requirements or thresholds to allow failing code
- ❌ Remove or disable CI workflows (`.github/workflows/*.yml`)
- ❌ Add lint ignore comments to skip validation
- ❌ Modify `.markdownlint-cli2.jsonc` or `pyproject.toml` to weaken existing rules

**Always fix the underlying issue instead:**

- ✅ Fix markdown formatting to meet the existing standards
- ✅ Add proper language tags to code blocks
- ✅ Correct table formatting
- ✅ Fix Python lint/format issues (`ruff check --fix` and `ruff format`)
- ✅ Ask the user if the rule seems problematic (don't bypass it yourself)

Quality checks exist to maintain learning quality and consistency. Bypassing them
degrades the project for all users. When CI fails, fix the code — never weaken
the checks.

## Dependencies

- SDL3 (with GPU API)
- CMake 3.24+
- A Vulkan/Metal/D3D12-capable GPU
- Python packages for diagram generation: `pip install numpy matplotlib`
