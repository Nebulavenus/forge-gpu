/*
 * forge_raster.h -- Header-only CPU triangle rasterizer for forge-gpu
 *
 * Software-rasterizes triangles from vertex/index buffers into an RGBA
 * pixel framebuffer using the edge function method.  Supports vertex
 * color interpolation, grayscale texture sampling, and alpha blending
 * (source-over compositing).
 *
 * The vertex format (ForgeRasterVertex) matches ForgeUiVertex -- same
 * field order and sizes -- so UI vertex/index buffers can be rasterized
 * directly into BMP images for visualization and testing.
 *
 * Supports:
 *   - RGBA8888 framebuffer creation, clearing, and BMP writing
 *   - Edge-function triangle rasterization with bounding box optimization
 *   - Barycentric interpolation of vertex colors and UV coordinates
 *   - Optional grayscale texture sampling (nearest-neighbor)
 *   - Source-over alpha blending
 *   - Indexed triangle drawing (vertex + index buffer batches)
 *   - 32-bit BMP output with alpha channel
 *
 * Limitations (intentional for a learning library):
 *   - No subpixel precision or fill rules beyond basic edge function test
 *   - No SIMD or other optimizations -- clarity over speed
 *   - Nearest-neighbor texture sampling only (no bilinear filtering)
 *   - No depth buffer or z-testing
 *   - No clipping (triangles are clamped to framebuffer bounds)
 *
 * Usage:
 *   #include "raster/forge_raster.h"
 *
 *   ForgeRasterBuffer buf = forge_raster_buffer_create(512, 512);
 *   forge_raster_clear(&buf, 0.1f, 0.1f, 0.1f, 1.0f);
 *
 *   ForgeRasterVertex v0 = {256, 50,  0,0, 1,0,0,1};
 *   ForgeRasterVertex v1 = {100, 400, 0,0, 0,1,0,1};
 *   ForgeRasterVertex v2 = {412, 400, 0,0, 0,0,1,1};
 *   forge_raster_triangle(&buf, &v0, &v1, &v2, NULL);
 *
 *   forge_raster_write_bmp(&buf, "triangle.bmp");
 *   forge_raster_buffer_destroy(&buf);
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_RASTER_H
#define FORGE_RASTER_H

#include <SDL3/SDL.h>
#include <stdint.h>  /* UINT32_MAX for BMP size validation */
#include <stdio.h>   /* FILE, fopen, fwrite, fclose for BMP writing */

/* ── Public Constants ────────────────────────────────────────────────────── */

/* Bytes per pixel in the framebuffer (RGBA8888) */
#define FORGE_RASTER_BPP 4

/* Maximum framebuffer or texture dimension (width or height).
 * Keeps all intermediate integer arithmetic within safe bounds:
 * width * BPP fits in int, and stride * height fits in size_t.
 * 16384 is 4x a typical 4K display -- generous for a learning library. */
#define FORGE_RASTER_MAX_DIM 16384

/* ── Public Types ────────────────────────────────────────────────────────── */

/* A single vertex with position, texture coordinates, and color.
 * Matches ForgeUiVertex layout: pos(x,y), uv(u,v), color(r,g,b,a). */
typedef struct ForgeRasterVertex {
    float x, y;        /* screen position in pixels -- no projection step,
                        * the rasterizer works directly in framebuffer coords */
    float u, v;        /* texture coordinates [0,1] -- interpolated across
                        * the triangle and used to sample the texture */
    float r, g, b, a;  /* vertex color (straight alpha) -- interpolated per
                        * pixel and multiplied with the texture sample */
} ForgeRasterVertex;   /* 32 bytes -- matches ForgeUiVertex layout */

/* An RGBA8888 pixel buffer (framebuffer).
 * Pixels are stored row-major, top-left origin, 4 bytes per pixel
 * in R, G, B, A order. */
typedef struct ForgeRasterBuffer {
    Uint8 *pixels;   /* RGBA8888, row-major, top-left origin */
    int    width;    /* width in pixels */
    int    height;   /* height in pixels */
    int    stride;   /* bytes per row -- stored separately from width so
                      * row addressing uses a single multiply, and to
                      * allow for potential row-alignment padding */
} ForgeRasterBuffer;

/* A single-channel (grayscale) texture for sampling.
 * Used for font atlas glyphs and other alpha-only textures. */
