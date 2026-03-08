# Common Libraries

Shared, header-only C libraries used across forge-gpu lessons. Each module
lives in its own subdirectory and has a detailed README with API reference.

## Modules

| Module      | Header(s)                  | Purpose                                     |
|-------------|----------------------------|---------------------------------------------|
| `math/`     | `forge_math.h`             | Vectors, matrices, quaternions, transforms  |
| `obj/`      | `forge_obj.h`              | Wavefront OBJ file parser                   |
| `gltf/`     | `forge_gltf.h`             | glTF 2.0 scene/material/hierarchy parser    |
| `ui/`       | `forge_ui.h`, `forge_ui_ctx.h`, `forge_ui_theme.h`, `forge_ui_window.h` | TTF parsing, font atlas, text layout, immediate-mode UI |
| `physics/`  | `forge_physics.h`          | Particle dynamics, rigid bodies, collisions |
| `shapes/`   | `forge_shapes.h`           | Procedural geometry (sphere, torus, etc.)   |
| `raster/`   | `forge_raster.h`           | CPU triangle rasterizer (edge functions)    |
| `capture/`  | `forge_capture.h`          | Screenshot and GIF capture utility          |

## Shared header

`forge.h` provides common utilities (argument parsing, error handling) used
by lesson `main.c` files.

## Usage

All libraries are header-only. Include the header and, where required, define
the implementation macro in exactly one translation unit:

```c
#define FORGE_MATH_IMPLEMENTATION
#include "math/forge_math.h"
```

See each module's README for detailed API documentation and usage examples.
