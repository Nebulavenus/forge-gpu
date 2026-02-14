/*
 * forge.h — Shared utilities for forge-gpu lessons
 *
 * This is a header-only library that grows with each lesson.
 * Include it for common helpers; every lesson can also stand alone
 * by using SDL directly.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_H
#define FORGE_H

#include <SDL3/SDL.h>

/*
 * FORGE_CHECK — validate a condition and bail out with an SDL error message.
 *
 * Usage:
 *   FORGE_CHECK(device != NULL, "Failed to create GPU device");
 *
 * Logs the message + SDL_GetError(), then returns 1 (for use in main).
 */
#define FORGE_CHECK(cond, msg)                                           \
    do {                                                                 \
        if (!(cond)) {                                                   \
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,                    \
                         "%s: %s", (msg), SDL_GetError());               \
            return 1;                                                    \
        }                                                                \
    } while (0)

#endif /* FORGE_H */