typedef struct ForgeRasterTexture {
    const Uint8 *pixels;  /* single-channel (grayscale), row-major -- one byte
                           * per texel is enough for font atlases and masks;
                           * the texel value multiplies the vertex color */
    int          width;   /* width in texels */
    int          height;  /* height in texels */
} ForgeRasterTexture;

/* ── Public API ──────────────────────────────────────────────────────────── */

/* Allocate an RGBA8888 framebuffer.  Returns a buffer with pixels set to
 * NULL on allocation failure.  The caller must check buf.pixels != NULL. */
static inline ForgeRasterBuffer forge_raster_buffer_create(int width, int height);

/* Free the pixel memory allocated by forge_raster_buffer_create. */
static inline void forge_raster_buffer_destroy(ForgeRasterBuffer *buf);

/* Fill the entire framebuffer with a solid color (components in [0,1]). */
static inline void forge_raster_clear(ForgeRasterBuffer *buf,
                                      float r, float g, float b, float a);

/* Rasterize a single triangle into the framebuffer.
 *
 * Uses the edge function method: compute barycentric coordinates for each
 * pixel in the triangle's bounding box, interpolate vertex attributes, and
 * alpha-blend onto the framebuffer.
 *
 * If texture is non-NULL, interpolated UVs sample the grayscale texture
 * and multiply with the interpolated vertex color -- the same model Dear
 * ImGui uses: font atlas for text, white pixel for solid shapes. */
static inline void forge_raster_triangle(ForgeRasterBuffer *buf,
                                         const ForgeRasterVertex *v0,
                                         const ForgeRasterVertex *v1,
                                         const ForgeRasterVertex *v2,
                                         const ForgeRasterTexture *texture);

/* Draw triangles from vertex and index arrays (batch draw call).
 *
 * Every three consecutive indices form one triangle.  index_count must
 * be a multiple of 3.  Each index is validated against vertex_count. */
static inline void forge_raster_triangles_indexed(ForgeRasterBuffer *buf,
                                                  const ForgeRasterVertex *vertices,
                                                  int vertex_count,
                                                  const Uint32 *indices,
                                                  int index_count,
                                                  const ForgeRasterTexture *texture);

/* Write the framebuffer to a 32-bit BMP file.  BMP stores pixels as BGRA
 * in bottom-up row order; this function handles the conversion from our
 * RGBA top-down format.  Returns true on success. */
static inline bool forge_raster_write_bmp(const ForgeRasterBuffer *buf,
                                          const char *path);

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── Implementation ───────────────────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* ── Internal Helpers ────────────────────────────────────────────────────── */

/* Minimum / maximum of two floats */
static inline float forge_raster__min2f(float a, float b)
{
    return a < b ? a : b;
}

static inline float forge_raster__max2f(float a, float b)
{
    return a > b ? a : b;
}

/* Minimum / maximum of three floats */
static inline float forge_raster__min3f(float a, float b, float c)
{
    return forge_raster__min2f(forge_raster__min2f(a, b), c);
}

static inline float forge_raster__max3f(float a, float b, float c)
{
    return forge_raster__max2f(forge_raster__max2f(a, b), c);
}

