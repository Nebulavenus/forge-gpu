# Large File Write Strategy for Task Agents

## The Problem

Task agents writing forge-gpu lesson `main.c` files (~1200-1800 lines) routinely
hit the `CLAUDE_CODE_MAX_OUTPUT_TOKENS` limit (32K tokens) when attempting a
single Write tool call. When this happens:

1. The Write call fails silently — the file is never created
2. All the agent's research and planning work is wasted
3. The entire lesson must be redone from scratch, including re-planning
4. This is a **fatal, unrecoverable error** for that agent

## The Solution: Chunked Write via Multiple Agents

Instead of one agent writing the entire `main.c`, decompose the file into
logical sections and assign each section to a separate agent. Each agent writes
its section to a temporary file, then a final assembly step concatenates them.

### Recommended Decomposition for a Typical Lesson main.c

Split into 3-4 agents, each producing 400-600 lines:

#### Agent A: Header + Helpers (~400 lines)

Write to `/tmp/lesson_XX_part_a.c`:

- File header comment
- `#define SDL_MAIN_USE_CALLBACKS 1`
- All `#include` directives (math, SDL, compiled shaders)
- All `#define` constants
- All `typedef struct` definitions (vertex types, uniform structs, app_state)
- `create_shader()` helper
- `upload_gpu_buffer()` helper

#### Agent B: Geometry + Init (~500 lines)

Write to `/tmp/lesson_XX_part_b.c`:

- Procedural geometry generators (cube, sphere, portal, grid, etc.)
- `SDL_AppInit()` — device/window setup, format selection, sampler creation,
  geometry upload, pipeline creation, camera/light init

#### Agent C: Event + Render + Quit (~500 lines)

Write to `/tmp/lesson_XX_part_c.c`:

- `SDL_AppEvent()` — input handling
- `SDL_AppIterate()` — full render loop (shadow pass, main pass, debug pass)
- `SDL_AppQuit()` — resource cleanup

### Assembly Step (Main Agent)

```bash
cat /tmp/lesson_XX_part_a.c /tmp/lesson_XX_part_b.c /tmp/lesson_XX_part_c.c \
    > lessons/gpu/XX-lesson-name/main.c
```

### Critical Rules for Chunked Agents

1. **Agent A runs first** — it defines all types, constants, and helpers that
   B and C depend on. Agents B and C can run in parallel after A completes.

2. **Share a contract** — The plan must specify the exact struct layouts,
   function signatures, constant names, and pipeline variable names so all
   agents produce compatible code. Include this contract verbatim in each
   agent's prompt.

3. **No redundant includes** — Only Agent A writes `#include` directives.
   Agents B and C write raw C code that continues the translation unit.

4. **Each chunk must be syntactically self-contained** — no partial functions
   split across chunks. Each chunk ends with a complete function.

5. **Target 400-600 lines per chunk** — this keeps each Write call well under
   the 32K token output limit (~15K tokens for 500 lines of commented C).

### Alternative: Single Agent with Sequential Writes

If using multiple agents feels too complex, a single agent can write the file
in sequential Write calls to different temp files, then concatenate:

```text
Prompt the agent to:
1. Write Part A (header+helpers) to /tmp/part_a.c
2. Write Part B (geometry+init) to /tmp/part_b.c
3. Write Part C (event+render+quit) to /tmp/part_c.c
4. Concatenate: cat /tmp/part_a.c /tmp/part_b.c /tmp/part_c.c > main.c
```

This works because each Write is a separate tool call with manageable output.

### Plan Template Addition

Every lesson plan that includes a `main.c` over ~800 lines MUST include:

````markdown
### main.c Decomposition

This file is too large for a single Write call. Use the chunked-write pattern:

- **Part A** (lines ~1-400): [list what goes here]
- **Part B** (lines ~401-900): [list what goes here]
- **Part C** (lines ~901-end): [list what goes here]

Agents B and C depend on Agent A's type definitions. Run A first, then B+C
in parallel.
````

## Recovery: When a Write Failure Occurs

If an agent reports `API Error: Claude's response exceeded the 32000 output
token maximum`:

1. The file was NOT written — do not assume partial content exists
2. **NEVER write a fallback or simplified version** — this destroys all the
   planning and coding work. The user will lose hours of work if you write a
   dumbed-down replacement instead of the full implementation.
3. **STOP and report the failure** to the user with the exact error
4. Re-plan using the chunked approach above
5. Re-run with properly decomposed agents
6. The agent's research reading IS useful — resume the agent if possible to
   avoid re-reading reference files
