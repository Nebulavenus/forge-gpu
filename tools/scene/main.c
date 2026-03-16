/*
 * forge-scene-tool -- CLI scene hierarchy extraction tool
 *
 * Reads a glTF/GLB file, extracts the node hierarchy from the parsed
 * scene, and writes a binary .fscene file plus a .meta.json metadata
 * sidecar.
 *
 * Built as a reusable project-level tool at tools/scene/ -- not scoped
 * to a single lesson.  The Python pipeline plugin invokes it as a
 * subprocess.  Asset Lesson 09 teaches how it works.
 *
 * .fscene binary format (all little-endian):
 *
 *   Header (24 bytes):
 *     magic          4 bytes   "FSCN"
 *     version        u32       1
 *     node_count     u32       number of nodes
 *     mesh_count     u32       number of glTF meshes
 *     root_count     u32       number of root node indices
 *     reserved       u32       0 (future use)
 *
 *   Root indices:    root_count x u32
 *   Mesh table:     mesh_count x 8 bytes (first_submesh u32, submesh_count u32)
 *   Node table:     node_count x 192 bytes (fixed-size entries)
 *   Children array: total_children x u32
 *
 *   Node entry (192 bytes):
 *     name               64 bytes (null-terminated, zero-padded)
 *     parent             i32 (-1 = root)
 *     mesh_index         i32 (-1 = no mesh)
 *     skin_index         i32 (-1 = no skin)
 *     first_child        u32 (index into children array)
 *     child_count        u32
 *     has_trs            u32 (1 = TRS valid, 0 = raw matrix)
 *     translation        float[3]
 *     rotation           float[4] (x, y, z, w -- glTF quaternion order)
 *     scale              float[3]
 *     local_transform    float[16] (column-major 4x4)
 *
 * Usage:
 *   forge-scene-tool <input.gltf> <output.fscene> [--verbose]
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdint.h>

#include "gltf/forge_gltf.h"
#include "pipeline/forge_pipeline.h"  /* FORGE_PIPELINE_MAX_NODES etc. */

/* ── Constants ───────────────────────────────────────────────────────────── */

#define FSCENE_MAGIC      "FSCN"
#define FSCENE_MAGIC_SIZE 4
#define FSCENE_VERSION    1
#define FSCENE_HEADER_SIZE 24
#define FSCENE_NODE_SIZE   192

/* ── Tool options ────────────────────────────────────────────────────────── */

typedef struct ToolOptions {
    const char *input_path;   /* source glTF/GLB file */
    const char *output_path;  /* destination .fscene file */
    bool        verbose;      /* print statistics to stdout */
    bool        skins;        /* also write a .fskin file */
} ToolOptions;

/* ── Binary helpers ──────────────────────────────────────────────────────── */

#include "../common/binary_io.h"

/* ── Filename extraction helper ──────────────────────────────────────────── */

/* Return a pointer to the filename portion of a path (after the last
 * directory separator).  Returns the original pointer if no separator. */
static const char *basename_from_path(const char *path)
{
    if (!path) return "";
    const char *name = path;
    const char *slash = SDL_strrchr(path, '/');
    if (slash) name = slash + 1;
    const char *backslash = SDL_strrchr(name, '\\');
    if (backslash) name = backslash + 1;
    return name;
}

/* ── JSON string helper ──────────────────────────────────────────────────── */

/* Write a JSON-escaped string value (with surrounding quotes). */
static void write_json_string(SDL_IOStream *io, const char *s)
{
    SDL_WriteIO(io, "\"", 1);
    if (s) {
        for (const char *c = s; *c != '\0'; c++) {
            switch (*c) {
                case '"':  SDL_WriteIO(io, "\\\"", 2); break;
                case '\\': SDL_WriteIO(io, "\\\\", 2); break;
                case '\n': SDL_WriteIO(io, "\\n", 2);  break;
                case '\r': SDL_WriteIO(io, "\\r", 2);  break;
                case '\t': SDL_WriteIO(io, "\\t", 2);  break;
                default:
                    if ((unsigned char)*c < 0x20) {
                        char buf[8];
                        SDL_snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)*c);
                        SDL_WriteIO(io, buf, 6);
                    } else {
                        SDL_WriteIO(io, c, 1);
                    }
                    break;
            }
        }
    }
    SDL_WriteIO(io, "\"", 1);
}

