# forge_arena.h — Arena (Bump) Allocator

Header-only arena allocator for batch-lifetime memory management.

## Usage

```c
#include "arena/forge_arena.h"

ForgeArena arena = forge_arena_create(0);  /* 0 = default block size */
int *data = forge_arena_alloc(&arena, 100 * sizeof(int));
if (!data) { /* handle allocation failure */ }
/* ... use data — no individual free needed ... */
forge_arena_destroy(&arena);  /* frees everything at once */
```

## API

| Function | Description |
|----------|-------------|
| `forge_arena_create(size)` | Create arena (0 = default block size) |
| `forge_arena_destroy(arena)` | Free all blocks |
| `forge_arena_alloc(arena, size)` | Allocate with default alignment (`FORGE_ARENA_DEFAULT_ALIGN`) |
| `forge_arena_alloc_aligned(arena, size, align)` | Allocate with explicit alignment |
| `forge_arena_reset(arena)` | Reuse memory without freeing blocks |
| `forge_arena_used(arena)` | Bytes currently in use |
| `forge_arena_capacity(arena)` | Total backing memory |

All allocations are zero-initialized. The arena grows automatically by
appending new blocks when the current block is full.

## Dependencies

- SDL3 (for `SDL_malloc`, `SDL_free`, `SDL_memset`, `SDL_Log`)

## See also

- [Engine Lesson 12 — Memory Arenas](../../lessons/engine/12-memory-arenas/)
- [Engine Lesson 04 — Pointers & Memory](../../lessons/engine/04-pointers-and-memory/)
