/*
 * Engine Lesson 12 -- Memory Arenas
 *
 * Demonstrates: Arena (bump) allocation -- the simplest and fastest
 * allocator pattern used in games and asset loaders.
 *
 * This program shows three arena lifetime patterns:
 *   1. Application lifetime  -- allocated at startup, freed at shutdown
 *   2. Per-task lifetime     -- allocated for one job, freed when done
 *   3. Per-frame lifetime    -- allocated each frame, reset at frame end
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stddef.h>
#include "arena/forge_arena.h"

/* ── Constants ──────────────────────────────────────────────────────────── */

#define DEMO_BLOCK_SIZE  4096  /* arena block size for demo 1 (bytes) */
#define SETTINGS_COUNT   8     /* number of app settings in demo 1 */
#define NAMES_COUNT      4     /* number of name pointers in demo 1 */

/* ── Types ───────────────────────────────────────────────────────────────── */

/* A vertex with position and color -- the kind of data a renderer might
 * batch-allocate from an arena. */
typedef struct Vertex {
    float x, y, z;  /* position in world space */
    float r, g, b;  /* vertex color (0..1 per channel) */
} Vertex;

/* A named entity with an ID -- the kind of data a game world might
 * allocate per level or per scene. */
typedef struct Entity {
    int   id;            /* unique identifier */
    float pos_x, pos_y;  /* position in the game world */
    char  name[32];      /* display name (null-terminated) */
} Entity;

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static void print_arena_stats(const char *label, const ForgeArena *arena)
{
    size_t used = forge_arena_used(arena);
    size_t cap  = forge_arena_capacity(arena);
    double pct  = cap > 0 ? (double)used / (double)cap * 100.0 : 0.0;
    SDL_Log("  [%s] used: %zu / %zu bytes (%.1f%%)", label, used, cap, pct);
}

/* ── Demo 1: Application lifetime ────────────────────────────────────────── */
/* Allocate configuration and lookup tables that live for the entire run. */

static void demo_app_lifetime(void)
{
    SDL_Log("--- Demo 1: Application Lifetime Arena ---");

    ForgeArena app_arena = forge_arena_create(DEMO_BLOCK_SIZE);
    if (!app_arena.first) {
        SDL_Log("  ERROR: failed to create arena");
        return;
    }
    SDL_Log("  Created arena with 4 KB default block size");

    /* Allocate a settings table. */
    int setting_count = SETTINGS_COUNT;
    float *settings = (float *)forge_arena_alloc(
        &app_arena, (size_t)setting_count * sizeof(float));
    if (!settings) {
        SDL_Log("  ERROR: allocation failed");
        forge_arena_destroy(&app_arena);
        return;
    }

    settings[0] = 1920.0f;  /* width */
    settings[1] = 1080.0f;  /* height */
    settings[2] = 60.0f;    /* target fps */
    settings[3] = 0.8f;     /* volume */
    settings[4] = 1.0f;     /* gamma */
    settings[5] = 90.0f;    /* fov */
    settings[6] = 0.1f;     /* near plane */
    settings[7] = 1000.0f;  /* far plane */

    SDL_Log("  Allocated %d settings (%.0fx%.0f, %.0f fps, FOV %.0f)",
            setting_count, settings[0], settings[1], settings[2], settings[5]);

    /* Allocate a name table. */
    int name_count = NAMES_COUNT;
    const char **names = (const char **)forge_arena_alloc(
        &app_arena, (size_t)name_count * sizeof(const char *));
    if (names) {
        names[0] = "player";
        names[1] = "enemy_a";
        names[2] = "enemy_b";
        names[3] = "item_health";
        SDL_Log("  Allocated %d name pointers", name_count);
    }

    print_arena_stats("app", &app_arena);

    /* The arena (and everything in it) is freed in one call.
     * No need to free settings or names individually. */
    forge_arena_destroy(&app_arena);
    SDL_Log("  Destroyed app arena -- all memory freed at once");
    SDL_Log("");
}

/* ── Demo 2: Per-task lifetime (asset loading) ───────────────────────────── */
/* Simulate loading game entities from a "level file".  The arena holds
 * all level data and is destroyed when the level unloads. */

