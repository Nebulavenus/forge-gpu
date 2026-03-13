# tools/common

Shared helpers for forge-gpu C tools.

## binary_io.h

Header-only library of static inline functions for writing little-endian
values to SDL I/O streams. Used by the mesh, animation, and scene export
tools.

### Functions

| Function | Description |
|----------|-------------|
| `write_u32_le` | Write a `uint32_t` in little-endian byte order |
| `write_i32_le` | Write an `int32_t` in little-endian byte order |
| `write_float_le` | Write a `float` in little-endian byte order |

### Usage

Include the header directly — no separate `.c` file needed:

```c
#include "tools/common/binary_io.h"

SDL_IOStream *io = SDL_IOFromFile("output.bin", "wb");
write_u32_le(io, 42);
write_float_le(io, 3.14f);
SDL_CloseIO(io);
```
