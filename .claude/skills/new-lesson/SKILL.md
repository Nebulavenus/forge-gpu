---
name: new-lesson
description: Scaffold a new forge-gpu lesson with all required files
argument-hint: [number] [name] [description]
disable-model-invocation: true
---

Create a new lesson for the forge-gpu project. The user will provide:

- **Number**: two-digit lesson number (e.g. 02)
- **Name**: short kebab-case name (e.g. first-triangle)
- **Description**: what the lesson teaches

If any of these are missing, ask the user before proceeding.

## Steps

1. **Determine what math is needed**:
   - Will this lesson use vectors (positions, colors, directions)?
   - Will it use matrices (transformations, rotations)?
   - Check if the math library (`common/math/forge_math.h`) has what you need
   - If new math operations are needed, use `/math-lesson` to add them first

2. **Create the lesson directory**: `lessons/gpu/$ARGUMENTS[0]-$ARGUMENTS[1]/`

3. **Create main.c** using the SDL callback architecture:
   - `#define SDL_MAIN_USE_CALLBACKS 1` before includes
   - Include required headers:

     ```c
     #include <SDL3/SDL.h>
     #include <SDL3/SDL_main.h>
     #include <stddef.h>    /* offsetof */
     #include "math/forge_math.h"  /* ALWAYS include the math library */
     ```

   - `SDL_AppInit` — create GPU device, window, claim swapchain, allocate app_state
   - `SDL_AppEvent` — handle SDL_EVENT_QUIT (return SDL_APP_SUCCESS)
   - `SDL_AppIterate` — per-frame GPU work
   - `SDL_AppQuit` — cleanup in reverse order, SDL_free the app_state
   - Use `SDL_calloc` / `SDL_free` for app_state (not malloc/free)
   - Every SDL GPU call gets error handling with `SDL_Log` and descriptive messages
   - No magic numbers — `#define` or `enum` everything
   - Extensive comments explaining *why*, not just *what*
   - Use C99, matching SDL's own style
   - **Use math library types for all math operations** (see "Using the Math Library" below)

4. **Create CMakeLists.txt**:

   ```cmake
   add_executable(NN-name WIN32 main.c)
   target_include_directories(NN-name PRIVATE ${FORGE_COMMON_DIR})
   target_link_libraries(NN-name PRIVATE SDL3::SDL3)

   add_custom_command(TARGET NN-name POST_BUILD
       COMMAND ${CMAKE_COMMAND} -E copy_if_different
           $<TARGET_FILE:SDL3::SDL3-shared>
           $<TARGET_FILE_DIR:NN-name>
   )
   ```

5. **Create README.md** following this structure:
   - `# Lesson NN — Title`
   - `## What you'll learn` — bullet list of concepts
   - `## Result` — describe what the reader will see (add screenshot placeholder)
   - `## Key concepts` — explain each new API concept introduced
   - `## Math` — if the lesson uses math operations, link to relevant math lessons
   - `## Building` — standard cmake build instructions
   - `## AI skill` — mention the matching skill created in step 10, with a
     relative link to `.claude/skills/<topic>/SKILL.md`, the `/skill-name`
     invocation, and a note that users can copy it into their own projects
   - `## Exercises` — 3-4 exercises that extend the lesson

6. **Update the root CMakeLists.txt**: add `add_subdirectory(lessons/NN-name)` under "GPU Lessons"

7. **Update README.md**: add a row to the GPU Lessons table

8. **Update PLAN.md**: check off the lesson if it was listed, or add it

9. **Build and test**: run `cmake --build build --config Debug` and verify it runs

10. **Create a matching skill**: add `.claude/skills/<topic>/SKILL.md` that
    distills the lesson into a reusable pattern with YAML frontmatter

11. **Run markdown linting**: Use the `/markdown-lint` skill to verify all markdown files pass linting:

    ```bash
    npx markdownlint-cli2 "**/*.md"
    ```

    If errors found, auto-fix first then manually fix remaining issues (especially MD040 language tags)

## Using the Math Library

**CRITICAL:** GPU lessons must use the math library (`common/math/forge_math.h`) for all math operations. Never write bespoke math in GPU lessons.

### Vertex data structures

**Always use math library types for vertex attributes:**

```c
typedef struct Vertex {
    vec2 position;   /* NOT float x, y */
    vec3 color;      /* NOT float r, g, b */
} Vertex;
```

**HLSL mapping:**

- `vec2` in C → `float2` in HLSL shader
- `vec3` in C → `float3` in HLSL shader
- `vec4` in C → `float4` in HLSL shader
- Memory layout is identical — no conversion needed

### Vertex attribute setup

```c
vertex_attributes[0].offset = offsetof(Vertex, position);  /* NOT offsetof(Vertex, x) */
vertex_attributes[1].offset = offsetof(Vertex, color);     /* NOT offsetof(Vertex, r) */
```

### Initializing vertex data

Use designated initializers with math library types:

```c
static const Vertex vertices[] = {
    { .position = { 0.0f, 0.5f }, .color = { 1.0f, 0.0f, 0.0f } },
    /* ... */
};
```

Or use constructor functions explicitly:

```c
Vertex v;
v.position = vec2_create(0.0f, 0.5f);
v.color = vec3_create(1.0f, 0.0f, 0.0f);
```

### Common math operations

**Transformations:**

```c
mat4 rotation = mat4_rotate_z(angle);
mat4 translation = mat4_translate(vec3_create(x, y, z));
mat4 scale = mat4_scale(vec3_create(sx, sy, sz));
```

**Vector operations:**

```c
vec3 sum = vec3_add(a, b);
vec3 normalized = vec3_normalize(v);
float distance = vec3_length(vec3_sub(target, position));
```

### When you need new math

If the math library doesn't have an operation you need:

1. Check `common/math/forge_math.h` — might already exist
2. Check `lessons/math/` — might have a lesson teaching it
3. Use `/math-lesson` to add it:

   ```bash
   /math-lesson 02 quaternions "Quaternion rotations"
   ```

4. This creates: math lesson + library update + documentation

### Cross-referencing math lessons

In the lesson README, add a "Math" section linking to relevant math lessons:

```markdown
## Math

This lesson uses:
- **Vectors** — [Math Lesson 01](../math/01-vectors/) for positions and colors
- **Matrices** — [Math Lesson 02](../math/02-matrices/) for rotations
```

## Code style reminders

- SDL naming: `SDL_PrefixedNames` for public, `lowercase_snake` for local
- The `app_state` struct holds all state passed between callbacks
- Build on previous lessons — reference what was introduced before
- Each lesson should introduce ONE new concept at a time
- **Always use the math library** — no bespoke math in GPU lessons
- Link to math lessons when explaining concepts
