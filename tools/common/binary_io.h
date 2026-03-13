/*
 * Binary I/O helpers shared across forge-gpu tools.
 *
 * Static inline functions for writing little-endian values to SDL I/O
 * streams.  Include this header directly — no separate .c file needed.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_TOOLS_BINARY_IO_H
#define FORGE_TOOLS_BINARY_IO_H

#include <SDL3/SDL.h>
#include <string.h> /* memcpy */

/* Write a uint32 in little-endian byte order to an SDL I/O stream. */
static inline bool write_u32_le(SDL_IOStream *io, uint32_t val)
{
    uint8_t bytes[4];
    bytes[0] = (uint8_t)(val);
    bytes[1] = (uint8_t)(val >> 8);
    bytes[2] = (uint8_t)(val >> 16);
    bytes[3] = (uint8_t)(val >> 24);
    return SDL_WriteIO(io, bytes, 4) == 4;
}

/* Write a signed int32 in little-endian byte order. */
static inline bool write_i32_le(SDL_IOStream *io, int32_t val)
{
    uint32_t uval;
    memcpy(&uval, &val, sizeof(uval));
    return write_u32_le(io, uval);
}

/* Write a float in little-endian byte order. */
static inline bool write_float_le(SDL_IOStream *io, float val)
{
    uint32_t bits;
    memcpy(&bits, &val, sizeof(bits));
    return write_u32_le(io, bits);
}

#endif /* FORGE_TOOLS_BINARY_IO_H */
