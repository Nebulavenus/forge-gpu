# Engine Lessons

Learn the tools and techniques for building graphics applications — the
infrastructure that makes everything else possible.

## Why Engine Lessons?

GPU lessons teach rendering. Math lessons teach the theory. But between "I
understand the code" and "I have a running application" lies a gap:

- How does CMake actually find and link libraries?
- Why does my build fail with undefined references?
- What's the difference between a static and shared library?
- How do I structure a project that grows beyond one file?
- Why does my program crash, and how do I find out where?

Engine lessons fill that gap. They teach the practical engineering that turns
source code into a working program.

## Philosophy

- **Demystify the toolchain** — Build systems, compilers, and linkers follow
  logical rules. Once you understand them, errors become solvable problems
  instead of mysterious walls.
- **Hands-on** — Every lesson includes a project you can build, break, and fix
- **Explain the errors** — Show common failure modes and how to diagnose them
- **Connected to practice** — Link to GPU and math lessons where these concepts
  appear in real use
- **Platform-aware** — Note differences between Windows, macOS, and Linux where
  they matter

## Lessons

| # | Topic | What you'll learn |
|---|-------|-------------------|
| 01 | [Intro to C](01-intro-to-c/) | Types, functions, control flow, arrays, strings, pointers, structs, dynamic memory — using SDL's cross-platform APIs |
| 02 | [CMake Fundamentals](02-cmake-fundamentals/) | Targets, properties, `add_executable`, `target_link_libraries`, generator expressions, reading build errors |
| 03 | [FetchContent & Dependencies](03-fetchcontent-dependencies/) | `FetchContent` lifecycle, version pinning with `GIT_TAG`, imported targets, adding dependencies, offline builds |
| 04 | [Pointers & Memory](04-pointers-and-memory/) | Stack vs heap, `malloc`/`free`, pointer arithmetic, `sizeof`/`offsetof`, struct padding, vertex buffer uploads |
| 05 | [Header-Only Libraries](05-header-only-libraries/) | `static inline`, include guards vs `#pragma once`, one-definition rule, how `forge_math.h` works |
| 06 | [Reading Error Messages](06-reading-error-messages/) | Build pipeline phases, compiler errors, linker errors, runtime crashes, warnings, systematic fixing strategy |
| 07 | [Using a Debugger](07-using-a-debugger/) | Breakpoints, stepping, inspecting variables, call stack, conditional breakpoints, watchpoints |

<!-- Add lessons here as they are created -->

## What you'll learn

- How CMake finds, links, and builds libraries — and what to do when it doesn't
- C language features that matter for graphics: pointers, memory layout, structs
- How to read and fix compiler, linker, and runtime errors
- Project structure patterns that scale from one file to many modules
- How to add engine features like resource managers, asset pipelines, and input
  abstraction

## Result

After working through the engine lessons you will be able to set up, build,
debug, and extend a C/CMake graphics project on any platform — and diagnose
the errors that inevitably appear along the way.

## Key concepts

- **Targets and properties** — CMake's model for executables, libraries, and
  their relationships
- **Translation units** — How the compiler turns `.c` files into object files
  and the linker combines them
- **Memory ownership** — Stack vs heap, who allocates, who frees
- **Error messages** — What the compiler/linker is actually telling you and how
  to act on it
- **Platform differences** — Where Windows, macOS, and Linux diverge in
  practice

## Building

```bash
cmake -B build
cmake --build build --config Debug

# Easy way — use the run script
python scripts/run.py engine/01              # by number
python scripts/run.py cmake-fundamentals     # by name

# Or run the executable directly
# Windows
build\lessons\engine\01-topic-name\Debug\01-topic-name.exe

# Linux / macOS
./build/lessons/engine/01-topic-name/01-topic-name
```

## Exercises

1. Break a build on purpose — remove a `target_link_libraries` call and read
   the linker error. Can you identify the missing symbol and the library it
   belongs to?
2. Add a new source file to an existing lesson and update `CMakeLists.txt` to
   compile it. What happens if you forget?
3. Use a debugger to set a breakpoint inside `SDL_AppInit`, inspect a variable,
   and step through one frame of the main loop.

## How Engine Lessons Work

Each lesson includes:

1. **Example project** — A small, focused program demonstrating the concept
2. **README** — Detailed explanation with diagrams and examples
3. **Common errors** — What goes wrong and how to fix it
4. **Cross-references** — Links to GPU/math lessons where this knowledge applies

## Topics to Cover

Engine lessons address the practical engineering side of graphics development:

### Build systems

- **CMake fundamentals** — Targets, properties, `add_executable`,
  `target_link_libraries`, generator expressions
- **FetchContent** — How forge-gpu downloads SDL automatically, and how to
  add your own dependencies
- **Compiler flags** — Debug vs Release, warnings, optimization levels

### C language

- **Pointers and memory** — Stack vs heap, `malloc`/`free`, common pitfalls
- **Header-only libraries** — How `forge_math.h` works, `static inline`,
  include guards vs `#pragma once`
- **Structs and data layout** — `offsetof`, padding, alignment (relevant to
  vertex buffer layouts in GPU lessons)

### Debugging

- **Reading error messages** — Compiler errors, linker errors, runtime crashes
- **Using a debugger** — Breakpoints, stepping, inspecting variables
- **Common bugs** — Uninitialized memory, buffer overflows, use-after-free

### Project structure

- **Organizing a multi-file project** — Headers, source files, modules
- **Shared libraries** — How `common/` works, building reusable code
- **Asset management** — Loading files at runtime, relative paths, working
  directories

## Integration with Other Lessons

Engine lessons support the GPU and math curriculum:

- **Before GPU Lesson 01**: Understanding CMake helps you modify the build
- **Before GPU Lesson 04**: Knowing how file loading works helps with textures
- **When builds fail**: Engine lessons explain what went wrong and why
- **When starting your own project**: Engine lessons teach you to set up the
  infrastructure

## Learning Path

**If you're new to C/CMake:**

1. Start with the first engine lesson to understand the build system
2. Then begin GPU Lesson 01 with confidence
3. Return to engine lessons when you hit toolchain problems

**If you have C experience but are new to CMake:**

- Jump to CMake-specific lessons
- Skim C lessons for platform-specific details you might not know

**If you're debugging a problem:**

- Check the "Common errors" section of relevant engine lessons
- Engine lessons explain the error messages you're seeing

## Adding New Engine Lessons

Use the `/engine-lesson` skill:

```bash
/engine-lesson 01 cmake-fundamentals "CMake targets, properties, and linking"
```

This creates:

- A new engine lesson with example project
- README with explanation and common errors
- Cross-references to GPU/math lessons

See [.claude/skills/engine-lesson/](../../.claude/skills/engine-lesson/) for
details.

---

**Remember:** The build system, compiler, and debugger are tools — not obstacles.
Understanding them makes every other lesson easier and every project you build
more solid.
