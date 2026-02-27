/*
 * forge_ui.h -- Header-only TrueType font parser and rasterizer for forge-gpu
 *
 * Parses a TrueType (.ttf) font file and extracts table metadata, font
 * metrics, character-to-glyph mapping, and simple glyph outlines.  Also
 * rasterizes glyph outlines into single-channel alpha bitmaps using
 * scanline rasterization with the non-zero winding fill rule.
 *
 * Supports:
 *   - TTF offset table and table directory parsing
 *   - head table (unitsPerEm, bounding box, indexToLocFormat)
 *   - hhea table (ascender, descender, lineGap, numberOfHMetrics)
 *   - maxp table (numGlyphs)
 *   - cmap table with format 4 (BMP Unicode to glyph index mapping)
 *   - loca table (short and long format glyph offsets)
 *   - glyf table (simple glyph outlines with contours, flags, coordinates)
 *   - hmtx table (per-glyph advance widths and left side bearings)
 *   - Glyph rasterization with configurable supersampled anti-aliasing
 *   - Font atlas building (rectangle packing, UV coordinates, glyph metadata)
 *   - Grayscale BMP writing for atlas and glyph visualization
 *
 * Limitations (intentional for a learning library):
 *   - No compound glyph parsing (detected and skipped with a log message)
 *   - No hinting or grid-fitting instructions
 *   - No kerning (kern table) or advanced positioning (GPOS)
 *   - No glyph substitution (GSUB)
 *   - No sub-pixel rendering (ClearType-style RGB anti-aliasing)
 *   - TrueType outlines only (no CFF/OpenType outlines)
 *
 * Usage:
 *   #include "ui/forge_ui.h"
 *
 *   ForgeUiFont font;
 *   if (forge_ui_ttf_load("font.ttf", &font)) {
 *       Uint16 glyph_idx = forge_ui_ttf_glyph_index(&font, 'A');
 *       ForgeUiTtfGlyph glyph;
 *       if (forge_ui_ttf_load_glyph(&font, glyph_idx, &glyph)) {
 *           // glyph.points[], glyph.flags[], glyph.contour_ends[]
 *           forge_ui_ttf_glyph_free(&glyph);
 *       }
 *       forge_ui_ttf_free(&font);
 *   }
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_UI_H
#define FORGE_UI_H

#include <SDL3/SDL.h>
#include <stdio.h>   /* FILE, fopen, fwrite, fclose for BMP writing */
#include <limits.h>  /* INT_MAX for text layout validation */

/* ── Public Constants ────────────────────────────────────────────────────── */

/* Per-point flag: set when the point lies on the contour curve.
 * Off-curve points are quadratic Bézier control points. */
#define FORGE_UI_FLAG_ON_CURVE  0x01

/* ── Public Types ────────────────────────────────────────────────────────── */

/* A 2D point in font units (integer coordinates). */
typedef struct ForgeUiPoint {
    Sint16 x;  /* horizontal position in font units */
    Sint16 y;  /* vertical position in font units (y-up: ascender positive) */
} ForgeUiPoint;

/* An entry in the TTF table directory. */
typedef struct ForgeUiTtfTableEntry {
    char   tag[5];    /* 4-character tag + null terminator */
    Uint32 checksum;
    Uint32 offset;
    Uint32 length;
} ForgeUiTtfTableEntry;

/* head table -- global font metadata. */
typedef struct ForgeUiTtfHead {
    Uint16 units_per_em;     /* font design units per em square */
    Sint16 x_min;            /* global bounding box minimum x */
    Sint16 y_min;            /* global bounding box minimum y */
    Sint16 x_max;            /* global bounding box maximum x */
    Sint16 y_max;            /* global bounding box maximum y */
    Sint16 index_to_loc_fmt; /* 0 = short (uint16), 1 = long (uint32) */
} ForgeUiTtfHead;

/* hhea table -- horizontal header metrics. */
typedef struct ForgeUiTtfHhea {
    Sint16 ascender;             /* typographic ascender (positive) */
    Sint16 descender;            /* typographic descender (negative) */
    Sint16 line_gap;             /* additional spacing between lines */
    Uint16 number_of_h_metrics;  /* entries in hmtx table */
} ForgeUiTtfHhea;

/* maxp table -- maximum profile. */
typedef struct ForgeUiTtfMaxp {
    Uint16 num_glyphs;  /* total number of glyphs in the font */
} ForgeUiTtfMaxp;

/* A parsed simple glyph from the glyf table. */
typedef struct ForgeUiTtfGlyph {
    Sint16       x_min;          /* bounding box left edge (font units) */
    Sint16       y_min;          /* bounding box bottom edge (font units) */
    Sint16       x_max;          /* bounding box right edge (font units) */
    Sint16       y_max;          /* bounding box top edge (font units) */
    Uint16       contour_count;  /* number of contours */
    Uint16       point_count;    /* total number of points */
    Uint16      *contour_ends;   /* last point index of each contour */
    Uint8       *flags;          /* per-point flags (bit 0 = on-curve) */
    ForgeUiPoint *points;        /* absolute coordinates in font units */
} ForgeUiTtfGlyph;

/* Top-level font structure holding all parsed data. */
typedef struct ForgeUiFont {
    /* Raw file data (kept for on-demand glyph parsing) */
    Uint8  *data;
    size_t  data_size;

    /* Table directory */
    Uint16              num_tables;
    ForgeUiTtfTableEntry *tables;

    /* Parsed table data */
    ForgeUiTtfHead head;
    ForgeUiTtfHhea hhea;
    ForgeUiTtfMaxp maxp;

    /* cmap format 4 data (stored for glyph index lookups) */
    Uint16  cmap_seg_count;     /* number of segments in format 4 */
    Uint16 *cmap_end_codes;
    Uint16 *cmap_start_codes;
    Sint16 *cmap_id_deltas;
    Uint16 *cmap_id_range_offsets;
    const Uint8 *cmap_id_range_base; /* pointer into font data for range calc */

    /* loca table (glyph offsets into glyf) */
    Uint32 *loca_offsets;       /* numGlyphs + 1 entries, always uint32 */

    /* hmtx table (per-glyph horizontal metrics) */
    Uint16 *hmtx_advance_widths;    /* numberOfHMetrics entries */
    Sint16 *hmtx_left_side_bearings; /* numberOfHMetrics entries */
    Uint16  hmtx_last_advance;      /* advance width shared by trailing glyphs */

    /* Offsets to key tables within font data */
    Uint32 glyf_offset;        /* start of glyf table in file */
} ForgeUiFont;

/* ── Rasterization Types ─────────────────────────────────────────────────── */

/* Options controlling glyph rasterization quality.
 * supersample_level controls anti-aliasing:
 *   1 = no anti-aliasing (binary on/off per pixel)
 *   2 = 2x2 supersampling (4 samples per pixel)
 *   4 = 4x4 supersampling (16 samples per pixel, recommended)
 *   8 = 8x8 supersampling (64 samples per pixel, high quality) */
typedef struct ForgeUiRasterOpts {
    int supersample_level;  /* samples per pixel axis (1, 2, 4, or 8) */
} ForgeUiRasterOpts;

/* A rasterized glyph bitmap — single-channel alpha coverage.
 *
 * Each pixel is a uint8 coverage value: 0 = empty, 255 = fully covered.
 * This becomes the alpha channel in a font atlas texture — the actual text
 * color comes from vertex color or a uniform, not from this bitmap.
 *
 * Bearing offsets describe how the bitmap positions relative to the pen
 * position on the baseline:
 *   bearing_x: horizontal offset from the pen to the left edge of the bitmap
 *   bearing_y: vertical offset from the baseline to the top edge of the bitmap
 *
 * When rendering text, place the bitmap at:
 *   screen_x = pen_x + bearing_x
 *   screen_y = pen_y - bearing_y  (y-down screen coordinates) */
typedef struct ForgeUiGlyphBitmap {
    int     width;       /* bitmap width in pixels */
    int     height;      /* bitmap height in pixels */
    Uint8  *pixels;      /* width * height coverage values (row-major, top-down) */
    int     bearing_x;   /* horizontal offset from pen to bitmap left edge */
    int     bearing_y;   /* vertical offset from baseline to bitmap top edge */
} ForgeUiGlyphBitmap;

/* ── Font Atlas Types ────────────────────────────────────────────────────── */

/* UV rectangle within the atlas — normalized coordinates [0.0, 1.0].
 * (u0, v0) is the top-left corner, (u1, v1) is the bottom-right. */
typedef struct ForgeUiUVRect {
    float u0;  /* left edge in normalized atlas coordinates */
    float v0;  /* top edge in normalized atlas coordinates */
    float u1;  /* right edge in normalized atlas coordinates */
    float v1;  /* bottom edge in normalized atlas coordinates */
} ForgeUiUVRect;

/* Per-glyph metadata stored in the atlas.  Contains everything a renderer
 * and text layout system need to position and draw each character. */
typedef struct ForgeUiPackedGlyph {
    Uint32      codepoint;      /* Unicode codepoint this glyph represents */
    Uint16      glyph_index;    /* glyph index within the font */
    ForgeUiUVRect uv;           /* UV rectangle within the atlas texture */
    int         bitmap_w;       /* glyph bitmap width in pixels */
    int         bitmap_h;       /* glyph bitmap height in pixels */
    int         bearing_x;      /* horizontal offset from pen to bitmap left */
    int         bearing_y;      /* vertical offset from baseline to bitmap top */
    Uint16      advance_width;  /* horizontal advance in font units */
} ForgeUiPackedGlyph;

/* A font atlas — a single texture containing all requested glyphs plus
 * a white pixel region for solid-colored geometry rendering.
 *
 * The atlas stores key font metrics so that text layout can operate without
 * needing a separate ForgeUiFont reference.  All metric fields are set by
 * forge_ui_atlas_build() from the font's head and hhea tables. */
typedef struct ForgeUiFontAtlas {
    Uint8              *pixels;      /* atlas pixel data (single-channel, row-major) */
    int                 width;       /* atlas width in pixels (power of two) */
    int                 height;      /* atlas height in pixels (power of two) */
    ForgeUiPackedGlyph *glyphs;      /* per-glyph metadata array */
    int                 glyph_count; /* number of packed glyphs */
    ForgeUiUVRect       white_uv;    /* UV rect for the 2x2 white pixel region */

    /* Font metrics (set by forge_ui_atlas_build for text layout) */
    float               pixel_height;  /* pixel height used when building the atlas */
    Uint16              units_per_em;  /* font design units per em square */
    Sint16              ascender;      /* typographic ascender in font units (positive) */
    Sint16              descender;     /* typographic descender in font units (negative) */
    Sint16              line_gap;      /* additional inter-line spacing in font units */
} ForgeUiFontAtlas;

/* ── Text Layout Types ──────────────────────────────────────────────────── */

/* Universal UI vertex format: position + UV + color.
 * Position is in screen-space pixel coordinates (origin top-left, x-right,
 * y-down).  UV indexes into the font atlas texture.  Color is per-vertex
 * RGBA so different text blocks can have distinct colors without changing
 * pipeline state. */
typedef struct ForgeUiVertex {
    float pos_x;   /* screen-space x position in pixels */
    float pos_y;   /* screen-space y position in pixels */
    float uv_u;    /* horizontal atlas texture coordinate [0, 1] */
    float uv_v;    /* vertical atlas texture coordinate [0, 1] */
    float r;       /* red color component [0, 1] */
    float g;       /* green color component [0, 1] */
    float b;       /* blue color component [0, 1] */
    float a;       /* alpha color component [0, 1] */
} ForgeUiVertex;

/* Text alignment modes for multi-line text layout. */
typedef enum ForgeUiTextAlign {
    FORGE_UI_TEXT_ALIGN_LEFT   = 0,  /* left edge flush (default) */
    FORGE_UI_TEXT_ALIGN_CENTER = 1,  /* centered within max_width */
    FORGE_UI_TEXT_ALIGN_RIGHT  = 2   /* right edge flush at max_width */
} ForgeUiTextAlign;

/* Options controlling text layout behavior. */
typedef struct ForgeUiTextOpts {
    float             max_width;  /* line width limit in pixels (0 = no wrap) */
    ForgeUiTextAlign  alignment;  /* horizontal text alignment */
    float             r;          /* default text color: red [0, 1] */
    float             g;          /* default text color: green [0, 1] */
    float             b;          /* default text color: blue [0, 1] */
    float             a;          /* default text color: alpha [0, 1] */
} ForgeUiTextOpts;

/* Result of text layout — vertex and index arrays ready for GPU upload.
 * Vertices use ForgeUiVertex format.  Indices are uint32 forming CCW
 * triangle pairs (six indices per visible character quad). */
typedef struct ForgeUiTextLayout {
    ForgeUiVertex *vertices;     /* vertex array (4 per visible character) */
    int            vertex_count; /* total vertex count */
    Uint32        *indices;      /* index array (6 per visible character) */
    int            index_count;  /* total index count */
    float          total_width;  /* bounding box width in pixels */
    float          total_height; /* bounding box height in pixels */
    int            line_count;   /* number of lines produced */
} ForgeUiTextLayout;

/* Text measurement result — bounding box without generating vertices. */
typedef struct ForgeUiTextMetrics {
    float  width;       /* total bounding box width in pixels */
    float  height;      /* total bounding box height in pixels */
    int    line_count;  /* number of lines */
} ForgeUiTextMetrics;

/* ── Public API ──────────────────────────────────────────────────────────── */

/* Load a TTF font file and parse its table directory and core tables.
 * Returns true on success, false on error (logged via SDL_Log).
 * The font must be freed with forge_ui_ttf_free() when no longer needed. */
static bool forge_ui_ttf_load(const char *path, ForgeUiFont *out_font);

/* Free all memory allocated by forge_ui_ttf_load(). */
static void forge_ui_ttf_free(ForgeUiFont *font);

/* Look up the glyph index for a Unicode codepoint using the cmap table.
 * Returns 0 (the .notdef glyph) if the codepoint is not mapped. */
static Uint16 forge_ui_ttf_glyph_index(const ForgeUiFont *font,
                                        Uint32 codepoint);

/* Parse a simple glyph from the glyf table on demand.
 * Returns true on success, false if the glyph is compound or invalid.
 * The glyph must be freed with forge_ui_ttf_glyph_free(). */
static bool forge_ui_ttf_load_glyph(const ForgeUiFont *font,
                                     Uint16 glyph_index,
                                     ForgeUiTtfGlyph *out_glyph);

/* Free memory allocated by forge_ui_ttf_load_glyph(). */
static void forge_ui_ttf_glyph_free(ForgeUiTtfGlyph *glyph);

/* Rasterize a glyph into a single-channel alpha bitmap.
 *
 * Converts the glyph's quadratic Bézier outlines into a pixel grid using
 * scanline rasterization with the non-zero winding fill rule.  The bitmap
 * is sized from the glyph's bounding box scaled to the target pixel height.
 *
 * Parameters:
 *   font         — loaded font (provides unitsPerEm for scaling)
 *   glyph_index  — which glyph to rasterize (from forge_ui_ttf_glyph_index)
 *   pixel_height — desired height in pixels (e.g. 64.0f for 64px text)
 *   opts         — rasterization options (NULL for defaults: 4x4 supersample)
 *   out_bitmap   — receives the rasterized bitmap; caller must free with
 *                  forge_ui_glyph_bitmap_free()
 *
 * Returns true on success, false on error (logged via SDL_Log).
 * Returns true with zero-size bitmap for whitespace glyphs (no contours). */
