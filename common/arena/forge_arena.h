/*
 * forge_arena.h — Header-only arena (bump) allocator for forge-gpu
 *
 * An arena allocator provides fast, batch-lifetime memory management.
 * Allocations are pointer bumps (a single addition), and all memory is
 * freed at once when the arena is destroyed or reset.  No individual
 * free calls are needed or possible — this is the key advantage.
 *
 * Games and asset loaders benefit from arenas because their memory
 * often has clear batch lifetimes:
 *
 *   - Application lifetime  — allocated at startup, freed at shutdown
 *   - Level lifetime        — allocated on level load, freed on unload
 *   - Frame lifetime        — allocated each frame, reset at frame end
 *   - Asset load lifetime   — allocated during a parse, freed together
 *
 * This implementation uses a linked list of fixed-size blocks.  When
 * the current block is full, a new block is allocated automatically.
 * All pointers remain valid until the arena is destroyed or reset.
 *
 * Dependencies:
 *   - SDL3 (for SDL_malloc, SDL_free, SDL_memset, SDL_Log)
 *
 * Usage:
 *   #include "arena/forge_arena.h"
 *
 *   ForgeArena arena = forge_arena_create(0);  // 0 = default block size
 *   int *data = forge_arena_alloc(&arena, 100 * sizeof(int));
 *   // ... use data — no need to free it individually ...
 *   forge_arena_destroy(&arena);  // frees everything at once
 *
 * See: lessons/engine/12-memory-arenas/ for a full tutorial
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_ARENA_H
#define FORGE_ARENA_H

#include <SDL3/SDL.h>
#include <stddef.h>  /* size_t, max_align_t */
#include <stdint.h>  /* uintptr_t */

/* ── Configuration ───────────────────────────────────────────────────────── */

/* Default block size when the caller passes 0 to forge_arena_create().
 * 64 KB is a reasonable default: large enough for most asset loads,
 * small enough to avoid waste for light use. */
#define FORGE_ARENA_DEFAULT_BLOCK_SIZE (64 * 1024)

/* Default alignment for forge_arena_alloc().  8 bytes is sufficient
 * for int, float, double, and pointers on both 32-bit and 64-bit
 * platforms.  On non-MSVC 64-bit targets, long double may require
 * 16-byte alignment — use forge_arena_alloc_aligned() for that case.
 * MSVC C99 does not provide max_align_t, so we use a fixed value. */
#define FORGE_ARENA_DEFAULT_ALIGN 8

/* ── Types ───────────────────────────────────────────────────────────────── */

/* A single contiguous block of memory.  Blocks form a singly-linked list
 * so the arena can grow without invalidating earlier allocations. */
typedef struct ForgeArenaBlock {
    struct ForgeArenaBlock *next;  /* next block in chain (NULL = last) */
    size_t capacity;               /* usable bytes after the header */
    size_t used;                   /* bytes consumed so far */
    /* Allocation data follows immediately in memory.  The first usable
     * byte is at (char *)block + sizeof(ForgeArenaBlock), aligned up
     * to FORGE_ARENA_DEFAULT_ALIGN. */
} ForgeArenaBlock;

/* The arena itself.  Small enough to live on the stack or inside another
 * struct — it is just bookkeeping; the actual memory lives in blocks. */
typedef struct ForgeArena {
    ForgeArenaBlock *first;             /* head of block list */
    ForgeArenaBlock *current;           /* block we are allocating from */
    size_t           default_block_size; /* capacity for new blocks */
    size_t           total_allocated;   /* sum of all block capacities */
    size_t           total_used;        /* sum of all blocks' used bytes */
} ForgeArena;

/* ── API ─────────────────────────────────────────────────────────────────── */

/* Create an arena with the given default block size.
 * Pass 0 for default_block_size to use FORGE_ARENA_DEFAULT_BLOCK_SIZE.
 * Returns a zero-initialized arena if the first block allocation fails. */
static ForgeArena forge_arena_create(size_t default_block_size);

/* Destroy an arena and free all its blocks.  After this call the arena
 * is zeroed and safe to ignore.  All pointers obtained from the arena
 * become invalid. */
