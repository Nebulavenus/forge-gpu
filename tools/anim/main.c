/*
 * forge-anim-tool -- CLI animation extraction tool
 *
 * Reads a glTF/GLB file, extracts animation clips from the parsed
 * animations[] array, and writes a binary .fanim file plus a .meta.json
 * metadata sidecar.
 *
 * Built as a reusable project-level tool at tools/anim/ -- not scoped to
 * a single lesson.  The Python pipeline plugin invokes it as a subprocess.
 * Asset Lesson 08 teaches how it works.
 *
 * .fanim binary format (all little-endian):
 *   Header:
 *     magic          4 bytes   "FANM"
 *     version        u32       1
 *     clip_count     u32       number of animation clips
 *
 *   Per clip:
 *     name           64 bytes  null-terminated clip name (padded with zeros)
 *     duration       float     max timestamp across all samplers
 *     sampler_count  u32       number of samplers in this clip
 *     channel_count  u32       number of channels in this clip
 *
 *   Per sampler (immediately after the clip header):
 *     keyframe_count    u32    number of keyframes
 *     value_components  u32    3 for translation/scale, 4 for rotation, N for weights
 *     interpolation     u32    0 = LINEAR, 1 = STEP
 *     timestamps[]      float  keyframe_count floats
 *     values[]          float  keyframe_count * value_components floats
 *
 *   Per channel (immediately after all samplers for this clip):
 *     target_node    i32       index into scene nodes (-1 if unset)
 *     target_path    u32       0 = translation, 1 = rotation, 2 = scale, 3 = weights
 *     sampler_index  u32       index into this clip's sampler array
 *
 * Usage:
 *   forge-anim-tool <input.gltf> <output.fanim> [--verbose]
 *   forge-anim-tool <input.gltf> --split --output-dir <dir> [--verbose]
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stdint.h>
#include <string.h>

#include "gltf/forge_gltf.h"

/* ── Constants ───────────────────────────────────────────────────────────── */

#define FANIM_MAGIC      "FANM"
#define FANIM_MAGIC_SIZE 4
#define FANIM_VERSION    1
#define FANIMS_VERSION   1  /* .fanims JSON manifest version */

/* Runtime limits — must match forge_pipeline.h so the loader accepts files
 * we produce.  Validated before writing any .fanim output. */
#define MAX_CLIP_STEMS       256
#define MAX_ANIM_SAMPLERS    512
#define MAX_ANIM_CHANNELS    512
#define MAX_KEYFRAMES        65536

/* Buffer sizes for stem names, file paths, and model names. */
#define STEM_NAME_MAX        256
#define CLIP_PATH_MAX        1024
#define MODEL_NAME_MAX       256
#define STEM_SUFFIX_RESERVE  12   /* room for "_2147483647" + NUL */

/* ── Tool options ────────────────────────────────────────────────────────── */

typedef struct ToolOptions {
    const char *input_path;   /* source glTF/GLB file */
    const char *output_path;  /* destination .fanim file (single-file mode) */
    const char *output_dir;   /* destination directory (--split mode) */
    bool        verbose;      /* print statistics to stdout */
    bool        split;        /* write one .fanim per clip */
} ToolOptions;

/* ── Binary helper ───────────────────────────────────────────────────────── */

#include "../common/binary_io.h"

/* ── Filename extraction helper ──────────────────────────────────────────── */

/* Return a pointer to the filename portion of a path (after the last
 * directory separator).  Returns the original pointer if no separator. */
static const char *basename_from_path(const char *path)
{
    if (!path) return "";
    const char *name = path;
    const char *slash = strrchr(path, '/');
    if (slash) name = slash + 1;
    const char *backslash = strrchr(name, '\\');
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
    opts->output_dir = NULL;
    opts->verbose = false;
    opts->split = false;

    int positional = 0;
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            opts->verbose = true;
        } else if (strcmp(argv[i], "--split") == 0) {
            opts->split = true;
        } else if (strcmp(argv[i], "--output-dir") == 0) {
            if (i + 1 >= argc) {
                SDL_Log("Error: --output-dir requires an argument");
                return false;
            }
            opts->output_dir = argv[++i];
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

    if (!opts->input_path) {
        SDL_Log("Usage: forge-anim-tool <input.gltf> <output.fanim> [--verbose]\n"
                "       forge-anim-tool <input.gltf> --split --output-dir <dir> [--verbose]");
        return false;
    }

    /* Enforce mutually exclusive CLI modes. */
    if (opts->split) {
        if (!opts->output_dir) {
            SDL_Log("Error: --split requires --output-dir <dir>");
            return false;
        }
        if (opts->output_path) {
            SDL_Log("Error: positional <output.fanim> is invalid with --split");
            return false;
        }
    } else {
        if (!opts->output_path) {
            SDL_Log("Usage: forge-anim-tool <input.gltf> <output.fanim> [--verbose]");
            return false;
        }
        if (opts->output_dir) {
            SDL_Log("Error: --output-dir is only valid with --split");
            return false;
        }
    }
    return true;
}