static bool forge_ui_rasterize_glyph(const ForgeUiFont *font,
                                      Uint16 glyph_index,
                                      float pixel_height,
                                      const ForgeUiRasterOpts *opts,
                                      ForgeUiGlyphBitmap *out_bitmap);

/* Free pixel data allocated by forge_ui_rasterize_glyph(). */
static void forge_ui_glyph_bitmap_free(ForgeUiGlyphBitmap *bitmap);

/* ── hmtx API ───────────────────────────────────────────────────────────── */

/* Look up the advance width (in font units) for a glyph index.
 * The advance width tells the text layout system how far to move the pen
 * position after rendering this glyph.
 *
 * Glyphs with index < numberOfHMetrics have individual advance widths.
 * Glyphs at or beyond numberOfHMetrics share the last advance width in
 * the table (this is common in monospaced fonts where all glyphs share
 * the same width). */
static Uint16 forge_ui_ttf_advance_width(const ForgeUiFont *font,
                                          Uint16 glyph_index);

/* ── Font Atlas API ─────────────────────────────────────────────────────── */

/* Build a font atlas from a set of codepoints.
 *
 * Rasterizes every requested glyph at the specified pixel height using
 * the rasterizer from forge_ui_rasterize_glyph(), then packs all bitmaps
 * into a single power-of-two texture using shelf (row-based) packing.
 *
 * Parameters:
 *   font            — loaded font
 *   pixel_height    — glyph rendering height in pixels (e.g. 32.0f)
 *   codepoints      — array of Unicode codepoints to include
 *   codepoint_count — number of codepoints in the array
 *   padding         — pixels of empty space around each glyph (1–2 recommended)
 *   out_atlas       — receives the built atlas; caller must free with
 *                     forge_ui_atlas_free()
 *
 * Returns true on success, false on error (logged via SDL_Log). */
static bool forge_ui_atlas_build(const ForgeUiFont *font,
                                  float pixel_height,
                                  const Uint32 *codepoints,
                                  int codepoint_count,
                                  int padding,
                                  ForgeUiFontAtlas *out_atlas);

/* Free all memory allocated by forge_ui_atlas_build(). */
static void forge_ui_atlas_free(ForgeUiFontAtlas *atlas);

/* Look up a packed glyph by codepoint.
 * Returns a pointer to the ForgeUiPackedGlyph or NULL if not found.
 * The returned pointer is valid until the atlas is freed. */
static const ForgeUiPackedGlyph *forge_ui_atlas_lookup(
    const ForgeUiFontAtlas *atlas, Uint32 codepoint);

/* ── Text Layout API ────────────────────────────────────────────────────── */

/* Lay out a string of text into positioned, textured quads.
 *
 * Converts an ASCII string into vertex and index arrays suitable for GPU
 * rendering.  Each visible character becomes a quad (4 vertices, 6 indices)
 * with screen-space positions, atlas UV coordinates, and per-vertex color.
 *
 * Coordinates use a screen-space convention: origin at top-left, x increases
 * rightward, y increases downward.  The (x, y) parameter specifies the pen
 * starting position — x is the left edge of the first character, y is the
 * baseline of the first line.
 *
 * Parameters:
 *   atlas      — font atlas with glyph metadata and font metrics
 *   text       — null-terminated ASCII string to lay out
 *   x, y       — starting pen position (x = left edge, y = baseline)
 *   opts       — layout options (NULL for defaults: no wrap, left align, white)
 *   out_layout — receives vertex/index arrays; caller must free with
 *                forge_ui_text_layout_free()
 *
 * Returns true on success, false on error (logged via SDL_Log). */
static bool forge_ui_text_layout(const ForgeUiFontAtlas *atlas,
                                  const char *text,
                                  float x, float y,
                                  const ForgeUiTextOpts *opts,
                                  ForgeUiTextLayout *out_layout);

/* Free vertex and index arrays allocated by forge_ui_text_layout(). */
static void forge_ui_text_layout_free(ForgeUiTextLayout *layout);

/* Measure text dimensions without generating vertices.
 *
 * Performs the same layout calculation as forge_ui_text_layout() but only
 * computes the bounding box and line count.  Useful for centering text,
 * sizing UI containers, or pre-calculating layout before committing to
 * vertex generation.
 *
 * Parameters:
 *   atlas — font atlas with glyph metadata and font metrics
 *   text  — null-terminated ASCII string to measure
 *   opts  — layout options (max_width, alignment affect wrapping)
 *
 * Returns a ForgeUiTextMetrics with width, height, and line count. */
static ForgeUiTextMetrics forge_ui_text_measure(const ForgeUiFontAtlas *atlas,
                                                 const char *text,
                                                 const ForgeUiTextOpts *opts);

/* ── BMP Writing (internal helper) ──────────────────────────────────────── */

/* Write a single-channel grayscale bitmap as a BMP file.
 *
 * BMP format stores pixels bottom-up (row 0 = bottom of image) with each
 * row padded to a 4-byte boundary.  We write an 8-bit indexed BMP with a
 * 256-entry grayscale palette (0=black, 255=white).
 *
 * This is an internal helper (double-underscore prefix) shared across
 * lessons for writing atlas and glyph visualization images.
 *
 * Returns true on success, false on error (logged via SDL_Log). */
static bool forge_ui__write_grayscale_bmp(const char *path,
                                           const Uint8 *pixels,
                                           int width, int height);

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── Implementation ──────────────────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* ── Big-endian byte reading helpers ─────────────────────────────────────── */
/* TTF files store all multi-byte integers in big-endian (network) byte order.
 * These helpers read values from a raw byte pointer and convert to the host
 * byte order. */

static Uint16 forge_ui__read_u16(const Uint8 *p)
{
    return (Uint16)((Uint16)p[0] << 8 | (Uint16)p[1]);
}

static Sint16 forge_ui__read_i16(const Uint8 *p)
{
    return (Sint16)forge_ui__read_u16(p);
}

static Uint32 forge_ui__read_u32(const Uint8 *p)
{
    return (Uint32)p[0] << 24 | (Uint32)p[1] << 16 |
           (Uint32)p[2] << 8  | (Uint32)p[3];
}

/* ── Table directory helpers ─────────────────────────────────────────────── */

/* Find a table entry by its 4-character tag.
 * Returns NULL if the table is not present in the font. */
static const ForgeUiTtfTableEntry *forge_ui__find_table(
    const ForgeUiFont *font, const char *tag)
{
    for (Uint16 i = 0; i < font->num_tables; i++) {
        if (font->tables[i].tag[0] == tag[0] &&
            font->tables[i].tag[1] == tag[1] &&
            font->tables[i].tag[2] == tag[2] &&
            font->tables[i].tag[3] == tag[3]) {
            return &font->tables[i];
        }
    }
    return NULL;
}

/* ── Offset table parsing ────────────────────────────────────────────────── */
/* The offset table is the very first structure in a TTF file:
 *   bytes 0-3:  sfVersion (0x00010000 for TrueType)
 *   bytes 4-5:  numTables
 *   bytes 6-7:  searchRange
 *   bytes 8-9:  entrySelector
 *   bytes 10-11: rangeShift */

#define FORGE_UI__OFFSET_TABLE_SIZE 12
#define FORGE_UI__TABLE_ENTRY_SIZE  16

static bool forge_ui__parse_offset_table(ForgeUiFont *font)
{
    if (font->data_size < FORGE_UI__OFFSET_TABLE_SIZE) {
        SDL_Log("forge_ui__parse_offset_table: file too small for offset table");
        return false;
    }

    Uint32 sf_version = forge_ui__read_u32(font->data);
    if (sf_version != 0x00010000) {
        SDL_Log("forge_ui__parse_offset_table: unsupported sfVersion 0x%08X "
                "(expected 0x00010000 for TrueType)", sf_version);
        return false;
    }

    font->num_tables = forge_ui__read_u16(font->data + 4);

    /* Validate that the file is large enough for the table directory */
    size_t dir_end = FORGE_UI__OFFSET_TABLE_SIZE +
                     (size_t)font->num_tables * FORGE_UI__TABLE_ENTRY_SIZE;
    if (font->data_size < dir_end) {
        SDL_Log("forge_ui__parse_offset_table: file too small for %u table "
                "directory entries", font->num_tables);
        return false;
    }

    /* Parse each table directory entry */
    font->tables = (ForgeUiTtfTableEntry *)SDL_malloc(
        sizeof(ForgeUiTtfTableEntry) * font->num_tables);
    if (!font->tables) {
        SDL_Log("forge_ui__parse_offset_table: allocation failed");
        return false;
    }

    const Uint8 *entry = font->data + FORGE_UI__OFFSET_TABLE_SIZE;
    for (Uint16 i = 0; i < font->num_tables; i++) {
        font->tables[i].tag[0] = (char)entry[0];
        font->tables[i].tag[1] = (char)entry[1];
        font->tables[i].tag[2] = (char)entry[2];
        font->tables[i].tag[3] = (char)entry[3];
        font->tables[i].tag[4] = '\0';
        font->tables[i].checksum = forge_ui__read_u32(entry + 4);
        font->tables[i].offset   = forge_ui__read_u32(entry + 8);
        font->tables[i].length   = forge_ui__read_u32(entry + 12);

        /* Validate that the table's offset and length fit within the file.
         * Promote to uint64_t to avoid overflow when adding offset + length. */
        Uint64 tbl_offset = (Uint64)font->tables[i].offset;
        Uint64 tbl_length = (Uint64)font->tables[i].length;
        if (tbl_offset > font->data_size ||
            tbl_length > font->data_size ||
            tbl_offset + tbl_length > font->data_size) {
            SDL_Log("forge_ui__parse_offset_table: table '%.4s' "
                    "offset+length (%u+%u) exceeds file size (%zu)",
                    font->tables[i].tag,
                    font->tables[i].offset,
                    font->tables[i].length,
                    font->data_size);
            SDL_free(font->tables);
            font->tables = NULL;
            return false;
        }

        entry += FORGE_UI__TABLE_ENTRY_SIZE;
    }

    return true;
}

/* ── head table parsing ──────────────────────────────────────────────────── */
/* The head table contains global font metadata.
 * Key fields at fixed offsets within the table:
 *   offset 18: unitsPerEm   (uint16)
 *   offset 36: xMin         (int16)
 *   offset 38: yMin         (int16)
 *   offset 40: xMax         (int16)
 *   offset 42: yMax         (int16)
 *   offset 50: indexToLocFormat (int16) */

#define FORGE_UI__HEAD_MIN_SIZE 54

static bool forge_ui__parse_head(ForgeUiFont *font)
{
    const ForgeUiTtfTableEntry *t = forge_ui__find_table(font, "head");
    if (!t) {
        SDL_Log("forge_ui__parse_head: 'head' table not found");
        return false;
    }
    if (t->length < FORGE_UI__HEAD_MIN_SIZE) {
        SDL_Log("forge_ui__parse_head: 'head' table too small (%u bytes)",
                t->length);
        return false;
    }

    const Uint8 *p = font->data + t->offset;
    font->head.units_per_em     = forge_ui__read_u16(p + 18);
    font->head.x_min            = forge_ui__read_i16(p + 36);
    font->head.y_min            = forge_ui__read_i16(p + 38);
    font->head.x_max            = forge_ui__read_i16(p + 40);
    font->head.y_max            = forge_ui__read_i16(p + 42);
    font->head.index_to_loc_fmt = forge_ui__read_i16(p + 50);

    if (font->head.units_per_em == 0) {
        SDL_Log("forge_ui__parse_head: unitsPerEm is 0 (invalid)");
        return false;
    }

    return true;
}

/* ── hhea table parsing ──────────────────────────────────────────────────── */
/* The hhea table contains horizontal header metrics.
 *   offset 4:  ascender         (int16)
 *   offset 6:  descender        (int16)
 *   offset 8:  lineGap          (int16)
 *   offset 34: numberOfHMetrics (uint16) */

#define FORGE_UI__HHEA_MIN_SIZE 36

static bool forge_ui__parse_hhea(ForgeUiFont *font)
{
    const ForgeUiTtfTableEntry *t = forge_ui__find_table(font, "hhea");
    if (!t) {
        SDL_Log("forge_ui__parse_hhea: 'hhea' table not found");
        return false;
    }
    if (t->length < FORGE_UI__HHEA_MIN_SIZE) {
        SDL_Log("forge_ui__parse_hhea: 'hhea' table too small (%u bytes)",
                t->length);
        return false;
    }

    const Uint8 *p = font->data + t->offset;
    font->hhea.ascender             = forge_ui__read_i16(p + 4);
    font->hhea.descender            = forge_ui__read_i16(p + 6);
    font->hhea.line_gap             = forge_ui__read_i16(p + 8);
    font->hhea.number_of_h_metrics  = forge_ui__read_u16(p + 34);

    return true;
}

/* ── maxp table parsing ──────────────────────────────────────────────────── */
/* The maxp table provides the total glyph count.
 *   offset 4: numGlyphs (uint16) */

#define FORGE_UI__MAXP_MIN_SIZE 6

static bool forge_ui__parse_maxp(ForgeUiFont *font)
{
    const ForgeUiTtfTableEntry *t = forge_ui__find_table(font, "maxp");
    if (!t) {
        SDL_Log("forge_ui__parse_maxp: 'maxp' table not found");
        return false;
    }
    if (t->length < FORGE_UI__MAXP_MIN_SIZE) {
        SDL_Log("forge_ui__parse_maxp: 'maxp' table too small (%u bytes)",
                t->length);
        return false;
    }

    const Uint8 *p = font->data + t->offset;
    font->maxp.num_glyphs = forge_ui__read_u16(p + 4);

    return true;
}

/* ── cmap table parsing (format 4) ───────────────────────────────────────── */
/* The cmap table maps Unicode codepoints to glyph indices.  We look for a
 * platform 3 (Windows) / encoding 1 (Unicode BMP) subtable, or platform 0
 * (Unicode) as a fallback, then parse format 4 (segmented mapping).
 *
 * Format 4 structure (after the subtable header):
 *   offset 0:  format          (uint16, must be 4)
 *   offset 2:  length          (uint16)
 *   offset 6:  segCountX2      (uint16, segCount * 2)
 *   offset 8:  searchRange     (uint16)
 *   offset 10: entrySelector   (uint16)
 *   offset 12: rangeShift      (uint16)
 *   offset 14: endCode[]       (uint16 * segCount)
 *   then:      reservedPad     (uint16)
 *   then:      startCode[]     (uint16 * segCount)
 *   then:      idDelta[]       (int16 * segCount)
 *   then:      idRangeOffset[] (uint16 * segCount) */

