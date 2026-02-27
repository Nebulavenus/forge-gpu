/*
 * Fuzz harness for ForgeUiTextInputState buffer manipulation
 *
 * Exercises the text input editing paths (insertion, backspace, delete,
 * cursor movement) in random sequences with random data to shake out
 * memory safety issues, off-by-one writes, and invariant violations.
 *
 * Architecture:
 *   - Deterministic xorshift32 PRNG (seeded from argv or fixed default)
 *   - Each iteration allocates a small buffer with canary sentinel bytes,
 *     initializes a ForgeUiTextInputState, runs a random sequence of
 *     operations, and asserts invariants after every single operation.
 *   - On failure, prints the seed and iteration for exact reproducibility.
 *
 * This operates directly on ForgeUiTextInputState and the buffer
 * manipulation logic extracted from forge_ui_ctx_text_input -- no atlas,
 * no rendering, no ForgeUiContext needed.
 *
 * Usage:
 *   fuzz_text_input [seed] [iterations]
 *
 *   seed:       PRNG seed (default: 0xDEADBEEF)
 *   iterations: number of test rounds (default: FORGE_FUZZ_ITERATIONS
 *               or 100000)
 *
 * Exit code: 0 on success, 1 on any invariant violation.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include <string.h>
#include <stdlib.h>

/* We only need the ForgeUiTextInputState type -- pull it from the
 * context header.  The header also includes forge_ui.h and SDL.h. */
#include "ui/forge_ui_ctx.h"

/* ── Configurable iteration count ──────────────────────────────────────── */

#ifndef FORGE_FUZZ_ITERATIONS
#define FORGE_FUZZ_ITERATIONS 100000
#endif

/* ── Default PRNG seed ─────────────────────────────────────────────────── */

#define DEFAULT_SEED 0xDEADBEEFu

/* ── Canary sentinel for detecting out-of-bounds writes ────────────────── */

#define CANARY_SIZE   8        /* bytes past the end of the buffer */
#define CANARY_BYTE   0xDE     /* sentinel value */

/* ── Buffer capacity range ─────────────────────────────────────────────── */

#define MIN_CAPACITY  1        /* minimum buffer capacity (just '\0') */
#define MAX_CAPACITY  256      /* maximum buffer capacity */

/* ── Operation sequence range ──────────────────────────────────────────── */

#define MIN_OPS       1        /* minimum operations per iteration */
#define MAX_OPS       200      /* maximum operations per iteration */

/* ── Maximum insert length per operation ───────────────────────────────── */

#define MAX_INSERT_LEN 4       /* 1-4 random bytes per INSERT op */

/* ── Operation types ───────────────────────────────────────────────────── */

typedef enum FuzzOp {
    OP_INSERT = 0,   /* insert 1-4 random bytes at cursor */
    OP_BACKSPACE,    /* delete byte before cursor */
    OP_DELETE,       /* delete byte at cursor */
    OP_MOVE_LEFT,    /* cursor-- */
    OP_MOVE_RIGHT,   /* cursor++ */
    OP_HOME,         /* cursor = 0 */
    OP_END,          /* cursor = length */
    OP_COUNT         /* number of operations (must be last) */
} FuzzOp;

/* ── xorshift32 PRNG ──────────────────────────────────────────────────── */

static Uint32 prng_state;

static void prng_seed(Uint32 seed)
{
    /* xorshift32 must not be seeded with zero */
    prng_state = seed ? seed : 1u;
}

static Uint32 prng_next(void)
{
    Uint32 x = prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_state = x;
    return x;
}

/* Random integer in [lo, hi] (inclusive) */
static int prng_range(int lo, int hi)
{
    if (lo >= hi) return lo;
    Uint32 range = (Uint32)(hi - lo + 1);
    return lo + (int)(prng_next() % range);
}

/* Random byte (0x00 - 0xFF) */
static Uint8 prng_byte(void)
{
    return (Uint8)(prng_next() & 0xFF);
}

/* ── Failure reporting ─────────────────────────────────────────────────── */

static Uint32 g_seed;
static int    g_iteration;
static int    g_op_index;

