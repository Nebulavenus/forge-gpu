/*
 * basisu_c_wrapper.h — C API for Basis Universal KTX2 transcoding
 *
 * Thin C-linkage wrapper around the Basis Universal C++ transcoder.
 * Used by forge_pipeline.h to transcode UASTC-compressed KTX2 textures
 * into GPU block-compressed formats (BC7, BC5) at runtime.
 *
 * Link against the basisu_transcoder static library to resolve these symbols.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BASISU_C_WRAPPER_H
#define BASISU_C_WRAPPER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Target formats for transcoding ────────────────────────────────────── */

typedef enum BasisuTargetFormat {
    BASISU_TARGET_BC7_RGBA,  /* BC7: 16 bytes per 4x4 block, RGBA */
    BASISU_TARGET_BC5_RG     /* BC5: 16 bytes per 4x4 block, RG only */
} BasisuTargetFormat;

/* ── Transcoded mip level ──────────────────────────────────────────────── */

typedef struct BasisuMipLevel {
    void    *data;       /* GPU-ready compressed block data (caller frees) */
    uint32_t data_size;  /* byte count of compressed data */
    uint32_t width;      /* mip level width in pixels */
    uint32_t height;     /* mip level height in pixels */
} BasisuMipLevel;

/* ── Transcoded texture result ─────────────────────────────────────────── */

typedef struct BasisuTranscodedTexture {
    BasisuMipLevel *levels;      /* array of mip levels (level_count entries) */
    uint32_t        level_count; /* number of mip levels */
    uint32_t        width;       /* base (mip 0) width in pixels */
    uint32_t        height;      /* base (mip 0) height in pixels */
} BasisuTranscodedTexture;

/* ── Functions ─────────────────────────────────────────────────────────── */

/*
 * basisu_init — Initialize the Basis Universal transcoder tables.
 *
 * Must be called once before any transcoding.  Safe to call multiple times
 * (subsequent calls are no-ops).
 */
void basisu_init(void);

/*
 * basisu_transcode_ktx2 — Transcode a KTX2 file to a block-compressed format.
 *
 * Parses the KTX2 container, validates UASTC supercompression, and transcodes
 * each mip level into the requested target format.  The result struct is
 * populated with one entry per mip level, each containing a heap-allocated
 * buffer of GPU-ready compressed blocks.
 *
 * Parameters:
 *   ktx2_data  — raw KTX2 file bytes
 *   ktx2_size  — byte count
 *   target     — desired block-compressed output format
 *   result     — populated on success; free with basisu_free_transcoded()
 *
 * Returns true on success, false on any error (logged to stderr).
 */
bool basisu_transcode_ktx2(const void *ktx2_data, uint32_t ktx2_size,
                           BasisuTargetFormat target,
                           BasisuTranscodedTexture *result);

/*
 * basisu_free_transcoded — Release all memory owned by a transcoded texture.
 *
 * Safe to call on a zeroed struct (no-op).
 */
void basisu_free_transcoded(BasisuTranscodedTexture *tex);

#ifdef __cplusplus
}
#endif

#endif /* BASISU_C_WRAPPER_H */