static bool forge_ui__parse_cmap(ForgeUiFont *font)
{
    const ForgeUiTtfTableEntry *t = forge_ui__find_table(font, "cmap");
    if (!t) {
        SDL_Log("forge_ui__parse_cmap: 'cmap' table not found");
        return false;
    }

    const Uint8 *cmap = font->data + t->offset;
    Uint32 cmap_length = t->length;

    /* The cmap header has version (uint16) and numTables (uint16) -- 4 bytes */
    if (cmap_length < 4) {
        SDL_Log("forge_ui__parse_cmap: 'cmap' table too small for header "
                "(%u bytes)", cmap_length);
        return false;
    }

    Uint16 num_subtables = forge_ui__read_u16(cmap + 2);

    /* Validate that the subtable records fit within the cmap table.
     * Each record is 8 bytes, starting at offset 4. */
    size_t records_end = (size_t)num_subtables * 8 + 4;
    if (records_end < 4 || records_end > cmap_length) {
        SDL_Log("forge_ui__parse_cmap: %u subtable records exceed cmap "
                "table length (%u bytes)", num_subtables, cmap_length);
        return false;
    }

    /* Search for a suitable subtable:
     * Priority 1: platform 3 (Windows), encoding 1 (Unicode BMP)
     * Priority 2: platform 0 (Unicode), any encoding */
    Uint32 subtable_offset = 0;
    bool found = false;

    for (Uint16 i = 0; i < num_subtables; i++) {
        size_t rec_off = 4 + (size_t)i * 8;
        if (rec_off + 8 > cmap_length) {
            SDL_Log("forge_ui__parse_cmap: subtable record %u extends past "
                    "cmap table", i);
            return false;
        }
        const Uint8 *rec = cmap + rec_off;
        Uint16 platform = forge_ui__read_u16(rec);
        Uint16 encoding = forge_ui__read_u16(rec + 2);
        Uint32 offset   = forge_ui__read_u32(rec + 4);

        if (platform == 3 && encoding == 1) {
            subtable_offset = offset;
            found = true;
            break; /* Best match -- stop searching */
        }
        if (platform == 0 && !found) {
            subtable_offset = offset;
            found = true;
            /* Keep searching in case platform 3 appears later */
        }
    }

    if (!found) {
        SDL_Log("forge_ui__parse_cmap: no Unicode cmap subtable found");
        return false;
    }

    /* Validate subtable offset before reading the format field */
    if (subtable_offset + 2 > cmap_length) {
        SDL_Log("forge_ui__parse_cmap: subtable offset %u exceeds cmap "
                "table length (%u)", subtable_offset, cmap_length);
        return false;
    }

    /* Parse the subtable -- we only support format 4 */
    const Uint8 *sub = cmap + subtable_offset;
    Uint16 format = forge_ui__read_u16(sub);
    if (format != 4) {
        SDL_Log("forge_ui__parse_cmap: unsupported cmap format %u "
                "(only format 4 is implemented)", format);
        return false;
    }

    /* Format 4 header is 14 bytes; validate before reading fields */
    size_t sub_avail = cmap_length - subtable_offset;
    if (sub_avail < 14) {
        SDL_Log("forge_ui__parse_cmap: format 4 subtable header truncated "
                "(%zu bytes available)", sub_avail);
        return false;
    }

    Uint16 seg_count_x2 = forge_ui__read_u16(sub + 6);

    /* seg_count_x2 must be nonzero and even */
    if (seg_count_x2 == 0 || (seg_count_x2 & 1) != 0) {
        SDL_Log("forge_ui__parse_cmap: invalid segCountX2 = %u "
                "(must be nonzero and even)", seg_count_x2);
        return false;
    }

    Uint16 seg_count = seg_count_x2 / 2;
    font->cmap_seg_count = seg_count;

    /* Validate that sub_avail has room for the four parallel arrays plus
     * the 14-byte header and the 2-byte reservedPad between endCode and
     * startCode:  14 + segCountX2 + 2 + segCountX2 * 3 = 16 + segCountX2*4 */
    size_t arrays_end = 16 + (size_t)seg_count_x2 * 4;
    if (arrays_end > sub_avail) {
        SDL_Log("forge_ui__parse_cmap: format 4 segment arrays require "
                "%zu bytes but subtable has %zu", arrays_end, sub_avail);
        return false;
    }

    /* Allocate arrays for the four parallel segment arrays */
    font->cmap_end_codes = (Uint16 *)SDL_malloc(
        sizeof(Uint16) * seg_count);
    font->cmap_start_codes = (Uint16 *)SDL_malloc(
        sizeof(Uint16) * seg_count);
    font->cmap_id_deltas = (Sint16 *)SDL_malloc(
        sizeof(Sint16) * seg_count);
    font->cmap_id_range_offsets = (Uint16 *)SDL_malloc(
        sizeof(Uint16) * seg_count);

    if (!font->cmap_end_codes || !font->cmap_start_codes ||
        !font->cmap_id_deltas || !font->cmap_id_range_offsets) {
        SDL_Log("forge_ui__parse_cmap: allocation failed");
        SDL_free(font->cmap_end_codes);
        SDL_free(font->cmap_start_codes);
        SDL_free(font->cmap_id_deltas);
        SDL_free(font->cmap_id_range_offsets);
        font->cmap_end_codes       = NULL;
        font->cmap_start_codes     = NULL;
        font->cmap_id_deltas       = NULL;
        font->cmap_id_range_offsets = NULL;
        return false;
    }

    /* endCode array starts at offset 14 in the subtable */
    const Uint8 *end_codes   = sub + 14;
    /* startCode array is after endCode + 2-byte reservedPad */
    const Uint8 *start_codes = end_codes + seg_count_x2 + 2;
    /* idDelta array follows startCode */
    const Uint8 *id_deltas   = start_codes + seg_count_x2;
    /* idRangeOffset array follows idDelta */
    const Uint8 *id_ranges   = id_deltas + seg_count_x2;

    for (Uint16 i = 0; i < seg_count; i++) {
        font->cmap_end_codes[i]         = forge_ui__read_u16(end_codes + i * 2);
        font->cmap_start_codes[i]       = forge_ui__read_u16(start_codes + i * 2);
        font->cmap_id_deltas[i]         = forge_ui__read_i16(id_deltas + i * 2);
        font->cmap_id_range_offsets[i]  = forge_ui__read_u16(id_ranges + i * 2);
    }

    /* Store a pointer to the idRangeOffset array in the raw data so we can
     * perform glyph index lookups that use non-zero idRangeOffset values.
     * The spec defines that when idRangeOffset[i] != 0, the glyph index is
     * at: *(idRangeOffset[i]/2 + (c - startCode[i]) + &idRangeOffset[i])
     * which means we need the actual address within the data. */
    font->cmap_id_range_base = (const Uint8 *)id_ranges;

    return true;
}

/* ── loca table parsing ──────────────────────────────────────────────────── */
/* The loca table maps glyph indices to byte offsets within the glyf table.
 * It has numGlyphs + 1 entries so you can compute each glyph's size by
 * subtracting consecutive offsets.
 *
 * Short format (indexToLocFormat == 0): offsets stored as uint16, actual
 *   byte offset = stored_value * 2
 * Long format (indexToLocFormat == 1): offsets stored as uint32 directly */

static bool forge_ui__parse_loca(ForgeUiFont *font)
{
    const ForgeUiTtfTableEntry *t = forge_ui__find_table(font, "loca");
    if (!t) {
        SDL_Log("forge_ui__parse_loca: 'loca' table not found");
        return false;
    }

    Uint32 count = (Uint32)font->maxp.num_glyphs + 1;

    /* indexToLocFormat must be 0 (short) or 1 (long) */
    if (font->head.index_to_loc_fmt != 0 && font->head.index_to_loc_fmt != 1) {
        SDL_Log("forge_ui__parse_loca: invalid index_to_loc_fmt %d "
                "(must be 0 or 1)", font->head.index_to_loc_fmt);
        return false;
    }

    /* Validate that the loca table is large enough for all entries */
    size_t required_bytes = (font->head.index_to_loc_fmt == 0)
                                ? (size_t)count * 2
                                : (size_t)count * 4;
    if (t->length < required_bytes) {
        SDL_Log("forge_ui__parse_loca: 'loca' table too small (%u bytes) "
                "for %u entries (need %zu bytes)",
                t->length, count, required_bytes);
        return false;
    }

    font->loca_offsets = (Uint32 *)SDL_malloc(sizeof(Uint32) * count);
    if (!font->loca_offsets) {
        SDL_Log("forge_ui__parse_loca: allocation failed");
        return false;
    }

    const Uint8 *p = font->data + t->offset;

    if (font->head.index_to_loc_fmt == 0) {
        /* Short format: uint16 offsets, multiply by 2 to get byte offset */
        for (Uint32 i = 0; i < count; i++) {
            font->loca_offsets[i] = (Uint32)forge_ui__read_u16(p + i * 2) * 2;
        }
    } else {
        /* Long format: uint32 offsets used directly */
        for (Uint32 i = 0; i < count; i++) {
            font->loca_offsets[i] = forge_ui__read_u32(p + i * 4);
        }
    }

    return true;
}

/* ── hmtx table parsing ──────────────────────────────────────────────────── */
/* The hmtx table contains per-glyph horizontal metrics: advance width and
 * left side bearing.  The first numberOfHMetrics entries each have both
 * fields (4 bytes: uint16 advanceWidth + int16 lsb).  Glyphs beyond
 * numberOfHMetrics share the last advance width and only store an lsb. */

static bool forge_ui__parse_hmtx(ForgeUiFont *font)
{
    const ForgeUiTtfTableEntry *t = forge_ui__find_table(font, "hmtx");
    if (!t) {
        SDL_Log("forge_ui__parse_hmtx: 'hmtx' table not found");
        return false;
    }

    Uint16 num_h_metrics = font->hhea.number_of_h_metrics;
    if (num_h_metrics == 0) {
        SDL_Log("forge_ui__parse_hmtx: numberOfHMetrics is 0 (invalid)");
        return false;
    }

    /* Validate table size: need 4 bytes per longHorMetric entry */
    size_t required = (size_t)num_h_metrics * 4;
    if (t->length < required) {
        SDL_Log("forge_ui__parse_hmtx: 'hmtx' table too small (%u bytes) "
                "for %u entries (need %zu bytes)",
                t->length, num_h_metrics, required);
        return false;
    }

    font->hmtx_advance_widths = (Uint16 *)SDL_malloc(
        sizeof(Uint16) * num_h_metrics);
    font->hmtx_left_side_bearings = (Sint16 *)SDL_malloc(
        sizeof(Sint16) * num_h_metrics);

    if (!font->hmtx_advance_widths || !font->hmtx_left_side_bearings) {
        SDL_Log("forge_ui__parse_hmtx: allocation failed");
        SDL_free(font->hmtx_advance_widths);
        SDL_free(font->hmtx_left_side_bearings);
        font->hmtx_advance_widths = NULL;
        font->hmtx_left_side_bearings = NULL;
        return false;
    }

    const Uint8 *p = font->data + t->offset;
    for (Uint16 i = 0; i < num_h_metrics; i++) {
        font->hmtx_advance_widths[i]     = forge_ui__read_u16(p + (size_t)i * 4);
        font->hmtx_left_side_bearings[i] = forge_ui__read_i16(p + (size_t)i * 4 + 2);
    }

    /* Store the last advance width for glyphs beyond numberOfHMetrics */
    font->hmtx_last_advance = font->hmtx_advance_widths[num_h_metrics - 1];

    return true;
}

/* ── glyf offset caching ────────────────────────────────────────────────── */

static bool forge_ui__cache_glyf_offset(ForgeUiFont *font)
{
    const ForgeUiTtfTableEntry *t = forge_ui__find_table(font, "glyf");
    if (!t) {
        SDL_Log("forge_ui__cache_glyf_offset: 'glyf' table not found");
        return false;
    }
    font->glyf_offset = t->offset;
    return true;
}

/* ── Glyph flag constants ────────────────────────────────────────────────── */
/* Flags for each point in a simple glyph outline (from the glyf table).
 * These control whether the point is on-curve and how its coordinates
 * are encoded. */

#define FORGE_UI__FLAG_ON_CURVE    FORGE_UI_FLAG_ON_CURVE
#define FORGE_UI__FLAG_X_SHORT     0x02  /* x coordinate is 1 byte */
#define FORGE_UI__FLAG_Y_SHORT     0x04  /* y coordinate is 1 byte */
#define FORGE_UI__FLAG_REPEAT      0x08  /* next byte is repeat count */
#define FORGE_UI__FLAG_X_SAME      0x10  /* x is same (short=positive) */
#define FORGE_UI__FLAG_Y_SAME      0x20  /* y is same (short=positive) */

/* ── Public function implementations ─────────────────────────────────────── */

static bool forge_ui_ttf_load(const char *path, ForgeUiFont *out_font)
{
    if (!out_font) {
        SDL_Log("forge_ui_ttf_load: out_font is NULL");
        return false;
    }
    SDL_memset(out_font, 0, sizeof(ForgeUiFont));

    if (!path) {
        SDL_Log("forge_ui_ttf_load: path is NULL");
        return false;
    }

    /* Load the entire file into memory.  We keep the data around because
     * glyph parsing happens on demand and reads directly from the buffer. */
    out_font->data = (Uint8 *)SDL_LoadFile(path, &out_font->data_size);
    if (!out_font->data) {
        SDL_Log("forge_ui_ttf_load: failed to load '%s': %s",
                path, SDL_GetError());
        return false;
    }

    /* Parse the offset table and table directory */
    if (!forge_ui__parse_offset_table(out_font)) {
        forge_ui_ttf_free(out_font);
        return false;
    }

    /* Parse required tables in dependency order:
     * head first (indexToLocFormat needed by loca),
     * maxp next (numGlyphs needed by loca),
     * then hhea (numberOfHMetrics needed by hmtx),
     * then cmap, loca, hmtx, and cache glyf offset */
    if (!forge_ui__parse_head(out_font)  ||
        !forge_ui__parse_maxp(out_font)  ||
        !forge_ui__parse_hhea(out_font)  ||
        !forge_ui__parse_cmap(out_font)  ||
        !forge_ui__parse_loca(out_font)  ||
        !forge_ui__parse_hmtx(out_font)  ||
        !forge_ui__cache_glyf_offset(out_font)) {
        forge_ui_ttf_free(out_font);
        return false;
    }

    return true;
}

static void forge_ui_ttf_free(ForgeUiFont *font)
{
    if (!font) return;

    SDL_free(font->hmtx_left_side_bearings);
    SDL_free(font->hmtx_advance_widths);
    SDL_free(font->loca_offsets);
    SDL_free(font->cmap_id_range_offsets);
    SDL_free(font->cmap_id_deltas);
    SDL_free(font->cmap_start_codes);
    SDL_free(font->cmap_end_codes);
    SDL_free(font->tables);
    SDL_free(font->data);

    SDL_memset(font, 0, sizeof(ForgeUiFont));
}

static Uint16 forge_ui_ttf_glyph_index(const ForgeUiFont *font,
                                         Uint32 codepoint)
{
    if (!font) return 0;

    /* cmap format 4 only supports the Basic Multilingual Plane (0-65535) */
    if (codepoint > 0xFFFF) {
        return 0;
    }
    Uint16 cp = (Uint16)codepoint;

    /* Search the segment array for the segment containing this codepoint.
     * Segments are sorted by endCode, so we scan until endCode >= cp. */
    for (Uint16 i = 0; i < font->cmap_seg_count; i++) {
        if (font->cmap_end_codes[i] >= cp) {
            if (font->cmap_start_codes[i] > cp) {
                /* Codepoint falls in a gap between segments */
                return 0;
            }

            if (font->cmap_id_range_offsets[i] == 0) {
                /* Simple case: glyph index = codepoint + idDelta */
                return (Uint16)(cp + font->cmap_id_deltas[i]);
            }

            /* Complex case: use idRangeOffset to index into a glyph array.
             * The formula from the spec:
             *   addr = idRangeOffset[i] + 2*(cp - startCode[i])
             *          + &idRangeOffset[i]
             *   glyph_index = *addr
             *   if (glyph_index != 0) glyph_index += idDelta[i] */
            Uint32 range_offset = font->cmap_id_range_offsets[i];
            Uint32 char_offset = (Uint32)(cp - font->cmap_start_codes[i]);
            size_t byte_offset = (size_t)i * 2 +
                                 (size_t)range_offset +
                                 (size_t)char_offset * 2;
            const Uint8 *glyph_addr = font->cmap_id_range_base + byte_offset;

            /* Validate that glyph_addr + 2 is within the font buffer */
            if (glyph_addr < font->data ||
                glyph_addr + 2 > font->data + font->data_size) {
                return 0; /* out-of-bounds — return .notdef */
            }

            Uint16 glyph_index = forge_ui__read_u16(glyph_addr);
            if (glyph_index != 0) {
                glyph_index = (Uint16)(glyph_index + font->cmap_id_deltas[i]);
            }
            return glyph_index;
        }
    }

    return 0; /* .notdef */
}

