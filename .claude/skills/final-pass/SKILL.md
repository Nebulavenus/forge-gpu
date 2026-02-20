---
name: final-pass
description: Run a quality review pass on a lesson before publishing, catching the recurring issues found across 51 closed PRs
argument-hint: "[lesson-number or lesson-name]"
disable-model-invocation: false
---

Run a systematic quality review on a GPU or math lesson before publishing.
This skill encodes every recurring theme from PR review feedback across the
project's history (51 closed PRs, 177 review comments). Running this pass
before `/publish-lesson` should eliminate most reviewer findings.

The user provides:

- **Lesson number or name** (e.g. `17` or `normal-maps`)

If missing, infer from the current branch name or most recent lesson directory.

## How to run this skill

Work through each section below **in order**. For each check, read the relevant
files and verify compliance. Report a summary at the end with pass/fail per
section and specific issues found.

Use a Task agent (model: haiku) for builds, shader compilation, linting, and
other command execution — never run those directly from the main agent.

---

## 1. SDL GPU bool return checks (40+ PR comments historically)

**This is the single most common PR finding.** Every SDL function that returns
`bool` must be checked. Search **all** `.c` and `.h` files in the lesson
directory (not just `main.c`) for every SDL call and verify each one that
returns `bool` has error handling.

**Functions that return bool (non-exhaustive):**

- `SDL_Init`
- `SDL_ClaimWindowForGPUDevice`
- `SDL_SetGPUSwapchainParameters`
- `SDL_SubmitGPUCommandBuffer`
- `SDL_UploadToGPUBuffer` (via command buffer submit)
- `SDL_WindowSupportsGPUSwapchainComposition`
- `SDL_SetGPUBufferName` / `SDL_SetGPUTextureName`

**Required pattern:**

```c
if (!SDL_SomeFunction(args)) {
    SDL_Log("SDL_SomeFunction failed: %s", SDL_GetError());
    /* clean up any resources allocated so far */
    return false; /* or SDL_APP_FAILURE */
}
```

**What to check:**

- [ ] Every `SDL_SubmitGPUCommandBuffer` call checks the return value
- [ ] Every `SDL_SetGPUSwapchainParameters` call checks the return value
- [ ] `SDL_Init` return is checked in `SDL_AppInit`
- [ ] `SDL_ClaimWindowForGPUDevice` return is checked
- [ ] Helper functions that call SDL submit propagate failure (return NULL or false)
- [ ] Failure paths log the **function name** and `SDL_GetError()`
- [ ] Failure paths clean up resources allocated before the failure point

**How to search:** Use Grep for patterns like `SDL_Submit`, `SDL_SetGPUSwapchain`,
`SDL_Init(`, `SDL_Claim` across all `.c` and `.h` files in the lesson directory
and verify each has an `if (!...)` wrapper.

---

## 2. Magic numbers (20+ PR comments)

Every numeric literal that represents a tuning parameter, spec-defined default,
buffer size, or domain constant must be a `#define` or `enum` at the top of the
file (or in a shared header if reused).

**What to check:**

- [ ] No bare float literals used as thresholds, cutoffs, or defaults (e.g. `0.5f`
  alpha cutoff, `0.3f` rotation speed, `50.0f` light distance)
- [ ] No bare integer literals for array sizes, cascade counts, sample counts
- [ ] Spec-defined values cite the spec (e.g. `/* glTF 2.0 sec 3.9.4 */`)
- [ ] Sentinel values like `1e30f` or `FLT_MAX` are named
  (`#define AABB_SENTINEL 1e30f`)
- [ ] Mathematical constants like `2.0f` in formulas are acceptable only when
  they are inherent to the math (e.g. `2.0 * dot(N, I)` in reflection) — but
  domain-specific multipliers should be named

**How to search:** Grep for bare float patterns like `[^a-zA-Z_]0\.\d+f` and
integer patterns. Inspect each for whether it should be a named constant.

---

## 3. Resource leaks on error paths (15+ PR comments)

When initialization fails partway through, all resources allocated before the
failure point must be released. This is especially important for GPU resources.

**What to check:**

- [ ] Every early-return in init/load functions releases GPU buffers, textures,
  and samplers allocated earlier in the same function
- [ ] `gpu_primitive_count` (or equivalent) is updated incrementally so cleanup
  can release partial uploads
- [ ] Transfer buffer failures don't leak the destination GPU buffer
- [ ] Sampler creation failures don't leak previously created samplers
- [ ] Helper functions (e.g. `upload_gpu_buffer`, `create_white_texture`) return
  NULL on failure and don't leak internal resources

**How to check:** Read each init/load function, identify all `SDL_CreateGPU*`
and `SDL_ReleaseGPU*` calls, and verify every create has a matching release on
every error path.

---

## 4. Naming conventions (15+ PR comments)

**Public API** (in `common/` headers): `Prefix_PascalCase` for types
(e.g. `ForgeGltfScene`), `prefix_snake_case` for functions
(e.g. `forge_gltf_load`).

**Internal typedefs** (in lesson `main.c`): **PascalCase** for struct typedefs
(e.g. `SceneVertex`, `VertUniforms`, `GpuPrimitive`). This is the project
convention, confirmed in `.coderabbit.yaml` and consistent across all lessons.