static void demo_task_lifetime(void)
{
    SDL_Log("--- Demo 2: Per-Task Lifetime Arena (Level Load) ---");

    ForgeArena level_arena = forge_arena_create(0);  /* 0 = use default size */
    if (!level_arena.first) {
        SDL_Log("  ERROR: failed to create arena");
        return;
    }
    SDL_Log("  Created level arena (default %d KB block size)",
            FORGE_ARENA_DEFAULT_BLOCK_SIZE / 1024);

    /* "Load" entities for this level. */
    int entity_count = 5;
    Entity *entities = (Entity *)forge_arena_alloc(
        &level_arena, (size_t)entity_count * sizeof(Entity));
    if (!entities) {
        SDL_Log("  ERROR: allocation failed");
        forge_arena_destroy(&level_arena);
        return;
    }

    const char *entity_names[] = {
        "player", "guard_01", "guard_02", "chest", "door"
    };
    for (int i = 0; i < entity_count; i++) {
        entities[i].id = i;
        entities[i].pos_x = (float)(i * 10);
        entities[i].pos_y = (float)(i * 5);
        SDL_strlcpy(entities[i].name, entity_names[i],
                     sizeof(entities[i].name));
    }

    SDL_Log("  Loaded %d entities:", entity_count);
    for (int i = 0; i < entity_count; i++) {
        SDL_Log("    [%d] '%s' at (%.0f, %.0f)",
                entities[i].id, entities[i].name,
                entities[i].pos_x, entities[i].pos_y);
    }

    /* Allocate spawn points for this level. */
    int spawn_count = 3;
    float *spawn_x = (float *)forge_arena_alloc(
        &level_arena, (size_t)spawn_count * sizeof(float));
    float *spawn_y = (float *)forge_arena_alloc(
        &level_arena, (size_t)spawn_count * sizeof(float));
    if (spawn_x && spawn_y) {
        spawn_x[0] = 0.0f;   spawn_y[0] = 0.0f;
        spawn_x[1] = 50.0f;  spawn_y[1] = 25.0f;
        spawn_x[2] = 100.0f; spawn_y[2] = 50.0f;
        SDL_Log("  Loaded %d spawn points", spawn_count);
    }

    print_arena_stats("level", &level_arena);

    /* Level unload: one call frees entities, spawn points, and everything
     * else allocated from this arena.  Compare this to tracking and
     * freeing each allocation individually. */
    forge_arena_destroy(&level_arena);
    SDL_Log("  Unloaded level -- all level memory freed at once");
    SDL_Log("");
}

/* ── Demo 3: Per-frame lifetime (temporary scratch memory) ───────────────── */
/* Each frame allocates temporary data (vertex batches, sort keys, etc.)
 * and resets the arena at the end of the frame.  The backing memory is
 * reused without new allocations. */

static void demo_frame_lifetime(void)
{
    SDL_Log("--- Demo 3: Per-Frame Lifetime Arena (Scratch Memory) ---");

    ForgeArena frame_arena = forge_arena_create(4096);
    if (!frame_arena.first) {
        SDL_Log("  ERROR: failed to create arena");
        return;
    }
    int frame_count = 3;

    for (int frame = 0; frame < frame_count; frame++) {
        /* Reset at the start of each frame -- memory is reused. */
        forge_arena_reset(&frame_arena);

        /* Allocate a batch of vertices for this frame's UI. */
        int vert_count = 6 + frame * 3;  /* varies per frame */
        Vertex *verts = (Vertex *)forge_arena_alloc(
            &frame_arena, (size_t)vert_count * sizeof(Vertex));
        if (!verts) {
            SDL_Log("  Frame %d: vertex allocation failed", frame);
            continue;
        }

        for (int v = 0; v < vert_count; v++) {
            verts[v].x = (float)v * 10.0f;
            verts[v].y = (float)v * 5.0f;
            verts[v].z = 0.0f;
            verts[v].r = 1.0f;
            verts[v].g = (float)v / (float)vert_count;
            verts[v].b = 0.0f;
        }

        /* Allocate sort keys for depth sorting. */
        int *sort_keys = (int *)forge_arena_alloc(
            &frame_arena, (size_t)vert_count * sizeof(int));
        if (sort_keys) {
            for (int i = 0; i < vert_count; i++) {
                sort_keys[i] = vert_count - i;
            }
        }

        SDL_Log("  Frame %d: %d vertices, %d sort keys",
                frame, vert_count, sort_keys ? vert_count : 0);
        print_arena_stats("frame", &frame_arena);

        /* End of frame -- no free calls needed.  The reset at the top
         * of the next iteration reclaims all memory. */
    }

    SDL_Log("  Capacity never grew: %zu bytes (reused each frame)",
            forge_arena_capacity(&frame_arena));

    forge_arena_destroy(&frame_arena);
    SDL_Log("  Destroyed frame arena");
    SDL_Log("");
}