/* Clamp a float to [lo, hi] */
static inline float forge_raster__clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* Clamp an int to [lo, hi] */
static inline int forge_raster__clamp_int(int x, int lo, int hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

/* Convert a float [0,1] to a byte [0,255] */
static inline Uint8 forge_raster__to_byte(float f)
{
    return (Uint8)(forge_raster__clampf(f, 0.0f, 1.0f) * 255.0f + 0.5f);
}

/* Convert a byte [0,255] to a float [0,1] */
static inline float forge_raster__to_float(Uint8 b)
{
    return (float)b / 255.0f;
}

/* The 2D orient function (edge function / signed parallelogram area).
 *
 * orient2d(a, b, p) = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x)
 *
 * Returns:
 *   > 0  if p is to the left of edge a->b (CCW side)
 *   = 0  if p is exactly on the edge
 *   < 0  if p is to the right of edge a->b (CW side)
 *
 * This is the 2D cross product of vectors (a->b) and (a->p), which equals
 * twice the signed area of the triangle (a, b, p). */
static inline float forge_raster__orient2d(float ax, float ay,
                                           float bx, float by,
                                           float px, float py)
{
    return (bx - ax) * (py - ay) - (by - ay) * (px - ax);
}

/* Absolute value of a float (avoids <math.h> dependency) */
static inline float forge_raster__fabsf(float x)
{
    return x < 0.0f ? -x : x;
}

/* Returns false for NaN, Infinity, and extremely large magnitudes.
 * NaN fails both comparisons (IEEE 754), so this catches all non-finite
 * values without requiring <math.h>. */
static inline bool forge_raster__is_safe_coord(float x)
{
    return x >= -1e7f && x <= 1e7f;
}

/* ── Buffer Operations ───────────────────────────────────────────────────── */

static inline ForgeRasterBuffer forge_raster_buffer_create(int width, int height)
{
    ForgeRasterBuffer buf;
    buf.pixels = NULL;
    buf.width  = 0;
    buf.height = 0;
    buf.stride = 0;

    if (width <= 0 || height <= 0 ||
        width > FORGE_RASTER_MAX_DIM || height > FORGE_RASTER_MAX_DIM) {
        SDL_Log("forge_raster_buffer_create: invalid dimensions %dx%d "
                "(max %d)", width, height, FORGE_RASTER_MAX_DIM);
        return buf;
    }

    buf.width  = width;
    buf.height = height;
    buf.stride = width * FORGE_RASTER_BPP;  /* safe: max 16384*4 = 65536 */

    size_t size = (size_t)buf.stride * (size_t)height;
    buf.pixels = (Uint8 *)SDL_calloc(size, 1);
    if (!buf.pixels) {
        SDL_Log("forge_raster_buffer_create: allocation failed (%zu bytes)",
                size);
        buf.width  = 0;
        buf.height = 0;
        buf.stride = 0;
    }
    return buf;
}

static inline void forge_raster_buffer_destroy(ForgeRasterBuffer *buf)
{
    if (buf) {
        SDL_free(buf->pixels);
        buf->pixels = NULL;
        buf->width  = 0;
        buf->height = 0;
        buf->stride = 0;
    }
}

static inline void forge_raster_clear(ForgeRasterBuffer *buf,
                                      float r, float g, float b, float a)
{
    if (!buf || !buf->pixels) return;

    Uint8 rb = forge_raster__to_byte(r);
    Uint8 gb = forge_raster__to_byte(g);
    Uint8 bb = forge_raster__to_byte(b);
    Uint8 ab = forge_raster__to_byte(a);

    for (int y = 0; y < buf->height; y++) {
        Uint8 *row = buf->pixels + (size_t)y * (size_t)buf->stride;
        for (int x = 0; x < buf->width; x++) {
            row[x * FORGE_RASTER_BPP + 0] = rb;
            row[x * FORGE_RASTER_BPP + 1] = gb;
            row[x * FORGE_RASTER_BPP + 2] = bb;
            row[x * FORGE_RASTER_BPP + 3] = ab;
        }
    }
}

/* ── Triangle Rasterization ──────────────────────────────────────────────── */

static inline void forge_raster_triangle(ForgeRasterBuffer *buf,
                                         const ForgeRasterVertex *v0,
                                         const ForgeRasterVertex *v1,
                                         const ForgeRasterVertex *v2,
                                         const ForgeRasterTexture *texture)
{
    if (!buf || !buf->pixels || !v0 || !v1 || !v2) return;

    /* Reject vertices with non-finite coordinates (NaN, Infinity).
     * Casting such values to int is undefined behavior in C99. */
    if (!forge_raster__is_safe_coord(v0->x) ||
        !forge_raster__is_safe_coord(v0->y) ||
        !forge_raster__is_safe_coord(v1->x) ||
        !forge_raster__is_safe_coord(v1->y) ||
        !forge_raster__is_safe_coord(v2->x) ||
        !forge_raster__is_safe_coord(v2->y)) {
        return;
    }

    /* Compute the signed area of the triangle (twice the signed area).
     * Positive for CCW winding, negative for CW, zero for degenerate. */
    float area = forge_raster__orient2d(v0->x, v0->y,
                                        v1->x, v1->y,
                                        v2->x, v2->y);

    /* Skip degenerate or near-degenerate triangles.  An exact == 0 test
     * misses nearly-collinear vertices where 1/area produces extreme
     * barycentric values.  The negated form handles NaN (NaN >= x is
     * always false, so the check triggers). */
    if (!(forge_raster__fabsf(area) >= 1e-6f)) return;

    /* Compute bounding box of the triangle in pixel coordinates */
    float fmin_x = forge_raster__min3f(v0->x, v1->x, v2->x);
    float fmin_y = forge_raster__min3f(v0->y, v1->y, v2->y);
    float fmax_x = forge_raster__max3f(v0->x, v1->x, v2->x);
    float fmax_y = forge_raster__max3f(v0->y, v1->y, v2->y);

    /* Convert to integer pixel coordinates and clamp to framebuffer.
     * Truncation toward zero is correct here for positive coordinates
     * (which pixel positions always are after framebuffer clamping). */
    int min_x = forge_raster__clamp_int((int)fmin_x, 0, buf->width - 1);
    int min_y = forge_raster__clamp_int((int)fmin_y, 0, buf->height - 1);
    int max_x = forge_raster__clamp_int((int)fmax_x, 0, buf->width - 1);
    int max_y = forge_raster__clamp_int((int)fmax_y, 0, buf->height - 1);

    /* Precompute 1/area for barycentric normalization */
    float inv_area = 1.0f / area;

    /* Rasterize: test each pixel center in the bounding box */
    for (int y = min_y; y <= max_y; y++) {
        for (int x = min_x; x <= max_x; x++) {
            /* Sample at the pixel center (x + 0.5, y + 0.5) rather than
             * the corner — this is the same convention GPUs use and avoids
             * off-by-half-pixel artifacts at triangle edges. */
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;

            /* Compute the three edge functions.  Each edge function gives
             * the signed area of the sub-triangle formed by the opposite
             * vertex and the edge.  The naming maps each weight to the
             * vertex it "belongs to":
             *   w0 = orient2d(v1, v2, p) -> weight for v0
             *   w1 = orient2d(v2, v0, p) -> weight for v1
             *   w2 = orient2d(v0, v1, p) -> weight for v2 */
            float w0 = forge_raster__orient2d(v1->x, v1->y,
                                              v2->x, v2->y, px, py);
            float w1 = forge_raster__orient2d(v2->x, v2->y,
                                              v0->x, v0->y, px, py);
            float w2 = forge_raster__orient2d(v0->x, v0->y,
                                              v1->x, v1->y, px, py);

            /* Inside test: the pixel is inside if all three edge functions
             * have the same sign.  This works for both CCW (all >= 0) and
             * CW (all <= 0) winding orders. */
            bool inside = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f) ||
                          (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
            if (!inside) continue;

            /* Normalize to barycentric coordinates.  Because w0+w1+w2 = area,
             * dividing by area gives weights that sum to 1.0.  These weights
             * tell us "how much" of each vertex influences this pixel. */
            float b0 = w0 * inv_area;
            float b1 = w1 * inv_area;
            float b2 = w2 * inv_area;

            /* Interpolate vertex colors using barycentric weights */
            float src_r = b0 * v0->r + b1 * v1->r + b2 * v2->r;
            float src_g = b0 * v0->g + b1 * v1->g + b2 * v2->g;
            float src_b = b0 * v0->b + b1 * v1->b + b2 * v2->b;
            float src_a = b0 * v0->a + b1 * v1->a + b2 * v2->a;

            /* Optional texture sampling: interpolate UVs and sample the
             * grayscale texture.  The texel value multiplies all four color
             * channels — this is the Dear ImGui rendering model where the
             * font atlas provides alpha coverage and the vertex color
             * provides the RGB tint. */
            if (texture && texture->pixels &&
                texture->width > 0 && texture->height > 0) {
                float tu = b0 * v0->u + b1 * v1->u + b2 * v2->u;
                float tv = b0 * v0->v + b1 * v1->v + b2 * v2->v;

                /* Clamp UVs to valid range */
                tu = forge_raster__clampf(tu, 0.0f, 1.0f);
                tv = forge_raster__clampf(tv, 0.0f, 1.0f);

                /* Nearest-neighbor sampling: map [0,1] to texel index */
                int tx = (int)(tu * (float)(texture->width  - 1) + 0.5f);
                int ty = (int)(tv * (float)(texture->height - 1) + 0.5f);
                tx = forge_raster__clamp_int(tx, 0, texture->width  - 1);
                ty = forge_raster__clamp_int(ty, 0, texture->height - 1);

                size_t texel_idx = (size_t)ty * (size_t)texture->width +
                                   (size_t)tx;
                float texel = forge_raster__to_float(
                    texture->pixels[texel_idx]);

                /* Multiply all channels by the texel value */
                src_r *= texel;
                src_g *= texel;
                src_b *= texel;
                src_a *= texel;
            }

            /* Alpha blend (source-over compositing).
             *
             * The source-over formula composites a partially transparent
             * source color onto the existing destination:
             *   out_rgb = src_rgb * src_a + dst_rgb * (1 - src_a)
             *   out_a   = src_a + dst_a * (1 - src_a)
             *
             * When src_a = 1.0 (fully opaque), the source completely
             * replaces the destination.  When src_a = 0.0 (fully
             * transparent), the destination is unchanged. */
            Uint8 *pixel = buf->pixels +
                           (size_t)y * (size_t)buf->stride +
                           (size_t)x * FORGE_RASTER_BPP;

            float dst_r = forge_raster__to_float(pixel[0]);
            float dst_g = forge_raster__to_float(pixel[1]);
            float dst_b = forge_raster__to_float(pixel[2]);
            float dst_a = forge_raster__to_float(pixel[3]);

            float inv_a = 1.0f - src_a;
            pixel[0] = forge_raster__to_byte(src_r * src_a + dst_r * inv_a);
            pixel[1] = forge_raster__to_byte(src_g * src_a + dst_g * inv_a);
            pixel[2] = forge_raster__to_byte(src_b * src_a + dst_b * inv_a);
            pixel[3] = forge_raster__to_byte(src_a + dst_a * inv_a);
        }
    }
}

/* ── Indexed Drawing ─────────────────────────────────────────────────────── */

static inline void forge_raster_triangles_indexed(ForgeRasterBuffer *buf,
                                                  const ForgeRasterVertex *vertices,
                                                  int vertex_count,
                                                  const Uint32 *indices,
                                                  int index_count,
                                                  const ForgeRasterTexture *texture)
{
    if (!buf || !buf->pixels || !vertices || !indices) return;
    if (vertex_count <= 0 || index_count <= 0) return;

    /* Every three indices form one triangle */
    for (int i = 0; i + 2 < index_count; i += 3) {
        Uint32 i0 = indices[i + 0];
        Uint32 i1 = indices[i + 1];
        Uint32 i2 = indices[i + 2];

        /* Validate each index against the vertex array bounds */
        if (i0 >= (Uint32)vertex_count ||
            i1 >= (Uint32)vertex_count ||
            i2 >= (Uint32)vertex_count) {
            SDL_Log("forge_raster_triangles_indexed: index out of bounds "
                    "(%u, %u, %u) with vertex_count=%d",
                    (unsigned)i0, (unsigned)i1, (unsigned)i2, vertex_count);
            continue;
        }

        forge_raster_triangle(buf,
                              &vertices[i0], &vertices[i1], &vertices[i2],
                              texture);
    }
}

/* ── BMP Writing ─────────────────────────────────────────────────────────── */

/* BMP file header sizes */
#define FORGE_RASTER__BMP_FILE_HEADER  14   /* BITMAPFILEHEADER */
#define FORGE_RASTER__BMP_INFO_HEADER  40   /* BITMAPINFOHEADER */

static inline bool forge_raster_write_bmp(const ForgeRasterBuffer *buf,
                                          const char *path)
{
    if (!buf || !buf->pixels || !path) {
        SDL_Log("forge_raster_write_bmp: invalid arguments");
        return false;
    }

    int width  = buf->width;
    int height = buf->height;

    /* Compute sizes using size_t to avoid integer overflow.  BMP headers
     * use 32-bit fields, so we validate the total fits in Uint32. */
    size_t bmp_row_bytes   = (size_t)width * FORGE_RASTER_BPP;
    size_t pixel_data_size = bmp_row_bytes * (size_t)height;
    Uint32 data_offset     = FORGE_RASTER__BMP_FILE_HEADER +
                             FORGE_RASTER__BMP_INFO_HEADER;
    size_t file_size_st    = (size_t)data_offset + pixel_data_size;

    if (file_size_st > (size_t)UINT32_MAX) {
        SDL_Log("forge_raster_write_bmp: image too large for BMP format");
        return false;
    }

    Uint32 file_size = (Uint32)file_size_st;
    Uint32 pix_size  = (Uint32)pixel_data_size;

    Uint8 *file_buf = (Uint8 *)SDL_calloc(file_size_st, 1);
    if (!file_buf) {
        SDL_Log("forge_raster_write_bmp: allocation failed (%zu bytes)",
                file_size_st);
        return false;
    }

    /* ── BITMAPFILEHEADER (14 bytes) ─────────────────────────────────── */
    file_buf[0] = 'B';
    file_buf[1] = 'M';
    file_buf[2]  = (Uint8)(file_size);
    file_buf[3]  = (Uint8)(file_size >> 8);
    file_buf[4]  = (Uint8)(file_size >> 16);
    file_buf[5]  = (Uint8)(file_size >> 24);
    /* bytes 6-9: reserved (zeros from calloc) */
    file_buf[10] = (Uint8)(data_offset);
    file_buf[11] = (Uint8)(data_offset >> 8);
    file_buf[12] = (Uint8)(data_offset >> 16);
    file_buf[13] = (Uint8)(data_offset >> 24);

    /* ── BITMAPINFOHEADER (40 bytes) ─────────────────────────────────── */
    Uint8 *info = file_buf + FORGE_RASTER__BMP_FILE_HEADER;
    info[0] = FORGE_RASTER__BMP_INFO_HEADER; /* header size */
    /* Width (little-endian int32) -- cast to Uint32 for portable shifting */
    Uint32 w = (Uint32)width;
    info[4]  = (Uint8)(w);
    info[5]  = (Uint8)(w >> 8);
    info[6]  = (Uint8)(w >> 16);
    info[7]  = (Uint8)(w >> 24);
    /* Height (positive = bottom-up row order in the file) */
    Uint32 h = (Uint32)height;
    info[8]  = (Uint8)(h);
    info[9]  = (Uint8)(h >> 8);
    info[10] = (Uint8)(h >> 16);
    info[11] = (Uint8)(h >> 24);
    /* Planes (always 1) */
    info[12] = 1;
    /* Bits per pixel (32 for BGRA) */
    info[14] = 32;
    /* Compression: BI_RGB = 0 (from calloc) */
    /* Image size (set explicitly for maximum BMP reader compatibility) */
    info[20] = (Uint8)(pix_size);
    info[21] = (Uint8)(pix_size >> 8);
    info[22] = (Uint8)(pix_size >> 16);
    info[23] = (Uint8)(pix_size >> 24);
    /* Resolution, colors: all 0 (from calloc) */

    /* ── Pixel data ──────────────────────────────────────────────────── */
    /* BMP stores rows bottom-up: file row 0 is the bottom of the image.
     * Our framebuffer is top-down (row 0 = top), so we flip vertically.
     * BMP 32-bit pixel order is B, G, R, A -- we convert from RGBA. */
    Uint8 *pixel_dst = file_buf + data_offset;
    for (int y = 0; y < height; y++) {
        int bmp_row = height - 1 - y;
        const Uint8 *src_row = buf->pixels +
                               (size_t)y * (size_t)buf->stride;
        Uint8 *dst_row = pixel_dst +
                         (size_t)bmp_row * bmp_row_bytes;

        for (int x = 0; x < width; x++) {
            const Uint8 *src = src_row + (size_t)x * FORGE_RASTER_BPP;
            Uint8 *dst       = dst_row + (size_t)x * FORGE_RASTER_BPP;
            dst[0] = src[2];  /* B <- our offset 2 (blue) */
            dst[1] = src[1];  /* G <- our offset 1 (green) */
            dst[2] = src[0];  /* R <- our offset 0 (red) */
            dst[3] = src[3];  /* A <- our offset 3 (alpha) */
        }
    }

    /* Write the assembled buffer to disk */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        SDL_Log("forge_raster_write_bmp: cannot open '%s' for writing", path);
        SDL_free(file_buf);
        return false;
    }

    size_t written = fwrite(file_buf, 1, file_size_st, fp);
    fclose(fp);
    SDL_free(file_buf);

    if (written != file_size_st) {
        SDL_Log("forge_raster_write_bmp: incomplete write (%zu / %zu bytes)",
                written, file_size_st);
        return false;
    }

    return true;
}

#endif /* FORGE_RASTER_H */