/* ── Write .fanim binary ─────────────────────────────────────────────────── */

static bool write_fanim(const char *output_path,
                        const ForgeGltfScene *scene,
                        bool verbose)
{
    SDL_IOStream *io = SDL_IOFromFile(output_path, "wb");
    if (!io) {
        SDL_Log("Error: failed to open '%s' for writing: %s",
                output_path, SDL_GetError());
        return false;
    }

    int clip_count = scene->animation_count;

    /* ── Header ──────────────────────────────────────────────────────────── */
    bool ok = true;
    ok = ok && (SDL_WriteIO(io, FANIM_MAGIC, FANIM_MAGIC_SIZE) == FANIM_MAGIC_SIZE);
    ok = ok && write_u32_le(io, FANIM_VERSION);
    ok = ok && write_u32_le(io, (uint32_t)clip_count);

    if (!ok) {
        SDL_Log("Error: failed writing .fanim header");
        SDL_CloseIO(io);
        return false;
    }

    /* ── Per-clip data ───────────────────────────────────────────────────── */
    int ci;
    for (ci = 0; ci < clip_count; ci++) {
        const ForgeGltfAnimation *anim = &scene->animations[ci];

        /* Name: 64 bytes, null-terminated, zero-padded */
        char name_buf[FORGE_GLTF_NAME_SIZE];
        memset(name_buf, 0, sizeof(name_buf));
        SDL_strlcpy(name_buf, anim->name, sizeof(name_buf));
        ok = ok && (SDL_WriteIO(io, name_buf, sizeof(name_buf)) == sizeof(name_buf));

        /* Duration */
        ok = ok && write_float_le(io, anim->duration);

        /* Sampler and channel counts */
        ok = ok && write_u32_le(io, (uint32_t)anim->sampler_count);
        ok = ok && write_u32_le(io, (uint32_t)anim->channel_count);

        if (!ok) {
            SDL_Log("Error: failed writing clip %d header", ci);
            SDL_CloseIO(io);
            return false;
        }

        /* ── Per-sampler data ────────────────────────────────────────────── */
        int si;
        for (si = 0; si < anim->sampler_count; si++) {
            const ForgeGltfAnimSampler *samp = &anim->samplers[si];

            ok = ok && write_u32_le(io, (uint32_t)samp->keyframe_count);
            ok = ok && write_u32_le(io, (uint32_t)samp->value_components);
            ok = ok && write_u32_le(io, (uint32_t)samp->interpolation);

            /* Timestamps */
            int ki;
            for (ki = 0; ki < samp->keyframe_count; ki++) {
                ok = ok && write_float_le(io, samp->timestamps[ki]);
            }

            /* Values: keyframe_count * value_components floats */
            int total_values = samp->keyframe_count * samp->value_components;
            int vi;
            for (vi = 0; vi < total_values; vi++) {
                ok = ok && write_float_le(io, samp->values[vi]);
            }

            if (!ok) {
                SDL_Log("Error: failed writing clip %d sampler %d", ci, si);
                SDL_CloseIO(io);
                return false;
            }
        }

        /* ── Per-channel data ────────────────────────────────────────────── */
        int chi;
        for (chi = 0; chi < anim->channel_count; chi++) {
            const ForgeGltfAnimChannel *ch = &anim->channels[chi];

            ok = ok && write_i32_le(io, (int32_t)ch->target_node);
            ok = ok && write_u32_le(io, (uint32_t)ch->target_path);
            ok = ok && write_u32_le(io, (uint32_t)ch->sampler_index);
        }

        if (!ok) {
            SDL_Log("Error: failed writing clip %d channels", ci);
            SDL_CloseIO(io);
            return false;
        }

        if (verbose) {
            SDL_Log("  Clip %d: \"%s\" (%.3f s, %d samplers, %d channels)",
                    ci, anim->name, (double)anim->duration,
                    anim->sampler_count, anim->channel_count);
        }
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
        SDL_Log("Wrote %d clip(s) to '%s'", clip_count, output_path);
    }
    return true;
}