/* ── Demo 4: Arena growth ────────────────────────────────────────────────── */
/* Show that the arena grows automatically when the initial block is full. */

static void demo_growth(void)
{
    SDL_Log("--- Demo 4: Automatic Growth ---");

    /* Start with a tiny 256-byte arena. */
    ForgeArena arena = forge_arena_create(256);
    if (!arena.first) {
        SDL_Log("  ERROR: failed to create arena");
        return;
    }
    SDL_Log("  Created arena with 256-byte blocks");
    print_arena_stats("initial", &arena);

    /* Allocate more data than fits in one block.
     * Each Entity is sizeof(Entity) bytes (44 on most platforms);
     * 256 / sizeof(Entity) ~ 5 entities per block, so 10 entities
     * forces at least one new block allocation. */
    for (int i = 0; i < 10; i++) {
        Entity *e = (Entity *)forge_arena_alloc(&arena, sizeof(Entity));
        if (!e) {
            SDL_Log("  ERROR: alloc failed at iteration %d", i);
            break;
        }
        e->id = i;
    }

    SDL_Log("  After 10 entity allocations:");
    print_arena_stats("grown", &arena);
    SDL_Log("  Arena grew from 256 bytes to %zu bytes as needed",
            forge_arena_capacity(&arena));

    forge_arena_destroy(&arena);
    SDL_Log("");
}

/* ── Demo 5: Comparison with malloc/free ─────────────────────────────────── */
/* Show the difference in code complexity between arena and manual alloc. */

static void demo_comparison(void)
{
    SDL_Log("--- Demo 5: Arena vs malloc/free ---");
    SDL_Log("");
    SDL_Log("  With malloc/free (what you had before):");
    SDL_Log("    entities = SDL_malloc(5 * sizeof(Entity));");
    SDL_Log("    spawns_x = SDL_malloc(3 * sizeof(float));");
    SDL_Log("    spawns_y = SDL_malloc(3 * sizeof(float));");
    SDL_Log("    // ... use data ...");
    SDL_Log("    SDL_free(spawns_y);   // must not forget any");
    SDL_Log("    SDL_free(spawns_x);   // must not forget any");
    SDL_Log("    SDL_free(entities);   // must not double-free");
    SDL_Log("");
    SDL_Log("  With an arena:");
    SDL_Log("    arena = forge_arena_create(0);");
    SDL_Log("    entities = forge_arena_alloc(&arena, 5 * sizeof(Entity));");
    SDL_Log("    spawns_x = forge_arena_alloc(&arena, 3 * sizeof(float));");
    SDL_Log("    spawns_y = forge_arena_alloc(&arena, 3 * sizeof(float));");
    SDL_Log("    // ... use data ...");
    SDL_Log("    forge_arena_destroy(&arena);  // frees everything");
    SDL_Log("");
    SDL_Log("  Benefits:");
    SDL_Log("    - No individual free calls to track or forget");
    SDL_Log("    - Batch lifetime: all pointers invalidated together at destroy");
    SDL_Log("    - Reduced fragmentation (contiguous bump allocation per block)");
    SDL_Log("    - Allocation is a pointer bump -- faster than malloc");
    SDL_Log("    - Batch lifetimes match game patterns (level, frame, asset)");
    SDL_Log("");
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== Engine Lesson 12: Memory Arenas ===");
    SDL_Log("");

    demo_app_lifetime();
    demo_task_lifetime();
    demo_frame_lifetime();
    demo_growth();
    demo_comparison();

    SDL_Log("=== Done ===");

    SDL_Quit();
    return 0;
}
