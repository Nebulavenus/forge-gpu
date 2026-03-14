/*
 * basisu_c_wrapper.cpp — C-linkage wrapper for Basis Universal KTX2 transcoding
 *
 * Implements the C API declared in basisu_c_wrapper.h using the Basis Universal
 * C++ ktx2_transcoder class.  Compiled as part of the basisu_transcoder library.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "basisu_c_wrapper.h"
#include "transcoder/basisu_transcoder.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

/* ── Init flag ─────────────────────────────────────────────────────────── */

static bool s_basisu_initialized = false;

/* ── Map our target enum to Basis Universal's transcoder_texture_format ── */

static basist::transcoder_texture_format
target_to_basist(BasisuTargetFormat target)
{
    switch (target) {
    case BASISU_TARGET_BC7_RGBA: return basist::transcoder_texture_format::cTFBC7_RGBA;
    case BASISU_TARGET_BC5_RG:   return basist::transcoder_texture_format::cTFBC5_RG;
    default:                     return basist::transcoder_texture_format::cTFBC7_RGBA;
    }
}

/* ── Compute the byte size of a block-compressed mip level ────────────── */

static uint32_t compute_block_size(uint32_t width, uint32_t height,
                                   BasisuTargetFormat target)
{
    /* Both BC7 and BC5 use 16 bytes per 4x4 block */
    uint32_t blocks_x = (width  + 3) / 4;
    uint32_t blocks_y = (height + 3) / 4;
    (void)target; /* both formats are 16 bytes/block */
    return blocks_x * blocks_y * 16;
}

/* ── Public API ────────────────────────────────────────────────────────── */

extern "C" {

void basisu_init(void)
{
    if (s_basisu_initialized) return;
    basist::basisu_transcoder_init();
    s_basisu_initialized = true;
}

bool basisu_transcode_ktx2(const void *ktx2_data, uint32_t ktx2_size,
                           BasisuTargetFormat target,
                           BasisuTranscodedTexture *result)
{
    if (!ktx2_data || ktx2_size == 0 || !result) {
        fprintf(stderr, "basisu_transcode_ktx2: invalid arguments\n");
        return false;
    }

    memset(result, 0, sizeof(*result));

    if (!s_basisu_initialized) {
        basisu_init();
    }

    /* Create a KTX2 transcoder and feed it the file data */
    basist::ktx2_transcoder transcoder;
    if (!transcoder.init(static_cast<const uint8_t *>(ktx2_data), ktx2_size)) {
        fprintf(stderr, "basisu_transcode_ktx2: ktx2_transcoder::init failed "
                "(invalid or unsupported KTX2 data)\n");
        return false;
    }

    /* Start transcoding — this prepares internal state for level access */
    if (!transcoder.start_transcoding()) {
        fprintf(stderr, "basisu_transcode_ktx2: start_transcoding failed\n");
        return false;
    }

    uint32_t levels = transcoder.get_levels();
    if (levels == 0) {
        fprintf(stderr, "basisu_transcode_ktx2: KTX2 has 0 mip levels\n");
        return false;
    }

    /* Get base dimensions */
    basist::ktx2_image_level_info level0_info;
    if (!transcoder.get_image_level_info(level0_info, 0, 0, 0)) {
        fprintf(stderr, "basisu_transcode_ktx2: failed to get level 0 info\n");
        return false;
    }

    result->width  = level0_info.m_orig_width;
    result->height = level0_info.m_orig_height;
    result->level_count = levels;

    /* Allocate the mip level array */
    result->levels = static_cast<BasisuMipLevel *>(
        calloc(levels, sizeof(BasisuMipLevel)));
    if (!result->levels) {
        fprintf(stderr, "basisu_transcode_ktx2: allocation failed\n");
        return false;
    }

    basist::transcoder_texture_format basist_fmt = target_to_basist(target);

    /* Transcode each mip level */
    for (uint32_t i = 0; i < levels; i++) {
        basist::ktx2_image_level_info info;
        if (!transcoder.get_image_level_info(info, i, 0, 0)) {
            fprintf(stderr, "basisu_transcode_ktx2: failed to get level %u info\n", i);
            basisu_free_transcoded(result);
            return false;
        }

        uint32_t w = info.m_orig_width;
        uint32_t h = info.m_orig_height;
        uint32_t block_size = compute_block_size(w, h, target);
        uint32_t blocks_x = (w + 3) / 4;
        uint32_t blocks_y = (h + 3) / 4;
        uint32_t total_blocks = blocks_x * blocks_y;

        void *output = malloc(block_size);
        if (!output) {
            fprintf(stderr, "basisu_transcode_ktx2: allocation failed for "
                    "level %u (%u bytes)\n", i, block_size);
            basisu_free_transcoded(result);
            return false;
        }

        if (!transcoder.transcode_image_level(
                i, 0, 0,
                output, total_blocks,
                basist_fmt)) {
            fprintf(stderr, "basisu_transcode_ktx2: transcode failed for "
                    "level %u (%ux%u)\n", i, w, h);
            free(output);
            basisu_free_transcoded(result);
            return false;
        }

        result->levels[i].data      = output;
        result->levels[i].data_size = block_size;
        result->levels[i].width     = w;
        result->levels[i].height    = h;
    }

    return true;
}

void basisu_free_transcoded(BasisuTranscodedTexture *tex)
{
    if (!tex) return;
    if (tex->levels) {
        for (uint32_t i = 0; i < tex->level_count; i++) {
            free(tex->levels[i].data);
        }
        free(tex->levels);
    }
    memset(tex, 0, sizeof(*tex));
}

} /* extern "C" */
