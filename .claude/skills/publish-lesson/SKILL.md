---
name: publish-lesson
description: Validate, commit, and create a PR for a new forge-gpu lesson
argument-hint: "[lesson-number] [lesson-name]"
disable-model-invocation: false
---

Validate that a lesson is complete, create a feature branch, commit with a
descriptive message, push, and create a pull request.

The user provides:

- **Lesson number**: two-digit number (e.g. 03)
- **Lesson name**: kebab-case name (e.g. uniforms-and-motion)

If missing, infer from the most recent lesson directory in `lessons/`.

## Validation checklist

Before committing, verify the lesson has all required pieces from the
`new-lesson` skill:

### Required files

- [ ] `lessons/NN-name/main.c` exists
- [ ] `lessons/NN-name/CMakeLists.txt` exists
- [ ] `lessons/NN-name/README.md` exists
- [ ] `lessons/NN-name/shaders/` directory exists with shader source files
- [ ] `lessons/NN-name/assets/screenshot.png` exists
- [ ] `.claude/skills/<topic>/SKILL.md` exists (the matching skill)

### main.c structure

- [ ] Uses `#define SDL_MAIN_USE_CALLBACKS 1`
- [ ] Implements all 4 callbacks: `SDL_AppInit`, `SDL_AppEvent`, `SDL_AppIterate`, `SDL_AppQuit`
- [ ] Uses `SDL_calloc` for app_state allocation
- [ ] Includes error handling with `SDL_Log` on all GPU calls
- [ ] **Every SDL function that returns `bool` is checked** — this includes
  `SDL_SubmitGPUCommandBuffer`, `SDL_SetGPUSwapchainParameters`,
  `SDL_ClaimWindowForGPUDevice`, `SDL_Init`, etc. Each failure path must log
  the function name + `SDL_GetError()` and clean up or early-return. This is
  the most common PR review finding — verify every call site before publishing.
- [ ] No magic numbers — all literals are `#define` or `enum` constants
- [ ] Has comprehensive comments explaining *why* and *purpose*, not just *what* —
  every pipeline setting, resource binding, and API call states the reason for
  the choice (e.g. why CULLMODE_NONE, why we push uniforms each frame)

### Integration with project

- [ ] Root `CMakeLists.txt` includes `add_subdirectory(lessons/NN-name)`
- [ ] Root `README.md` has a table row for this lesson with link and description
- [ ] Root `PLAN.md` has the lesson checked off in completed section

### README.md structure

- [ ] Has `# Lesson NN — Title` header
- [ ] Has "What you'll learn" section
- [ ] Has "Result" section with a screenshot (`![...](assets/screenshot.png)`)
  — **not** just a placeholder
- [ ] Has "Key concepts" section explaining new APIs
- [ ] Has "Building" section
- [ ] Has "AI skill" section linking to `.claude/skills/<topic>/SKILL.md`
- [ ] Has "Exercises" section (3-4 exercises)

### Skill file structure

- [ ] YAML frontmatter with `name`, `description`, and appropriate flags
- [ ] Explains the key API pattern introduced in the lesson
- [ ] Includes code template or step-by-step instructions
- [ ] Documents common mistakes or gotchas

### Markdown linting

- [ ] All markdown files pass linting (`npx markdownlint-cli2 "**/*.md"`)
- [ ] Code blocks have language tags (MD040)
- [ ] Tables have consistent column counts (MD060)

If any checks fail, report them to the user and ask if they want to fix them
before proceeding.

## Git workflow

Once validation passes:

### 1. Start from the latest main

Ensure the feature branch is based on the latest main to avoid merge
conflicts and stale dependencies:

```bash
git checkout main
git pull origin main
```

### 2. Create a feature branch

Branch name format: `lesson-NN-name` (e.g., `lesson-03-uniforms-and-motion`)

```bash
git checkout -b lesson-NN-name
```

### 3. Stage all changes

Review what will be committed:

```bash
git status
git diff --cached
```

Stage all lesson-related files:

```bash
git add lessons/NN-name/
git add .claude/skills/<topic>/
git add CMakeLists.txt PLAN.md README.md
```

### 4. Run markdown linting

Before committing, verify all markdown files pass linting:

```bash
npx markdownlint-cli2 "**/*.md"
```

If errors are found:

1. Try auto-fix: `npx markdownlint-cli2 --fix "**/*.md"`
2. Manually fix remaining errors (especially MD040 - missing language tags on code blocks)
3. Re-run to verify: `npx markdownlint-cli2 "**/*.md"`
4. Stage any fixed files: `git add <fixed-files>`

Common fixes:

- Add language tags: `` ```text ``, `` ```c ``, `` ```bash ``, `` ```markdown ``
- For nested code blocks, use 4 backticks for outer fence
- Ensure tables have consistent column counts

**CRITICAL — Never bypass quality checks:**

- **NEVER** disable lint rules to make errors pass
- **NEVER** modify `.markdownlint-cli2.jsonc` to relax requirements
- **NEVER** remove or disable CI workflows to unblock the PR
- **ALWAYS** fix the underlying markdown formatting issue

If linting fails, the markdown MUST be fixed. Quality checks are non-negotiable.

### 5. Write a descriptive commit message

Format:

```text
Add Lesson NN — Title

Introduces [key concept] with a [description of what the lesson builds].

New APIs covered:
- SDL_SomeNewAPI — what it does
- SDL_AnotherAPI — what it does

Files added:
- lessons/NN-name/main.c — [brief description]
- lessons/NN-name/README.md — lesson documentation
- lessons/NN-name/shaders/*.hlsl — [shader description]
- .claude/skills/<topic>/SKILL.md — reusable skill for [concept]

Also updates root README.md, PLAN.md, and CMakeLists.txt to integrate
the new lesson.

Co-Authored-By: Claude Sonnet 4.5 <noreply@anthropic.com>
```

Present the commit message to the user for approval before committing.

### 6. Commit and push

```bash
git commit -m "$(cat <<'EOF'
[commit message from step 4]
EOF
)"
git push -u origin lesson-NN-name
```

### 7. Create a pull request

Use `gh pr create` with a structured description:

```bash
gh pr create --title "Add Lesson NN — Title" --body "$(cat <<'EOF'
## Summary
Adds Lesson NN teaching [key concept].

This lesson shows how to [what the user builds], introducing:
- **[API/concept 1]** — [why it matters]
- **[API/concept 2]** — [why it matters]

## What's included
- ✅ Lesson NN implementation (`lessons/NN-name/`)
- ✅ HLSL shaders with SPIRV/DXIL bytecodes
- ✅ Comprehensive README with exercises
- ✅ Reusable skill (`.claude/skills/<topic>/`)
- ✅ Screenshot in README
- ✅ Updated project docs (README, PLAN, CMakeLists)

## Test plan
- [x] Builds on Windows (MSVC) with Vulkan backend
- [ ] Tested on macOS (Metal) — *needs verification*
- [ ] Tested on Linux (Vulkan) — *needs verification*
- [x] Shaders compile to both SPIRV and DXIL
- [x] Runs without errors, displays expected result

🤖 Generated with [Claude Code](https://claude.com/claude-code)
EOF
)"
```

Present the PR URL to the user when done.

## Notes

- This skill does NOT auto-merge — the user or maintainer must review the PR
- If the lesson builds on experimental features, mention that in the PR description
- Always test that the lesson builds and runs before creating the PR
- The branch name should match the lesson directory name for consistency