/* ── Argument parsing ────────────────────────────────────────────────────── */

static bool parse_args(int argc, char *argv[], ToolOptions *opts)
{
    opts->input_path = NULL;
    opts->output_path = NULL;
    opts->verbose = false;
    opts->skins = false;

    int positional = 0;
    int i;
    for (i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--verbose") == 0 || SDL_strcmp(argv[i], "-v") == 0) {
            opts->verbose = true;
        } else if (SDL_strcmp(argv[i], "--skins") == 0) {
            opts->skins = true;
        } else if (argv[i][0] == '-') {
            SDL_Log("Error: unknown option '%s'", argv[i]);
            return false;
        } else {
            if (positional == 0) {
                opts->input_path = argv[i];
            } else if (positional == 1) {
                opts->output_path = argv[i];
            } else {
                SDL_Log("Error: too many positional arguments");
                return false;
            }
            positional++;
        }
    }

    if (!opts->input_path || !opts->output_path) {
        SDL_Log("Usage: forge-scene-tool <input.gltf> <output.fscene> "
               "[--verbose] [--skins]");
        return false;
    }
    return true;
}

/* ── Mesh table: maps glTF mesh index to .fmesh submesh ranges ───────────── */

/* Build a mesh table that maps each glTF mesh to its submesh range.
 * In the .fmesh format, primitives are stored sequentially — mesh 0's
 * primitives come first, then mesh 1's, etc.  Each mesh entry records
 * (first_submesh, submesh_count) so the renderer knows which draw calls
 * belong to a given node. */
typedef struct MeshEntry {
    uint32_t first_submesh;  /* index of first submesh in .fmesh */
    uint32_t submesh_count;  /* number of submeshes (primitives) */
} MeshEntry;

static MeshEntry *build_mesh_table(const ForgeGltfScene *scene)
{
    if (scene->mesh_count == 0) {
        return NULL;
    }

    MeshEntry *table = (MeshEntry *)SDL_calloc(
        (size_t)scene->mesh_count, sizeof(MeshEntry));
    if (!table) {
        SDL_Log("Error: allocation failed for mesh table");
        return NULL;
    }

    /* The mesh tool writes primitives in mesh order, so first_submesh is
     * the running sum of previous mesh primitive counts. */
    uint32_t running = 0;
    int mi;
    for (mi = 0; mi < scene->mesh_count; mi++) {
        table[mi].first_submesh = running;
        table[mi].submesh_count = (uint32_t)scene->meshes[mi].primitive_count;
        running += table[mi].submesh_count;
    }

    return table;
}

/* ── Write .fscene binary ────────────────────────────────────────────────── */

