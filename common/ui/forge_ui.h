/*
 * forge_ui.h -- Header-only TrueType font parser for forge-gpu
 *
 * Parses a TrueType (.ttf) font file and extracts table metadata, font
 * metrics, character-to-glyph mapping, and simple glyph outlines.
 *
 * Supports:
 *   - TTF offset table and table directory parsing
 *   - head table (unitsPerEm, bounding box, indexToLocFormat)
 *   - hhea table (ascender, descender, lineGap, numberOfHMetrics)
 *   - maxp table (numGlyphs)
 *   - cmap table with format 4 (BMP Unicode to glyph index mapping)
 *   - loca table (short and long format glyph offsets)
 *   - glyf table (simple glyph outlines with contours, flags, coordinates)
 *
 * Limitations (intentional for a learning library):
 *   - No compound glyph parsing (detected and skipped with a log message)
 *   - No hinting or grid-fitting instructions
 *   - No kerning (kern table) or advanced positioning (GPOS)
 *   - No glyph substitution (GSUB)
 *   - No rasterization (outlines only -- rasterization is a future lesson)
 *   - No per-glyph horizontal metrics (hmtx) -- future lesson
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

/* ── Public Types ────────────────────────────────────────────────────────── */

/* A 2D point in font units (integer coordinates). */
typedef struct ForgeUiPoint {
    Sint16 x;
    Sint16 y;
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
    Sint16       x_min;          /* glyph bounding box */
    Sint16       y_min;
    Sint16       x_max;
    Sint16       y_max;
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
    Uint8  *cmap_id_range_base; /* pointer into font data for range calc */

    /* loca table (glyph offsets into glyf) */
    Uint32 *loca_offsets;       /* numGlyphs + 1 entries, always uint32 */

    /* Offsets to key tables within font data */
    Uint32 glyf_offset;        /* start of glyf table in file */
} ForgeUiFont;

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
    font->cmap_id_range_base = (Uint8 *)id_ranges;

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

#define FORGE_UI__FLAG_ON_CURVE    0x01  /* point is on the curve */
#define FORGE_UI__FLAG_X_SHORT     0x02  /* x coordinate is 1 byte */
#define FORGE_UI__FLAG_Y_SHORT     0x04  /* y coordinate is 1 byte */
#define FORGE_UI__FLAG_REPEAT      0x08  /* next byte is repeat count */
#define FORGE_UI__FLAG_X_SAME      0x10  /* x is same (short=positive) */
#define FORGE_UI__FLAG_Y_SAME      0x20  /* y is same (short=positive) */

/* ── Public function implementations ─────────────────────────────────────── */

static bool forge_ui_ttf_load(const char *path, ForgeUiFont *out_font)
{
    SDL_memset(out_font, 0, sizeof(ForgeUiFont));

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
     * then hhea, cmap, loca, and cache glyf offset */
    if (!forge_ui__parse_head(out_font)  ||
        !forge_ui__parse_maxp(out_font)  ||
        !forge_ui__parse_hhea(out_font)  ||
        !forge_ui__parse_cmap(out_font)  ||
        !forge_ui__parse_loca(out_font)  ||
        !forge_ui__cache_glyf_offset(out_font)) {
        forge_ui_ttf_free(out_font);
        return false;
    }

    return true;
}

static void forge_ui_ttf_free(ForgeUiFont *font)
{
    if (!font) return;

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
        sizeof(Uint16) * (Uint16)num_contours);
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

#endif /* FORGE_UI_H */