/* ── Write .meta.json sidecar ────────────────────────────────────────────── */

static bool write_meta_json(const char *fanim_path, const char *source_path,
                            const ForgeGltfScene *scene)
{
    /* Build the .meta.json path by replacing the .fanim extension. */
    size_t path_len = strlen(fanim_path);
    const char *dot = strrchr(fanim_path, '.');
    size_t stem_len = dot ? (size_t)(dot - fanim_path) : path_len;

    size_t meta_len = stem_len + 11; /* ".meta.json\0" */
    char *meta_path = (char *)SDL_malloc(meta_len);
    if (!meta_path) {
        SDL_Log("Error: allocation failed for meta path");
        return false;
    }
    SDL_snprintf(meta_path, meta_len, "%.*s.meta.json", (int)stem_len, fanim_path);

    SDL_IOStream *io = SDL_IOFromFile(meta_path, "wb");
    if (!io) {
        SDL_Log("Error: failed to open '%s' for writing: %s",
                meta_path, SDL_GetError());
        SDL_free(meta_path);
        return false;
    }

    const char *source_name = basename_from_path(source_path);
    int clip_count = scene->animation_count;

    SDL_IOprintf(io, "{\n");
    SDL_IOprintf(io, "  \"source\": ");
    write_json_string(io, source_name);
    SDL_IOprintf(io, ",\n");
    SDL_IOprintf(io, "  \"format_version\": %d,\n", FANIM_VERSION);
    SDL_IOprintf(io, "  \"clip_count\": %d,\n", clip_count);

    /* Per-clip metadata */
    SDL_IOprintf(io, "  \"clips\": [\n");
    int ci;
    for (ci = 0; ci < clip_count; ci++) {
        const ForgeGltfAnimation *anim = &scene->animations[ci];

        /* Count total keyframes across all samplers for statistics */
        int total_keyframes = 0;
        int si;
        for (si = 0; si < anim->sampler_count; si++) {
            total_keyframes += anim->samplers[si].keyframe_count;
        }

        SDL_IOprintf(io, "    {\n");
        SDL_IOprintf(io, "      \"name\": ");
        write_json_string(io, anim->name);
        SDL_IOprintf(io, ",\n");
        SDL_IOprintf(io, "      \"duration\": %.6f,\n", (double)anim->duration);
        SDL_IOprintf(io, "      \"sampler_count\": %d,\n", anim->sampler_count);
        SDL_IOprintf(io, "      \"channel_count\": %d,\n", anim->channel_count);
        SDL_IOprintf(io, "      \"total_keyframes\": %d\n", total_keyframes);
        SDL_IOprintf(io, "    }%s\n", (ci < clip_count - 1) ? "," : "");
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

/* ── Per-clip split helpers ──────────────────────────────────────────────── */

/* Sanitize a clip name for use as a filename.
 * Lowercases letters, replaces spaces and special chars with underscores,
 * strips leading/trailing underscores. Result is written to `out`. */
static void sanitize_clip_name(const char *name, char *out, size_t out_size)
{
    if (!name || !out || out_size == 0) return;

    size_t j = 0;
    size_t i;
    for (i = 0; name[i] != '\0' && j < out_size - 1; i++) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z') {
            out[j++] = (char)(c + ('a' - 'A'));
        } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            out[j++] = c;
        } else {
            /* Replace spaces/special chars with underscore, but avoid
             * consecutive underscores */
            if (j > 0 && out[j - 1] != '_') {
                out[j++] = '_';
            }
        }
    }
    /* Strip trailing underscore */
    if (j > 0 && out[j - 1] == '_') {
        j--;
    }
    /* Fallback for empty names */
    if (j == 0) {
        SDL_strlcpy(out, "clip", out_size);
        return;
    }
    out[j] = '\0';
}

/* Deduplicate a sanitized clip stem against already-used stems.
 * If `base` collides with an existing entry, appends "_2", "_3", etc.
 * Returns false if the stem table overflows. */