static bool write_fscene(const char *output_path,
                         const ForgeGltfScene *scene,
                         const MeshEntry *mesh_table,
                         bool verbose)
{
    SDL_IOStream *io = SDL_IOFromFile(output_path, "wb");
    if (!io) {
        SDL_Log("Error: failed to open '%s' for writing: %s",
                output_path, SDL_GetError());
        return false;
    }

    bool ok = true;

    /* ── Header (24 bytes) ───────────────────────────────────────────────── */
    ok = ok && (SDL_WriteIO(io, FSCENE_MAGIC, FSCENE_MAGIC_SIZE) == FSCENE_MAGIC_SIZE);
    ok = ok && write_u32_le(io, FSCENE_VERSION);
    ok = ok && write_u32_le(io, (uint32_t)scene->node_count);
    ok = ok && write_u32_le(io, (uint32_t)scene->mesh_count);
    ok = ok && write_u32_le(io, (uint32_t)scene->root_node_count);
    ok = ok && write_u32_le(io, 0); /* reserved */

    if (!ok) {
        SDL_Log("Error: failed writing .fscene header");
        SDL_CloseIO(io);
        return false;
    }

    /* ── Root indices ────────────────────────────────────────────────────── */
    int ri;
    for (ri = 0; ri < scene->root_node_count; ri++) {
        ok = ok && write_u32_le(io, (uint32_t)scene->root_nodes[ri]);
    }
    if (!ok) {
        SDL_Log("Error: failed writing root indices");
        SDL_CloseIO(io);
        return false;
    }

    /* ── Mesh table ──────────────────────────────────────────────────────── */
    int mi;
    for (mi = 0; mi < scene->mesh_count; mi++) {
        ok = ok && write_u32_le(io, mesh_table[mi].first_submesh);
        ok = ok && write_u32_le(io, mesh_table[mi].submesh_count);
    }
    if (!ok) {
        SDL_Log("Error: failed writing mesh table");
        SDL_CloseIO(io);
        return false;
    }

    /* ── Node table ──────────────────────────────────────────────────────── */
    /* Compute per-node first_child offsets into the children array. */
    uint32_t child_offset = 0;
    int ni;
    for (ni = 0; ni < scene->node_count; ni++) {
        const ForgeGltfNode *node = &scene->nodes[ni];

        /* Name: 64 bytes, null-terminated, zero-padded */
        char name_buf[FORGE_GLTF_NAME_SIZE];
        SDL_memset(name_buf, 0, sizeof(name_buf));
        SDL_strlcpy(name_buf, node->name, sizeof(name_buf));
        ok = ok && (SDL_WriteIO(io, name_buf, sizeof(name_buf)) == sizeof(name_buf));

        /* Parent, mesh, skin indices */
        ok = ok && write_i32_le(io, (int32_t)node->parent);
        ok = ok && write_i32_le(io, (int32_t)node->mesh_index);
        ok = ok && write_i32_le(io, (int32_t)node->skin_index);

        /* Child references */
        ok = ok && write_u32_le(io, child_offset);
        ok = ok && write_u32_le(io, (uint32_t)node->child_count);

        /* TRS decomposition */
        ok = ok && write_u32_le(io, node->has_trs ? 1u : 0u);
        ok = ok && write_float_le(io, node->translation.x);
        ok = ok && write_float_le(io, node->translation.y);
        ok = ok && write_float_le(io, node->translation.z);
        ok = ok && write_float_le(io, node->rotation.x);
        ok = ok && write_float_le(io, node->rotation.y);
        ok = ok && write_float_le(io, node->rotation.z);
        ok = ok && write_float_le(io, node->rotation.w);
        ok = ok && write_float_le(io, node->scale_xyz.x);
        ok = ok && write_float_le(io, node->scale_xyz.y);
        ok = ok && write_float_le(io, node->scale_xyz.z);

        /* Local transform matrix (column-major 4x4 = 16 floats) */
        const float *m = node->local_transform.m;
        int fi;
        for (fi = 0; fi < 16; fi++) {
            ok = ok && write_float_le(io, m[fi]);
        }

        if (!ok) {
            SDL_Log("Error: failed writing node %d", ni);
            SDL_CloseIO(io);
            return false;
        }

        child_offset += (uint32_t)node->child_count;

        if (verbose) {
            SDL_Log("  Node %d: \"%s\" parent=%d mesh=%d children=%d",
                    ni, node->name, node->parent,
                    node->mesh_index, node->child_count);
        }
    }

    /* ── Children array ──────────────────────────────────────────────────── */
    for (ni = 0; ni < scene->node_count; ni++) {
        const ForgeGltfNode *node = &scene->nodes[ni];
        int ci;
        for (ci = 0; ci < node->child_count; ci++) {
            ok = ok && write_u32_le(io, (uint32_t)node->children[ci]);
        }
    }
    if (!ok) {
        SDL_Log("Error: failed writing children array");
        SDL_CloseIO(io);
        return false;
    }

    /* ── Finalize ────────────────────────────────────────────────────────── */
    if (SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR) {
        SDL_Log("Error: write error on '%s': %s", output_path, SDL_GetError());
        SDL_CloseIO(io);
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("Error: failed to close '%s': %s", output_path, SDL_GetError());
        return false;
    }

    if (verbose) {
        SDL_Log("Wrote %d node(s), %d mesh(es), %d root(s) to '%s'",
                scene->node_count, scene->mesh_count,
                scene->root_node_count, output_path);
    }
    return true;
}

/* ── Write .meta.json sidecar ────────────────────────────────────────────── */

