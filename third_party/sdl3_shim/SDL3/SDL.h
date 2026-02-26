/*
 * SDL3 Shim — Minimal SDL3 stand-in for console-only lessons
 *
 * Provides just enough of the SDL3 API (SDL_Log, SDL_Init, SDL_malloc, etc.)
 * to build engine and math lessons without the full SDL3 library.
 * GPU lessons still require real SDL3.
 *
 * Usage:
 *   cmake -B build -DFORGE_USE_SHIM=ON
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef SDL3_SHIM_SDL_H
#define SDL3_SHIM_SDL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>

/* ── Types ──────────────────────────────────────────────────────────────── */

typedef uint8_t Uint8;
typedef int16_t Sint16;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;

/* ── Init / Quit ────────────────────────────────────────────────────────── */

#define SDL_INIT_VIDEO 0x00000020u

static inline bool SDL_Init(Uint32 flags)
{
    (void)flags;
    return true;
}

static inline void SDL_Quit(void) { }

static inline const char *SDL_GetError(void)
{
    return "(SDL3 shim)";
}

/* ── Logging ────────────────────────────────────────────────────────────── */
/*
 * SDL_Log is printf-style.  The real implementation writes to the platform
 * debug output; this shim uses printf to stdout.
 *
 * Implemented as a variadic static inline because a macro wrapping printf
 * with __VA_ARGS__ is less portable across C standards.
 */

static inline void SDL_Log(const char *fmt, ...)
{
    printf("INFO: ");
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

/* ── Memory ─────────────────────────────────────────────────────────────── */

static inline void *SDL_malloc(size_t size)            { return malloc(size); }
static inline void *SDL_calloc(size_t n, size_t sz)    { return calloc(n, sz); }
static inline void *SDL_realloc(void *p, size_t size)  { return realloc(p, size); }
static inline void  SDL_free(void *p)                  { free(p); }

static inline void *SDL_memcpy(void *dst, const void *src, size_t n)
{
    return memcpy(dst, src, n);
}

static inline void *SDL_memset(void *dst, int c, size_t n)
{
    return memset(dst, c, n);
}

static inline int SDL_memcmp(const void *a, const void *b, size_t n)
{
    return memcmp(a, b, n);
}

/* ── String helpers ─────────────────────────────────────────────────────── */

static inline size_t SDL_strlen(const char *s)         { return strlen(s); }
static inline int    SDL_strcmp(const char *a, const char *b) { return strcmp(a, b); }

static inline char *SDL_strdup(const char *s)
{
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

static inline int SDL_snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = vsnprintf(buf, n, fmt, args);
    va_end(args);
    return ret;
}

/* ── Math ───────────────────────────────────────────────────────────────── */

static inline float SDL_fabsf(float x)  { return x < 0 ? -x : x; }
static inline float SDL_sqrtf(float x)  { return (float)sqrt((double)x); }
static inline float SDL_fmodf(float x, float y) { return (float)fmod((double)x, (double)y); }
static inline float SDL_ceilf(float x)  { return (float)ceil((double)x); }
static inline float SDL_floorf(float x) { return (float)floor((double)x); }

/* ── Sorting ───────────────────────────────────────────────────────────── */

static inline void SDL_qsort(void *base, size_t nmemb, size_t size,
                              int (*compar)(const void *, const void *))
{
    qsort(base, nmemb, size, compar);
}

/* ── File I/O ───────────────────────────────────────────────────────────── */

static inline void *SDL_LoadFile(const char *file, size_t *datasize)
{
    FILE *fp = fopen(file, "rb");
    if (!fp) {
        if (datasize) *datasize = 0;
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (size < 0) {
        fclose(fp);
        if (datasize) *datasize = 0;
        return NULL;
    }

    void *data = malloc((size_t)size);
    if (!data) {
        fclose(fp);
        if (datasize) *datasize = 0;
        return NULL;
    }

    size_t bytes_read = fread(data, 1, (size_t)size, fp);
    fclose(fp);

    if (bytes_read != (size_t)size) {
        free(data);
        if (datasize) *datasize = 0;
        return NULL;
    }

    if (datasize) *datasize = (size_t)size;
    return data;
}

#endif /* SDL3_SHIM_SDL_H */