static bool dedupe_clip_stem(const char *base,
                             char stems[][STEM_NAME_MAX], int *stem_count,
                             int max_stems, char *out, size_t out_size)
{
    /* Trim base so there is always room for a suffix like "_999". */
    char base_trimmed[STEM_NAME_MAX];
    SDL_strlcpy(base_trimmed, base, sizeof(base_trimmed));
    if (SDL_strlen(base_trimmed) > out_size - STEM_SUFFIX_RESERVE) {
        base_trimmed[out_size - STEM_SUFFIX_RESERVE] = '\0';
    }
    SDL_strlcpy(out, base_trimmed, out_size);
    int suffix = 1;
    bool collision = true;
    while (collision) {
        collision = false;
        int i;
        for (i = 0; i < *stem_count; i++) {
            if (strcmp(stems[i], out) == 0) {
                collision = true;
                int n = SDL_snprintf(out, out_size, "%s_%d",
                                     base_trimmed, suffix++);
                if (n < 0 || (size_t)n >= out_size) {
                    return false; /* cannot produce a unique stem */
                }
                break;
            }
        }
    }
    if (*stem_count >= max_stems) {
        return false;
    }
    SDL_strlcpy(stems[*stem_count], out, STEM_NAME_MAX);
    (*stem_count)++;
    return true;
}

/* Write a single-clip .fanim file for one animation. */
static bool write_fanim_single_clip(const char *output_path,
                                     const ForgeGltfAnimation *anim,
                                     bool verbose)
{
    SDL_IOStream *io = SDL_IOFromFile(output_path, "wb");
    if (!io) {
        SDL_Log("Error: failed to open '%s' for writing: %s",
                output_path, SDL_GetError());
        return false;
    }

    bool ok = true;

    /* Header: 1 clip */
    ok = ok && (SDL_WriteIO(io, FANIM_MAGIC, FANIM_MAGIC_SIZE) == FANIM_MAGIC_SIZE);
    ok = ok && write_u32_le(io, FANIM_VERSION);
    ok = ok && write_u32_le(io, 1);

    if (!ok) {
        SDL_Log("Error: failed writing single-clip .fanim header");
        SDL_CloseIO(io);
        return false;
    }

    /* Clip header */
    char name_buf[FORGE_GLTF_NAME_SIZE];
    memset(name_buf, 0, sizeof(name_buf));
    SDL_strlcpy(name_buf, anim->name, sizeof(name_buf));
    ok = ok && (SDL_WriteIO(io, name_buf, sizeof(name_buf)) == sizeof(name_buf));
    ok = ok && write_float_le(io, anim->duration);
    ok = ok && write_u32_le(io, (uint32_t)anim->sampler_count);
    ok = ok && write_u32_le(io, (uint32_t)anim->channel_count);

    /* Samplers */
    int si;
    for (si = 0; si < anim->sampler_count && ok; si++) {
        const ForgeGltfAnimSampler *samp = &anim->samplers[si];
        ok = ok && write_u32_le(io, (uint32_t)samp->keyframe_count);
        ok = ok && write_u32_le(io, (uint32_t)samp->value_components);
        ok = ok && write_u32_le(io, (uint32_t)samp->interpolation);

        int ki;
        for (ki = 0; ki < samp->keyframe_count && ok; ki++) {
            ok = ok && write_float_le(io, samp->timestamps[ki]);
        }
        int total_values = samp->keyframe_count * samp->value_components;
        int vi;
        for (vi = 0; vi < total_values && ok; vi++) {
            ok = ok && write_float_le(io, samp->values[vi]);
        }
    }

    /* Channels */
    int chi;
    for (chi = 0; chi < anim->channel_count && ok; chi++) {
        const ForgeGltfAnimChannel *ch = &anim->channels[chi];
        ok = ok && write_i32_le(io, (int32_t)ch->target_node);
        ok = ok && write_u32_le(io, (uint32_t)ch->target_path);
        ok = ok && write_u32_le(io, (uint32_t)ch->sampler_index);
    }

    if (!ok || SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR) {
        SDL_Log("Error: write error on '%s': %s", output_path, SDL_GetError());
        SDL_CloseIO(io);
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("Error: failed to close '%s': %s", output_path, SDL_GetError());
        return false;
    }

    if (verbose) {
        SDL_Log("  Wrote clip \"%s\" (%.3f s) to '%s'",
                anim->name, (double)anim->duration, output_path);
    }
    return true;
}

/* Pre-compute sanitized + deduplicated clip stems for all animations.
 * Both the manifest writer and the per-clip exporter need identical stems;
 * generating them once avoids duplicated sanitize+dedupe passes and
 * guarantees the two paths stay in sync. */
