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

1. **Create the lesson directory**: `lessons/$ARGUMENTS[0]-$ARGUMENTS[1]/`

2. **Create main.c** using the SDL callback architecture:
   - `#define SDL_MAIN_USE_CALLBACKS 1` before includes
   - `SDL_AppInit` — create GPU device, window, claim swapchain, allocate app_state
   - `SDL_AppEvent` — handle SDL_EVENT_QUIT (return SDL_APP_SUCCESS)
   - `SDL_AppIterate` — per-frame GPU work
   - `SDL_AppQuit` — cleanup in reverse order, SDL_free the app_state
   - Use `SDL_calloc` / `SDL_free` for app_state (not malloc/free)
   - Every SDL GPU call gets error handling with `SDL_Log` and descriptive messages
   - No magic numbers — `#define` or `enum` everything
   - Extensive comments explaining *why*, not just *what*
   - Use C99, matching SDL's own style

3. **Create CMakeLists.txt**:
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

4. **Create README.md** following this structure:
   - `# Lesson NN — Title`
   - `## What you'll learn` — bullet list of concepts
   - `## Result` — describe what the reader will see (add screenshot placeholder)
   - `## Key concepts` — explain each new API concept introduced
   - `## Building` — standard cmake build instructions
   - `## AI skill` — mention the matching skill created in step 9, with a
     relative link to `.claude/skills/<topic>/SKILL.md`, the `/skill-name`
     invocation, and a note that users can copy it into their own projects
   - `## Exercises` — 3-4 exercises that extend the lesson

5. **Update the root CMakeLists.txt**: add `add_subdirectory(lessons/NN-name)`

6. **Update README.md**: add a row to the Lessons table

7. **Update PLAN.md**: check off the lesson if it was listed, or add it

8. **Build and test**: run `cmake --build build --config Debug` and verify it runs

9. **Create a matching skill**: add `.claude/skills/<topic>/SKILL.md` that
   distills the lesson into a reusable pattern with YAML frontmatter

## Code style reminders

- SDL naming: `SDL_PrefixedNames` for public, `lowercase_snake` for local
- The `app_state` struct holds all state passed between callbacks
- Build on previous lessons — reference what was introduced before
- Each lesson should introduce ONE new concept at a time
