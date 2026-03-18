# forge_containers.h — Stretchy Containers

Header-only dynamic arrays and hash maps using the fat-pointer pattern.
Every container is a plain typed pointer (`int *arr = NULL`). Macros manage
the backing storage; standard C indexing works unchanged.

## Usage

```c
#include "containers/forge_containers.h"

/* Dynamic array */
int *nums = NULL;
forge_arr_append(nums, 10);
forge_arr_append(nums, 20);
SDL_Log("len=%td val=%d", forge_arr_length(nums), nums[0]);
forge_arr_free(nums);

/* Hash map */
struct { int key; float value; } *map = NULL;
forge_hm_put(map, 42, 3.14f);
float v = forge_hm_get(map, 42);
forge_hm_free(map);

/* String map (strdup mode) */
struct { char *key; int value; } *smap = NULL;
forge_shm_init_strdup(smap);
forge_shm_put(smap, "hello", 1);
int sv = forge_shm_get(smap, "hello");
forge_shm_free(smap);
```

## API — Dynamic Arrays (`forge_arr_*`)

| Macro | Description |
|-------|-------------|
| `forge_arr_append(a, val)` | Append one element; grows if needed |
| `forge_arr_length(a)` | Element count (signed `ptrdiff_t`; 0 for NULL) |
| `forge_arr_length_unsigned(a)` | Element count (unsigned `size_t`) |
| `forge_arr_capacity(a)` | Allocated capacity |
| `forge_arr_pop(a)` | Remove and return last element (no bounds check) |
| `forge_arr_last(a)` | Return last element without removing (no bounds check) |
| `forge_arr_insert_at(a, i, val)` | Insert at index, shifting right |
| `forge_arr_insert_n_at(a, i, n)` | Insert n uninitialized slots at index |
| `forge_arr_delete_at(a, i)` | Delete at index, shifting left |
| `forge_arr_delete_n_at(a, i, n)` | Delete n elements at index |
| `forge_arr_swap_remove(a, i)` | O(1) unordered removal (swap with last) |
| `forge_arr_grow_by_ptr(a, n)` | Append n slots, return pointer to first |
| `forge_arr_grow_by_index(a, n)` | Append n slots, return index of first |
| `forge_arr_set_length(a, n)` | Set length (grows if needed, never shrinks allocation) |
| `forge_arr_set_capacity(a, n)` | Reserve capacity without changing length |
| `forge_arr_free(a)` | Free all memory, set pointer to NULL |

## API — Hash Maps (`forge_hm_*`)

Declare a pointer to a struct with a `key` field (and optionally `value`):

```c
struct { int key; float value; } *map = NULL;
```

| Macro | Description |
|-------|-------------|
| `forge_hm_put(m, key, val)` | Insert or update key-value pair |
| `forge_hm_put_struct(m, entry)` | Insert or update from a complete struct |
| `forge_hm_get(m, key)` | Look up value (returns default on miss) |
| `forge_hm_get_struct(m, key)` | Look up full struct |
| `forge_hm_get_ptr(m, key)` | Pointer to entry (pointer to default on miss) |
| `forge_hm_get_ptr_or_null(m, key)` | Pointer to entry (NULL on miss) |
| `forge_hm_find_index(m, key)` | Index of entry (>= 0), or -1 on miss |
| `forge_hm_remove(m, key)` | Remove entry; returns 1 if deleted, 0 if absent |
| `forge_hm_length(m)` | Entry count (signed; 0 for NULL) |
| `forge_hm_set_default(m, val)` | Set default value returned on missed lookups |
| `forge_hm_set_default_struct(m, entry)` | Set default struct |
| `forge_hm_get_ts(m, key, tmp)` | Thread-safe get (uses external temp variable) |
| `forge_hm_iter(m, i)` | Iterate: `i` runs 0..length-1; entry is `m[i + 1]` |
| `forge_hm_free(m)` | Free all memory, set pointer to NULL |

## API — String Maps (`forge_shm_*`)

Same as hash maps but keyed by `char *`. Three string ownership modes:

| Macro | Description |
|-------|-------------|
| `forge_shm_init_strdup(m)` | Initialize in strdup mode (library copies keys) |
| `forge_shm_init_arena(m)` | Initialize in arena mode (bulk key allocation) |
| `forge_shm_put(m, key, val)` | Insert or update |
| `forge_shm_put_struct(m, entry)` | Insert or update from struct |
| `forge_shm_get(m, key)` | Look up value |
| `forge_shm_get_struct(m, key)` | Look up full struct |
| `forge_shm_get_ptr(m, key)` | Pointer to entry (default on miss) |
| `forge_shm_get_ptr_or_null(m, key)` | Pointer to entry (NULL on miss) |
| `forge_shm_find_index(m, key)` | Index or -1 |
| `forge_shm_remove(m, key)` | Remove; returns 1 or 0 |
| `forge_shm_length(m)` | Entry count |
| `forge_shm_set_default(m, val)` | Set default value |
| `forge_shm_iter(m, i)` | Iterate entries |
| `forge_shm_free(m)` | Free all memory (frees key copies in strdup/arena mode) |

