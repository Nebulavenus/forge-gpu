/*
 * forge-texture-tool — Build-time KTX2 → .ftex transcoder
 *
 * Reads a UASTC KTX2 file (produced by basisu), transcodes each mip level
 * to BC7 or BC5 using the Basis Universal transcoder, and writes a .ftex
 * file containing GPU-ready compressed blocks.
 *
 * The .ftex format is a simple binary layout that the runtime can load
 * with a single SDL_LoadFile + pointer arithmetic — no transcoding or
 * third-party library needed at runtime.
 *
 * Usage:
 *   forge_texture_tool <input.ktx2> <output.ftex> <format>
 *
 * Formats:
 *   bc7_srgb   — BC7 in sRGB color space (base color, emissive)
 *   bc7_unorm  — BC7 in linear space (metallic-roughness, occlusion)
 *   bc5_unorm  — BC5 two-channel (normal maps: RG only, Z reconstructed)
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <stddef.h>

#include "basisu_c_wrapper.h"
#include "binary_io.h"

/* ── .ftex file format ────────────────────────────────────────────────── */

/* File layout:
 *   FtexHeader          (32 bytes)
 *   FtexMipEntry[mips]  (16 bytes each)
 *   block data          (contiguous, mip 0 first)
 */

/* .ftex format constants — must match FORGE_PIPELINE_FTEX_* definitions in
 * forge_pipeline.h.  Duplicated here because the tool is standalone and
 * does not include the full pipeline header. */
#define FTEX_MAGIC            0x58455446  /* "FTEX" little-endian */
#define FTEX_VERSION          1
#define FTEX_HEADER_SIZE      32  /* 8 × uint32 fields */
#define FTEX_MIP_ENTRY_SIZE   16  /* 4 × uint32 per mip level */
#define FTEX_MAX_MIP_LEVELS   32  /* max mip levels (supports up to 4G×4G) */

#define FTEX_FORMAT_BC7_SRGB   1  /* FORGE_PIPELINE_COMPRESSED_BC7_SRGB */
#define FTEX_FORMAT_BC7_UNORM  2  /* FORGE_PIPELINE_COMPRESSED_BC7_UNORM */
#define FTEX_FORMAT_BC5_UNORM  3  /* FORGE_PIPELINE_COMPRESSED_BC5_UNORM */

/* ── CLI argument parsing ─────────────────────────────────────────────── */

typedef struct {
    const char *input_path;
    const char *output_path;
    BasisuTargetFormat basisu_format;
    uint32_t           ftex_format;
    bool               verbose;
} ToolOptions;

/* Positional argument indices for parse_args */
enum {
    ARG_INPUT_PATH  = 0, /* path to input .ktx2 file */
    ARG_OUTPUT_PATH = 1, /* path to output .ftex file */
    ARG_FORMAT      = 2, /* target format string (bc7_srgb, bc7_unorm, bc5_unorm) */
    ARG_COUNT       = 3  /* total required positional arguments */
};

/* Parse CLI arguments into ToolOptions.  Populates input/output paths and
 * selects the Basis Universal target format + matching .ftex format code.
 * Returns false on invalid arguments (caller prints usage). */
static bool parse_args(int argc, char *argv[], ToolOptions *opts)
{
    opts->input_path   = NULL;
    opts->output_path  = NULL;
    opts->basisu_format = BASISU_TARGET_BC7_RGBA;
    opts->ftex_format   = FTEX_FORMAT_BC7_SRGB;
    opts->verbose       = false;

    int positional = 0;
    for (int i = 1; i < argc; i++) {
        if (SDL_strcmp(argv[i], "--verbose") == 0 ||
            SDL_strcmp(argv[i], "-v") == 0) {
            opts->verbose = true;
        } else if (argv[i][0] == '-') {
            SDL_Log("Error: unknown option '%s'", argv[i]);
            return false;
        } else {
            if (positional == ARG_INPUT_PATH) {
                opts->input_path = argv[i];
            } else if (positional == ARG_OUTPUT_PATH) {
                opts->output_path = argv[i];
            } else if (positional == ARG_FORMAT) {
                if (SDL_strcmp(argv[i], "bc7_srgb") == 0) {
                    opts->basisu_format = BASISU_TARGET_BC7_RGBA;
                    opts->ftex_format   = FTEX_FORMAT_BC7_SRGB;
                } else if (SDL_strcmp(argv[i], "bc7_unorm") == 0) {
                    opts->basisu_format = BASISU_TARGET_BC7_RGBA;
                    opts->ftex_format   = FTEX_FORMAT_BC7_UNORM;
                } else if (SDL_strcmp(argv[i], "bc5_unorm") == 0) {
                    opts->basisu_format = BASISU_TARGET_BC5_RG;
                    opts->ftex_format   = FTEX_FORMAT_BC5_UNORM;
                } else {
                    SDL_Log("Error: unknown format '%s' "
                            "(expected bc7_srgb, bc7_unorm, bc5_unorm)",
                            argv[i]);
                    return false;
                }
            } else {
                SDL_Log("Error: too many positional arguments");
                return false;
            }
            positional++;
        }
    }

    if (positional < ARG_COUNT) {
        SDL_Log("Usage: forge_texture_tool <input.ktx2> <output.ftex> "
                "<bc7_srgb|bc7_unorm|bc5_unorm> [--verbose]");
        return false;
    }
    return true;
}

/* ── Main ─────────────────────────────────────────────────────────────── */

/* Entry point: parse args, init Basis transcoder, load a UASTC KTX2 file,
 * transcode to BC7/BC5, and write the resulting GPU-ready blocks as a .ftex
 * file.  The .ftex format is designed for zero-copy runtime loading — no
 * transcoding library is needed at runtime. */
