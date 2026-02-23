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

**Be literal and exhaustive.** This is C — no RAII, no garbage collector. Every
resource you acquire must be released on every exit path, every struct field
must be documented, every error must be handled. Do not rationalize away
findings with "it's probably fine" or "the section comment covers it." If the
check says every field, check every field. If it says every error path, trace
every error path.

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
- `SDL_CancelGPUCommandBuffer`
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

## 2. Command buffer lifecycle (acquired from Lesson 24 review)

**Every acquired command buffer must be either submitted or canceled.** There
is no automatic cleanup — an abandoned command buffer is a resource leak. This
is C: if you acquire it, you release it, on every path.

**Key SDL3 constraint:** `SDL_CancelGPUCommandBuffer` is **not allowed** after
a swapchain texture has been acquired on that command buffer. After swapchain
acquisition, you **must** submit (even on error).

**What to check:**

- [ ] Every `SDL_AcquireGPUCommandBuffer` has a matching submit or cancel on
  **every** code path that follows — including early returns from failed
  `BeginRenderPass`, failed `ensure_*` helpers, etc.
- [ ] Error paths **before** swapchain acquisition use
  `SDL_CancelGPUCommandBuffer(cmd)`
- [ ] Error paths **after** swapchain acquisition use
  `SDL_SubmitGPUCommandBuffer(cmd)` (submit the partial/empty command buffer)
- [ ] The `!swapchain_tex` (minimized window) path submits the empty
  command buffer and returns `SDL_APP_CONTINUE`

**How to check:** Find `SDL_AcquireGPUCommandBuffer` in `SDL_AppIterate`, then
trace **every** `return` statement between that acquire and the final submit.
Each one must either submit or cancel `cmd` first. Do not skip any — enumerate
them all and verify individually.

---

## 3. Magic numbers (20+ PR comments)

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

## 4. Resource leaks on error paths (15+ PR comments)

When initialization fails partway through, all resources allocated before the
failure point must be released. This is C — no destructors, no RAII, no GC.
If you allocate it, you must free it on every exit path.

**What to check:**

- [ ] Every early-return in init/load functions releases GPU buffers, textures,
  and samplers allocated earlier in the same function
- [ ] `gpu_primitive_count` (or equivalent) is updated incrementally so cleanup
  can release partial uploads
- [ ] Transfer buffer failures don't leak the destination GPU buffer
- [ ] Sampler creation failures don't leak previously created samplers
- [ ] Helper functions (e.g. `upload_gpu_buffer`, `create_white_texture`) return
  NULL on failure and don't leak internal resources
- [ ] `init_fail` cleanup matches `SDL_AppQuit` cleanup — every resource freed
  in `SDL_AppQuit` must also be freed in `init_fail` (including conditional
  resources like `#ifdef FORGE_CAPTURE`)
- [ ] `ensure_*` functions that destroy-then-recreate handle partial failure
  (some resources recreated, some not) without leaking

**How to check:** Read each init/load function, identify all `SDL_CreateGPU*`
and `SDL_ReleaseGPU*` calls, and verify every create has a matching release on
every error path. Then diff `init_fail` against `SDL_AppQuit` line by line —
any resource freed in one but not the other is a bug.

---

## 5. Naming conventions (15+ PR comments)

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

## 6. Per-field intent comments (15+ PR comments)

**Every** struct field needs an inline comment — no exceptions, no "the section
header covers it." Section headers group related fields; inline comments
explain each individual field's purpose, units, format, or valid range.

A field named `exposure` is not self-documenting. Is it a multiplier or an EV
stop? What range is valid? What happens at 0? The inline comment answers this:
`/* brightness multiplier before tone mapping (>0) */`

**What to check:**

- [ ] Uniform struct fields have inline comments (units, range, purpose)
- [ ] Vertex layout fields document their semantic meaning
- [ ] **`app_state` fields each have an inline comment** — not just section
  headers. Every pipeline, texture, sampler, buffer, setting, and state
  variable gets its own comment explaining what it is, its format/units
  where applicable, and how it's used
- [ ] Push constant structs explain each member
- [ ] GPU type struct fields (e.g. `GpuPrimitive`, `GpuMaterial`, `ModelData`)
  document each field

**Example of good comments:**

```c
typedef struct {
    float cascade_splits[MAX_CASCADES]; /* view-space depth boundaries per cascade */
    float shadow_bias;                  /* depth bias in light-space to prevent acne */
    float shadow_texel_size;            /* 1.0 / shadow_map_resolution for PCF offsets */
    int   cascade_count;                /* number of active cascades (1..MAX_CASCADES) */
} shadow_params;
```

**Bad — section headers are not field comments:**

```c
/* Camera. */
vec3  cam_position;
float cam_yaw;
float cam_pitch;
```

**Good — every field documented:**

```c
/* Camera. */
vec3  cam_position;  /* world-space camera position */
float cam_yaw;       /* horizontal rotation (radians, 0 = +Z) */
float cam_pitch;     /* vertical rotation (radians, clamped ±1.5) */
```

---

## 7. Spec and documentation accuracy (5+ PR comments)

When referencing specifications (glTF 2.0, Vulkan, etc.) or external standards,
the wording must match the spec's normative language.

**What to check:**

- [ ] "MUST" vs "SHOULD" vs "MAY" matches the source spec exactly
- [ ] Section numbers or clause references are correct
- [ ] Algorithm descriptions match the reference (not a paraphrase that changes
  the meaning)
- [ ] External links are valid and point to the right section

---

## 8. Skill documentation completeness (5+ PR comments)

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

## 9. README structure and content

**What to check:**

- [ ] Starts with `# Lesson NN — Title`
- [ ] Has "What you'll learn" section near the top
- [ ] Has screenshot in a "Result" section (not a placeholder)
- [ ] **GPU lessons only:** If the lesson has shader files, has a "Shaders"
  section immediately before "Building" that lists each shader file with a
  brief description of what it does
- [ ] Has "Building" section with build commands
- [ ] Has "AI skill" section linking to the skill
- [ ] Ends with "Exercises" section (3-4 exercises)
- [ ] "What's next" comes before "Exercises" (not after)
- [ ] No use of banned words: "trick", "hack", "magic", "clever", "neat"
  (per CLAUDE.md tone principles)

---

## 10. Markdown linting

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

## 11. Python linting (if scripts were modified)

If any Python scripts in `scripts/` were added or modified:

```bash
ruff check scripts/
ruff format --check scripts/
```

- [ ] No lint errors from `ruff check`
- [ ] No format issues from `ruff format --check`
- [ ] Auto-fix with `ruff check --fix scripts/ && ruff format scripts/` if needed

---

## 12. Build and shader compilation

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
 2. Command buffer life   ✅ PASS  (N paths checked)
 3. Magic numbers         ✅ PASS
 4. Resource leaks        ⚠️  WARN  (1 potential leak in load_scene)
 5. Naming conventions    ✅ PASS
 6. Intent comments       ✅ PASS
 7. Spec accuracy         ✅ PASS
 8. Skill completeness    ✅ PASS
 9. README structure      ✅ PASS
10. Markdown lint         ✅ PASS
11. Python lint           ⏭️  SKIP  (no scripts modified)
12. Build & shaders       ✅ PASS
```

For each WARN or FAIL, list the specific file, line, and issue with a suggested
fix. Ask the user if they want you to apply the fixes before proceeding to
`/publish-lesson`.