Without `init_strdup` or `init_arena`, string maps default to user-managed
mode — the caller is responsible for keeping key strings alive.

## Preconditions

- **Lvalues only**: the first argument to every macro must be a plain pointer
  variable (macros reassign it on growth).
- **No side effects in arguments**: macro arguments may be evaluated more than
  once. Use local variables for expressions with side effects.
- **`forge_arr_pop` / `forge_arr_last`**: require `length > 0`. No bounds
  check is performed.
- **`forge_arr_insert_at` / `forge_arr_delete_at`** (and `_n_at` variants,
  `forge_arr_swap_remove`): the index must be in range (`0 <= i < length`).
  No bounds check is performed — an out-of-range index causes undefined
  behavior via `SDL_memmove` with a wrapped size.
- **Pointer invalidation**: any growth operation may reallocate. All prior
  pointers into the container become invalid.
- **Struct key padding**: non-string keys are hashed and compared byte-wise.
  Zero-initialize struct keys (e.g. `SDL_memset`) so padding bytes are
  deterministic.
- **Float keys**: `+0.0` and `-0.0` are different keys (different bit
  patterns). `NaN` with the same bit pattern is findable. `INFINITY` works
  as expected.
- **NULL string keys**: rejected safely (logged, not inserted).
- **Per-TU seed**: the global hash seed is `static`, so each translation unit
  that includes the header gets an independent seed sequence. In multi-file
  projects, hash maps created in different `.c` files may share seed values.
- **Thread safety**: all mutating operations (put, remove, free, and first-use
  lazy initialization) require external synchronization. The `_ts` get variants
  are safe for concurrent readers with no active writer, but the map must
  already be initialized (non-NULL) before concurrent reads begin — first-use
  lazy init is itself a mutation.

## Design

- **Header-only** — all functions are `static`, no separate compilation unit
- **C99** — uses `__typeof__` on GCC/Clang for rvalue key support.
  The hash map API (`forge_hm_*` / `forge_shm_*`) requires `__typeof__`
  (GCC/Clang) or `decltype` (MSVC in C++ mode) — it is not available on
  MSVC in C mode
- **SDL-only** — `SDL_realloc`, `SDL_free`, `SDL_memmove`, `SDL_memcmp`,
  `SDL_strcmp`, `SDL_strlen`, `SDL_memset`, `SDL_memcpy`, `SDL_Log`.
  No `<stdlib.h>`, `<string.h>`, or `<math.h>`
- **Naming** — `forge_arr_*` / `forge_hm_*` / `forge_shm_*` for public API;
  `forge_containers__` (double underscore) for internals
- **NULL-safe queries** — `forge_arr_length(NULL)` returns 0;
  `forge_hm_get(NULL, k)` returns the default value;
  `forge_arr_free(NULL)` is a no-op
- **Overflow-safe** — allocation size overflow is checked before `SDL_realloc`;
  grow failures are guarded in macros
- **Tested** — 68 tests covering arrays, hash maps, string maps, thread-safe
  gets, IEEE 754 edge cases, deletion stress, and invariant validation in
  `tests/containers/`

## Dependencies

- SDL3 (for `SDL_realloc`, `SDL_free`, `SDL_malloc`, `SDL_memmove`,
  `SDL_memset`, `SDL_memcmp`, `SDL_strcmp`, `SDL_strlen`, `SDL_Log`)

## Compile-time options

| Define | Effect |
|--------|--------|
| `FORGE_CONTAINERS_BUCKET_SIZE` | Bucket slot count: 8 (default) or 4 |
| `FORGE_CONTAINERS_SIPHASH_2_4` | Use spec-compliant SipHash-2-4 (slower, stronger) |
| `FORGE_CONTAINERS_STRONG_HASH` | Force SipHash for all key sizes (disable 4/8-byte fast paths) |

## See also

- [Engine Lesson 13 — Stretchy Containers](../../lessons/engine/13-stretchy-containers/)
- [Engine Lesson 12 — Memory Arenas](../../lessons/engine/12-memory-arenas/)
- [docs/stretchy-containers.md](../../docs/stretchy-containers.md) — full
  technical specification