static bool generate_clip_stems(const ForgeGltfScene *scene,
                                 char stems[][STEM_NAME_MAX], int *count)
{
    *count = 0;
    int ci;
    for (ci = 0; ci < scene->animation_count; ci++) {
        char sanitized[STEM_NAME_MAX];
        sanitize_clip_name(scene->animations[ci].name,
                           sanitized, sizeof(sanitized));
        char unique[STEM_NAME_MAX];
        if (!dedupe_clip_stem(sanitized, stems, count,
                              MAX_CLIP_STEMS, unique, sizeof(unique))) {
            SDL_Log("Error: too many clips for deduplication (max %d)",
                    MAX_CLIP_STEMS);
            return false;
        }
        /* dedupe_clip_stem already appended unique to stems[]. Copy the
         * final unique name back so the caller has it at stems[ci]. */
    }
    return true;
}

/* Write a .fanims JSON manifest listing the per-clip files.
 * The manifest includes clip name, file, duration, loop flag, and tags.
 * Users can edit this file to set loop flags and tags after export. */
static bool write_fanims_stub(const char *output_dir,
                               const char *model_name,
                               const ForgeGltfScene *scene,
                               const char stems[][STEM_NAME_MAX],
                               int stem_count)
{
    char manifest_path[CLIP_PATH_MAX];
    int n = SDL_snprintf(manifest_path, sizeof(manifest_path),
                         "%s/%s.fanims", output_dir, model_name);
    if (n < 0 || (size_t)n >= sizeof(manifest_path)) {
        SDL_Log("Error: manifest path too long for '%s/%s.fanims'",
                output_dir, model_name);
        return false;
    }

    SDL_IOStream *io = SDL_IOFromFile(manifest_path, "wb");
    if (!io) {
        SDL_Log("Error: failed to open '%s' for writing: %s",
                manifest_path, SDL_GetError());
        return false;
    }

    SDL_IOprintf(io, "{\n");
    SDL_IOprintf(io, "  \"version\": %d,\n", FANIMS_VERSION);
    SDL_IOprintf(io, "  \"model\": ");
    write_json_string(io, model_name);
    SDL_IOprintf(io, ",\n");
    SDL_IOprintf(io, "  \"clips\": {\n");

    int ci;
    for (ci = 0; ci < stem_count; ci++) {
        const ForgeGltfAnimation *anim = &scene->animations[ci];

        SDL_IOprintf(io, "    ");
        write_json_string(io, stems[ci]);
        SDL_IOprintf(io, ": { \"file\": \"%s.fanim\", \"duration\": %.6f, "
                     "\"loop\": false, \"tags\": [] }",
                     stems[ci], (double)anim->duration);
        if (ci < stem_count - 1) {
            SDL_IOprintf(io, ",");
        }
        SDL_IOprintf(io, "\n");
    }

    SDL_IOprintf(io, "  }\n");
    SDL_IOprintf(io, "}\n");

    if (SDL_GetIOStatus(io) == SDL_IO_STATUS_ERROR) {
        SDL_Log("Error: write error on '%s': %s", manifest_path, SDL_GetError());
        SDL_CloseIO(io);
        return false;
    }
    if (!SDL_CloseIO(io)) {
        SDL_Log("Error: failed to close '%s': %s", manifest_path, SDL_GetError());
        return false;
    }

    SDL_Log("Wrote manifest: '%s'", manifest_path);
    return true;
}

/* Extract the model name (filename stem) from a path. */
static void model_name_from_path(const char *path, char *out, size_t out_size)
{
    const char *name = basename_from_path(path);
    SDL_strlcpy(out, name, out_size);
    /* Strip extension */
    char *dot = strrchr(out, '.');
    if (dot) *dot = '\0';
}

/* ── Main processing ─────────────────────────────────────────────────────── */