static bool forge_ui_ttf_load_glyph(const ForgeUiFont *font,
                                      Uint16 glyph_index,
                                      ForgeUiTtfGlyph *out_glyph)
{
    if (!font || !out_glyph) {
        SDL_Log("forge_ui_ttf_load_glyph: NULL argument");
        return false;
    }
    SDL_memset(out_glyph, 0, sizeof(ForgeUiTtfGlyph));

    if (glyph_index >= font->maxp.num_glyphs) {
        SDL_Log("forge_ui_ttf_load_glyph: glyph index %u out of range "
                "(font has %u glyphs)", glyph_index, font->maxp.num_glyphs);
        return false;
    }

    /* Use loca to find the glyph's offset and size within glyf */
    Uint32 glyph_offset = font->loca_offsets[glyph_index];
    Uint32 next_offset  = font->loca_offsets[glyph_index + 1];

    /* A zero-length glyph (e.g. space) has no outline data */
    if (glyph_offset == next_offset) {
        /* Valid glyph with no contours (whitespace, etc.) */
        return true;
    }

    /* Validate loca entries: next must be >= current (otherwise the
     * subtraction below would underflow), and the glyph data must
     * fit within the file buffer. */
    if (next_offset < glyph_offset) {
        SDL_Log("forge_ui_ttf_load_glyph: malformed loca — "
                "next_offset (%u) < glyph_offset (%u) for glyph %u",
                next_offset, glyph_offset, glyph_index);
        return false;
    }
    if ((Uint64)font->glyf_offset + next_offset > font->data_size) {
        SDL_Log("forge_ui_ttf_load_glyph: glyph %u extends beyond file "
                "bounds (glyf_offset %u + next_offset %u > data_size %zu)",
                glyph_index, font->glyf_offset, next_offset,
                font->data_size);
        return false;
    }

    /* Compute glyph data bounds for all subsequent reads */
    const Uint8 *glyph_start = font->data + font->glyf_offset + glyph_offset;
    const Uint8 *glyph_end   = glyph_start + (next_offset - glyph_offset);
    const Uint8 *p = glyph_start;

    /* The glyph header is 10 bytes:
     *   offset 0: numberOfContours (int16) -- negative means compound
     *   offset 2: xMin (int16)
     *   offset 4: yMin (int16)
     *   offset 6: xMax (int16)
     *   offset 8: yMax (int16) */
    if (p + 10 > glyph_end) {
        SDL_Log("forge_ui_ttf_load_glyph: glyph %u data too small for header",
                glyph_index);
        return false;
    }

    Sint16 num_contours = forge_ui__read_i16(p);

    if (num_contours < 0) {
        SDL_Log("forge_ui_ttf_load_glyph: glyph %u is a compound glyph "
                "(numberOfContours = %d) -- skipping (not implemented)",
                glyph_index, num_contours);
        return false;
    }

    out_glyph->x_min = forge_ui__read_i16(p + 2);
    out_glyph->y_min = forge_ui__read_i16(p + 4);
    out_glyph->x_max = forge_ui__read_i16(p + 6);
    out_glyph->y_max = forge_ui__read_i16(p + 8);
    out_glyph->contour_count = (Uint16)num_contours;

    if (num_contours == 0) {
        return true;
    }

    /* ── Parse contour endpoints ─────────────────────────────────────── */
    /* After the 10-byte header, there are numContours uint16 values
     * giving the index of the last point in each contour. */
    const Uint8 *contour_data = p + 10;

    /* Bounds check: contour endpoints array */
    if (contour_data + (size_t)num_contours * 2 > glyph_end) {
        SDL_Log("forge_ui_ttf_load_glyph: glyph %u contour endpoints "
                "extend past glyph data", glyph_index);
        return false;
    }

    out_glyph->contour_ends = (Uint16 *)SDL_malloc(
        sizeof(Uint16) * (size_t)num_contours);
    if (!out_glyph->contour_ends) {
        SDL_Log("forge_ui_ttf_load_glyph: allocation failed (contour_ends)");
        return false;
    }

    for (int i = 0; i < num_contours; i++) {
        out_glyph->contour_ends[i] = forge_ui__read_u16(
            contour_data + (size_t)i * 2);
    }

    /* Total point count = last contour endpoint + 1 */
    Uint32 raw_point_count =
        (Uint32)out_glyph->contour_ends[num_contours - 1] + 1;
    if (raw_point_count > UINT16_MAX) {
        SDL_Log("forge_ui_ttf_load_glyph: glyph %u point count %u exceeds "
                "Uint16 range", glyph_index, raw_point_count);
        forge_ui_ttf_glyph_free(out_glyph);
        return false;
    }
    Uint16 point_count = (Uint16)raw_point_count;
    out_glyph->point_count = point_count;

    /* ── Skip hinting instructions ───────────────────────────────────── */
    /* After contour endpoints: uint16 instructionLength, then that many
     * bytes of instructions.  We skip them entirely. */
    const Uint8 *instr_ptr = contour_data + (size_t)num_contours * 2;

    /* Bounds check: instruction length field (2 bytes) */
    if (instr_ptr + 2 > glyph_end) {
        SDL_Log("forge_ui_ttf_load_glyph: glyph %u instruction length "
                "extends past glyph data", glyph_index);
        forge_ui_ttf_glyph_free(out_glyph);
        return false;
    }

    Uint16 instr_length = forge_ui__read_u16(instr_ptr);

    /* Bounds check: instruction bytes */
    if (instr_ptr + 2 + instr_length > glyph_end) {
        SDL_Log("forge_ui_ttf_load_glyph: glyph %u instruction data "
                "extends past glyph data", glyph_index);
        forge_ui_ttf_glyph_free(out_glyph);
        return false;
    }

    const Uint8 *flag_ptr = instr_ptr + 2 + instr_length;

    /* ── Parse flags (with repeat expansion) ─────────────────────────── */
    /* Each point has a flag byte.  If REPEAT is set, the next byte tells
     * how many additional times this flag repeats.  This compresses runs
     * of identical flags. */
    out_glyph->flags = (Uint8 *)SDL_malloc(point_count);
    if (!out_glyph->flags) {
        SDL_Log("forge_ui_ttf_load_glyph: allocation failed (flags)");
        forge_ui_ttf_glyph_free(out_glyph);
        return false;
    }

    Uint16 flags_read = 0;
    const Uint8 *fp = flag_ptr;
    while (flags_read < point_count) {
        if (fp + 1 > glyph_end) {
            SDL_Log("forge_ui_ttf_load_glyph: glyph %u flag data "
                    "extends past glyph data", glyph_index);
            forge_ui_ttf_glyph_free(out_glyph);
            return false;
        }
        Uint8 flag = *fp++;
        out_glyph->flags[flags_read++] = flag;

        if (flag & FORGE_UI__FLAG_REPEAT) {
            if (fp + 1 > glyph_end) {
                SDL_Log("forge_ui_ttf_load_glyph: glyph %u repeat count "
                        "extends past glyph data", glyph_index);
                forge_ui_ttf_glyph_free(out_glyph);
                return false;
            }
            Uint8 repeat_count = *fp++;
            for (Uint8 r = 0; r < repeat_count && flags_read < point_count; r++) {
                out_glyph->flags[flags_read++] = flag;
            }
        }
    }

    /* ── Parse x coordinates (delta-encoded) ─────────────────────────── */
    /* X coordinates are stored as deltas from the previous point.
     * The encoding depends on flags:
     *   X_SHORT set, X_SAME set:   1-byte positive delta
     *   X_SHORT set, X_SAME clear: 1-byte negative delta
     *   X_SHORT clear, X_SAME set: delta is 0 (x unchanged)
     *   X_SHORT clear, X_SAME clear: 2-byte signed delta */
    out_glyph->points = (ForgeUiPoint *)SDL_malloc(
        sizeof(ForgeUiPoint) * point_count);
    if (!out_glyph->points) {
        SDL_Log("forge_ui_ttf_load_glyph: allocation failed (points)");
        forge_ui_ttf_glyph_free(out_glyph);
        return false;
    }

    const Uint8 *coord_ptr = fp; /* fp points past the flags data */
    Sint16 x = 0;
    for (Uint16 i = 0; i < point_count; i++) {
        Uint8 flag = out_glyph->flags[i];
        if (flag & FORGE_UI__FLAG_X_SHORT) {
            if (coord_ptr + 1 > glyph_end) {
                SDL_Log("forge_ui_ttf_load_glyph: glyph %u x-coord data "
                        "extends past glyph data", glyph_index);
                forge_ui_ttf_glyph_free(out_glyph);
                return false;
            }
            Uint8 dx = *coord_ptr++;
            x += (flag & FORGE_UI__FLAG_X_SAME) ? (Sint16)dx : -(Sint16)dx;
        } else {
            if (!(flag & FORGE_UI__FLAG_X_SAME)) {
                if (coord_ptr + 2 > glyph_end) {
                    SDL_Log("forge_ui_ttf_load_glyph: glyph %u x-coord data "
                            "extends past glyph data", glyph_index);
                    forge_ui_ttf_glyph_free(out_glyph);
                    return false;
                }
                x += forge_ui__read_i16(coord_ptr);
                coord_ptr += 2;
            }
            /* else: X_SAME without X_SHORT means delta is 0 */
        }
        out_glyph->points[i].x = x;
    }

    /* ── Parse y coordinates (same encoding scheme as x) ─────────────── */
    Sint16 y = 0;
    for (Uint16 i = 0; i < point_count; i++) {
        Uint8 flag = out_glyph->flags[i];
        if (flag & FORGE_UI__FLAG_Y_SHORT) {
            if (coord_ptr + 1 > glyph_end) {
                SDL_Log("forge_ui_ttf_load_glyph: glyph %u y-coord data "
                        "extends past glyph data", glyph_index);
                forge_ui_ttf_glyph_free(out_glyph);
                return false;
            }
            Uint8 dy = *coord_ptr++;
            y += (flag & FORGE_UI__FLAG_Y_SAME) ? (Sint16)dy : -(Sint16)dy;
        } else {
            if (!(flag & FORGE_UI__FLAG_Y_SAME)) {
                if (coord_ptr + 2 > glyph_end) {
                    SDL_Log("forge_ui_ttf_load_glyph: glyph %u y-coord data "
                            "extends past glyph data", glyph_index);
                    forge_ui_ttf_glyph_free(out_glyph);
                    return false;
                }
                y += forge_ui__read_i16(coord_ptr);
                coord_ptr += 2;
            }
        }
        out_glyph->points[i].y = y;
    }

    return true;
}