#define FUZZ_FAIL(msg, ...)                                               \
    do {                                                                  \
        SDL_Log("FUZZ FAIL: " msg, ##__VA_ARGS__);                        \
        SDL_Log("  seed=%u  iteration=%d  op_index=%d",                   \
                (unsigned)g_seed, g_iteration, g_op_index);               \
        return false;                                                     \
    } while (0)

/* ── Invariant checker ─────────────────────────────────────────────────── */

/* Check all ForgeUiTextInputState invariants and canary integrity.
 * Returns true if all checks pass, false (with diagnostic log) on failure. */
static bool check_invariants(const ForgeUiTextInputState *st,
                             const Uint8 *canary_start,
                             int capacity)
{
    /* cursor in range */
    if (st->cursor < 0) {
        FUZZ_FAIL("cursor < 0: cursor=%d", st->cursor);
    }
    if (st->cursor > st->length) {
        FUZZ_FAIL("cursor > length: cursor=%d length=%d",
                  st->cursor, st->length);
    }

    /* length in range */
    if (st->length < 0) {
        FUZZ_FAIL("length < 0: length=%d", st->length);
    }
    if (st->length >= st->capacity) {
        FUZZ_FAIL("length >= capacity: length=%d capacity=%d",
                  st->length, st->capacity);
    }

    /* null termination */
    if (st->buffer[st->length] != '\0') {
        FUZZ_FAIL("buffer[length] != '\\0': buffer[%d]=0x%02X",
                  st->length, (unsigned)(Uint8)st->buffer[st->length]);
    }

    /* capacity unchanged */
    if (st->capacity != capacity) {
        FUZZ_FAIL("capacity changed: expected=%d actual=%d",
                  capacity, st->capacity);
    }

    /* canary bytes (detect out-of-bounds writes past buffer[capacity-1]) */
    for (int i = 0; i < CANARY_SIZE; i++) {
        if (canary_start[i] != CANARY_BYTE) {
            FUZZ_FAIL("canary[%d] corrupted: expected=0x%02X actual=0x%02X "
                      "(buffer overrun detected)",
                      i, CANARY_BYTE, (unsigned)canary_start[i]);
        }
    }

    return true;
}

/* ── Buffer manipulation operations ────────────────────────────────────── */
/* These replicate the exact logic from forge_ui_ctx_text_input so the
 * fuzzer exercises the same code paths a real caller would hit. */

static void do_insert(ForgeUiTextInputState *st, const char *text, int len)
{
    if (len <= 0) return;
    size_t raw_len = (size_t)len;

    /* Same guards as forge_ui_ctx_text_input */
    if (raw_len > (size_t)(st->capacity - 1)) return;
    int insert_len = (int)raw_len;
    if (insert_len >= st->capacity - st->length) return;

    SDL_memmove(st->buffer + st->cursor + insert_len,
                st->buffer + st->cursor,
                (size_t)(st->length - st->cursor));
    SDL_memcpy(st->buffer + st->cursor, text, (size_t)insert_len);
    st->cursor += insert_len;
    st->length += insert_len;
    st->buffer[st->length] = '\0';
}

static void do_backspace(ForgeUiTextInputState *st)
{
    if (st->cursor <= 0) return;
    SDL_memmove(st->buffer + st->cursor - 1,
                st->buffer + st->cursor,
                (size_t)(st->length - st->cursor));
    st->cursor--;
    st->length--;
    st->buffer[st->length] = '\0';
}

static void do_delete(ForgeUiTextInputState *st)
{
    if (st->cursor >= st->length) return;
    SDL_memmove(st->buffer + st->cursor,
                st->buffer + st->cursor + 1,
                (size_t)(st->length - st->cursor - 1));
    st->length--;
    st->buffer[st->length] = '\0';
}

static void do_move_left(ForgeUiTextInputState *st)
{
    if (st->cursor > 0) st->cursor--;
}

static void do_move_right(ForgeUiTextInputState *st)
{
    if (st->cursor < st->length) st->cursor++;
}

static void do_home(ForgeUiTextInputState *st)
{
    st->cursor = 0;
}

static void do_end(ForgeUiTextInputState *st)
{
    st->cursor = st->length;
}

/* ── Single fuzz iteration ─────────────────────────────────────────────── */

static bool fuzz_iteration(int iter)
{
    g_iteration = iter;
    g_op_index = -1;

    /* Choose a random buffer capacity in [MIN_CAPACITY, MAX_CAPACITY] */
    int capacity = prng_range(MIN_CAPACITY, MAX_CAPACITY);

    /* Allocate buffer + canary.  The canary sits immediately past the
     * buffer and is filled with CANARY_BYTE.  Any write past
     * buffer[capacity-1] will corrupt the canary. */
    size_t alloc_size = (size_t)capacity + CANARY_SIZE;
    Uint8 *raw = (Uint8 *)SDL_malloc(alloc_size);
    if (!raw) {
        SDL_Log("FUZZ: malloc failed (alloc_size=%zu)", alloc_size);
        return false;
    }

    /* Zero the buffer, fill canary with sentinel */
    SDL_memset(raw, 0, (size_t)capacity);
    SDL_memset(raw + capacity, CANARY_BYTE, CANARY_SIZE);

    Uint8 *canary_start = raw + capacity;

    /* Initialize state: empty buffer, cursor at 0 */
    ForgeUiTextInputState st;
    st.buffer   = (char *)raw;
    st.capacity = capacity;
    st.length   = 0;
    st.cursor   = 0;

    /* Check invariants at start */
    if (!check_invariants(&st, canary_start, capacity)) {
        SDL_free(raw);
        return false;
    }

    /* Run a random number of operations */
    int num_ops = prng_range(MIN_OPS, MAX_OPS);

    for (int op = 0; op < num_ops; op++) {
        g_op_index = op;

        FuzzOp which = (FuzzOp)(prng_next() % OP_COUNT);

        switch (which) {
        case OP_INSERT: {
            /* Generate 1-4 random bytes.  Mix of zero bytes, high-bit
             * bytes (invalid UTF-8), and valid ASCII. */
            int insert_len = prng_range(1, MAX_INSERT_LEN);
            char insert_buf[MAX_INSERT_LEN + 1];
            for (int i = 0; i < insert_len; i++) {
                insert_buf[i] = (char)prng_byte();
                /* Avoid generating a zero byte inside the insert string
                 * since SDL_strlen would truncate at it and the insert
                 * path checks text_input[0] != '\0'.  Instead, replace
                 * interior zeros with 0x01 and test zero bytes only as
                 * the first byte (which the guard rejects). */
                if (i > 0 && insert_buf[i] == '\0') {
                    insert_buf[i] = '\x01';
                }
            }
            /* Null-terminate so strlen works correctly */
            insert_buf[insert_len] = '\0';

            /* If the first byte is '\0', the real code path skips the
             * insert entirely, so skip it here too (matching behavior). */
            if (insert_buf[0] != '\0') {
                do_insert(&st, insert_buf, insert_len);
            }
            break;
        }
        case OP_BACKSPACE:
            do_backspace(&st);
            break;
        case OP_DELETE:
            do_delete(&st);
            break;
        case OP_MOVE_LEFT:
            do_move_left(&st);
            break;
        case OP_MOVE_RIGHT:
            do_move_right(&st);
            break;
        case OP_HOME:
            do_home(&st);
            break;
        case OP_END:
            do_end(&st);
            break;
        case OP_COUNT:
            break;  /* unreachable */
        }

        /* Assert invariants after every operation */
        if (!check_invariants(&st, canary_start, capacity)) {
            SDL_Log("  operation=%d (type=%d)", op, (int)which);
            SDL_Log("  buffer state: length=%d cursor=%d capacity=%d",
                    st.length, st.cursor, st.capacity);
            SDL_free(raw);
            return false;
        }
    }

    /* Final consistency check */
    g_op_index = num_ops;
    if (!check_invariants(&st, canary_start, capacity)) {
        SDL_free(raw);
        return false;
    }

    SDL_free(raw);
    return true;
}

/* ── Main ────────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    /* Parse seed from argv[1], or use default */
    g_seed = DEFAULT_SEED;
    if (argc > 1) {
        unsigned long parsed = strtoul(argv[1], NULL, 0);
        g_seed = (Uint32)parsed;
    }

    /* Parse iteration count from argv[2], or use compile-time default */
    int iterations = FORGE_FUZZ_ITERATIONS;
    if (argc > 2) {
        int parsed = atoi(argv[2]);
        if (parsed > 0) iterations = parsed;
    }

    if (!SDL_Init(0)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("=== Text Input Fuzz Harness ===");
    SDL_Log("  seed:       0x%08X (%u)", (unsigned)g_seed, (unsigned)g_seed);
    SDL_Log("  iterations: %d", iterations);
    SDL_Log("  capacity:   %d - %d bytes", MIN_CAPACITY, MAX_CAPACITY);
    SDL_Log("  ops/iter:   %d - %d", MIN_OPS, MAX_OPS);
    SDL_Log("  canary:     %d bytes (0x%02X)", CANARY_SIZE, CANARY_BYTE);
    SDL_Log("");

    prng_seed(g_seed);

    int progress_interval = iterations / 10;
    if (progress_interval <= 0) progress_interval = 1;

    for (int i = 0; i < iterations; i++) {
        if (i > 0 && (i % progress_interval) == 0) {
            SDL_Log("  ... %d / %d iterations (%.0f%%)",
                    i, iterations, (double)i / (double)iterations * 100.0);
        }

        if (!fuzz_iteration(i)) {
            SDL_Log("");
            SDL_Log("FAILED at iteration %d", i);
            SDL_Log("Reproduce: %s 0x%08X %d",
                    argv[0], (unsigned)g_seed, iterations);
            SDL_Quit();
            return 1;
        }
    }

    SDL_Log("");
    SDL_Log("=== PASSED: %d iterations, 0 invariant violations ===",
            iterations);
    SDL_Quit();
    return 0;
}