static bool process_animations(const ToolOptions *opts)
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
        SDL_Log("Loaded '%s': %d animation(s), %d node(s)",
                opts->input_path, scene.animation_count, scene.node_count);
    }

    /* ── Step 2: Validate against runtime limits ─────────────────────────── */
    if (scene.animation_count == 0) {
        SDL_Log("Warning: '%s' contains no animations — writing empty .fanim",
                opts->input_path);
    }

    if (scene.animation_count > MAX_CLIP_STEMS) {
        SDL_Log("Error: '%s' has %d clips (max %d)",
                opts->input_path, scene.animation_count, MAX_CLIP_STEMS);
        forge_arena_destroy(&gltf_arena);
        return false;
    }

    {
        int ci;
        for (ci = 0; ci < scene.animation_count; ci++) {
            const ForgeGltfAnimation *anim = &scene.animations[ci];
            if (anim->sampler_count > MAX_ANIM_SAMPLERS) {
                SDL_Log("Error: clip '%s' has %d samplers (max %d)",
                        anim->name, anim->sampler_count, MAX_ANIM_SAMPLERS);
                forge_arena_destroy(&gltf_arena);
                return false;
            }
            if (anim->channel_count > MAX_ANIM_CHANNELS) {
                SDL_Log("Error: clip '%s' has %d channels (max %d)",
                        anim->name, anim->channel_count, MAX_ANIM_CHANNELS);
                forge_arena_destroy(&gltf_arena);
                return false;
            }
            {
                int si;
                for (si = 0; si < anim->sampler_count; si++) {
                    const ForgeGltfAnimSampler *samp = &anim->samplers[si];
                    /* TRS paths require 3 (vec3) or 4 (quat) components.
                     * Morph weights have variable component count
                     * (= number of targets), so only require >= 1. */
                    if (samp->value_components < 1) {
                        SDL_Log("Error: clip '%s' sampler %d has invalid "
                                "value_components=%d (expected >= 1)",
                                anim->name, si, samp->value_components);
                        forge_arena_destroy(&gltf_arena);
                        return false;
                    }
                    if (samp->keyframe_count > MAX_KEYFRAMES) {
                        SDL_Log("Error: clip '%s' sampler %d has %d keyframes "
                                "(max %d)", anim->name, si,
                                samp->keyframe_count, MAX_KEYFRAMES);
                        forge_arena_destroy(&gltf_arena);
                        return false;
                    }
                }
            }
        }
    }

    /* ── Step 3: Write output ────────────────────────────────────────────── */
    if (opts->split) {
        /* Ensure the output directory exists before writing files. */
        if (!SDL_CreateDirectory(opts->output_dir)) {
            SDL_Log("Error: failed to create output directory '%s': %s",
                    opts->output_dir, SDL_GetError());
            forge_arena_destroy(&gltf_arena);
            return false;
        }

        /* Pre-compute sanitized + deduplicated stems once so the per-clip
         * exporter and the manifest writer use identical names. */
        char clip_stems[MAX_CLIP_STEMS][STEM_NAME_MAX];
        int stem_count = 0;
        if (!generate_clip_stems(&scene, clip_stems, &stem_count)) {
            forge_arena_destroy(&gltf_arena);
            return false;
        }

        int ci;
        for (ci = 0; ci < scene.animation_count; ci++) {
            char clip_path[CLIP_PATH_MAX];
            int pn = SDL_snprintf(clip_path, sizeof(clip_path),
                                  "%s/%s.fanim",
                                  opts->output_dir, clip_stems[ci]);
            if (pn < 0 || (size_t)pn >= sizeof(clip_path)) {
                SDL_Log("Error: clip path too long for '%s/%s.fanim'",
                        opts->output_dir, clip_stems[ci]);
                forge_arena_destroy(&gltf_arena);
                return false;
            }

            if (!write_fanim_single_clip(clip_path, &scene.animations[ci],
                                          opts->verbose)) {
                forge_arena_destroy(&gltf_arena);
                return false;
            }
        }

        /* Write .fanims stub manifest */
        char model_name[MODEL_NAME_MAX];
        model_name_from_path(opts->input_path, model_name, sizeof(model_name));
        if (!write_fanims_stub(opts->output_dir, model_name, &scene,
                               (const char (*)[STEM_NAME_MAX])clip_stems,
                               stem_count)) {
            forge_arena_destroy(&gltf_arena);
            return false;
        }

        SDL_Log("split %d clip(s) from '%s' into '%s'",
                scene.animation_count, basename_from_path(opts->input_path),
                opts->output_dir);
    } else {
        /* Single-file mode: all clips in one .fanim */
        if (!write_fanim(opts->output_path, &scene, opts->verbose)) {
            forge_arena_destroy(&gltf_arena);
            return false;
        }

        if (!write_meta_json(opts->output_path, opts->input_path, &scene)) {
            forge_arena_destroy(&gltf_arena);
            return false;
        }

        SDL_Log("extracted %d clip(s) from '%s'",
                scene.animation_count, basename_from_path(opts->input_path));
    }

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

    bool ok = process_animations(&opts);

    SDL_Quit();
    return ok ? 0 : 1;
}