static void forge_arena_destroy(ForgeArena *arena);

/* Allocate 'size' bytes from the arena with default alignment.
 * The returned memory is zero-initialized.
 * Returns NULL if the allocation cannot be satisfied (out of memory). */
static void *forge_arena_alloc(ForgeArena *arena, size_t size);

/* Allocate 'size' bytes with explicit alignment (must be a power of 2).
 * The returned memory is zero-initialized.
 * Returns NULL on failure. */
static void *forge_arena_alloc_aligned(ForgeArena *arena,
                                       size_t size, size_t align);

/* Reset the arena: mark all blocks as empty without freeing them.
 * Existing pointers become invalid, but the backing memory is reused.
 * Useful for per-frame arenas that are filled and drained every frame. */
static void forge_arena_reset(ForgeArena *arena);

/* Return the total number of bytes currently in use across all blocks. */
static size_t forge_arena_used(const ForgeArena *arena);

/* Return the total capacity (allocated backing memory) across all blocks. */
static size_t forge_arena_capacity(const ForgeArena *arena);

/* ══════════════════════════════════════════════════════════════════════════
 * Implementation (header-only — all functions are static)
 * ══════════════════════════════════════════════════════════════════════════ */

/* ── Internal helpers ────────────────────────────────────────────────────── */

/* Align 'value' up to the next multiple of 'align'.
 * 'align' must be a power of two. */
static size_t forge_arena__align_up(size_t value, size_t align)
{
    return (value + (align - 1)) & ~(align - 1);
}

/* Compute the usable data offset inside a block.  The data region starts
 * after the ForgeArenaBlock header, aligned to FORGE_ARENA_DEFAULT_ALIGN. */
static size_t forge_arena__header_size(void)
{
    return forge_arena__align_up(sizeof(ForgeArenaBlock),
                                FORGE_ARENA_DEFAULT_ALIGN);
}

/* Return a pointer to the start of the data region in a block. */
static char *forge_arena__block_data(ForgeArenaBlock *block)
{
    return (char *)block + forge_arena__header_size();
}

/* Allocate a new block with at least 'min_capacity' usable bytes. */
static ForgeArenaBlock *forge_arena__new_block(size_t min_capacity,
                                                size_t default_size)
{
    /* Use the larger of min_capacity and default_size so we don't
     * thrash on large single allocations. */
    size_t capacity = min_capacity > default_size ? min_capacity
                                                  : default_size;
    size_t header = forge_arena__header_size();

    /* Check for overflow in header + capacity. */
    if (capacity > SIZE_MAX - header) {
        SDL_Log("forge_arena: block size overflow");
        return NULL;
    }
    size_t total  = header + capacity;

    ForgeArenaBlock *block = (ForgeArenaBlock *)SDL_malloc(total);
    if (!block) {
        SDL_Log("forge_arena: failed to allocate block of %zu bytes",
                total);
        return NULL;
    }

    block->next     = NULL;
    block->capacity = capacity;
    block->used     = 0;
    /* Zero the data region so allocations are zero-initialized. */
    SDL_memset(forge_arena__block_data(block), 0, capacity);
    return block;
}

/* ── Public API implementation ───────────────────────────────────────────── */

static ForgeArena forge_arena_create(size_t default_block_size)
{
    ForgeArena arena;
    SDL_memset(&arena, 0, sizeof(arena));

    if (default_block_size == 0) {
        default_block_size = FORGE_ARENA_DEFAULT_BLOCK_SIZE;
    }
    arena.default_block_size = default_block_size;

    ForgeArenaBlock *block = forge_arena__new_block(default_block_size,
                                                     default_block_size);
    if (block) {
        arena.first           = block;
        arena.current         = block;
        arena.total_allocated = block->capacity;
    }
    return arena;
}

static void forge_arena_destroy(ForgeArena *arena)
{
    if (!arena) return;

    ForgeArenaBlock *block = arena->first;
    while (block) {
        ForgeArenaBlock *next = block->next;
        SDL_free(block);
        block = next;
    }

    SDL_memset(arena, 0, sizeof(*arena));
}