static void forge_ui_ttf_glyph_free(ForgeUiTtfGlyph *glyph)
{
    if (!glyph) return;

    SDL_free(glyph->points);
    SDL_free(glyph->flags);
    SDL_free(glyph->contour_ends);

    SDL_memset(glyph, 0, sizeof(ForgeUiTtfGlyph));
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── Glyph Rasterization Implementation ─────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* ── Edge types for scanline rasterization ───────────────────────────────── */
/* An edge is one segment of a glyph contour — either a line segment or a
 * quadratic Bézier curve.  During rasterization we iterate all edges for
 * each scanline and find where they cross that y-coordinate. */

#define FORGE_UI__EDGE_LINE     0
#define FORGE_UI__EDGE_QUAD     1

/* Maximum edges per glyph.  Most glyphs have far fewer than this.
 * Each contour segment (line or curve) becomes one edge. */
#define FORGE_UI__MAX_EDGES     4096

/* Maximum scanline crossings per row.  Even complex glyphs rarely exceed
 * a few dozen crossings per scanline. */
#define FORGE_UI__MAX_CROSSINGS 256

/* Bitmap padding in pixels.  A small margin prevents edge pixels from
 * being clipped at the bitmap boundary. */
#define FORGE_UI__BITMAP_PAD    1

/* Default supersampling level when no ForgeUiRasterOpts are provided.
 * 4 means 4x4 = 16 samples per pixel — good balance of quality and speed. */
#define FORGE_UI__DEFAULT_SS    4

/* Floating-point comparison epsilon for near-zero tests in the quadratic
 * solver and degenerate edge detection. */
#define FORGE_UI__EPSILON       1e-6f

typedef struct ForgeUi__Edge {
    int type;           /* FORGE_UI__EDGE_LINE or FORGE_UI__EDGE_QUAD */
    float x0, y0;       /* start point (scaled pixels, y-flipped) */
    float x1, y1;       /* end point for lines; or control point for quads */
    float x2, y2;       /* end point for quads (unused for lines) */
    int   winding;      /* +1 or -1 depending on contour direction */
} ForgeUi__Edge;

/* A scanline crossing: the x position and winding direction where a
 * contour edge crosses a given y-coordinate. */
typedef struct ForgeUi__Crossing {
    float x;       /* x-coordinate where the edge crosses the scanline */
    int   winding; /* +1 (upward crossing) or -1 (downward crossing) */
} ForgeUi__Crossing;

/* ── Contour reconstruction helpers ──────────────────────────────────────── */
/* Walk a contour's points and emit edges (lines and quadratic Béziers).
 *
 * TrueType contours encode curves with on-curve and off-curve points:
 *   on → on:  straight line segment
 *   on → off → on:  quadratic Bézier curve
 *   off → off:  implicit on-curve midpoint between them (TrueType compression)
 *
 * The implicit midpoint rule halves storage for smooth curves: if two
 * consecutive off-curve points exist, the midpoint between them is an
 * implied on-curve point.  This works because most glyph curves join
 * smoothly, and the midpoint is exactly where the tangent directions
 * would naturally meet. */

static int forge_ui__build_edges(
    const ForgeUiTtfGlyph *glyph,
    float scale,
    float y_offset,  /* bitmap top edge in font units (y_max * scale + pad) */
    ForgeUi__Edge *edges,
    int max_edges)
{
    int edge_count = 0;
    bool overflow_logged = false;

    for (int c = 0; c < glyph->contour_count; c++) {
        Uint16 end = glyph->contour_ends[c];

        /* Validate contour endpoint is within the point array */
        if (end >= glyph->point_count) continue;

        Uint16 start = (c == 0) ? 0 : (Uint16)(glyph->contour_ends[c - 1] + 1);

        /* Validate monotonicity: start must not exceed end */
        if (start > end) continue;

        int count = (int)(end - start + 1);

        if (count < 2) continue;

        /* Walk the contour and emit edges.  We need to handle the three
         * cases: on→on (line), on→off→on (Bézier), off→off (implicit
         * midpoint between them). */

        /* Find the first on-curve point to start from.  If the first point
         * is off-curve, compute the implicit midpoint with the last point. */
        Uint16 first_idx = start;
        float first_x, first_y;
        bool first_on = (glyph->flags[start] & FORGE_UI__FLAG_ON_CURVE) != 0;

        if (first_on) {
            first_x = (float)glyph->points[start].x * scale;
            first_y = y_offset - (float)glyph->points[start].y * scale;
        } else {
            /* First point is off-curve — check if last point is on-curve */
            bool last_on = (glyph->flags[end] & FORGE_UI__FLAG_ON_CURVE) != 0;
            if (last_on) {
                /* Start from the last on-curve point */
                first_x = (float)glyph->points[end].x * scale;
                first_y = y_offset - (float)glyph->points[end].y * scale;
                /* We'll process from 'start' which is off-curve */
            } else {
                /* Both first and last are off-curve — midpoint is start */
                first_x = ((float)glyph->points[start].x +
                            (float)glyph->points[end].x) * 0.5f * scale;
                first_y = y_offset -
                           ((float)glyph->points[start].y +
                            (float)glyph->points[end].y) * 0.5f * scale;
            }
        }

        float cur_x = first_x;
        float cur_y = first_y;

        int i = (int)start;
        while (i <= (int)end) {
            bool on_i = (glyph->flags[i] & FORGE_UI__FLAG_ON_CURVE) != 0;

            if (on_i) {
                /* Current point is on-curve.  Emit a line from cur to this. */
                float px = (float)glyph->points[i].x * scale;
                float py = y_offset - (float)glyph->points[i].y * scale;

                /* Skip degenerate zero-length lines */
                if (px != cur_x || py != cur_y) {
                    if (edge_count >= max_edges) {
                        if (!overflow_logged) {
                            SDL_Log("forge_ui__build_edges: edge limit "
                                    "(%d) exceeded — glyph may render "
                                    "incorrectly", max_edges);
                            overflow_logged = true;
                        }
                    } else {
                        ForgeUi__Edge *e = &edges[edge_count++];
                        e->type = FORGE_UI__EDGE_LINE;
                        e->x0 = cur_x; e->y0 = cur_y;
                        e->x1 = px;    e->y1 = py;
                        e->winding = (e->y0 < e->y1) ? 1 : -1;
                    }
                }
                cur_x = px;
                cur_y = py;
                i++;
            } else {
                /* Current point is off-curve — it's a Bézier control point.
                 * Look ahead to find the next point. */
                float cx = (float)glyph->points[i].x * scale;
                float cy = y_offset - (float)glyph->points[i].y * scale;

                /* Find the next point (wrapping around the contour) */
                int next_i = (i == (int)end) ? (int)start : (i + 1);
                bool next_on = (glyph->flags[next_i] & FORGE_UI__FLAG_ON_CURVE) != 0;

                float nx, ny;
                if (next_on) {
                    /* off → on: standard quadratic Bézier */
                    nx = (float)glyph->points[next_i].x * scale;
                    ny = y_offset - (float)glyph->points[next_i].y * scale;
                    /* Advance past both the off-curve (i) and the on-curve
                     * (next_i).  When next_i wrapped to start, the contour
                     * has closed — override i to end+1 so the while-loop
                     * exits cleanly (the +2 alone could overshoot end). */
                    i += 2;
                    if (next_i == (int)start) {
                        i = (int)end + 1;
                    }
                } else {
                    /* off → off: implicit midpoint is on-curve */
                    float nx2 = (float)glyph->points[next_i].x * scale;
                    float ny2 = y_offset - (float)glyph->points[next_i].y * scale;
                    nx = (cx + nx2) * 0.5f;
                    ny = (cy + ny2) * 0.5f;
                    i++; /* advance past this off-curve; next iteration handles next_i */
                }

                /* Emit a quadratic Bézier edge: cur → (cx,cy) → (nx,ny) */
                if (edge_count >= max_edges) {
                    if (!overflow_logged) {
                        SDL_Log("forge_ui__build_edges: edge limit "
                                "(%d) exceeded — glyph may render "
                                "incorrectly", max_edges);
                        overflow_logged = true;
                    }
                } else {
                    ForgeUi__Edge *e = &edges[edge_count++];
                    e->type = FORGE_UI__EDGE_QUAD;
                    e->x0 = cur_x; e->y0 = cur_y;
                    e->x1 = cx;    e->y1 = cy;
                    e->x2 = nx;    e->y2 = ny;
                    /* Winding: based on the vertical direction from start to end */
                    e->winding = (e->y0 < e->y2) ? 1 : -1;
                }

                cur_x = nx;
                cur_y = ny;
            }
        }

        /* Close the contour: emit an edge from current position back to
         * the first point (if they don't already coincide). */
        if (cur_x != first_x || cur_y != first_y) {
            if (edge_count >= max_edges) {
                if (!overflow_logged) {
                    SDL_Log("forge_ui__build_edges: edge limit "
                            "(%d) exceeded — glyph may render "
                            "incorrectly", max_edges);
                    overflow_logged = true;
                }
            } else {
                ForgeUi__Edge *e = &edges[edge_count++];
                e->type = FORGE_UI__EDGE_LINE;
                e->x0 = cur_x;  e->y0 = cur_y;
                e->x1 = first_x; e->y1 = first_y;
                e->winding = (e->y0 < e->y1) ? 1 : -1;
            }
        }
    }

    return edge_count;
}

/* ── Scanline–line intersection ──────────────────────────────────────────── */
/* For a line segment from (x0,y0) to (x1,y1), find the x coordinate
 * where it crosses horizontal scanline y.  Returns true if the crossing
 * exists (y is within the edge's vertical range). */

static bool forge_ui__line_crossing(
    float x0, float y0, float x1, float y1,
    float scan_y, float *out_x)
{
    /* Horizontal edges never cross a scanline */
    if (y0 == y1) return false;

    /* Check if scan_y is within the edge's vertical extent.
     * Use half-open interval [min_y, max_y) to avoid double-counting
     * at shared vertices between consecutive edges. */
    float min_y, max_y;
    if (y0 < y1) { min_y = y0; max_y = y1; }
    else          { min_y = y1; max_y = y0; }

    if (scan_y < min_y || scan_y >= max_y) return false;

    /* Linear interpolation: solve for x at scan_y */
    float t = (scan_y - y0) / (y1 - y0);
    *out_x = x0 + t * (x1 - x0);
    return true;
}

/* ── Scanline–quadratic Bézier intersection ──────────────────────────────── */
/* For a quadratic Bézier from p0 through control p1 to p2, find all
 * x coordinates where it crosses horizontal scanline y.
 *
 * The curve's y-coordinate as a function of parameter t is:
 *   Y(t) = (1-t)^2 * y0 + 2(1-t)t * y1 + t^2 * y2
 *
 * Setting Y(t) = scan_y and rearranging:
 *   a*t^2 + b*t + c = 0
 * where:
 *   a = y0 - 2*y1 + y2
 *   b = 2*(y1 - y0)
 *   c = y0 - scan_y
 *
 * This is a standard quadratic equation.  We solve it and evaluate x
 * at each valid t in [0, 1). */

static int forge_ui__quad_crossings(
    float x0, float y0,   /* start */
    float x1, float y1,   /* control */
    float x2, float y2,   /* end */
    float scan_y,
    int   winding,
    ForgeUi__Crossing *crossings,
    int max_crossings)
{
    int found = 0;

    float a = y0 - 2.0f * y1 + y2;
    float b = 2.0f * (y1 - y0);
    float c = y0 - scan_y;

    float t_values[2];
    int t_count = 0;

    if (SDL_fabsf(a) < FORGE_UI__EPSILON) {
        /* Near-linear: solve b*t + c = 0 */
        if (SDL_fabsf(b) > FORGE_UI__EPSILON) {
            float t = -c / b;
            if (t >= 0.0f && t < 1.0f) {
                t_values[t_count++] = t;
            }
        }
    } else {
        float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            float sqrt_disc = SDL_sqrtf(disc);
            float inv_2a = 1.0f / (2.0f * a);

            float t1 = (-b - sqrt_disc) * inv_2a;
            float t2 = (-b + sqrt_disc) * inv_2a;

            if (t1 >= 0.0f && t1 < 1.0f) t_values[t_count++] = t1;
            if (t2 >= 0.0f && t2 < 1.0f && SDL_fabsf(t2 - t1) > FORGE_UI__EPSILON) {
                t_values[t_count++] = t2;
            }
        }
    }

    /* Evaluate x and per-root winding at each valid t.
     *
     * The winding direction must be computed per root, not per edge,
     * because a non-monotonic quadratic can cross a scanline twice
     * with opposite vertical directions.  We use the derivative:
     *   dY/dt = 2[(1-t)(y1 - y0) + t(y2 - y1)]
     * Positive dY/dt means the curve is moving downward in bitmap
     * coordinates (y increases downward), so winding = +1.  Negative
     * means upward, so winding = -1. */
    for (int i = 0; i < t_count && found < max_crossings; i++) {
        float t = t_values[i];
        float mt = 1.0f - t;
        float x = mt * mt * x0 + 2.0f * mt * t * x1 + t * t * x2;

        /* dY/dt at this root determines crossing direction */
        float dydt = 2.0f * (mt * (y1 - y0) + t * (y2 - y1));
        int root_winding;
        if (SDL_fabsf(dydt) < FORGE_UI__EPSILON) {
            /* Tangent crossing — use the edge's overall direction */
            root_winding = winding;
        } else {
            root_winding = (dydt > 0.0f) ? 1 : -1;
        }

        crossings[found].x = x;
        crossings[found].winding = root_winding;
        found++;
    }

    return found;
}

/* ── Comparison function for sorting crossings by x ──────────────────────── */

static int forge_ui__crossing_cmp(const void *a, const void *b)
{
    float xa = ((const ForgeUi__Crossing *)a)->x;
    float xb = ((const ForgeUi__Crossing *)b)->x;
    if (xa < xb) return -1;
    if (xa > xb) return  1;
    return 0;
}

/* ── Scanline rasterization core ─────────────────────────────────────────── */
/* Rasterize a single scanline at y-coordinate scan_y into the bitmap row.
 *
 * Algorithm:
 * 1. Find all crossings of contour edges with this scanline
 * 2. Sort crossings by x
 * 3. Walk left to right, accumulating the winding number
 * 4. When winding != 0, the pixel is inside the glyph — fill it */

static void forge_ui__rasterize_scanline(
    const ForgeUi__Edge *edges, int edge_count,
    float scan_y,
    Uint8 *row, int width)
{
    ForgeUi__Crossing crossings[FORGE_UI__MAX_CROSSINGS];
    int num_crossings = 0;

    for (int i = 0; i < edge_count; i++) {
        const ForgeUi__Edge *e = &edges[i];

        if (e->type == FORGE_UI__EDGE_LINE) {
            float cx;
            if (forge_ui__line_crossing(e->x0, e->y0, e->x1, e->y1,
                                         scan_y, &cx)) {
                if (num_crossings >= FORGE_UI__MAX_CROSSINGS) {
                    SDL_Log("forge_ui__rasterize_scanline: crossing limit "
                            "(%d) exceeded at y=%.1f", FORGE_UI__MAX_CROSSINGS,
                            (double)scan_y);
                    break;
                }
                crossings[num_crossings].x = cx;
                crossings[num_crossings].winding = e->winding;
                num_crossings++;
            }
        } else {
            /* Quadratic Bézier edge */
            int space = FORGE_UI__MAX_CROSSINGS - num_crossings;
            if (space <= 0) {
                SDL_Log("forge_ui__rasterize_scanline: crossing limit "
                        "(%d) exceeded at y=%.1f", FORGE_UI__MAX_CROSSINGS,
                        (double)scan_y);
                break;
            }
            int added = forge_ui__quad_crossings(
                e->x0, e->y0, e->x1, e->y1, e->x2, e->y2,
                scan_y, e->winding,
                &crossings[num_crossings], space);
            num_crossings += added;
        }
    }

    if (num_crossings < 2) return;

    /* Sort crossings by x position */
    SDL_qsort(crossings, (size_t)num_crossings,
              sizeof(ForgeUi__Crossing), forge_ui__crossing_cmp);

    /* Walk crossings and fill using the non-zero winding rule.
     * The winding number starts at 0.  Each crossing adds its winding
     * value.  When winding transitions from 0 to non-zero we record the
     * entry x-position; when it returns to 0 we fill from that entry
     * to the current crossing. */
    int winding = 0;
    float fill_start_x = 0.0f; /* x where we last entered the glyph */
    for (int i = 0; i < num_crossings; i++) {
        int prev_winding = winding;
        winding += crossings[i].winding;

        if (prev_winding == 0 && winding != 0) {
            /* Entered the glyph — record start position */
            fill_start_x = crossings[i].x;
        } else if (prev_winding != 0 && winding == 0) {
            /* Exited the glyph — fill from recorded entry to here */
            float fill_end = crossings[i].x;

            /* Clamp to bitmap bounds */
            int px_start = (int)fill_start_x;
            int px_end   = (int)SDL_ceilf(fill_end);
            if (px_start < 0) px_start = 0;
            if (px_end > width) px_end = width;

            for (int px = px_start; px < px_end; px++) {
                row[px] = 255;
            }
        }
        /* When prev_winding != 0 && winding != 0 we are still inside the
         * glyph — no fill boundary to emit, so no action is needed. */
    }
}

/* ── Main rasterization function ─────────────────────────────────────────── */

