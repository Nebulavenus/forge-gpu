/*
 * Engine Lesson 11 — Git & Version Control
 *
 * Demonstrates why version control matters through a C program that:
 *   1. Simulates the three git areas (working directory, staging, repository)
 *   2. Computes checksums to detect file changes (like git does)
 *   3. Shows how a three-way merge works at the string level
 *
 * Git is a command-line tool, not a C API — but these concepts map directly
 * to what git does internally.  Understanding them in code makes the
 * command-line behavior less mysterious.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>

/* ── Simple string hash (FNV-1a) ──────────────────────────────────────────
 * Git uses SHA-1 (160-bit) to identify every object.  We use FNV-1a here
 * because it is short enough to read in a lesson — the principle is the
 * same: feed in bytes, get out a fixed-size fingerprint.  If even one byte
 * changes, the hash changes.                                               */
#define FNV_OFFSET_BASIS 0x811C9DC5u
#define FNV_PRIME        0x01000193u

static Uint32 hash_string(const char *str)
{
    Uint32 h = FNV_OFFSET_BASIS;
    while (*str) {
        h ^= (Uint32)(unsigned char)*str++;
        h *= FNV_PRIME;
    }
    return h;
}

/* ── Simulated "file version" ─────────────────────────────────────────────
 * Represents a snapshot of file contents at a point in time.  Git stores
 * these as "blob" objects, identified by their SHA-1 hash.                 */
typedef struct FileVersion {
    const char *name;       /* filename */
    const char *contents;   /* file contents at this version */
    Uint32      checksum;   /* hash of contents (like git's object ID) */
} FileVersion;

static FileVersion version_create(const char *name, const char *contents)
{
    FileVersion v;
    v.name     = name;
    v.contents = contents;
    v.checksum = hash_string(contents);
    return v;
}

/* ── Three-area simulation ────────────────────────────────────────────────
 * Git tracks files through three areas:
 *   1. Working directory — files on disk (what you edit)
 *   2. Staging area (index) — files marked for the next commit
 *   3. Repository (HEAD) — the last committed snapshot
 *
 * This is the concept that trips up most beginners.  Our simulation makes
 * it concrete: each area holds a separate copy of the file contents, and
 * we can see exactly what changes at each step.                            */
#define MAX_FILES 8

typedef struct GitArea {
    const char *name;               /* area label ("working dir", "staging", "HEAD") */
    FileVersion files[MAX_FILES];   /* file snapshots stored in this area */
    int         count;              /* number of files currently in the area */
} GitArea;

static void area_init(GitArea *area, const char *name)
{
    area->name  = name;
    area->count = 0;
}

static void area_add(GitArea *area, FileVersion v)
{
    if (area->count < MAX_FILES) {
        area->files[area->count++] = v;
    } else {
        SDL_Log("area_add: '%s' is full (%d/%d), '%s' not added",
                area->name, area->count, MAX_FILES, v.name);
    }
}