static void *forge_arena_alloc_aligned(ForgeArena *arena,
                                       size_t size, size_t align)
{
    if (!arena || !arena->current || size == 0) return NULL;

    /* Alignment must be a power of 2 and non-zero. */
    if (align == 0 || (align & (align - 1)) != 0) {
        SDL_Log("forge_arena: invalid alignment %zu (must be power of 2)",
                align);
        return NULL;
    }

    ForgeArenaBlock *block = arena->current;
    char *data = forge_arena__block_data(block);

    /* Compute aligned offset within the current block. */
    size_t current_ptr = (size_t)(uintptr_t)(data + block->used);
    size_t aligned_ptr = forge_arena__align_up(current_ptr, align);
    size_t padding     = aligned_ptr - current_ptr;

    /* Check for overflow in padding + size. */
    if (size > SIZE_MAX - padding) return NULL;
    size_t total_need  = padding + size;

    /* Use subtraction form to avoid overflow in the comparison:
     * block->used + total_need <= block->capacity
     * becomes total_need <= block->capacity - block->used. */
    if (total_need <= block->capacity - block->used) {
        /* Fits in current block. */
        void *result = data + block->used + padding;
        block->used += total_need;
        arena->total_used += total_need;
        return result;
    }

    /* Current block is full — try reusing existing blocks in the chain
     * before allocating a new one.  After forge_arena_reset(), the block
     * chain is preserved but all blocks have used == 0.  Walking the
     * chain here lets us reuse those blocks instead of orphaning them
     * by overwriting block->next with a freshly allocated block. */
    while (block->next) {
        block = block->next;
        data = forge_arena__block_data(block);
        current_ptr = (size_t)(uintptr_t)(data + block->used);
        aligned_ptr = forge_arena__align_up(current_ptr, align);
        padding     = aligned_ptr - current_ptr;
        if (size <= SIZE_MAX - padding) {
            total_need = padding + size;
            if (total_need <= block->capacity - block->used) {
                /* Fits in this existing block. */
                arena->current = block;
                void *result = data + block->used + padding;
                block->used += total_need;
                arena->total_used += total_need;
                return result;
            }
        }
    }

    /* Exhausted all existing blocks — allocate a new one and append
     * it at the end of the chain (block is now the last block).  Pass
     * size + align - 1 so the new block has room for worst-case
     * alignment padding. */
    if (size > SIZE_MAX - (align - 1)) return NULL;
    ForgeArenaBlock *new_block = forge_arena__new_block(size + align - 1,
                                                        arena->default_block_size);
    if (!new_block) return NULL;

    block->next    = new_block;
    arena->current = new_block;
    arena->total_allocated += new_block->capacity;

    /* Fresh block is aligned to FORGE_ARENA_DEFAULT_ALIGN, so we only
     * need to handle the requested alignment. */
    data = forge_arena__block_data(new_block);
    current_ptr = (size_t)(uintptr_t)data;
    aligned_ptr = forge_arena__align_up(current_ptr, align);
    padding     = aligned_ptr - current_ptr;
    total_need  = padding + size;

    void *result = data + padding;
    new_block->used = total_need;
    arena->total_used += total_need;
    return result;
}

static void *forge_arena_alloc(ForgeArena *arena, size_t size)
{
    return forge_arena_alloc_aligned(arena, size, FORGE_ARENA_DEFAULT_ALIGN);
}

static void forge_arena_reset(ForgeArena *arena)
{
    if (!arena) return;

    ForgeArenaBlock *block = arena->first;
    while (block) {
        /* Zero the used portion for clean reuse. */
        SDL_memset(forge_arena__block_data(block), 0, block->used);
        block->used = 0;
        block = block->next;
    }

    arena->current    = arena->first;
    arena->total_used = 0;
}

static size_t forge_arena_used(const ForgeArena *arena)
{
    return arena ? arena->total_used : 0;
}

static size_t forge_arena_capacity(const ForgeArena *arena)
{
    return arena ? arena->total_allocated : 0;
}

#endif /* FORGE_ARENA_H */