static bool forge_ui_rasterize_glyph(const ForgeUiFont *font,
                                      Uint16 glyph_index,
                                      float pixel_height,
                                      const ForgeUiRasterOpts *opts,
                                      ForgeUiGlyphBitmap *out_bitmap)
{
    if (!font || !out_bitmap) {
        SDL_Log("forge_ui_rasterize_glyph: NULL argument");
        return false;
    }
    SDL_memset(out_bitmap, 0, sizeof(ForgeUiGlyphBitmap));

    if (!(pixel_height > 0.0f)) {
        SDL_Log("forge_ui_rasterize_glyph: pixel_height must be positive "
                "(got %f)", (double)pixel_height);
        return false;
    }

    /* Default options: 4x4 supersampling.
     * Only powers of two up to 8 are allowed — arbitrary values could
     * cause enormous allocations (hi-res buffer is bmp_w*ss × bmp_h*ss). */
    int ss = FORGE_UI__DEFAULT_SS;
    if (opts) {
        int req = opts->supersample_level;
        if (req == 1 || req == 2 || req == 4 || req == 8) {
            ss = req;
        } else if (req >= 1) {
            SDL_Log("forge_ui_rasterize_glyph: invalid supersample_level "
                    "%d — must be 1, 2, 4, or 8; using default %d",
                    req, FORGE_UI__DEFAULT_SS);
        }
    }

    /* Load the glyph outline */
    ForgeUiTtfGlyph glyph;
    if (!forge_ui_ttf_load_glyph(font, glyph_index, &glyph)) {
        SDL_Log("forge_ui_rasterize_glyph: failed to load glyph %u",
                glyph_index);
        return false;
    }

    /* Whitespace glyphs have no contours — return success with zero-size bitmap */
    if (glyph.contour_count == 0) {
        forge_ui_ttf_glyph_free(&glyph);
        return true;
    }

    /* Compute scale factor: font units → pixels.
     * scale = pixel_height / unitsPerEm */
    float scale = pixel_height / (float)font->head.units_per_em;

    /* Compute bitmap dimensions from the glyph's bounding box.
     * Font coordinates have y-up; bitmaps have y=0 at top.  We flip
     * y during edge building so the bitmap renders correctly.
     *
     * bearing_x: horizontal offset from pen position to glyph left edge
     * bearing_y: vertical offset from baseline to glyph top edge */
    float scaled_x_min = (float)glyph.x_min * scale;
    float scaled_y_min = (float)glyph.y_min * scale;
    float scaled_x_max = (float)glyph.x_max * scale;
    float scaled_y_max = (float)glyph.y_max * scale;

    int bmp_w = (int)SDL_ceilf(scaled_x_max - scaled_x_min) + 2 * FORGE_UI__BITMAP_PAD;
    int bmp_h = (int)SDL_ceilf(scaled_y_max - scaled_y_min) + 2 * FORGE_UI__BITMAP_PAD;

    if (bmp_w <= 0 || bmp_h <= 0) {
        forge_ui_ttf_glyph_free(&glyph);
        return true; /* degenerate glyph */
    }

    /* The y_offset is where y=0 in font units maps to in bitmap coordinates.
     * Since we flip y (bitmap y=0 is top), y_max maps to the top of the
     * bitmap, and y_min maps to the bottom. */
    float y_offset = scaled_y_max + (float)FORGE_UI__BITMAP_PAD;
    float x_offset = -scaled_x_min + (float)FORGE_UI__BITMAP_PAD;

    /* Build edges from contour data.  The edge builder handles all three
     * segment types (on→on lines, on→off→on Béziers, off→off implicit
     * midpoints) and applies scaling and y-flip. */
    ForgeUi__Edge *edges = (ForgeUi__Edge *)SDL_malloc(
        sizeof(ForgeUi__Edge) * FORGE_UI__MAX_EDGES);
    if (!edges) {
        SDL_Log("forge_ui_rasterize_glyph: allocation failed (edges)");
        forge_ui_ttf_glyph_free(&glyph);
        return false;
    }

    /* Adjust edge coordinates: add x_offset so glyph is centered in bitmap */
    int edge_count = forge_ui__build_edges(&glyph, scale, y_offset,
                                            edges, FORGE_UI__MAX_EDGES);

    /* Apply x_offset to all edge x-coordinates */
    for (int i = 0; i < edge_count; i++) {
        edges[i].x0 += x_offset;
        edges[i].x1 += x_offset;
        if (edges[i].type == FORGE_UI__EDGE_QUAD) {
            edges[i].x2 += x_offset;
        }
    }

    /* Allocate the output bitmap */
    Uint8 *pixels = (Uint8 *)SDL_calloc((size_t)bmp_w * (size_t)bmp_h, 1);
    if (!pixels) {
        SDL_Log("forge_ui_rasterize_glyph: allocation failed (pixels)");
        SDL_free(edges);
        forge_ui_ttf_glyph_free(&glyph);
        return false;
    }

    if (ss <= 1) {
        /* No supersampling: one sample per pixel at pixel center */
        for (int y = 0; y < bmp_h; y++) {
            float scan_y = (float)y + 0.5f;
            forge_ui__rasterize_scanline(edges, edge_count, scan_y,
                                          &pixels[y * bmp_w], bmp_w);
        }
    } else {
        /* Supersampling: sample a ss×ss grid per pixel and average.
         *
         * For each pixel, we cast ss horizontal scanlines evenly spaced
         * within the pixel, and for each scanline we check whether each
         * sub-pixel column is inside.  The fraction of "inside" samples
         * becomes the coverage value (0–255).
         *
         * We rasterize into a high-resolution buffer (ss× in both axes),
         * then downsample to the final bitmap. */

        /* Guard against integer overflow BEFORE the multiplication to
         * avoid signed overflow undefined behavior (C99 6.5/5). */
        if (bmp_w > INT_MAX / ss || bmp_h > INT_MAX / ss) {
            SDL_Log("forge_ui_rasterize_glyph: supersample dimensions "
                    "overflow (%d × %d × %d)", bmp_w, bmp_h, ss);
            SDL_free(pixels);
            SDL_free(edges);
            forge_ui_ttf_glyph_free(&glyph);
            return false;
        }
        int hi_w = bmp_w * ss;  /* safe: overflow checked above */

        /* Use a single row buffer for the high-res scanline, then
         * accumulate into a per-pixel coverage counter. */
        Uint8 *hi_row = (Uint8 *)SDL_calloc((size_t)hi_w, 1);
        /* Coverage accumulator per output pixel row — we need to track
         * the sum across ss sub-scanlines. */
        int *coverage = (int *)SDL_calloc((size_t)bmp_w, sizeof(int));

        if (!hi_row || !coverage) {
            SDL_Log("forge_ui_rasterize_glyph: allocation failed (supersample)");
            SDL_free(hi_row);
            SDL_free(coverage);
            SDL_free(pixels);
            SDL_free(edges);
            forge_ui_ttf_glyph_free(&glyph);
            return false;
        }

        /* Scale edges to high-resolution grid */
        float ss_f = (float)ss;
        for (int i = 0; i < edge_count; i++) {
            edges[i].x0 *= ss_f;
            edges[i].y0 *= ss_f;
            edges[i].x1 *= ss_f;
            edges[i].y1 *= ss_f;
            if (edges[i].type == FORGE_UI__EDGE_QUAD) {
                edges[i].x2 *= ss_f;
                edges[i].y2 *= ss_f;
            }
        }

        for (int y = 0; y < bmp_h; y++) {
            SDL_memset(coverage, 0, sizeof(int) * (size_t)bmp_w);

            for (int sub_y = 0; sub_y < ss; sub_y++) {
                float scan_y = (float)(y * ss + sub_y) + 0.5f;
                SDL_memset(hi_row, 0, (size_t)hi_w);
                forge_ui__rasterize_scanline(edges, edge_count, scan_y,
                                              hi_row, hi_w);

                /* Accumulate sub-pixel coverage into output pixels */
                for (int x = 0; x < bmp_w; x++) {
                    int sum = 0;
                    int base = x * ss;
                    for (int sub_x = 0; sub_x < ss; sub_x++) {
                        if (hi_row[base + sub_x]) sum++;
                    }
                    coverage[x] += sum;
                }
            }

            /* Convert coverage counts to 0–255 */
            int total_samples = ss * ss;
            for (int x = 0; x < bmp_w; x++) {
                int val = (coverage[x] * 255 + total_samples / 2) / total_samples;
                if (val > 255) val = 255;
                pixels[y * bmp_w + x] = (Uint8)val;
            }
        }

        SDL_free(coverage);
        SDL_free(hi_row);
    }

    /* Fill output bitmap struct */
    out_bitmap->width     = bmp_w;
    out_bitmap->height    = bmp_h;
    out_bitmap->pixels    = pixels;
    out_bitmap->bearing_x = (int)SDL_floorf(scaled_x_min) - FORGE_UI__BITMAP_PAD;
    out_bitmap->bearing_y = (int)SDL_ceilf(scaled_y_max) + FORGE_UI__BITMAP_PAD;

    SDL_free(edges);
    forge_ui_ttf_glyph_free(&glyph);
    return true;
}

