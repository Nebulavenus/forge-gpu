# Math Lessons

Learn the mathematics behind graphics and game programming.

## Why Math Lessons?

Understanding the math makes you a better graphics programmer and helps you work
more effectively with AI tools. These lessons teach **the concepts** through
hands-on demos, while the **math library** (`common/math/`) provides reusable
implementations for your projects.

## Philosophy

- **Intuition first** — Geometric meaning before formulas
- **Hands-on** — Every lesson includes a demo program you can run
- **Well-documented** — Clear explanations with visual analogies
- **Connected to practice** — Links to GPU lessons showing real-world usage
- **Build with confidence** — Understanding math helps you solve problems, not just copy code

## Lessons

| # | Topic | What you'll learn |
|---|-------|-------------------|
| 01 | [Vectors](01-vectors/) | Addition, dot/cross products, normalization, lerp |
| 02 | [Coordinate Spaces](02-coordinate-spaces/) | Model, world, view, clip, NDC, screen spaces and transformations |
| 03 | [Orthographic Projection](03-orthographic-projection/) | Orthographic vs perspective, 2D rendering, shadow maps |

*More lessons coming soon:* Matrices (deep dive), quaternions, and more.

## How Math Lessons Work

Each lesson includes:

1. **Demo program** — A small C program demonstrating the concept
2. **README** — Detailed explanation with formulas and geometric intuition
3. **Math library updates** — Implementations in `common/math/forge_math.h`
4. **Cross-references** — Links to GPU lessons using this math

### Running a lesson

```bash
cmake -B build
cmake --build build --config Debug

# Windows
build\lessons\math\01-vectors\Debug\01-vectors.exe

# Linux / macOS
./build/lessons/math/01-vectors/01-vectors
```

## The Math Library

All math operations are implemented in the **forge-gpu math library** at
`common/math/forge_math.h`. It's header-only, well-documented, and reusable.

**Quick reference:**

- `vec2`, `vec3`, `vec4` — Vectors (map to HLSL `float2/3/4`)
- `mat4` — 4×4 matrices (column-major, matches HLSL `float4x4`)
- Operations: `add`, `sub`, `scale`, `dot`, `cross`, `normalize`, `lerp`, etc.

See [common/math/README.md](../../common/math/README.md) for complete API docs.

## Integration with GPU Lessons

Math lessons teach the theory. GPU lessons show it in practice:

- **[Lesson 02 — First Triangle](../gpu/02-first-triangle/)** uses `vec2` for positions, `vec3` for colors
- **[Lesson 03 — Uniforms & Motion](../gpu/03-uniforms-and-motion/)** uses `mat4_rotate_z` for animation

When you understand both the math AND the GPU API, you can build confidently.

## Using the Math Library in Your Projects

1. **Copy the header**: `common/math/forge_math.h`
2. **Include it**: `#include "math/forge_math.h"`
3. **Use it**:

   ```c
   vec3 position = vec3_create(0.0f, 1.0f, 0.0f);
   vec3 velocity = vec3_create(1.0f, 0.0f, 0.0f);
   position = vec3_add(position, velocity);
   ```

The math library is **standalone** — no SDL or GPU dependencies. Use it anywhere.

## Learning Path

**If you're new to graphics math:**

1. Start with [01-vectors](01-vectors/) — Foundation of everything
2. Move to matrices (coming soon) — Transformations and camera
3. Then dive into GPU lessons to see it in action

**If you're building a project:**

1. Copy `.claude/skills/` into your project (AI skills for building)
2. Use the math library for all math operations
3. When you hit a concept you don't understand, read the corresponding lesson

**If you're stuck on a GPU lesson:**

- Check if there's a math lesson explaining the concept
- Run the demo to see it in isolation
- Then return to the GPU lesson with better understanding

## Adding New Math

Need a math operation that doesn't exist yet?

Use the `/math-lesson` skill (or ask Claude to use it):

```bash
/math-lesson 02 quaternions "Quaternion rotations and slerp"
```

This creates:

- A new math lesson with demo program
- Implementation in `forge_math.h`
- Documentation and cross-references

See [.claude/skills/math-lesson/](../../.claude/skills/math-lesson/) for details.

## Exercises

Each math lesson includes exercises. Work through them to:

- Reinforce concepts
- Practice using the math library
- Build intuition for how math connects to graphics

## Further Reading

- **Math library design**: [common/math/DESIGN.md](../../common/math/DESIGN.md)
- **Math library API**: [common/math/README.md](../../common/math/README.md)
- **External resource**: [3Blue1Brown — Essence of Linear Algebra](https://www.youtube.com/playlist?list=PLZHQObOWTQDPD3MizzM2xVFitgF8hE_ab)

---

**Remember:** The goal isn't to memorize formulas. The goal is to **understand what
the math does** so you can use it confidently when building graphics applications.
