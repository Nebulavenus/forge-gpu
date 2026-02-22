---
name: create-lesson
description: Add lesson content (README, skill file, screenshot) to an already-working GPU lesson scene
argument-hint: "[number] [name]"
disable-model-invocation: true
---

Add lesson documentation and content to a GPU lesson that already has a working
scene. This is the "second half" of the advanced lesson workflow — the scene
exists and runs, and now it needs a README, skill file, and screenshot.

**When to use this skill:**

- A lesson directory exists with a working main.c and compiled shaders
- The scene builds, runs, and shows the intended result
- The lesson was scaffolded with `/start-lesson` and built iteratively
- You're ready to document what was built

**Preconditions (verify before proceeding):**

- `lessons/gpu/NN-name/main.c` exists and compiles
- Shaders exist in `shaders/` and are compiled in `shaders/compiled/`
- The lesson builds and runs correctly
- A `PLAN.md` exists describing what was built and the concept being taught

**Workflow context:**

1. `/start-lesson` — Scaffold directory + minimal main.c + PLAN.md
2. Build the scene iteratively (already done before this skill)
3. `/create-lesson` — Add README, skill file, screenshot **(this skill)**
4. `/final-pass` — Quality review
5. `/publish-lesson` — Commit and PR

## Arguments

The user (or you) provides:

- **Number**: two-digit lesson number (e.g. 20)
- **Name**: short kebab-case name (e.g. pbr-materials)

If not provided, infer from the existing lesson directory. If ambiguous, ask.

## Steps

### 1. Locate and read the lesson

Find the lesson directory at `lessons/gpu/NN-name/`. Read:

- **PLAN.md** — understand what concept is taught, what the scene contains,
  and what features were built
- **main.c** — understand the code structure, key API calls, and how the
  concept is implemented
- **Shader files** — understand what the shaders do (vertex transforms,
  lighting model, post-processing, etc.)

### 2. Verify the lesson builds and runs

Use a Task agent (`model: "haiku"`) to build the lesson:

```bash
cmake --build build --config Debug --target NN-name
```

If it fails, stop and tell the user — the scene needs to be fixed before
documentation can be written.

### 3. Create `README.md`

Follow the same structure as `/new-lesson` creates, but written around the
**existing working code** rather than generated alongside it. The README should
explain the new concept and point to where it appears in the code and shaders.

Structure:

````markdown
# Lesson NN — Title

[Brief subtitle — what the reader will learn]

## What you'll learn

- [Bullet list of concepts, focused on the ONE new concept]
- [Supporting concepts get brief mentions]

## Result

![Lesson NN screenshot](assets/screenshot.png)

[Describe what the reader sees and why it demonstrates the concept]

## Key concepts

### [New concept name]

[Explain the concept — what it is, why it matters, how it works.
Reference where it appears in main.c and the shaders.]

### [Supporting concept if needed]

[Brief explanation of supporting features that enable the main concept]

## Math

This lesson uses:
- [Link to relevant math lessons]
- [Link to math library functions used]

## Building

```bash
cmake -B build
cmake --build build --config Debug

# Windows
build\lessons\gpu\NN-name\Debug\NN-name.exe

# Linux / macOS
./build/lessons/gpu/NN-name/NN-name
```

## AI skill

This lesson's pattern is available as a reusable Claude Code skill:

- **Skill file**: [`.claude/skills/<topic>/SKILL.md`](../../../.claude/skills/<topic>/SKILL.md)
- **Invoke**: `/skill-name`

You can copy this skill into your own project's `.claude/skills/` directory
to use the same pattern with Claude Code.

## What's next

[Brief pointer to the next lesson or related concepts to explore]

## Exercises

1. [Exercise that modifies the new concept]
2. [Exercise that extends it]
3. [Exercise that combines it with a previous concept]
````

**Writing guidelines:**

- Focus the README on the **new concept** — don't re-explain camera setup,
  shadow mapping, etc. in full detail. Reference the lessons that taught those.
- Point to specific lines or sections of main.c and shaders where the concept
  appears: "In `main.c`, the `create_bloom_pipeline()` function sets up..."
- Use the PLAN.md "Concept" and "New concept integration" sections as a guide
  for what to emphasize
- Follow all tone guidelines from CLAUDE.md (no banned words, respect the
  material, explain reasoning)

### 4. Create the matching skill file

Create `.claude/skills/<topic>/SKILL.md` where `<topic>` is the kebab-case
name of the new concept (e.g. `pbr-materials`, `screen-space-reflections`).

Structure:

```markdown
---
name: <topic>
description: [One-line description of the pattern]
---

[Overview paragraph — when and why to use this pattern]

## Key API calls

- `SDL_SomeFunction` — [what it does in this context]
- [Other key calls]

## Correct order

1. [Step-by-step workflow for implementing this pattern]
2. [...]

## Common mistakes

- [Mistake 1 — what goes wrong and why]
- [Mistake 2 — ...]

## Code template

[Ready-to-use code skeleton showing the pattern, adapted from the lesson's
main.c. Include the essential structure but strip lesson-specific details.]
```

### 5. Capture a screenshot

Use the `/add-screenshot` skill to capture and embed the screenshot:

```bash
python scripts/capture_lesson.py lessons/gpu/NN-name
```