static bool write_meta_json(const char *fscene_path,
                            const char *source_path,
                            const ForgeGltfScene *scene,
                            const MeshEntry *mesh_table)
{
    /* Build the .meta.json path by replacing the .fscene extension. */
    size_t path_len = SDL_strlen(fscene_path);
    const char *dot = SDL_strrchr(fscene_path, '.');
    size_t stem_len = dot ? (size_t)(dot - fscene_path) : path_len;

    size_t meta_len = stem_len + 11; /* ".meta.json\0" */
    char *meta_path = (char *)SDL_malloc(meta_len);
    if (!meta_path) {
        SDL_Log("Error: allocation failed for meta path");
        return false;
    }
    SDL_snprintf(meta_path, meta_len, "%.*s.meta.json", (int)stem_len, fscene_path);

    SDL_IOStream *io = SDL_IOFromFile(meta_path, "wb");
    if (!io) {
        SDL_Log("Error: failed to open '%s' for writing: %s",
                meta_path, SDL_GetError());
        SDL_free(meta_path);
        return false;
    }

    const char *source_name = basename_from_path(source_path);

    SDL_IOprintf(io, "{\n");
    SDL_IOprintf(io, "  \"source\": ");
    write_json_string(io, source_name);
    SDL_IOprintf(io, ",\n");
    SDL_IOprintf(io, "  \"format_version\": %d,\n", FSCENE_VERSION);
    SDL_IOprintf(io, "  \"node_count\": %d,\n", scene->node_count);
    SDL_IOprintf(io, "  \"mesh_count\": %d,\n", scene->mesh_count);
    SDL_IOprintf(io, "  \"root_count\": %d,\n", scene->root_node_count);

    /* Count nodes with meshes and total children for statistics */
    int mesh_nodes = 0;
    int total_children = 0;
    int ni;
    for (ni = 0; ni < scene->node_count; ni++) {
        if (scene->nodes[ni].mesh_index >= 0) {
            mesh_nodes++;
        }
        total_children += scene->nodes[ni].child_count;
    }
    SDL_IOprintf(io, "  \"mesh_node_count\": %d,\n", mesh_nodes);
    SDL_IOprintf(io, "  \"total_children\": %d,\n", total_children);

    /* Mesh table summary */
    SDL_IOprintf(io, "  \"meshes\": [\n");
    int mi;
    for (mi = 0; mi < scene->mesh_count; mi++) {
        SDL_IOprintf(io, "    {\n");
        SDL_IOprintf(io, "      \"name\": ");
        write_json_string(io, scene->meshes[mi].name);
        SDL_IOprintf(io, ",\n");
        SDL_IOprintf(io, "      \"first_submesh\": %u,\n",
                     mesh_table[mi].first_submesh);
        SDL_IOprintf(io, "      \"submesh_count\": %u\n",
                     mesh_table[mi].submesh_count);
        SDL_IOprintf(io, "    }%s\n", (mi < scene->mesh_count - 1) ? "," : "");
    }
    SDL_IOprintf(io, "  ],\n");

    /* Node list summary */
    SDL_IOprintf(io, "  \"nodes\": [\n");
    for (ni = 0; ni < scene->node_count; ni++) {
        const ForgeGltfNode *node = &scene->nodes[ni];
        SDL_IOprintf(io, "    {\n");
        SDL_IOprintf(io, "      \"name\": ");
        write_json_string(io, node->name);
        SDL_IOprintf(io, ",\n");
        SDL_IOprintf(io, "      \"parent\": %d,\n", node->parent);
        SDL_IOprintf(io, "      \"mesh_index\": %d,\n", node->mesh_index);
        SDL_IOprintf(io, "      \"child_count\": %d\n", node->child_count);
        SDL_IOprintf(io, "    }%s\n", (ni < scene->node_count - 1) ? "," : "");
    }
    SDL_IOprintf(io, "  ]\n");
    SDL_IOprintf(io, "}\n");

    if (SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR) {
        SDL_Log("Error: write error on '%s': %s", meta_path, SDL_GetError());
        SDL_CloseIO(io);
        SDL_free(meta_path);
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("Error: failed to close '%s': %s", meta_path, SDL_GetError());
        SDL_free(meta_path);
        return false;
    }

    SDL_Log("Wrote metadata: '%s'", meta_path);
    SDL_free(meta_path);
    return true;
}

/* ── Write .fskin binary ─────────────────────────────────────────────────── */