static void forge_ui_glyph_bitmap_free(ForgeUiGlyphBitmap *bitmap)
{
    if (!bitmap) return;
    SDL_free(bitmap->pixels);
    SDL_memset(bitmap, 0, sizeof(ForgeUiGlyphBitmap));
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── hmtx Advance Width Lookup ──────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

static Uint16 forge_ui_ttf_advance_width(const ForgeUiFont *font,
                                          Uint16 glyph_index)
{
    if (!font) return 0;

    /* Glyphs with index < numberOfHMetrics have individual advance widths.
     * Glyphs at or beyond that index share the last advance width. */
    if (glyph_index < font->hhea.number_of_h_metrics) {
        return font->hmtx_advance_widths[glyph_index];
    }
    return font->hmtx_last_advance;
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── BMP Writing Implementation ─────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* BMP file constants */
#define FORGE_UI__BMP_HEADER_SIZE   14   /* BITMAPFILEHEADER size */
#define FORGE_UI__BMP_INFO_SIZE     40   /* BITMAPINFOHEADER size */
#define FORGE_UI__BMP_PALETTE_SIZE  1024 /* 256 entries * 4 bytes (BGRA) */

static bool forge_ui__write_grayscale_bmp(const char *path,
                                           const Uint8 *pixels,
                                           int width, int height)
{
    /* Validate arguments — prevent negative/zero dimensions from producing
     * invalid arithmetic for row_stride, pixel_data_size, and file_size. */
    if (!path) {
        SDL_Log("forge_ui__write_grayscale_bmp: path is NULL");
        return false;
    }
    if (!pixels) {
        SDL_Log("forge_ui__write_grayscale_bmp: pixels is NULL");
        return false;
    }
    if (width <= 0 || height <= 0) {
        SDL_Log("forge_ui__write_grayscale_bmp: invalid dimensions %dx%d",
                width, height);
        return false;
    }

    /* Each row must be padded to a 4-byte boundary */
    int row_stride = (width + 3) & ~3;

    /* Use 64-bit arithmetic to detect overflow before allocation */
    Uint64 pixel_data_size_64 = (Uint64)row_stride * (Uint64)height;
    if (pixel_data_size_64 > UINT32_MAX) {
        SDL_Log("forge_ui__write_grayscale_bmp: image too large (%dx%d)",
                width, height);
        return false;
    }
    /* Check that total file size (header + pixel data) fits in Uint32 */
    Uint64 hdr_overhead = (Uint64)FORGE_UI__BMP_HEADER_SIZE +
                          FORGE_UI__BMP_INFO_SIZE +
                          FORGE_UI__BMP_PALETTE_SIZE;
    if (pixel_data_size_64 > (Uint64)UINT32_MAX - hdr_overhead) {
        SDL_Log("forge_ui__write_grayscale_bmp: file size overflow (%dx%d)",
                width, height);
        return false;
    }
    Uint32 file_size = (Uint32)(hdr_overhead + pixel_data_size_64);

    Uint8 *buf = (Uint8 *)SDL_calloc(file_size, 1);
    if (!buf) {
        SDL_Log("forge_ui__write_grayscale_bmp: allocation failed");
        return false;
    }

    /* BITMAPFILEHEADER (14 bytes) */
    buf[0] = 'B'; buf[1] = 'M';                /* signature */
    buf[2] = (Uint8)(file_size);                /* file size (little-endian) */
    buf[3] = (Uint8)(file_size >> 8);
    buf[4] = (Uint8)(file_size >> 16);
    buf[5] = (Uint8)(file_size >> 24);
    /* bytes 6-9: reserved (0) */
    Uint32 data_offset = FORGE_UI__BMP_HEADER_SIZE + FORGE_UI__BMP_INFO_SIZE +
                         FORGE_UI__BMP_PALETTE_SIZE;
    buf[10] = (Uint8)(data_offset);
    buf[11] = (Uint8)(data_offset >> 8);
    buf[12] = (Uint8)(data_offset >> 16);
    buf[13] = (Uint8)(data_offset >> 24);

    /* BITMAPINFOHEADER (40 bytes) */
    Uint8 *info = buf + FORGE_UI__BMP_HEADER_SIZE;
    info[0] = FORGE_UI__BMP_INFO_SIZE;           /* header size */
    info[4] = (Uint8)(width);                    /* width (little-endian) */
    info[5] = (Uint8)(width >> 8);
    info[6] = (Uint8)(width >> 16);
    info[7] = (Uint8)(width >> 24);
    info[8]  = (Uint8)(height);                  /* height (little-endian) */
    info[9]  = (Uint8)(height >> 8);
    info[10] = (Uint8)(height >> 16);
    info[11] = (Uint8)(height >> 24);
    info[12] = 1;                                /* planes = 1 */
    info[14] = 8;                                /* bits per pixel = 8 */
    /* bytes 16-19: compression = 0 (BI_RGB) */
    Uint32 pixel_data_size = (Uint32)pixel_data_size_64;
    info[20] = (Uint8)(pixel_data_size);         /* image data size */
    info[21] = (Uint8)(pixel_data_size >> 8);
    info[22] = (Uint8)(pixel_data_size >> 16);
    info[23] = (Uint8)(pixel_data_size >> 24);

    /* Grayscale palette: 256 entries, each (B, G, R, 0) */
    Uint8 *palette = buf + FORGE_UI__BMP_HEADER_SIZE + FORGE_UI__BMP_INFO_SIZE;
    for (int i = 0; i < 256; i++) {
        palette[i * 4 + 0] = (Uint8)i;   /* blue */
        palette[i * 4 + 1] = (Uint8)i;   /* green */
        palette[i * 4 + 2] = (Uint8)i;   /* red */
        palette[i * 4 + 3] = 0;          /* reserved */
    }

    /* Pixel data: BMP stores rows bottom-up, so row 0 in the file is the
     * bottom of the image.  Our bitmap is stored top-down (row 0 = top),
     * so we need to flip. */
    Uint8 *pixel_dst = buf + data_offset;
    for (int y = 0; y < height; y++) {
        /* BMP row (height - 1 - y) gets our row y */
        int bmp_row = height - 1 - y;
        SDL_memcpy(&pixel_dst[bmp_row * row_stride],
                   &pixels[y * width],
                   (size_t)width);
    }

    /* Write to file using standard C I/O */
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        SDL_Log("forge_ui__write_grayscale_bmp: failed to open '%s' for writing",
                path);
        SDL_free(buf);
        return false;
    }
    size_t written = fwrite(buf, 1, file_size, fp);
    fclose(fp);
    SDL_free(buf);

    if (written != file_size) {
        SDL_Log("forge_ui__write_grayscale_bmp: incomplete write to '%s' "
                "(%zu of %u bytes)", path, written, file_size);
        return false;
    }
    return true;
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── Font Atlas Implementation ──────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* Maximum atlas dimension — 4096 is well within GPU limits for any modern
 * hardware.  A 4096x4096 single-channel atlas is 16MB. */
#define FORGE_UI__MAX_ATLAS_DIM  4096

/* Minimum atlas dimension — starting size for the power-of-two search. */
#define FORGE_UI__MIN_ATLAS_DIM  64

/* White pixel region size — a 2x2 block of fully white (255) pixels
 * used for drawing solid-colored geometry without switching textures. */
#define FORGE_UI__WHITE_SIZE     2

/* Safety margin multiplied into the area estimate to account for shelf-packing
 * inefficiency (row gaps above short glyphs, wasted ends of rows). */
#define FORGE_UI__PACKING_SAFETY_MARGIN  1.15f

/* ── Internal: temporary glyph data during atlas building ──────────────── */

typedef struct ForgeUi__GlyphEntry {
    Uint32              codepoint;
    Uint16              glyph_index;
    ForgeUiGlyphBitmap  bitmap;       /* rasterized bitmap (freed after copy) */
    Uint16              advance_width; /* from hmtx, in font units */
    int                 atlas_x;      /* placement x in atlas (set by packer) */
    int                 atlas_y;      /* placement y in atlas (set by packer) */
} ForgeUi__GlyphEntry;

/* ── Internal: comparison function for sorting glyphs by height ────────── */
/* Sort tallest first so shelf rows are filled efficiently. */

static int forge_ui__glyph_height_cmp(const void *a, const void *b)
{
    const ForgeUi__GlyphEntry *ga = (const ForgeUi__GlyphEntry *)a;
    const ForgeUi__GlyphEntry *gb = (const ForgeUi__GlyphEntry *)b;
    /* Sort by height descending (tallest first) */
    int ha = ga->bitmap.height;
    int hb = gb->bitmap.height;
    if (ha > hb) return -1;
    if (ha < hb) return  1;
    /* Tie-break by width descending for tighter rows */
    int wa = ga->bitmap.width;
    int wb = gb->bitmap.width;
    if (wa > wb) return -1;
    if (wa < wb) return  1;
    return 0;
}

/* ── Internal: shelf packer ────────────────────────────────────────────── */
/* Pack glyphs into an atlas using row-based (shelf) packing.
 *
 * Algorithm:
 *   1. Start with cursor at top-left (0, 0)
 *   2. Place each glyph left-to-right in the current row
 *   3. When a glyph doesn't fit horizontally, start a new row
 *   4. The row height is the tallest glyph placed in it
 *   5. If a new row doesn't fit vertically, packing fails
 *
 * The padding parameter adds empty space around each glyph to prevent
 * texture bleed during bilinear filtering. */

static bool forge_ui__shelf_pack(ForgeUi__GlyphEntry *entries,
                                  int count,
                                  int atlas_w, int atlas_h,
                                  int padding)
{
    int cursor_x = padding;  /* current x position in current row */
    int cursor_y = padding;  /* y position of current row's top edge */
    int row_h = 0;           /* height of tallest glyph in current row */

    for (int i = 0; i < count; i++) {
        int gw = entries[i].bitmap.width;
        int gh = entries[i].bitmap.height;

        /* Skip zero-size bitmaps (whitespace glyphs like space) */
        if (gw == 0 || gh == 0) {
            entries[i].atlas_x = 0;
            entries[i].atlas_y = 0;
            continue;
        }

        int padded_w = gw + padding;  /* width including right padding */
        int padded_h = gh + padding;  /* height including bottom padding */

        /* Reject glyphs wider or taller than the atlas can hold */
        if (padded_w + padding > atlas_w || padded_h + padding > atlas_h) {
            return false;  /* atlas too small for this glyph */
        }

        /* Check if glyph fits in current row */
        if (cursor_x + padded_w > atlas_w) {
            /* Start a new row below the current one */
            cursor_y += row_h + padding;
            cursor_x = padding;
            row_h = 0;
        }

        /* Check if new row fits vertically */
        if (cursor_y + padded_h > atlas_h) {
            return false; /* atlas too small */
        }

        /* Place glyph at current cursor position */
        entries[i].atlas_x = cursor_x;
        entries[i].atlas_y = cursor_y;

        /* Advance cursor and update row height */
        cursor_x += padded_w;
        if (padded_h > row_h) {
            row_h = padded_h;
        }
    }

    return true;
}

/* ── Internal: find smallest power-of-two atlas that fits ──────────────── */
/* Estimates the required area, picks an initial power-of-two size, then
 * tries packing.  If it fails, doubles the smaller dimension and retries. */

static bool forge_ui__find_atlas_size(ForgeUi__GlyphEntry *entries,
                                       int count,
                                       int padding,
                                       int *out_w, int *out_h)
{
    /* Estimate total area needed (sum of padded glyph areas + white pixel).
     * Use 64-bit arithmetic to avoid overflow when many large glyphs are
     * present.  Multiply by the packing safety margin to account for shelf
     * inefficiency (row gaps above short glyphs, wasted row ends). */
    Uint64 total_area_u = (Uint64)FORGE_UI__WHITE_SIZE * FORGE_UI__WHITE_SIZE;
    for (int i = 0; i < count; i++) {
        Uint64 pw = (Uint64)(entries[i].bitmap.width + padding * 2);
        Uint64 ph = (Uint64)(entries[i].bitmap.height + padding * 2);
        total_area_u += pw * ph;
    }
    total_area_u = (Uint64)((double)total_area_u *
                            (double)FORGE_UI__PACKING_SAFETY_MARGIN);
    /* Clamp to maximum atlas area to avoid overflow when cast to int */
    Uint64 max_area = (Uint64)FORGE_UI__MAX_ATLAS_DIM *
                      (Uint64)FORGE_UI__MAX_ATLAS_DIM;
    if (total_area_u > max_area) total_area_u = max_area;
    int total_area = (int)total_area_u;

    /* Find the smallest power-of-two square that exceeds the total area */
    int size = FORGE_UI__MIN_ATLAS_DIM;
    while (size * size < total_area && size < FORGE_UI__MAX_ATLAS_DIM) {
        size *= 2;
    }

    /* Try packing with progressively larger dimensions */
    int w = size;
    int h = size;

    while (w <= FORGE_UI__MAX_ATLAS_DIM && h <= FORGE_UI__MAX_ATLAS_DIM) {
        if (forge_ui__shelf_pack(entries, count, w, h, padding)) {
            *out_w = w;
            *out_h = h;
            return true;
        }

        /* Double the smaller dimension first, then the other */
        if (w <= h) {
            w *= 2;
        } else {
            h *= 2;
        }
    }

    SDL_Log("forge_ui__find_atlas_size: could not fit %d glyphs in a "
            "%dx%d atlas", count, FORGE_UI__MAX_ATLAS_DIM,
            FORGE_UI__MAX_ATLAS_DIM);
    return false;
}

/* ── Public atlas API ────────────────────────────────────────────────────── */

static bool forge_ui_atlas_build(const ForgeUiFont *font,
                                  float pixel_height,
                                  const Uint32 *codepoints,
                                  int codepoint_count,
                                  int padding,
                                  ForgeUiFontAtlas *out_atlas)
{
    /* Validate public pointer arguments before any dereference */
    if (!out_atlas) {
        SDL_Log("forge_ui_atlas_build: out_atlas is NULL");
        return false;
    }
    SDL_memset(out_atlas, 0, sizeof(ForgeUiFontAtlas));

    if (!font) {
        SDL_Log("forge_ui_atlas_build: font is NULL");
        return false;
    }
    if (!(pixel_height > 0.0f)) {
        SDL_Log("forge_ui_atlas_build: pixel_height must be positive (got %f)",
                (double)pixel_height);
        return false;
    }
    if (codepoint_count <= 0) {
        SDL_Log("forge_ui_atlas_build: no codepoints provided");
        return false;
    }
    if (!codepoints) {
        SDL_Log("forge_ui_atlas_build: codepoints is NULL");
        return false;
    }
    if (padding < 0) padding = 0;

    /* ── Phase 1: Rasterize all requested glyphs ─────────────────────── */

    /* Allocate one extra entry for the white pixel reservation so the shelf
     * packer assigns it a non-overlapping position alongside real glyphs. */
    ForgeUi__GlyphEntry *entries = (ForgeUi__GlyphEntry *)SDL_calloc(
        (size_t)codepoint_count + 1, sizeof(ForgeUi__GlyphEntry));
    if (!entries) {
        SDL_Log("forge_ui_atlas_build: allocation failed (entries)");
        return false;
    }

    /* Default rasterization options: 4x4 supersampling */
    ForgeUiRasterOpts opts;
    opts.supersample_level = 4;

    int valid_count = 0;
    for (int i = 0; i < codepoint_count; i++) {
        Uint32 cp = codepoints[i];
        Uint16 gi = forge_ui_ttf_glyph_index(font, cp);

        ForgeUiGlyphBitmap bitmap;
        if (!forge_ui_rasterize_glyph(font, gi, pixel_height, &opts, &bitmap)) {
            SDL_Log("forge_ui_atlas_build: failed to rasterize codepoint %u "
                    "(glyph %u) -- skipping", cp, gi);
            continue;
        }

        entries[valid_count].codepoint     = cp;
        entries[valid_count].glyph_index   = gi;
        entries[valid_count].bitmap        = bitmap;
        entries[valid_count].advance_width = forge_ui_ttf_advance_width(font, gi);
        entries[valid_count].atlas_x       = 0;
        entries[valid_count].atlas_y       = 0;
        valid_count++;
    }

    if (valid_count == 0) {
        SDL_Log("forge_ui_atlas_build: no glyphs could be rasterized");
        SDL_free(entries);
        return false;
    }

    /* ── Phase 2: Add white pixel reservation and sort ────────────────── */
    /* Insert a fake entry for the white pixel block so the shelf packer
     * reserves a non-overlapping position.  It has no real bitmap pixels
     * (pixels == NULL), so Phase 4 skips it during the copy step. */
    int white_entry_idx = valid_count;
    entries[white_entry_idx].codepoint   = 0;
    entries[white_entry_idx].glyph_index = 0;
    entries[white_entry_idx].bitmap.width  = FORGE_UI__WHITE_SIZE;
    entries[white_entry_idx].bitmap.height = FORGE_UI__WHITE_SIZE;
    entries[white_entry_idx].bitmap.pixels = NULL;
    entries[white_entry_idx].atlas_x     = 0;
    entries[white_entry_idx].atlas_y     = 0;
    int pack_count = valid_count + 1;

    SDL_qsort(entries, (size_t)pack_count, sizeof(ForgeUi__GlyphEntry),
              forge_ui__glyph_height_cmp);

    /* ── Phase 3: Find atlas dimensions and pack ─────────────────────── */
    int atlas_w = 0, atlas_h = 0;
    if (!forge_ui__find_atlas_size(entries, pack_count, padding,
                                    &atlas_w, &atlas_h)) {
        SDL_Log("forge_ui_atlas_build: failed to find suitable atlas dimensions");
        for (int i = 0; i < pack_count; i++) {
            forge_ui_glyph_bitmap_free(&entries[i].bitmap);
        }
        SDL_free(entries);
        return false;
    }

    /* ── Phase 4: Allocate atlas and copy glyph bitmaps ──────────────── */
    Uint8 *atlas_pixels = (Uint8 *)SDL_calloc(
        (size_t)atlas_w * (size_t)atlas_h, 1);
    if (!atlas_pixels) {
        SDL_Log("forge_ui_atlas_build: allocation failed (atlas pixels)");
        for (int i = 0; i < pack_count; i++) {
            forge_ui_glyph_bitmap_free(&entries[i].bitmap);
        }
        SDL_free(entries);
        return false;
    }

    /* Copy each glyph bitmap into the atlas at its packed position.
     * Skip entries with NULL pixels (white pixel reservation entry). */
    for (int i = 0; i < pack_count; i++) {
        ForgeUi__GlyphEntry *e = &entries[i];
        if (!e->bitmap.pixels || e->bitmap.width == 0
            || e->bitmap.height == 0) {
            continue;
        }

        for (int row = 0; row < e->bitmap.height; row++) {
            SDL_memcpy(
                &atlas_pixels[(e->atlas_y + row) * atlas_w + e->atlas_x],
                &e->bitmap.pixels[row * e->bitmap.width],
                (size_t)e->bitmap.width);
        }
    }

    /* ── Phase 5: Write white pixel region ───────────────────────────── */
    /* Find the white pixel reservation entry (sorted position may differ
     * from white_entry_idx) and write 255 at its packed position. */
    int white_x = 0, white_y = 0;
    for (int i = 0; i < pack_count; i++) {
        if (!entries[i].bitmap.pixels
            && entries[i].bitmap.width == FORGE_UI__WHITE_SIZE) {
            white_x = entries[i].atlas_x;
            white_y = entries[i].atlas_y;
            break;
        }
    }

    for (int wy = 0; wy < FORGE_UI__WHITE_SIZE; wy++) {
        for (int wx = 0; wx < FORGE_UI__WHITE_SIZE; wx++) {
            atlas_pixels[(white_y + wy) * atlas_w + (white_x + wx)] = 255;
        }
    }

    /* ── Phase 6: Build glyph metadata with UV coordinates ───────────── */
    ForgeUiPackedGlyph *packed = (ForgeUiPackedGlyph *)SDL_malloc(
        sizeof(ForgeUiPackedGlyph) * (size_t)valid_count);
    if (!packed) {
        SDL_Log("forge_ui_atlas_build: allocation failed (packed glyphs)");
        for (int i = 0; i < pack_count; i++) {
            forge_ui_glyph_bitmap_free(&entries[i].bitmap);
        }
        SDL_free(atlas_pixels);
        SDL_free(entries);
        return false;
    }

    float inv_w = 1.0f / (float)atlas_w;
    float inv_h = 1.0f / (float)atlas_h;

    /* Iterate over pack_count (includes white pixel reservation) but only
     * emit metadata for real glyph entries (skip the reservation). */
    int packed_idx = 0;
    for (int i = 0; i < pack_count; i++) {
        ForgeUi__GlyphEntry *e = &entries[i];

        /* Skip the white pixel reservation entry (NULL pixels, nonzero size) */
        if (!e->bitmap.pixels && e->bitmap.width > 0) {
            continue;
        }

        ForgeUiPackedGlyph *pg = &packed[packed_idx++];

        pg->codepoint     = e->codepoint;
        pg->glyph_index   = e->glyph_index;
        pg->bitmap_w      = e->bitmap.width;
        pg->bitmap_h      = e->bitmap.height;
        pg->bearing_x     = e->bitmap.bearing_x;
        pg->bearing_y     = e->bitmap.bearing_y;
        pg->advance_width = e->advance_width;

        /* Compute UV coordinates: pixel position / atlas dimension */
        if (e->bitmap.width > 0 && e->bitmap.height > 0) {
            pg->uv.u0 = (float)e->atlas_x * inv_w;
            pg->uv.v0 = (float)e->atlas_y * inv_h;
            pg->uv.u1 = (float)(e->atlas_x + e->bitmap.width) * inv_w;
            pg->uv.v1 = (float)(e->atlas_y + e->bitmap.height) * inv_h;
        } else {
            /* Whitespace glyphs have no bitmap — point UVs at white region */
            pg->uv.u0 = (float)white_x * inv_w;
            pg->uv.v0 = (float)white_y * inv_h;
            pg->uv.u1 = (float)(white_x + 1) * inv_w;
            pg->uv.v1 = (float)(white_y + 1) * inv_h;
        }
    }

    /* ── Phase 7: Fill output atlas struct ───────────────────────────── */
    out_atlas->pixels      = atlas_pixels;
    out_atlas->width       = atlas_w;
    out_atlas->height      = atlas_h;
    out_atlas->glyphs      = packed;
    out_atlas->glyph_count = valid_count;

    /* Store font metrics so text layout works without a separate font ref */
    out_atlas->pixel_height = pixel_height;
    out_atlas->units_per_em = font->head.units_per_em;
    out_atlas->ascender     = font->hhea.ascender;
    out_atlas->descender    = font->hhea.descender;
    out_atlas->line_gap     = font->hhea.line_gap;

    /* White pixel UV rect (center of the 2x2 block for safe bilinear sampling) */
    out_atlas->white_uv.u0 = (float)white_x * inv_w;
    out_atlas->white_uv.v0 = (float)white_y * inv_h;
    out_atlas->white_uv.u1 = (float)(white_x + FORGE_UI__WHITE_SIZE) * inv_w;
    out_atlas->white_uv.v1 = (float)(white_y + FORGE_UI__WHITE_SIZE) * inv_h;

    /* ── Cleanup: free individual glyph bitmaps (data is in atlas now) ── */
    for (int i = 0; i < pack_count; i++) {
        forge_ui_glyph_bitmap_free(&entries[i].bitmap);
    }
    SDL_free(entries);

    return true;
}

static void forge_ui_atlas_free(ForgeUiFontAtlas *atlas)
{
    if (!atlas) return;
    SDL_free(atlas->glyphs);
    SDL_free(atlas->pixels);
    SDL_memset(atlas, 0, sizeof(ForgeUiFontAtlas));
}

static const ForgeUiPackedGlyph *forge_ui_atlas_lookup(
    const ForgeUiFontAtlas *atlas, Uint32 codepoint)
{
    if (!atlas || !atlas->glyphs) return NULL;

    /* Linear search — the glyph count is small (typically < 200) */
    for (int i = 0; i < atlas->glyph_count; i++) {
        if (atlas->glyphs[i].codepoint == codepoint) {
            return &atlas->glyphs[i];
        }
    }
    return NULL;
}

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── Text Layout Implementation ──────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* Default text options: no wrapping, left-aligned, opaque white. */
static const ForgeUiTextOpts FORGE_UI__DEFAULT_TEXT_OPTS = {
    0.0f,                       /* max_width: no wrapping */
    FORGE_UI_TEXT_ALIGN_LEFT,   /* alignment */
    1.0f, 1.0f, 1.0f, 1.0f     /* color: opaque white */
};

/* Number of vertices per character quad */
#define FORGE_UI__VERTS_PER_QUAD  4
/* Number of indices per character quad (two CCW triangles) */
#define FORGE_UI__INDICES_PER_QUAD 6

/* Minimum capacity for vertex/index arrays (avoids tiny allocations) */
#define FORGE_UI__INITIAL_CHAR_CAPACITY 128

/* Tab stop width in multiples of space advance */
#define FORGE_UI__TAB_STOP_WIDTH 4

/* ── Internal: emit one character quad into vertex/index arrays ──────────── */

static void forge_ui__emit_quad(
    ForgeUiVertex *verts, int vert_base,
    Uint32 *indices, int idx_base,
    float x0, float y0, float x1, float y1,
    float u0, float v0, float u1, float v1,
    float r, float g, float b, float a)
{
    /* Four vertices: top-left, top-right, bottom-right, bottom-left.
     *
     *   0 --- 1       Triangle 0: (0, 1, 2) — CCW
     *   |   / |       Triangle 1: (2, 3, 0) — CCW
     *   | /   |
     *   3 --- 2
     */
    ForgeUiVertex *v = &verts[vert_base];

    v[0].pos_x = x0;  v[0].pos_y = y0;  /* top-left */
    v[0].uv_u  = u0;  v[0].uv_v  = v0;
    v[0].r = r;  v[0].g = g;  v[0].b = b;  v[0].a = a;

    v[1].pos_x = x1;  v[1].pos_y = y0;  /* top-right */
    v[1].uv_u  = u1;  v[1].uv_v  = v0;
    v[1].r = r;  v[1].g = g;  v[1].b = b;  v[1].a = a;

    v[2].pos_x = x1;  v[2].pos_y = y1;  /* bottom-right */
    v[2].uv_u  = u1;  v[2].uv_v  = v1;
    v[2].r = r;  v[2].g = g;  v[2].b = b;  v[2].a = a;

    v[3].pos_x = x0;  v[3].pos_y = y1;  /* bottom-left */
    v[3].uv_u  = u0;  v[3].uv_v  = v1;
    v[3].r = r;  v[3].g = g;  v[3].b = b;  v[3].a = a;

    /* Two CCW triangles */
    Uint32 base = (Uint32)vert_base;
    indices[idx_base + 0] = base + 0;
    indices[idx_base + 1] = base + 1;
    indices[idx_base + 2] = base + 2;
    indices[idx_base + 3] = base + 2;
    indices[idx_base + 4] = base + 3;
    indices[idx_base + 5] = base + 0;
}

/* ── Internal: apply horizontal alignment to vertices on one line ────────── */

static void forge_ui__align_line(
    ForgeUiVertex *verts, int line_start_vert, int line_end_vert,
    float line_width, float max_width, ForgeUiTextAlign alignment)
{
    float offset = 0.0f;

    if (alignment == FORGE_UI_TEXT_ALIGN_CENTER) {
        offset = (max_width - line_width) * 0.5f;
    } else if (alignment == FORGE_UI_TEXT_ALIGN_RIGHT) {
        offset = max_width - line_width;
    }

    if (offset == 0.0f) return;

    for (int i = line_start_vert; i < line_end_vert; i++) {
        verts[i].pos_x += offset;
    }
}

/* ── forge_ui_text_layout ────────────────────────────────────────────────── */

static bool forge_ui_text_layout(const ForgeUiFontAtlas *atlas,
                                  const char *text,
                                  float x, float y,
                                  const ForgeUiTextOpts *opts,
                                  ForgeUiTextLayout *out_layout)
{
    if (!atlas || !text || !out_layout) {
        SDL_Log("forge_ui_text_layout: NULL parameter");
        return false;
    }

    SDL_memset(out_layout, 0, sizeof(ForgeUiTextLayout));

    if (atlas->units_per_em == 0) {
        SDL_Log("forge_ui_text_layout: atlas has units_per_em == 0 (invalid)");
        return false;
    }

    const ForgeUiTextOpts *o = opts ? opts : &FORGE_UI__DEFAULT_TEXT_OPTS;

    /* Compute the font scale: converts font units to pixel units.
     * advance_width is stored in font units in ForgeUiPackedGlyph, so we
     * multiply by this scale to get pixel-space advance. */
    float scale = atlas->pixel_height / (float)atlas->units_per_em;

    /* Line height in pixels: (ascender - descender + lineGap) * scale.
     * ascender is positive, descender is negative, so this is a sum of
     * three positive-ish values. */
    float line_height = ((float)atlas->ascender - (float)atlas->descender +
                          (float)atlas->line_gap) * scale;

    /* Guard against degenerate fonts where line_height is non-positive.
     * A zero or negative line_height would prevent newline/wrapping from
     * advancing the pen vertically, producing overlapping lines. */
    if (line_height <= 0.0f) {
        line_height = 1.0f;
    }

    /* Compute space advance for tab stops */
    const ForgeUiPackedGlyph *space_glyph = forge_ui_atlas_lookup(atlas, ' ');
    float space_advance = space_glyph
        ? (float)space_glyph->advance_width * scale
        : atlas->pixel_height * 0.5f;  /* fallback: half the pixel height */

    /* Count visible characters to pre-allocate arrays.
     * We allocate for worst case (all characters visible). */
    size_t raw_len = SDL_strlen(text);
    if (raw_len > (size_t)INT_MAX) {
        SDL_Log("forge_ui_text_layout: text too long (%zu bytes)", raw_len);
        return false;
    }
    int text_len = (int)raw_len;
    if (text_len == 0) {
        out_layout->line_count = 1;
        return true;
    }

    /* Allocate vertex and index arrays */
    int capacity = text_len;
    if (capacity < FORGE_UI__INITIAL_CHAR_CAPACITY) {
        capacity = FORGE_UI__INITIAL_CHAR_CAPACITY;
    }

    /* Use size_t for allocation arithmetic to avoid overflow on 32-bit.
     * Check before multiplying: cap * VERTS_PER_QUAD * sizeof(vertex)
     * could exceed SIZE_MAX when capacity is large. */
    size_t cap = (size_t)capacity;
    if (cap > SIZE_MAX / (FORGE_UI__VERTS_PER_QUAD * sizeof(ForgeUiVertex))) {
        SDL_Log("forge_ui_text_layout: allocation size overflow (vertices)");
        return false;
    }
    if (cap > SIZE_MAX / (FORGE_UI__INDICES_PER_QUAD * sizeof(Uint32))) {
        SDL_Log("forge_ui_text_layout: allocation size overflow (indices)");
        return false;
    }
    size_t verts_bytes   = cap * FORGE_UI__VERTS_PER_QUAD * sizeof(ForgeUiVertex);
    size_t indices_bytes = cap * FORGE_UI__INDICES_PER_QUAD * sizeof(Uint32);

    ForgeUiVertex *verts = (ForgeUiVertex *)SDL_malloc(verts_bytes);
    Uint32 *indices = (Uint32 *)SDL_malloc(indices_bytes);

    if (!verts || !indices) {
        SDL_Log("forge_ui_text_layout: allocation failed");
        SDL_free(verts);
        SDL_free(indices);
        return false;
    }

    /* ── Layout loop ─────────────────────────────────────────────────── */
    float pen_x = x;
    float pen_y = y;
    float origin_x = x;            /* left edge for line resets */
    int quad_count = 0;             /* visible characters emitted */
    int line_count = 1;
    int line_start_vert = 0;        /* first vertex index of current line */
    float line_width = 0.0f;        /* pen advance on current line */
    float max_line_width = 0.0f;    /* widest line seen so far */

    for (int i = 0; i < text_len; i++) {
        unsigned char ch = (unsigned char)text[i];

        /* ── Newline: start a new line ────────────────────────────── */
        if (ch == '\n') {
            /* Apply alignment to the completed line */
            if (o->max_width > 0.0f && o->alignment != FORGE_UI_TEXT_ALIGN_LEFT) {
                forge_ui__align_line(verts, line_start_vert,
                                      quad_count * FORGE_UI__VERTS_PER_QUAD,
                                      line_width, o->max_width, o->alignment);
            }

            if (line_width > max_line_width) max_line_width = line_width;

            pen_x = origin_x;
            pen_y += line_height;
            line_width = 0.0f;
            line_start_vert = quad_count * FORGE_UI__VERTS_PER_QUAD;
            line_count++;
            continue;
        }

        /* ── Tab: advance to next tab stop ────────────────────────── */
        if (ch == '\t') {
            float tab_width = space_advance * FORGE_UI__TAB_STOP_WIDTH;
            if (tab_width > 0.0f) {
                float rel_x = pen_x - origin_x;
                float next_stop = tab_width *
                    (SDL_floorf(rel_x / tab_width) + 1.0f);
                pen_x = origin_x + next_stop;
                line_width = pen_x - origin_x;
            }
            continue;
        }

        /* ── Look up glyph in atlas ───────────────────────────────── */
        const ForgeUiPackedGlyph *glyph = forge_ui_atlas_lookup(atlas, ch);
        if (!glyph) continue;  /* skip unmapped characters */

        float advance = (float)glyph->advance_width * scale;

        /* ── Line wrapping: check if this character exceeds max_width ── */
        if (o->max_width > 0.0f && line_width + advance > o->max_width &&
            line_width > 0.0f) {
            /* Apply alignment to the completed line */
            if (o->alignment != FORGE_UI_TEXT_ALIGN_LEFT) {
                forge_ui__align_line(verts, line_start_vert,
                                      quad_count * FORGE_UI__VERTS_PER_QUAD,
                                      line_width, o->max_width, o->alignment);
            }

            if (line_width > max_line_width) max_line_width = line_width;

            pen_x = origin_x;
            pen_y += line_height;
            line_width = 0.0f;
            line_start_vert = quad_count * FORGE_UI__VERTS_PER_QUAD;
            line_count++;
        }

        /* ── Space: advance pen but don't emit a quad ─────────────── */
        if (ch == ' ') {
            pen_x += advance;
            line_width += advance;
            continue;
        }

        /* ── Visible character: emit quad ─────────────────────────── */
        if (glyph->bitmap_w == 0 || glyph->bitmap_h == 0) {
            /* Glyph has no visible bitmap — just advance the pen */
            pen_x += advance;
            line_width += advance;
            continue;
        }

        /* Compute screen-space quad position from pen + bearings.
         * bearing_x is the horizontal offset from pen to bitmap left edge.
         * bearing_y is the vertical offset from baseline to bitmap top edge.
         * In y-down screen coordinates: bitmap top = pen_y - bearing_y. */
        float qx0 = pen_x + (float)glyph->bearing_x;
        float qy0 = pen_y - (float)glyph->bearing_y;
        float qx1 = qx0 + (float)glyph->bitmap_w;
        float qy1 = qy0 + (float)glyph->bitmap_h;

        /* Emit the quad */
        forge_ui__emit_quad(
            verts, quad_count * FORGE_UI__VERTS_PER_QUAD,
            indices, quad_count * FORGE_UI__INDICES_PER_QUAD,
            qx0, qy0, qx1, qy1,
            glyph->uv.u0, glyph->uv.v0, glyph->uv.u1, glyph->uv.v1,
            o->r, o->g, o->b, o->a);

        quad_count++;
        pen_x += advance;
        line_width += advance;
    }

    /* ── Finalize last line ──────────────────────────────────────────── */
    if (o->max_width > 0.0f && o->alignment != FORGE_UI_TEXT_ALIGN_LEFT) {
        forge_ui__align_line(verts, line_start_vert,
                              quad_count * FORGE_UI__VERTS_PER_QUAD,
                              line_width, o->max_width, o->alignment);
    }

    if (line_width > max_line_width) max_line_width = line_width;

    /* ── Fill output struct ──────────────────────────────────────────── */
    out_layout->vertices     = verts;
    out_layout->vertex_count = quad_count * FORGE_UI__VERTS_PER_QUAD;
    out_layout->indices      = indices;
    out_layout->index_count  = quad_count * FORGE_UI__INDICES_PER_QUAD;
    out_layout->total_width  = max_line_width;
    out_layout->total_height = (float)line_count * line_height;
    out_layout->line_count   = line_count;

    return true;
}

static void forge_ui_text_layout_free(ForgeUiTextLayout *layout)
{
    if (!layout) return;
    SDL_free(layout->vertices);
    SDL_free(layout->indices);
    SDL_memset(layout, 0, sizeof(ForgeUiTextLayout));
}

/* ── forge_ui_text_measure ───────────────────────────────────────────────── */

static ForgeUiTextMetrics forge_ui_text_measure(const ForgeUiFontAtlas *atlas,
                                                 const char *text,
                                                 const ForgeUiTextOpts *opts)
{
    ForgeUiTextMetrics result = { 0.0f, 0.0f, 0 };

    if (!atlas || !text) return result;

    if (atlas->units_per_em == 0) return result;

    const ForgeUiTextOpts *o = opts ? opts : &FORGE_UI__DEFAULT_TEXT_OPTS;

    float scale = atlas->pixel_height / (float)atlas->units_per_em;
    float line_height = ((float)atlas->ascender - (float)atlas->descender +
                          (float)atlas->line_gap) * scale;

    /* Clamp to a safe minimum — same guard as forge_ui_text_layout */
    if (line_height <= 0.0f) {
        line_height = 1.0f;
    }

    const ForgeUiPackedGlyph *space_glyph = forge_ui_atlas_lookup(atlas, ' ');
    float space_advance = space_glyph
        ? (float)space_glyph->advance_width * scale
        : atlas->pixel_height * 0.5f;

    size_t raw_len = SDL_strlen(text);
    if (raw_len > (size_t)INT_MAX) return result;
    int text_len = (int)raw_len;
    if (text_len == 0) {
        result.line_count = 1;
        return result;
    }

    float pen_x = 0.0f;
    float max_line_width = 0.0f;
    int line_count = 1;

    for (int i = 0; i < text_len; i++) {
        unsigned char ch = (unsigned char)text[i];

        if (ch == '\n') {
            if (pen_x > max_line_width) max_line_width = pen_x;
            pen_x = 0.0f;
            line_count++;
            continue;
        }

        if (ch == '\t') {
            float tab_width = space_advance * FORGE_UI__TAB_STOP_WIDTH;
            if (tab_width > 0.0f) {
                float next_stop = tab_width *
                    (SDL_floorf(pen_x / tab_width) + 1.0f);
                pen_x = next_stop;
            }
            continue;
        }

        const ForgeUiPackedGlyph *glyph = forge_ui_atlas_lookup(atlas, ch);
        if (!glyph) continue;

        float advance = (float)glyph->advance_width * scale;

        if (o->max_width > 0.0f && pen_x + advance > o->max_width &&
            pen_x > 0.0f) {
            if (pen_x > max_line_width) max_line_width = pen_x;
            pen_x = 0.0f;
            line_count++;
        }

        pen_x += advance;
    }

    if (pen_x > max_line_width) max_line_width = pen_x;

    result.width      = max_line_width;
    result.height     = (float)line_count * line_height;
    result.line_count = line_count;
    return result;
}

#endif /* FORGE_UI_H */