**Local variables and app_state**: `lowercase_snake_case`.

**What to check:**

- [ ] Public types in `common/` use `Forge` prefix (e.g. `ForgeGltfScene`)
- [ ] Internal typedefs in main.c use PascalCase consistently
  (e.g. `VertUniforms`, not `vertUniforms` or `vert_uniforms`)
- [ ] The `app_state` struct uses lowercase_snake_case (exception: it holds
  all per-session state and is always lowercase by convention)
- [ ] Local helper functions use `snake_case` (not `camelCase`)
- [ ] `#define` constants use `UPPER_SNAKE_CASE`

**How to search:** Grep for `typedef struct` in main.c and verify consistent
PascalCase. Grep for `static` functions and verify snake_case.

---

## 5. Per-field intent comments (15+ PR comments)

Every struct field in uniform blocks, configuration structs, and vertex layouts
needs a comment explaining its purpose, units, and valid range.

**What to check:**

- [ ] Uniform struct fields have inline comments (units, range, purpose)
- [ ] Vertex layout fields document their semantic meaning
- [ ] Configuration/state struct fields explain what they control
- [ ] Push constant structs explain each member

**Example of good comments:**

```c
typedef struct {
    float cascade_splits[MAX_CASCADES]; /* view-space depth boundaries per cascade */
    float shadow_bias;                  /* depth bias in light-space to prevent acne */
    float shadow_texel_size;            /* 1.0 / shadow_map_resolution for PCF offsets */
    int   cascade_count;                /* number of active cascades (1..MAX_CASCADES) */
} shadow_params;
```

---

## 6. Spec and documentation accuracy (5+ PR comments)

When referencing specifications (glTF 2.0, Vulkan, etc.) or external standards,
the wording must match the spec's normative language.

**What to check:**

- [ ] "MUST" vs "SHOULD" vs "MAY" matches the source spec exactly
- [ ] Section numbers or clause references are correct
- [ ] Algorithm descriptions match the reference (not a paraphrase that changes
  the meaning)
- [ ] External links are valid and point to the right section

---

## 7. Skill documentation completeness (5+ PR comments)

The matching skill in `.claude/skills/<topic>/SKILL.md` must have all required
sections.

**What to check:**

- [ ] YAML frontmatter with `name` and `description`
- [ ] Overview paragraph explaining when to use the skill
- [ ] "Key API calls" section listing the SDL/math functions introduced
- [ ] "Correct order" or workflow section showing the sequence of operations
- [ ] "Common mistakes" section documenting gotchas
- [ ] "Ready-to-use template" or code skeleton

---

## 8. README structure and content

**What to check:**

- [ ] Starts with `# Lesson NN — Title`
- [ ] Has "What you'll learn" section near the top
- [ ] Has screenshot or GIF in a "Result" section (not a placeholder)
- [ ] Has "Building" section with build commands
- [ ] Has "AI skill" section linking to the skill
- [ ] Ends with "Exercises" section (3-4 exercises)
- [ ] "What's next" comes before "Exercises" (not after)
- [ ] No use of banned words: "trick", "hack", "magic", "clever", "neat"
  (per CLAUDE.md tone principles)

---

## 9. Markdown linting

Run the linter and resolve all issues.

```bash
npx markdownlint-cli2 "lessons/gpu/NN-name/**/*.md" "lessons/math/NN-name/**/*.md"
```

**Common issues from PR feedback:**

- [ ] All code blocks have language tags (`c`, `bash`, `text`, `hlsl`)
- [ ] Display math uses 3-line format (`$$\n...\n$$`), not inline `$$...$$`
- [ ] Tables have consistent column counts
- [ ] No trailing whitespace or missing blank lines around headings

---

## 10. Python linting (if scripts were modified)

If any Python scripts in `scripts/` were added or modified:

```bash
ruff check scripts/
ruff format --check scripts/
```

- [ ] No lint errors from `ruff check`
- [ ] No format issues from `ruff format --check`
- [ ] Auto-fix with `ruff check --fix scripts/ && ruff format scripts/` if needed

---

## 11. Build and shader compilation

Verify the lesson compiles and shaders are up to date.

```bash
python scripts/compile_shaders.py NN -v
cmake --build build --target lesson_NN_name
```

- [ ] Shaders compile to both SPIRV and DXIL without warnings
- [ ] Lesson builds with no errors or warnings
- [ ] Generated shader headers are up to date (not stale from a previous edit)

---

## Reporting

After completing all checks, report a summary table:

```text
Final Pass Results — Lesson NN: Name
=====================================

 1. SDL bool returns      ✅ PASS  (N calls checked)
 2. Magic numbers         ✅ PASS
 3. Resource leaks        ⚠️  WARN  (1 potential leak in load_scene)
 4. Naming conventions    ✅ PASS
 5. Intent comments       ✅ PASS
 6. Spec accuracy         ✅ PASS
 7. Skill completeness    ✅ PASS
 8. README structure      ✅ PASS
 9. Markdown lint         ✅ PASS
10. Python lint           ⏭️  SKIP  (no scripts modified)
11. Build & shaders       ✅ PASS
```

For each WARN or FAIL, list the specific file, line, and issue with a suggested
fix. Ask the user if they want you to apply the fixes before proceeding to
`/publish-lesson`.