/* Build a .fskin path by replacing the .fscene extension. */
static char *make_fskin_path(const char *fscene_path)
{
    const char *dot = SDL_strrchr(fscene_path, '.');
    size_t stem_len = dot ? (size_t)(dot - fscene_path) : SDL_strlen(fscene_path);
    size_t path_len = stem_len + 7; /* ".fskin\0" */
    char *path = (char *)SDL_malloc(path_len);
    if (!path) return NULL;
    SDL_snprintf(path, path_len, "%.*s.fskin", (int)stem_len, fscene_path);
    return path;
}

static bool write_fskin(const char *output_path,
                        const ForgeGltfScene *scene,
                        bool verbose)
{
    if (scene->skin_count == 0) {
        if (verbose) {
            SDL_Log("No skins found — skipping .fskin output");
        }
        return true;
    }

    if ((uint32_t)scene->skin_count > FORGE_PIPELINE_MAX_SKINS) {
        SDL_Log("Error: %d skins exceeds max %u",
                scene->skin_count, FORGE_PIPELINE_MAX_SKINS);
        return false;
    }

    char *fskin_path = make_fskin_path(output_path);
    if (!fskin_path) {
        SDL_Log("Error: allocation failed for .fskin path");
        return false;
    }

    SDL_IOStream *io = SDL_IOFromFile(fskin_path, "wb");
    if (!io) {
        SDL_Log("Error: failed to open '%s' for writing: %s",
                fskin_path, SDL_GetError());
        SDL_free(fskin_path);
        return false;
    }

    bool ok = true;

    /* Header (12 bytes): magic + version + skin_count */
    ok = ok && (SDL_WriteIO(io, FORGE_PIPELINE_FSKIN_MAGIC,
                            FORGE_PIPELINE_FSKIN_MAGIC_SIZE) ==
                FORGE_PIPELINE_FSKIN_MAGIC_SIZE);
    ok = ok && write_u32_le(io, FORGE_PIPELINE_FSKIN_VERSION);
    ok = ok && write_u32_le(io, (uint32_t)scene->skin_count);

    if (!ok) {
        SDL_Log("Error: failed writing .fskin header");
        SDL_CloseIO(io);
        SDL_free(fskin_path);
        return false;
    }

    /* Per-skin data */
    int si;
    for (si = 0; si < scene->skin_count; si++) {
        const ForgeGltfSkin *skin = &scene->skins[si];

        if ((uint32_t)skin->joint_count > FORGE_PIPELINE_MAX_SKIN_JOINTS) {
            SDL_Log("Error: skin %d has %d joints (max %u)",
                    si, skin->joint_count, FORGE_PIPELINE_MAX_SKIN_JOINTS);
            SDL_CloseIO(io);
            SDL_free(fskin_path);
            return false;
        }

        /* Name: 64 bytes, null-terminated, zero-padded */
        char name_buf[FORGE_GLTF_NAME_SIZE];
        SDL_memset(name_buf, 0, sizeof(name_buf));
        SDL_strlcpy(name_buf, skin->name, sizeof(name_buf));
        ok = ok && (SDL_WriteIO(io, name_buf, sizeof(name_buf)) ==
                    sizeof(name_buf));

        /* Joint count */
        ok = ok && write_u32_le(io, (uint32_t)skin->joint_count);

        /* Skeleton root node index */
        ok = ok && write_i32_le(io, (int32_t)skin->skeleton);

        /* Joint node indices */
        int ji;
        for (ji = 0; ji < skin->joint_count; ji++) {
            ok = ok && write_i32_le(io, (int32_t)skin->joints[ji]);
        }

        /* Inverse bind matrices (joint_count * 16 floats, column-major) */
        int mi;
        for (mi = 0; mi < skin->joint_count; mi++) {
            const float *m = skin->inverse_bind_matrices[mi].m;
            int fi;
            for (fi = 0; fi < 16; fi++) {
                ok = ok && write_float_le(io, m[fi]);
            }
        }

        if (!ok) {
            SDL_Log("Error: failed writing skin %d", si);
            SDL_CloseIO(io);
            SDL_free(fskin_path);
            return false;
        }

        if (verbose) {
            SDL_Log("  Skin %d: \"%s\" joints=%d skeleton=%d",
                    si, skin->name, skin->joint_count, skin->skeleton);
        }
    }

    /* Finalize */
    if (SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR) {
        SDL_Log("Error: write error on '%s': %s", fskin_path, SDL_GetError());
        SDL_CloseIO(io);
        SDL_free(fskin_path);
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("Error: failed to close '%s': %s", fskin_path, SDL_GetError());
        SDL_free(fskin_path);
        return false;
    }

    SDL_Log("Wrote %d skin(s) to '%s'", scene->skin_count, fskin_path);
    SDL_free(fskin_path);
    return true;
}

