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
 *     value_components  u32    3 for translation/scale, 4 for rotation
 *     interpolation     u32    0 = LINEAR, 1 = STEP
 *     timestamps[]      float  keyframe_count floats
 *     values[]          float  keyframe_count * value_components floats
 *
 *   Per channel (immediately after all samplers for this clip):
 *     target_node    i32       index into scene nodes (-1 if unset)
 *     target_path    u32       0 = translation, 1 = rotation, 2 = scale
 *     sampler_index  u32       index into this clip's sampler array
 *
 * Usage:
 *   forge-anim-tool <input.gltf> <output.fanim> [--verbose]
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

/* ── Tool options ────────────────────────────────────────────────────────── */

typedef struct ToolOptions {
    const char *input_path;   /* source glTF/GLB file */
    const char *output_path;  /* destination .fanim file */
    bool        verbose;      /* print statistics to stdout */
} ToolOptions;

/* ── Binary helper ───────────────────────────────────────────────────────── */

/* Write a uint32 in little-endian byte order to an SDL I/O stream. */
static bool write_u32_le(SDL_IOStream *io, uint32_t val)
{
    uint8_t bytes[4];
    bytes[0] = (uint8_t)(val);
    bytes[1] = (uint8_t)(val >> 8);
    bytes[2] = (uint8_t)(val >> 16);
    bytes[3] = (uint8_t)(val >> 24);
    return SDL_WriteIO(io, bytes, 4) == 4;
}

/* Write a signed int32 in little-endian byte order. */
static bool write_i32_le(SDL_IOStream *io, int32_t val)
{
    uint32_t uval;
    memcpy(&uval, &val, sizeof(uval));
    return write_u32_le(io, uval);
}

/* Write a float in little-endian byte order. */
static bool write_float_le(SDL_IOStream *io, float val)
{
    uint32_t bits;
    memcpy(&bits, &val, sizeof(bits));
    return write_u32_le(io, bits);
}

/* ── Filename extraction helper ──────────────────────────────────────────── */

/* Return a pointer to the filename portion of a path (after the last
 * directory separator).  Returns the original pointer if no separator. */
static const char *basename_from_path(const char *path)
{
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
    opts->verbose = false;

    int positional = 0;
    int i;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            opts->verbose = true;
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
        SDL_Log("Usage: forge-anim-tool <input.gltf> <output.fanim> [--verbose]");
        return false;
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

/* ── Main processing ─────────────────────────────────────────────────────── */

static bool process_animations(const ToolOptions *opts)
{
    /* ── Step 1: Load glTF ───────────────────────────────────────────────── */
    ForgeGltfScene scene;
    if (!forge_gltf_load(opts->input_path, &scene)) {
        SDL_Log("Error: failed to load '%s'", opts->input_path);
        return false;
    }

    if (opts->verbose) {
        SDL_Log("Loaded '%s': %d animation(s), %d node(s)",
                opts->input_path, scene.animation_count, scene.node_count);
    }

    /* ── Step 2: Check for animations ────────────────────────────────────── */
    if (scene.animation_count == 0) {
        SDL_Log("Warning: '%s' contains no animations — writing empty .fanim",
                opts->input_path);
    }

    /* ── Step 3: Write .fanim binary ─────────────────────────────────────── */
    if (!write_fanim(opts->output_path, &scene, opts->verbose)) {
        forge_gltf_free(&scene);
        return false;
    }

    /* ── Step 4: Write .meta.json sidecar ────────────────────────────────── */
    if (!write_meta_json(opts->output_path, opts->input_path, &scene)) {
        forge_gltf_free(&scene);
        return false;
    }

    /* Print summary to stdout (captured by the Python pipeline plugin) */
    SDL_Log("extracted %d clip(s) from '%s'",
            scene.animation_count, basename_from_path(opts->input_path));

    forge_gltf_free(&scene);
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
