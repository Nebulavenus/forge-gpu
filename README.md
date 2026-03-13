# forge-gpu

forge-gpu aims to be a helpful guide to developing real-time graphics with
SDL3's GPU API and building games. The project is written using C99, and
features standalone lessons, tracks covering rendering, math, engine
fundamentals, UI, physics, and asset pipelines.

The lessons build on shared, header-only C libraries, a Python asset
pipeline, and include reusable AI skills.
SDL3 GPU handles the platform abstraction over Vulkan, D3D12, and Metal.

<table>
<tr>
<td align="center" valign="top">
<a href="lessons/gpu/02-first-triangle/"><img src="lessons/gpu/02-first-triangle/assets/screenshot.png" width="280" alt="First Triangle" /></a><br />
First Triangle
</td>
<td align="center" valign="top">
<a href="lessons/gpu/23-point-light-shadows/"><img src="lessons/gpu/23-point-light-shadows/assets/screenshot.png" width="280" alt="Point Light Shadows" /></a><br />
Point Light Shadows
</td>
<td align="center" valign="top">
<a href="lessons/gpu/30-planar-reflections/"><img src="lessons/gpu/30-planar-reflections/assets/screenshot.png" width="280" alt="Planar Reflections" /></a><br />
Planar Reflections
</td>
</tr>
<tr>
<td align="center" valign="top">
<a href="lessons/gpu/32-skinning-animations/"><img src="lessons/gpu/32-skinning-animations/assets/screenshot.png" width="280" alt="Skinning Animations" /></a><br />
Skinning Animations
</td>
<td align="center" valign="top">
<a href="lessons/gpu/28-ui-rendering/"><img src="lessons/gpu/28-ui-rendering/assets/screenshot.png" width="280" alt="UI Rendering" /></a><br />
UI Rendering
</td>
<td align="center" valign="top">
<a href="lessons/gpu/38-indirect-drawing/"><img src="lessons/gpu/38-indirect-drawing/assets/screenshot.png" width="280" alt="Indirect Drawing" /></a><br />
Indirect Drawing
</td>
</tr>
</table>

See the [GPU track](lessons/gpu/) for all lessons.

## Topics covered

Following along the GPU track you can build programs covering shadow
mapping, screen-space reflections, bloom, SSAO, skeletal animation, stencil
portals, and deferred decals. Lessons aim to
introduce a new concept, sometimes more, building on the previous ones.

Other tracks fill in the foundations: math topics including transform and
lighting calculations, how CMake and C work, how to build a UI system
from font parsing to interactive controls, and how to process raw assets into
GPU optimized formats.

## What you'll get

Header-only C libraries
([math](common/math/),
[arena](common/arena/),
[OBJ](common/obj/),
[glTF](common/gltf/),
[UI](common/ui/),
[physics](common/physics/),
[procedural shapes](common/shapes/),
[rasterization](common/raster/),
[pipeline loader](common/pipeline/),
[capture](common/capture/),
[audio](common/audio/),
[scene](common/scene/)),
a [Python asset pipeline](pipeline/), and
[Claude Code skills](.claude/skills/) that encode lesson patterns for
AI-assisted building. All tested, documented, and usable in your own projects.

## Getting started

### Prerequisites

- **CMake 3.24+**
- **A C compiler** (MSVC, GCC, or Clang)
- **A GPU** with Vulkan, Direct3D 12, or Metal support
- **Python 3.10+** (for helper scripts and the asset pipeline)
- **Git LFS** — binary assets and animated GIFs are stored with Git Large File Storage

SDL3 is fetched as part of running the build.

After cloning, run `git lfs install && git lfs pull` to fetch large files.

### Build and run

```bash
cmake -B build
cmake --build build --config Debug
python scripts/run.py 01                  # by number
python scripts/run.py first-triangle      # by name
python scripts/run.py                     # list all lessons
```

Run `python scripts/setup.py` to verify your environment and diagnose
missing tools. If you run into build problems, see the
[build guide](docs/building.md) — or work through the
[engine track](lessons/engine/), which covers CMake, compilers, and
common errors from the ground up.

## Lesson tracks

Tracks are specialized to specific topics. They cross-reference each other — GPU
lessons link to helpful math lessons, engine lessons explain the build
system and other relevant topics.

- **[GPU](lessons/gpu/)** — Rendering from a triangle to shadow maps, reflections, animation, and stencil effects
- **[Math](lessons/math/)** — Vectors, matrices, quaternions, projections, color spaces, noise, Bézier curves
- **[Engine](lessons/engine/)** — CMake, C fundamentals, debugging, RenderDoc, git
- **[UI](lessons/ui/)** — Immediate-mode UI from TTF parsing to draggable windows and developer tools
- **[Physics](lessons/physics/)** — Particles, rigid bodies, collisions, constraints
- **[Asset Pipeline](lessons/assets/)** — Texture compression, mesh optimization, procedural geometry, asset bundles, animation pipeline

## Using Claude Code

The lessons, libraries, and skills are structured so
[Claude Code](https://claude.ai/code) can navigate them.

While learning:

- *"What does SDL_ClaimWindowForGPUDevice actually do?"*
- *"Why do we need a transfer buffer to upload vertex data?"*
- *"Explain the dot product and show me the math lesson"*

While building:

- *"Use the forge-sdl-gpu-setup skill to create an SDL GPU application"*
- *"Add a rotating quad using the math library"*
- *"Help me add textures to my renderer"*

## License

[zlib](LICENSE) — This project uses same license used by SDL.