/* ── Main processing ─────────────────────────────────────────────────────── */

static bool process_scene(const ToolOptions *opts)
{
    /* ── Step 1: Load glTF ───────────────────────────────────────────────── */
    ForgeArena gltf_arena = forge_arena_create(0);
    if (!gltf_arena.first) {
        SDL_Log("Error: out of memory creating arena for '%s'", opts->input_path);
        return false;
    }
    ForgeGltfScene scene;
    if (!forge_gltf_load(opts->input_path, &scene, &gltf_arena)) {
        SDL_Log("Error: failed to load '%s'", opts->input_path);
        forge_arena_destroy(&gltf_arena);
        return false;
    }

    if (opts->verbose) {
        SDL_Log("Loaded '%s': %d node(s), %d mesh(es), %d root(s)",
                opts->input_path, scene.node_count,
                scene.mesh_count, scene.root_node_count);
    }

    /* ── Step 2: Validate against runtime loader limits ────────────────── */
    if ((uint32_t)scene.node_count > FORGE_PIPELINE_MAX_NODES) {
        SDL_Log("Error: '%s' has %d nodes (max %u)",
                opts->input_path, scene.node_count, FORGE_PIPELINE_MAX_NODES);
        forge_arena_destroy(&gltf_arena);
        return false;
    }
    if ((uint32_t)scene.mesh_count > FORGE_PIPELINE_MAX_SCENE_MESHES) {
        SDL_Log("Error: '%s' has %d meshes (max %u)",
                opts->input_path, scene.mesh_count,
                FORGE_PIPELINE_MAX_SCENE_MESHES);
        forge_arena_destroy(&gltf_arena);
        return false;
    }
    if ((uint32_t)scene.root_node_count > FORGE_PIPELINE_MAX_ROOTS) {
        SDL_Log("Error: '%s' has %d roots (max %u)",
                opts->input_path, scene.root_node_count,
                FORGE_PIPELINE_MAX_ROOTS);
        forge_arena_destroy(&gltf_arena);
        return false;
    }

    if (scene.node_count == 0) {
        SDL_Log("Warning: '%s' contains no nodes — writing empty .fscene",
                opts->input_path);
    }

    /* ── Step 3: Build mesh table ────────────────────────────────────────── */
    MeshEntry *mesh_table = build_mesh_table(&scene);
    if (scene.mesh_count > 0 && !mesh_table) {
        forge_arena_destroy(&gltf_arena);
        return false;
    }

    /* ── Step 4: Write .fscene binary ────────────────────────────────────── */
    if (!write_fscene(opts->output_path, &scene, mesh_table, opts->verbose)) {
        SDL_free(mesh_table);
        forge_arena_destroy(&gltf_arena);
        return false;
    }

    /* ── Step 5: Write .meta.json sidecar ────────────────────────────────── */
    if (!write_meta_json(opts->output_path, opts->input_path, &scene,
                         mesh_table)) {
        SDL_free(mesh_table);
        forge_arena_destroy(&gltf_arena);
        return false;
    }

    /* ── Step 6: Write .fskin binary (optional) ────────────────────────── */
    if (opts->skins) {
        if (!write_fskin(opts->output_path, &scene, opts->verbose)) {
            SDL_free(mesh_table);
            forge_arena_destroy(&gltf_arena);
            return false;
        }
    }

    /* Print summary to stdout (captured by the Python pipeline plugin) */
    SDL_Log("extracted %d node(s) from '%s'",
            scene.node_count, basename_from_path(opts->input_path));

    SDL_free(mesh_table);
    forge_arena_destroy(&gltf_arena);
    return true;
}

/* ── Entry point ─────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    /* SDL_Init is required for SDL_Log and SDL_IOStream. */
    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    ToolOptions opts;
    if (!parse_args(argc, argv, &opts)) {
        SDL_Quit();
        return 1;
    }

    bool ok = process_scene(&opts);

    SDL_Quit();
    return ok ? 0 : 1;
}