int main(int argc, char *argv[])
{
    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    ToolOptions opts;
    if (!parse_args(argc, argv, &opts)) {
        SDL_Quit();
        return 1;
    }

    /* Initialize Basis Universal transcoder tables */
    basisu_init();

    /* Load the KTX2 file */
    size_t file_size = 0;
    void *file_data = SDL_LoadFile(opts.input_path, &file_size);
    if (!file_data) {
        SDL_Log("Error: failed to load '%s': %s",
                opts.input_path, SDL_GetError());
        SDL_Quit();
        return 1;
    }
    if (file_size > UINT32_MAX) {
        SDL_Log("Error: '%s' exceeds 4 GB", opts.input_path);
        SDL_free(file_data);
        SDL_Quit();
        return 1;
    }

    /* Transcode UASTC → BC7/BC5 */
    BasisuTranscodedTexture result;
    if (!basisu_transcode_ktx2(file_data, (uint32_t)file_size,
                                opts.basisu_format, &result)) {
        SDL_Log("Error: transcoding failed for '%s'", opts.input_path);
        SDL_free(file_data);
        SDL_Quit();
        return 1;
    }
    SDL_free(file_data);

    if (opts.verbose) {
        SDL_Log("Transcoded: %ux%u, %u mips, format %u",
                result.width, result.height, result.level_count,
                opts.ftex_format);
    }

    /* Validate mip count — zero is structurally invalid (no data to write),
     * and exceeding the maximum risks offset overflow. */
    if (result.level_count == 0 || result.level_count > FTEX_MAX_MIP_LEVELS) {
        SDL_Log("Error: mip count %u invalid (expected 1..%u)",
                result.level_count, FTEX_MAX_MIP_LEVELS);
        basisu_free_transcoded(&result);
        SDL_Quit();
        return 1;
    }

    /* Reject zero-dimension output — a valid KTX2 should never transcode
     * to 0×0, but catch transcoder bugs early rather than writing an
     * invalid .ftex that the loader would reject. */
    if (result.width == 0 || result.height == 0) {
        SDL_Log("Error: transcoded texture has zero dimensions (%ux%u)",
                result.width, result.height);
        basisu_free_transcoded(&result);
        SDL_Quit();
        return 1;
    }

    /* Compute data offsets — header, then mip entries, then block data.
     * Use uint64_t to detect overflow before truncating to uint32_t. */
    uint64_t data_offset64 = (uint64_t)FTEX_HEADER_SIZE
                           + (uint64_t)result.level_count * FTEX_MIP_ENTRY_SIZE;
    if (data_offset64 > UINT32_MAX) {
        SDL_Log("Error: data offset overflow for '%s'", opts.input_path);
        basisu_free_transcoded(&result);
        SDL_Quit();
        return 1;
    }

    /* Write the .ftex file */
    SDL_IOStream *out = SDL_IOFromFile(opts.output_path, "wb");
    if (!out) {
        SDL_Log("Error: failed to create '%s': %s",
                opts.output_path, SDL_GetError());
        basisu_free_transcoded(&result);
        SDL_Quit();
        return 1;
    }

    /* Header (32 bytes) */
    bool ok = true;
    ok = ok && write_u32_le(out, FTEX_MAGIC);
    ok = ok && write_u32_le(out, FTEX_VERSION);
    ok = ok && write_u32_le(out, opts.ftex_format);
    ok = ok && write_u32_le(out, result.width);
    ok = ok && write_u32_le(out, result.height);
    ok = ok && write_u32_le(out, result.level_count);
    ok = ok && write_u32_le(out, 0); /* reserved */
    ok = ok && write_u32_le(out, 0); /* reserved */

    /* Mip entries (16 bytes each).
     * Use uint64_t for the running offset to detect overflow before
     * truncating to the 32-bit value stored in the file. */
    uint64_t running_offset64 = data_offset64;
    for (uint32_t i = 0; i < result.level_count && ok; i++) {
        uint64_t next = running_offset64 + (uint64_t)result.levels[i].data_size;
        if (next > UINT32_MAX) {
            SDL_Log("Error: .ftex exceeds 4 GB at mip %u for '%s'",
                    i, opts.input_path);
            ok = false;
            break;
        }
        ok = ok && write_u32_le(out, (uint32_t)running_offset64);
        ok = ok && write_u32_le(out, result.levels[i].data_size);
        ok = ok && write_u32_le(out, result.levels[i].width);
        ok = ok && write_u32_le(out, result.levels[i].height);
        running_offset64 = next;
    }

    /* Block data */
    for (uint32_t i = 0; i < result.level_count && ok; i++) {
        if (!result.levels[i].data || result.levels[i].data_size == 0) {
            SDL_Log("Error: mip %u has NULL data or zero size", i);
            ok = false;
            break;
        }
        size_t written = SDL_WriteIO(out, result.levels[i].data,
                                      result.levels[i].data_size);
        if (written != result.levels[i].data_size) ok = false;
    }

    if (!SDL_CloseIO(out)) {
        SDL_Log("Error: failed to close '%s': %s",
                opts.output_path, SDL_GetError());
        ok = false;
    }

    if (!ok) {
        SDL_Log("Error: write failed for '%s'", opts.output_path);
        /* Remove partial output to prevent a corrupt .ftex from poisoning
         * later pipeline runs. */
        SDL_RemovePath(opts.output_path);
        basisu_free_transcoded(&result);
        SDL_Quit();
        return 1;
    }

    uint32_t total_bytes = (uint32_t)running_offset64;
    SDL_Log("%s -> %s (%ux%u, %u mips, %u bytes)",
            opts.input_path, opts.output_path,
            result.width, result.height, result.level_count, total_bytes);

    basisu_free_transcoded(&result);
    SDL_Quit();
    return 0;
}