Verify:

- `lessons/gpu/NN-name/assets/screenshot.png` exists and looks correct
- The README references it: `![Lesson NN screenshot](assets/screenshot.png)`

### 6. Update root `README.md`

Add a row to the GPU Lessons gallery table in the root README. Follow the
existing format — lesson number, linked name, brief description.

### 7. Update root `PLAN.md`

Mark the lesson as completed (change `[ ]` to `[x]`), or add it if it wasn't
listed.

### 8. Update the lesson's `PLAN.md`

Check off the documentation steps:

```markdown
- [x] README written (`/create-lesson`)
- [x] Skill file created (`/create-lesson`)
- [x] Screenshot captured
```

### 9. Run markdown linting

Use the `/markdown-lint` skill to verify all markdown passes:

```bash
npx markdownlint-cli2 "lessons/gpu/NN-name/**/*.md"
npx markdownlint-cli2 ".claude/skills/<topic>/**/*.md"
```

If errors are found:

1. Try auto-fix: `npx markdownlint-cli2 --fix "lessons/gpu/NN-name/**/*.md"`
2. Manually fix remaining issues (especially MD040 — missing language tags)
3. Verify: re-run the lint command

### 10. Report to the user

Tell the user:

- The README, skill file, and screenshot have been added
- The lesson's PLAN.md has been updated
- The root README.md and PLAN.md have been updated
- Markdown linting passes
- Suggest next steps: `/final-pass` for quality review, then `/publish-lesson`

## What this skill does NOT do

- Does **not** create the lesson directory or main.c (that's `/start-lesson`)
- Does **not** build the scene (that's manual iterative work)
- Does **not** create a branch or PR (that's `/publish-lesson`)
- Does **not** do a full quality review (that's `/final-pass`)

## README-style reminders

- README uses `# Lesson NN — Title` format (em dash, not hyphen)
- All code blocks have language tags (` ```c `, ` ```bash `, ` ```text `)
- Display math blocks use three-line format (`$$\n...\n$$`)
- Cross-reference math lessons in a "Math" section
- Cross-reference previous GPU lessons when building on their concepts
- No banned words: "trick", "hack", "magic", "clever", "neat"
- Explain *why* techniques work, not just *what* they do

## Diagrams and Formulas

**Find opportunities to create compelling diagrams and visualizations via the
matplotlib scripts** — they increase reader engagement and help learners
understand the topics being taught. Use the `/create-diagram` skill to add
diagrams following the project's visual identity and quality standards.

### Matplotlib diagrams

For geometric or visual diagrams (UV mapping, filtering comparison), add a
diagram function to `scripts/forge_diagrams/gpu_diagrams.py`:

1. Write a function following the existing pattern (shared `setup_axes`,
   `draw_vector`, `save` helpers from `_common.py`)
2. Register it in the `DIAGRAMS` dict in `__main__.py` with the lesson key (e.g. `"gpu/04"`)
3. Run `python scripts/forge_diagrams --lesson gpu/NN` to generate the PNG
4. Reference in the README: `![Description](assets/diagram_name.png)`

### Mermaid diagrams

For **flow/pipeline diagrams** (texture upload flow, MVP pipeline), use inline
mermaid blocks — GitHub renders them natively:

````markdown
```mermaid
flowchart LR
    A[Step 1] -->|transform| B[Step 2] --> C[Step 3]
```
````

Use mermaid for sequential flows.

### KaTeX math

For **formulas**, use inline `$...$` and display `$$...$$` math notation:

- Inline: `$\text{MVP} = P \times V \times M$`
- Display math blocks must be split across three lines (CI enforces this):

```text
$$
x_{\text{screen}} = \frac{x \cdot n}{-z}
$$
```

Keep worked examples (step-by-step with numbers) in ` ```text ` blocks.

## Code style reminders

- Naming: `PascalCase` for typedefs (e.g. `Vertex`, `GpuPrimitive`),
  `lowercase_snake_case` for local variables and functions (e.g. `app_state`),
  `UPPER_SNAKE_CASE` for `#define` constants,
  `Prefix_PascalCase` for public API types (e.g. `ForgeCapture`) and
  `prefix_snake_case` for public API functions (e.g. `forge_capture_init`)
- The `app_state` struct holds all state passed between callbacks
- Build on previous lessons — reference what was introduced before
- Each lesson should introduce ONE new concept at a time
- **Always use the math library** — no bespoke math in GPU lessons
- Link to math lessons when explaining concepts
- **Never extract assets from glTFs à la carte** — when a lesson uses a glTF
  model, copy the complete model (`.gltf`, `.bin`, and all referenced textures)
  into the lesson's `assets/` directory and load it with `forge_gltf_load()`.
  The model's node transforms, materials, and textures should drive the scene
  layout, not hand-coded geometry.
- **Always check SDL return values** — every SDL GPU function that returns
  `bool` must be checked. Log the function name and `SDL_GetError()` on
  failure, then clean up resources and early-return. This includes
  `SDL_SubmitGPUCommandBuffer`, `SDL_SetGPUSwapchainParameters`,
  `SDL_ClaimWindowForGPUDevice`, `SDL_Init`, and others. This is a
  recurring PR review item — get it right the first time.