/* Find a file by name and return its index, or -1 if not found */
static int area_find(const GitArea *area, const char *name)
{
    for (int i = 0; i < area->count; i++) {
        if (SDL_strcmp(area->files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void area_update(GitArea *area, const char *name, const char *contents)
{
    int idx = area_find(area, name);
    if (idx >= 0) {
        area->files[idx].contents = contents;
        area->files[idx].checksum = hash_string(contents);
    }
}

static void area_print(const GitArea *area)
{
    SDL_Log("  [%s]", area->name);
    for (int i = 0; i < area->count; i++) {
        SDL_Log("    %-12s  checksum: %08X  contents: \"%s\"",
                area->files[i].name,
                area->files[i].checksum,
                area->files[i].contents);
    }
}

/* ── Detect changes between two areas ─────────────────────────────────────
 * This is what 'git status' does: compare working dir vs staging (unstaged
 * changes) and staging vs HEAD (staged changes).                           */
static void detect_changes(const GitArea *from, const GitArea *to)
{
    SDL_Log("  Changes: %s -> %s", from->name, to->name);
    int changes = 0;
    for (int i = 0; i < from->count; i++) {
        int j = area_find(to, from->files[i].name);
        if (j < 0) {
            SDL_Log("    + %s (new file)", from->files[i].name);
            changes++;
        } else if (from->files[i].checksum != to->files[j].checksum) {
            SDL_Log("    ~ %s (modified)", from->files[i].name);
            changes++;
        }
    }
    if (!changes) {
        SDL_Log("    (no changes)");
    }
}

/* ── Three-way merge simulation ───────────────────────────────────────────
 * Git merges by comparing two branches against their common ancestor (the
 * "base").  For each file, there are four possible outcomes:
 *
 *   1. Neither branch changed it -> keep the base version
 *   2. Only one branch changed it -> take that branch's version
 *   3. Both branches changed it the same way -> take either (identical)
 *   4. Both branches changed it differently -> CONFLICT
 *
 * This is a simplified per-file merge (real git merges line-by-line).      */
static void three_way_merge(const FileVersion *base,
                            const FileVersion *ours,
                            const FileVersion *theirs)
{
    int ours_changed   = (base->checksum != ours->checksum);
    int theirs_changed = (base->checksum != theirs->checksum);

    SDL_Log("    File: %s", base->name);
    SDL_Log("      base:   \"%s\"  [%08X]", base->contents, base->checksum);
    SDL_Log("      ours:   \"%s\"  [%08X]", ours->contents, ours->checksum);
    SDL_Log("      theirs: \"%s\"  [%08X]", theirs->contents, theirs->checksum);

    if (!ours_changed && !theirs_changed) {
        SDL_Log("      -> Neither changed: keep base");
    } else if (ours_changed && !theirs_changed) {
        SDL_Log("      -> Only we changed: take ours");
    } else if (!ours_changed && theirs_changed) {
        SDL_Log("      -> Only they changed: take theirs");
    } else if (ours->checksum == theirs->checksum) {
        SDL_Log("      -> Both changed identically: take either");
    } else {
        SDL_Log("      -> CONFLICT: both changed differently!");
        SDL_Log("         You must resolve this manually.");
    }
}

/* ── .gitignore pattern matching (simplified) ─────────────────────────────
 * Demonstrates why certain files should not be tracked.  Real gitignore
 * supports globs, negation, and directory-only patterns — this simplified
 * version checks suffix matches, which covers the most common C/CMake
 * patterns.                                                                */
static bool should_ignore(const char *filename)
{
    /* Common patterns for C/CMake projects */
    static const char *ignore_suffixes[] = {
        ".o",     /* object files (GCC/Clang)         */
        ".obj",   /* object files (MSVC)              */
        ".exe",   /* Windows executables              */
        ".pdb",   /* MSVC debug databases             */
        ".dll",   /* Windows shared libraries         */
        ".so",    /* Linux shared libraries           */
        ".dylib", /* macOS shared libraries           */
    };
    static const char *ignore_dirs[] = {
        "build/",              /* CMake build directory          */
        ".vs/",                /* Visual Studio settings         */
        "CMakeFiles/",         /* CMake internals                */
    };
    static const char *ignore_exact[] = {
        "compile_commands.json", /* clangd / IDE integration     */
        "CMakeCache.txt",        /* CMake cache                  */
        ".DS_Store",             /* macOS Finder metadata        */
    };

    /* Check suffix matches */
    size_t flen = SDL_strlen(filename);
    for (size_t i = 0; i < SDL_arraysize(ignore_suffixes); i++) {
        size_t slen = SDL_strlen(ignore_suffixes[i]);
        if (flen >= slen &&
            SDL_strcmp(filename + flen - slen, ignore_suffixes[i]) == 0) {
            return true;
        }
    }

    /* Check directory prefixes */
    for (size_t i = 0; i < SDL_arraysize(ignore_dirs); i++) {
        size_t dlen = SDL_strlen(ignore_dirs[i]);
        if (flen >= dlen && SDL_strncmp(filename, ignore_dirs[i], dlen) == 0) {
            return true;
        }
    }

    /* Check exact matches */
    for (size_t i = 0; i < SDL_arraysize(ignore_exact); i++) {
        if (SDL_strcmp(filename, ignore_exact[i]) == 0) {
            return true;
        }
    }

    return false;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== Engine Lesson 11: Git & Version Control ===");
    SDL_Log("");

    /* ── Part 1: Content hashing ──────────────────────────────────────── */
    SDL_Log("--- 1. Content hashing (how git identifies files) ---");
    SDL_Log("");
    SDL_Log("  Git identifies every file by the hash of its contents.");
    SDL_Log("  If the contents change, the hash changes.");
    SDL_Log("  If two files have identical contents, they share a hash.");
    SDL_Log("");

    FileVersion v1 = version_create("main.c", "int main() { return 0; }");
    FileVersion v2 = version_create("main.c", "int main() { return 1; }");
    FileVersion v3 = version_create("copy.c", "int main() { return 0; }");

    SDL_Log("  %-12s  \"%s\"  -> %08X", v1.name, v1.contents, v1.checksum);
    SDL_Log("  %-12s  \"%s\"  -> %08X", v2.name, v2.contents, v2.checksum);
    SDL_Log("  %-12s  \"%s\"  -> %08X", v3.name, v3.contents, v3.checksum);
    SDL_Log("");
    SDL_Log("  main.c v1 vs v2: %s (contents differ)",
            v1.checksum == v2.checksum ? "SAME" : "DIFFERENT");
    SDL_Log("  main.c v1 vs copy.c: %s (identical contents)",
            v1.checksum == v3.checksum ? "SAME" : "DIFFERENT");
    SDL_Log("");

    /* ── Part 2: The three areas ──────────────────────────────────────── */
    SDL_Log("--- 2. Git's three areas (working dir / staging / HEAD) ---");
    SDL_Log("");
    SDL_Log("  Working directory  ->  Staging area  ->  Repository (HEAD)");
    SDL_Log("  (files on disk)       (git add)          (git commit)");
    SDL_Log("");

    /* Simulate initial commit */
    GitArea working, staging, head;
    area_init(&working, "working dir");
    area_init(&staging, "staging");
    area_init(&head,    "HEAD");

    FileVersion initial = version_create("config.h", "#define VERSION 1");
    area_add(&working, initial);
    area_add(&staging, initial);
    area_add(&head,    initial);

    SDL_Log("  After initial commit (all areas match):");
    area_print(&head);
    SDL_Log("");

    /* Edit a file in working directory */
    area_update(&working, "config.h", "#define VERSION 2");
    SDL_Log("  After editing config.h in working directory:");
    area_print(&working);
    area_print(&staging);
    SDL_Log("");

    /* git status: compare working vs staging, staging vs HEAD */
    SDL_Log("  'git status' compares the areas:");
    detect_changes(&working, &staging);
    detect_changes(&staging, &head);
    SDL_Log("");

    /* git add config.h */
    area_update(&staging, "config.h", "#define VERSION 2");
    SDL_Log("  After 'git add config.h' (staged for commit):");
    detect_changes(&working, &staging);
    detect_changes(&staging, &head);
    SDL_Log("");

    /* git commit */
    area_update(&head, "config.h", "#define VERSION 2");
    SDL_Log("  After 'git commit' (all areas match again):");
    detect_changes(&working, &staging);
    detect_changes(&staging, &head);
    SDL_Log("");

    /* ── Part 3: Three-way merge ──────────────────────────────────────── */
    SDL_Log("--- 3. Three-way merge (how git resolves branches) ---");
    SDL_Log("");
    SDL_Log("  Git merges by comparing two branches against their");
    SDL_Log("  common ancestor (the base).  For each file:");
    SDL_Log("    - Neither changed  -> keep base");
    SDL_Log("    - One changed      -> take that version");
    SDL_Log("    - Both changed     -> CONFLICT (manual resolution)");
    SDL_Log("");

    /* Scenario: base, our branch, their branch */
    FileVersion base_readme   = version_create("README.md",
                                               "# My Project");
    FileVersion ours_readme   = version_create("README.md",
                                               "# My Project v2");
    FileVersion theirs_readme = version_create("README.md",
                                               "# My Project");

    FileVersion base_cfg      = version_create("config.h",
                                               "#define MAX 100");
    FileVersion ours_cfg      = version_create("config.h",
                                               "#define MAX 200");
    FileVersion theirs_cfg    = version_create("config.h",
                                               "#define MAX 500");

    SDL_Log("  Merge scenario:");
    three_way_merge(&base_readme, &ours_readme, &theirs_readme);
    SDL_Log("");
    three_way_merge(&base_cfg, &ours_cfg, &theirs_cfg);
    SDL_Log("");

    /* ── Part 4: .gitignore patterns ──────────────────────────────────── */
    SDL_Log("--- 4. .gitignore (what NOT to track) ---");
    SDL_Log("");
    SDL_Log("  C/CMake projects generate many files that should not");
    SDL_Log("  be committed.  A .gitignore file tells git to skip them.");
    SDL_Log("");

    static const char *test_files[] = {
        "main.c",                   /* source — track          */
        "CMakeLists.txt",           /* build config — track    */
        "main.o",                   /* object file — ignore    */
        "main.obj",                 /* MSVC object — ignore    */
        "app.exe",                  /* executable — ignore     */
        "build/main.o",             /* build dir — ignore      */
        ".vs/settings.json",        /* VS settings — ignore    */
        "compile_commands.json",    /* clangd DB — ignore      */
        "README.md",                /* docs — track            */
        "app.pdb",                  /* debug DB — ignore       */
        ".DS_Store",                /* macOS metadata — ignore */
    };

    for (size_t i = 0; i < SDL_arraysize(test_files); i++) {
        bool ignored = should_ignore(test_files[i]);
        SDL_Log("  %-28s %s",
                test_files[i],
                ignored ? "[IGNORE]" : "[TRACK]");
    }
    SDL_Log("");

    SDL_Log("  A typical .gitignore for C/CMake projects:");
    SDL_Log("    build/");
    SDL_Log("    *.o");
    SDL_Log("    *.obj");
    SDL_Log("    *.exe");
    SDL_Log("    *.pdb");
    SDL_Log("    *.dll");
    SDL_Log("    *.so");
    SDL_Log("    *.dylib");
    SDL_Log("    .vs/");
    SDL_Log("    .vscode/settings.json");
    SDL_Log("    compile_commands.json");
    SDL_Log("    CMakeCache.txt");
    SDL_Log("    CMakeFiles/");
    SDL_Log("    .DS_Store");
    SDL_Log("");

    /* ── Summary ──────────────────────────────────────────────────────── */
    SDL_Log("=== Summary ===");
    SDL_Log("");
    SDL_Log("  Git tracks content by hashing file contents.");
    SDL_Log("  Files move through three areas:");
    SDL_Log("    working dir -> staging (git add) -> HEAD (git commit)");
    SDL_Log("  Merging uses a three-way comparison against the common");
    SDL_Log("  ancestor to detect conflicts.");
    SDL_Log("  .gitignore prevents build artifacts from being tracked.");
    SDL_Log("");
    SDL_Log("  Read the README for full coverage of branching, submodules,");
    SDL_Log("  worktrees, and hands-on exercises.");

    SDL_Quit();
    return 0;
}
