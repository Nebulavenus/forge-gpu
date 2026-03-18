/*
 * forge_containers.h — Header-only stretchy containers for forge-gpu
 *
 * Fat-pointer dynamic arrays and hash maps in a single header.
 * Every container is a plain typed pointer: `int *nums = NULL;`
 * Macros grow, query, and free the backing storage while the user
 * indexes the pointer like a normal C array.
 *
 * Dependencies:
 *   - SDL3 (for SDL_realloc, SDL_free, SDL_memmove, SDL_memset, SDL_memcmp,
 *           SDL_strcmp, SDL_strlen, SDL_Log)
 *
 * Usage:
 *   #include "containers/forge_containers.h"
 *
 *   int *nums = NULL;
 *   forge_arr_append(nums, 42);
 *   forge_arr_append(nums, 99);
 *   SDL_Log("length = %td", forge_arr_length(nums));
 *   forge_arr_free(nums);
 *
 * See: lessons/engine/13-stretchy-containers/ for a full tutorial
 * See: docs/stretchy-containers.md for the design specification
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_CONTAINERS_H
#define FORGE_CONTAINERS_H

#include <SDL3/SDL.h>
#include <stddef.h>   /* size_t, ptrdiff_t, offsetof */
#include <stdint.h>   /* uintptr_t, uint32_t, uint64_t */

/* ══════════════════════════════════════════════════════════════════════════
 * Configuration
 * ══════════════════════════════════════════════════════════════════════════ */

/* Bucket size for hash map index.  Default is 8 slots per bucket (one
 * cache line for the hash array on 64-bit).  Define to 4 before including
 * this header to use smaller buckets. */
#ifndef FORGE_CONTAINERS_BUCKET_SIZE
#define FORGE_CONTAINERS_BUCKET_SIZE 8
#endif

/* Minimum array capacity on first allocation. */
#define FORGE_CONTAINERS__MIN_CAP 4

/* Cache line alignment for bucket storage. */
#define FORGE_CONTAINERS__CACHE_LINE 64

/* String arena minimum block size. */
#define FORGE_CONTAINERS__ARENA_MIN_BLOCK 512

/* String arena maximum block size (1 MB). */
#define FORGE_CONTAINERS__ARENA_MAX_BLOCK (1024 * 1024)

/* ══════════════════════════════════════════════════════════════════════════
 * Platform detection
 * ══════════════════════════════════════════════════════════════════════════ */

/* typeof support: GCC/Clang have __typeof__, MSVC does not in C mode. */
#if defined(__GNUC__) || defined(__clang__)
  #define FORGE_CONTAINERS__HAS_TYPEOF 1
#else
  #define FORGE_CONTAINERS__HAS_TYPEOF 0
#endif

/* Rotate intrinsics for hash functions. */
static size_t forge_containers__rotl(size_t x, int n)
{
    int bits = (int)(sizeof(size_t) * 8);
    n &= bits - 1;
    return n ? (x << n) | (x >> (bits - n)) : x;
}

static size_t forge_containers__rotr(size_t x, int n)
{
    int bits = (int)(sizeof(size_t) * 8);
    n &= bits - 1;
    return n ? (x >> n) | (x << (bits - n)) : x;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Hidden header (fat pointer metadata)
 * ══════════════════════════════════════════════════════════════════════════ */

/* Forward declaration for the hash index. */
typedef struct forge_containers__hash_index forge_containers__hash_index;

/* The hidden header sits immediately before the user pointer.
 * Both arrays and hash maps share this layout. */
typedef struct forge_containers__header {
    size_t                        length;      /* elements currently stored */
    size_t                        capacity;    /* allocated element slots */
    forge_containers__hash_index *hash_table;  /* NULL for plain arrays */
    ptrdiff_t                     temp;        /* scratch for macro communication */
} forge_containers__header;

/* Retrieve the header from a user pointer.  The header is stored
 * immediately before the element data. */
#define forge_containers__hdr(p) \
    ((forge_containers__header *)((char *)(p) - sizeof(forge_containers__header)))

/* ══════════════════════════════════════════════════════════════════════════
 * Array internals
 * ══════════════════════════════════════════════════════════════════════════ */

/* Grow the backing array so it can hold at least `needed` total elements.
 * `elem_size` is sizeof one element.  The pointer is updated in place
 * through the void** parameter. */
static void forge_containers__arr_grow(void **p, size_t needed, size_t elem_size)
{
    forge_containers__header *hdr = NULL;
    size_t old_cap = 0;
    size_t new_cap;
    size_t alloc_size;
    void *new_block;

    if (elem_size == 0) return;

    if (*p) {
        hdr = forge_containers__hdr(*p);
        old_cap = hdr->capacity;
    }

    if (needed <= old_cap) return;

    /* Growth policy: double or use needed, minimum 4. */
    new_cap = old_cap * 2;
    if (new_cap < needed) new_cap = needed;
    if (new_cap < FORGE_CONTAINERS__MIN_CAP) new_cap = FORGE_CONTAINERS__MIN_CAP;

    /* Overflow check: ensure new_cap * elem_size + header fits in size_t. */
    if (new_cap > (SIZE_MAX - sizeof(forge_containers__header)) / elem_size) {
        SDL_Log("forge_containers: allocation size overflow");
        return;
    }
    alloc_size = sizeof(forge_containers__header) + new_cap * elem_size;

    if (*p) {
        new_block = SDL_realloc(hdr, alloc_size);
    } else {
        new_block = SDL_realloc(NULL, alloc_size);
    }

    if (!new_block) {
        SDL_Log("forge_containers: allocation failed (%zu bytes)", alloc_size);
        return;
    }

    hdr = (forge_containers__header *)new_block;

    if (old_cap == 0) {
        /* First allocation — zero the header. */
        hdr->length     = 0;
        hdr->capacity   = 0;
        hdr->hash_table = NULL;
        hdr->temp       = 0;
    }

    /* Zero newly available element slots.  Use captured old_cap rather than
     * hdr->capacity to avoid reading a field before it is updated. */
    {
        char *data = (char *)new_block + sizeof(forge_containers__header);
        size_t old_bytes = old_cap * elem_size;
        size_t new_bytes = new_cap * elem_size;
        if (new_bytes > old_bytes) {
            SDL_memset(data + old_bytes, 0, new_bytes - old_bytes);
        }
    }

    hdr->capacity = new_cap;
    *p = (char *)new_block + sizeof(forge_containers__header);
}

/* Ensure capacity for at least `count` more elements beyond current length. */
static void forge_containers__arr_maybe_grow(void **p, size_t count, size_t elem_size)
{
    size_t len = *p ? forge_containers__hdr(*p)->length : 0;
    /* Overflow check on len + count. */
    if (count > SIZE_MAX - len) {
        SDL_Log("forge_containers: element count overflow");
        return;
    }
    if (len + count > (*p ? forge_containers__hdr(*p)->capacity : 0)) {
        forge_containers__arr_grow(p, len + count, elem_size);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * Array macros (forge_arr_*)
 * ══════════════════════════════════════════════════════════════════════════ */

/* Internal: true when the array has at least one spare slot.
 * Guards writes after a failed grow on an already-allocated array. */
#define forge_containers__has_room(a) \
    ((a) && forge_containers__hdr(a)->length < forge_containers__hdr(a)->capacity)

/* Append one element. Grows if needed. Evaluates to the appended value.
 * If allocation fails, the append is silently skipped. */
#define forge_arr_append(a, val) \
    (forge_containers__arr_maybe_grow((void **)&(a), 1, sizeof(*(a))), \
     forge_containers__has_room(a) \
         ? (a)[forge_containers__hdr(a)->length++] = (val) : (val))

/* Number of stored elements (signed, ptrdiff_t).  0 for NULL. */
#define forge_arr_length(a) \
    ((a) ? (ptrdiff_t)forge_containers__hdr(a)->length : 0)

/* Number of stored elements (unsigned, size_t).  0 for NULL. */
#define forge_arr_length_unsigned(a) \
    ((a) ? forge_containers__hdr(a)->length : (size_t)0)

/* Current allocated capacity.  0 for NULL. */
#define forge_arr_capacity(a) \
    ((a) ? forge_containers__hdr(a)->capacity : (size_t)0)

/* Remove and return the last element.  No bounds check. */
#define forge_arr_pop(a) \
    ((a)[--forge_containers__hdr(a)->length])

/* Return the last element without removing it.  No bounds check. */
#define forge_arr_last(a) \
    ((a)[forge_containers__hdr(a)->length - 1])

/* Free all memory and set pointer to NULL. */
#define forge_arr_free(a) \
    ((a) ? (void)(SDL_free(forge_containers__hdr(a)), (a) = NULL) : (void)0)

/* Insert one element at index i, shifting subsequent elements right. */
#define forge_arr_insert_at(a, i, val)                                       \
    (forge_containers__arr_maybe_grow((void **)&(a), 1, sizeof(*(a))),       \
     forge_containers__has_room(a)                                           \
         ? (SDL_memmove(&(a)[(i) + 1], &(a)[(i)],                           \
                 (forge_containers__hdr(a)->length - (size_t)(i))             \
                     * sizeof(*(a))),                                         \
            forge_containers__hdr(a)->length++,                               \
            (a)[(i)] = (val)) : (val))

/* Insert n uninitialized elements at index i. */
#define forge_arr_insert_n_at(a, i, n)                                       \
    (forge_containers__arr_maybe_grow((void **)&(a), (n), sizeof(*(a))),     \
     ((a) && forge_containers__hdr(a)->length + (size_t)(n)                  \
           <= forge_containers__hdr(a)->capacity)                             \
         ? (SDL_memmove(&(a)[(i) + (n)], &(a)[(i)],                         \
                 (forge_containers__hdr(a)->length - (size_t)(i))             \
                     * sizeof(*(a))),                                         \
            SDL_memset(&(a)[(i)], 0, (size_t)(n) * sizeof(*(a))),            \
            forge_containers__hdr(a)->length += (size_t)(n))                  \
         : (size_t)0)

/* Delete the element at index i, shifting subsequent elements left. */
#define forge_arr_delete_at(a, i)                                            \
    (SDL_memmove(&(a)[(i)], &(a)[(i) + 1],                                  \
                 (forge_containers__hdr(a)->length - (size_t)(i) - 1)        \
                     * sizeof(*(a))),                                         \
     forge_containers__hdr(a)->length--)

/* Delete n elements starting at index i. */
#define forge_arr_delete_n_at(a, i, n)                                       \
    (SDL_memmove(&(a)[(i)], &(a)[(i) + (n)],                                \
                 (forge_containers__hdr(a)->length - (size_t)(i)             \
                     - (size_t)(n)) * sizeof(*(a))),                          \
     forge_containers__hdr(a)->length -= (size_t)(n))

/* O(1) unordered removal: overwrite element at i with the last element. */
#define forge_arr_swap_remove(a, i)                                          \
    ((a)[(i)] = (a)[forge_containers__hdr(a)->length - 1],                   \
     forge_containers__hdr(a)->length--)

/* Append n uninitialized elements and return pointer to the first new one.
 * Returns NULL if allocation fails. */
#define forge_arr_grow_by_ptr(a, n)                                          \
    (forge_containers__arr_maybe_grow((void **)&(a), (n), sizeof(*(a))),     \
     ((a) && forge_containers__hdr(a)->length + (size_t)(n)                  \
           <= forge_containers__hdr(a)->capacity)                             \
         ? (forge_containers__hdr(a)->length += (size_t)(n),                 \
            &(a)[forge_containers__hdr(a)->length - (size_t)(n)])            \
         : NULL)

/* Append n uninitialized elements and return index of the first new one.
 * Returns -1 if allocation fails. */
#define forge_arr_grow_by_index(a, n)                                        \
    (forge_containers__arr_maybe_grow((void **)&(a), (n), sizeof(*(a))),     \
     ((a) && forge_containers__hdr(a)->length + (size_t)(n)                  \
           <= forge_containers__hdr(a)->capacity)                             \
         ? (forge_containers__hdr(a)->length += (size_t)(n),                 \
            (ptrdiff_t)(forge_containers__hdr(a)->length - (size_t)(n)))     \
         : (ptrdiff_t)-1)

/* Set the length to n.  If n > length, new slots are uninitialized.
 * Grows if n > capacity.  Never shrinks the allocation.
 * If grow fails, length is not changed. */
#define forge_arr_set_length(a, n)                                           \
    (forge_containers__arr_maybe_grow((void **)&(a),                         \
         (size_t)(n) > forge_arr_length_unsigned(a)                          \
             ? (size_t)(n) - forge_arr_length_unsigned(a) : 0,               \
         sizeof(*(a))),                                                      \
     ((a) && (size_t)(n) <= forge_containers__hdr(a)->capacity)              \
         ? (forge_containers__hdr(a)->length = (size_t)(n)) : (size_t)0)

/* Ensure capacity >= n without changing length. */
#define forge_arr_set_capacity(a, n)                                         \
    ((size_t)(n) > forge_arr_capacity(a)                                     \
        ? forge_containers__arr_grow((void **)&(a), (size_t)(n),             \
                                     sizeof(*(a)))                           \
        : (void)0)

/* ══════════════════════════════════════════════════════════════════════════
 * Hash functions
 * ══════════════════════════════════════════════════════════════════════════ */

/* Global seed for per-table seed derivation.  Each new hash index gets
 * a unique seed derived from this, then the global seed advances via LCG. */
static size_t forge_containers__global_seed = 0x31415926;

/* Advance the global seed using a linear congruential generator. */
static size_t forge_containers__advance_seed(void)
{
    size_t seed = forge_containers__global_seed;
#if INTPTR_MAX == INT64_MAX
    /* 64-bit Knuth LCG constants. */
    size_t a = ((size_t)0x27BB2EE6 << 32) | (size_t)0x87B0B0FD;
    size_t b = (size_t)0xB504F32D;
#else
    /* 32-bit Knuth LCG constants. */
    size_t a = 2147001325u;
    size_t b =  715136305u;
#endif
    forge_containers__global_seed = seed * a + b;
    return seed;
}

/* 4-byte hash (Jenkins/Wang mix).  Input is read as little-endian u32. */
static size_t forge_containers__hash4(const void *key, size_t seed)
{
    uint32_t h;
    SDL_memcpy(&h, key, 4);
    h ^= (uint32_t)seed;
    h = (h ^ 61u) ^ (h >> 16);
    h = h + (h << 3);
    h = h ^ (h >> 4);
    h = h * 0x27d4eb2du;  /* Jenkins/Wang multiplicative constant */
    h ^= (uint32_t)seed;
    h = h ^ (h >> 15);
#if INTPTR_MAX == INT64_MAX
    return (((size_t)h << 32) | (size_t)h) ^ seed;
#else
    return (size_t)h;
#endif
}

/* 8-byte hash (Thomas Wang 64-bit mix).  64-bit platforms only. */
#if INTPTR_MAX == INT64_MAX
static size_t forge_containers__hash8(const void *key, size_t seed)
{
    uint64_t h;
    SDL_memcpy(&h, key, 8);
    h ^= seed;
    h = (~h) + (h << 21);
    h ^= forge_containers__rotr(h, 24);
    h *= 265;   /* Thomas Wang 64-bit mix constants */
    h ^= forge_containers__rotr(h, 14);
    h ^= seed;
    h *= 21;    /* Thomas Wang 64-bit mix constants */
    h ^= forge_containers__rotr(h, 28);
    h += (h << 31);
    h = (~h) + (h << 18);
    return (size_t)h;
}
#endif

/* SipHash variant for arbitrary key sizes.  Default: SipHash-1-1 (fast).
 * Enable FORGE_CONTAINERS_SIPHASH_2_4 for spec-compliant SipHash-2-4. */
#ifdef FORGE_CONTAINERS_SIPHASH_2_4
  #define FORGE_CONTAINERS__SIP_CROUNDS 2
  #define FORGE_CONTAINERS__SIP_FROUNDS 4
#else
  #define FORGE_CONTAINERS__SIP_CROUNDS 1
  #define FORGE_CONTAINERS__SIP_FROUNDS 1
#endif

#define FORGE_CONTAINERS__SIPROUND(v0, v1, v2, v3)                           \
    do {                                                                     \
        int sb_ = (int)(sizeof(size_t) * 8);                                 \
        (v0) += (v1); (v1) = forge_containers__rotl((v1), 13);              \
        (v1) ^= (v0); (v0) = forge_containers__rotl((v0), sb_ / 2);        \
        (v2) += (v3); (v3) = forge_containers__rotl((v3), 16);              \
        (v3) ^= (v2);                                                        \
        (v2) += (v1); (v1) = forge_containers__rotl((v1), 17);              \
        (v1) ^= (v2); (v2) = forge_containers__rotl((v2), sb_ / 2);        \
        (v0) += (v3); (v3) = forge_containers__rotl((v3), 21);              \
        (v3) ^= (v0);                                                        \
    } while (0)

static size_t forge_containers__siphash(const void *key, size_t len, size_t seed)
{
    const unsigned char *data = (const unsigned char *)key;
    size_t v0, v1, v2, v3;
    size_t word;
    size_t i;
    int r;

    /* Truncated constants for 32-bit. */
#if INTPTR_MAX == INT64_MAX
    v0 = (size_t)0x736f6d6570736575ULL ^  seed;
    v1 = (size_t)0x646f72616e646f6dULL ^ ~seed;
    v2 = (size_t)0x6c7967656e657261ULL ^  seed;
    v3 = (size_t)0x7465646279746573ULL ^ ~seed;
#else
    v0 = (size_t)0x70736575u ^  seed;
    v1 = (size_t)0x6e646f6du ^ ~seed;
    v2 = (size_t)0x6e657261u ^  seed;
    v3 = (size_t)0x79746573u ^ ~seed;
#endif

    /* Process full words. */
    {
        size_t nwords = len / sizeof(size_t);
        for (i = 0; i < nwords; i++) {
            SDL_memcpy(&word, data + i * sizeof(size_t), sizeof(size_t));
            v3 ^= word;
            for (r = 0; r < FORGE_CONTAINERS__SIP_CROUNDS; r++) {
                FORGE_CONTAINERS__SIPROUND(v0, v1, v2, v3);
            }
            v0 ^= word;
        }
    }

    /* Final partial word with length in high byte. */
    {
        size_t remaining = len & (sizeof(size_t) - 1);
        const unsigned char *tail = data + (len - remaining);
        word = (size_t)(len & 0xFF) << ((sizeof(size_t) - 1) * 8);
        for (i = 0; i < remaining; i++) {
            word |= (size_t)tail[i] << (i * 8);
        }
        v3 ^= word;
        for (r = 0; r < FORGE_CONTAINERS__SIP_CROUNDS; r++) {
            FORGE_CONTAINERS__SIPROUND(v0, v1, v2, v3);
        }
        v0 ^= word;
    }

    /* Finalization. */
    v2 ^= 0xFF;
    for (r = 0; r < FORGE_CONTAINERS__SIP_FROUNDS; r++) {
        FORGE_CONTAINERS__SIPROUND(v0, v1, v2, v3);
    }

#ifdef FORGE_CONTAINERS_SIPHASH_2_4
    return v0 ^ v1 ^ v2 ^ v3;
#else
    return v1 ^ v2 ^ v3;
#endif
}

/* Dispatch hash by key size for non-string keys. */
static size_t forge_containers__hash(const void *key, size_t key_size, size_t seed)
{
#ifndef FORGE_CONTAINERS_STRONG_HASH
    if (key_size == 4) return forge_containers__hash4(key, seed);
  #if INTPTR_MAX == INT64_MAX
    if (key_size == 8) return forge_containers__hash8(key, seed);
  #endif
#endif
    return forge_containers__siphash(key, key_size, seed);
}

/* String hash: rotate-and-add loop + Wang avalanche. */
static size_t forge_containers__hash_string(const char *str, size_t seed)
{
    size_t h = seed;
    if (!str) return seed;
    while (*str) {
        h = forge_containers__rotl(h, 9) + (size_t)(unsigned char)*str;
        str++;
    }
    h ^= seed;
    h = (~h) + (h << 18);
    h ^= forge_containers__rotr(h, 31);
    h *= 21;
    h ^= forge_containers__rotr(h, 11);
    h += (h << 6);
    h ^= forge_containers__rotr(h, 22);
    return h + seed;
}

/* Clamp hash to reserved sentinel range.  0 = empty, 1 = deleted. */
static size_t forge_containers__hash_clamp(size_t h)
{
    return h < 2 ? 2 : h;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Hash index structure
 * ══════════════════════════════════════════════════════════════════════════ */

/* Bucket: parallel arrays of cached hashes and data indices. */
typedef struct forge_containers__bucket {
    size_t    hash[FORGE_CONTAINERS_BUCKET_SIZE];   /* cached hash per slot (0=empty, 1=deleted, >=2=occupied) */
    ptrdiff_t index[FORGE_CONTAINERS_BUCKET_SIZE];  /* data array index per slot (-1=empty, -2=deleted, >=0=entry) */
} forge_containers__bucket;

/* String arena block for arena-mode string maps. */
typedef struct forge_containers__arena_block {
    struct forge_containers__arena_block *next;  /* next block in chain (NULL = last) */
    size_t capacity;                             /* usable bytes in this block */
    size_t used;                                 /* bytes consumed from the end (high-to-low packing) */
    /* Data follows immediately after this header. */
} forge_containers__arena_block;

/* String ownership modes. */
#define FORGE_CONTAINERS__MODE_NONE    0  /* non-string map */
#define FORGE_CONTAINERS__MODE_USER    1  /* user manages key lifetime */
#define FORGE_CONTAINERS__MODE_STRDUP  2  /* library duplicates keys */
#define FORGE_CONTAINERS__MODE_ARENA   3  /* library arena-allocates keys */

/* The hash index: metadata + cache-aligned bucket storage. */
struct forge_containers__hash_index {
    char  *temp_key;          /* set by shm_put_key to the stored key; read by forge_shm_put macro */
    size_t slot_count;        /* total slots (power of 2) */
    size_t slot_count_log2;   /* log2(slot_count) */
    size_t used_count;        /* live entries in the index */
    size_t tombstone_count;   /* deleted slots */
    size_t grow_threshold;    /* ~75% of slot_count */
    size_t tombstone_threshold; /* ~18.75% of slot_count */
    size_t shrink_threshold;  /* ~25% of slot_count */
    size_t seed;              /* per-table hash seed */
    struct {
        unsigned char mode;   /* FORGE_CONTAINERS__MODE_* */
        forge_containers__arena_block *first_block;  /* head of arena block chain */
        size_t block_counter;                          /* blocks allocated (drives size doubling) */
    } arena;
    forge_containers__bucket *buckets;  /* cache-aligned bucket storage (within same allocation) */
};

/* Compute thresholds for a given slot count. */
static void forge_containers__compute_thresholds(forge_containers__hash_index *idx)
{
    size_t sc = idx->slot_count;
    idx->grow_threshold      = sc - (sc >> 2);             /* ~75% */
    idx->tombstone_threshold = (sc >> 3) + (sc >> 4);      /* ~18.75% */
    idx->shrink_threshold    = (sc <= (size_t)FORGE_CONTAINERS_BUCKET_SIZE)
                                   ? 0 : (sc >> 2);        /* ~25%, or 0 at min */
}

/* Log2 of a power-of-2 value.  Precondition: v >= 1. */
static size_t forge_containers__log2(size_t v)
{
    size_t r = 0;
    while (v > 1) { v >>= 1; r++; }
    return r;
}

/* Allocate a new hash index with the given slot count. */
static forge_containers__hash_index *forge_containers__index_new(size_t slot_count, size_t seed)
{
    size_t bucket_count;
    size_t bucket_storage;
    size_t meta_size;
    size_t aligned_offset;
    size_t total;
    forge_containers__hash_index *idx;
    size_t i, j;

    if (slot_count < (size_t)FORGE_CONTAINERS_BUCKET_SIZE) {
        slot_count = (size_t)FORGE_CONTAINERS_BUCKET_SIZE;
    }

    bucket_count   = slot_count / (size_t)FORGE_CONTAINERS_BUCKET_SIZE;
    bucket_storage = bucket_count * sizeof(forge_containers__bucket);
    meta_size      = sizeof(forge_containers__hash_index);

    /* Align bucket storage to cache line. */
    aligned_offset = (meta_size + FORGE_CONTAINERS__CACHE_LINE - 1)
                     & ~(size_t)(FORGE_CONTAINERS__CACHE_LINE - 1);
    total = aligned_offset + bucket_storage;

    idx = (forge_containers__hash_index *)SDL_malloc(total);
    if (!idx) {
        SDL_Log("forge_containers: hash index allocation failed (%zu bytes)", total);
        return NULL;
    }
    SDL_memset(idx, 0, total);

    idx->slot_count      = slot_count;
    idx->slot_count_log2 = forge_containers__log2(slot_count);
    idx->seed            = seed;
    idx->buckets         = (forge_containers__bucket *)((char *)idx + aligned_offset);

    forge_containers__compute_thresholds(idx);

    /* Invariant: at least one slot must always be empty to terminate probes. */
    if (idx->grow_threshold + idx->tombstone_threshold >= idx->slot_count) {
        SDL_Log("forge_containers: threshold invariant violated "
                "(grow=%zu + tombstone=%zu >= slots=%zu)",
                idx->grow_threshold, idx->tombstone_threshold, idx->slot_count);
    }

    /* Initialize empty-slot sentinel for index values.  Hash values are
     * already 0 (empty) from the SDL_memset above; only index needs -1. */
    for (i = 0; i < bucket_count; i++) {
        for (j = 0; j < (size_t)FORGE_CONTAINERS_BUCKET_SIZE; j++) {
            idx->buckets[i].index[j] = -1;
        }
    }

    return idx;
}

/* Free a hash index and its arena blocks if any. */
static void forge_containers__index_free(forge_containers__hash_index *idx)
{
    if (!idx) return;
    /* Free arena blocks. */
    {
        forge_containers__arena_block *blk = idx->arena.first_block;
        while (blk) {
            forge_containers__arena_block *next = blk->next;
            SDL_free(blk);
            blk = next;
        }
    }
    SDL_free(idx);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Hash map internals
 * ══════════════════════════════════════════════════════════════════════════ */

/* Find a key in the hash index.  Returns the data-array index (user-visible
 * space, >= 0) or -1 if not found.  If `out_slot_bucket` and `out_slot_pos`
 * are non-NULL, they are set to the bucket/slot where the key was found
 * (or the first empty/tombstone slot for insertion). */
static ptrdiff_t forge_containers__hm_find(
    forge_containers__hash_index *idx,
    const void *key, size_t key_size, size_t hash,
    const void *data, size_t elem_size, size_t key_offset,
    size_t *out_slot_bucket, size_t *out_slot_pos,
    int is_string)
{
    size_t mask = idx->slot_count - 1;
    size_t pos  = hash & mask;
    size_t step = (size_t)FORGE_CONTAINERS_BUCKET_SIZE;
    size_t tombstone_bucket = (size_t)-1;
    size_t tombstone_slot   = (size_t)-1;

    for (;;) {
        size_t bi = pos / (size_t)FORGE_CONTAINERS_BUCKET_SIZE;
        size_t si_start = pos & ((size_t)FORGE_CONTAINERS_BUCKET_SIZE - 1);
        size_t si;
        forge_containers__bucket *b = &idx->buckets[bi];

        /* Scan from starting slot through end of bucket, then wrap. */
        for (si = si_start; si < (size_t)FORGE_CONTAINERS_BUCKET_SIZE; si++) {
            if (b->hash[si] == 0) {
                /* Empty slot — key not found. */
                if (out_slot_bucket) {
                    if (tombstone_bucket != (size_t)-1) {
                        *out_slot_bucket = tombstone_bucket;
                        *out_slot_pos    = tombstone_slot;
                    } else {
                        *out_slot_bucket = bi;
                        *out_slot_pos    = si;
                    }
                }
                return -1;
            }
            if (b->hash[si] == 1) {
                /* Tombstone — remember it for potential insertion. */
                if (tombstone_bucket == (size_t)-1) {
                    tombstone_bucket = bi;
                    tombstone_slot   = si;
                }
                continue;
            }
            if (b->hash[si] == hash) {
                /* Hash match — compare full key. */
                ptrdiff_t di = b->index[si];
                /* di is in user-visible space; raw array index = di + 1. */
                const char *entry = (const char *)data + ((size_t)di + 1) * elem_size;
                const char *entry_key = entry + key_offset;
                int match;
                if (is_string) {
                    match = SDL_strcmp(*(const char *const *)entry_key,
                                      *(const char *const *)key) == 0;
                } else {
                    match = SDL_memcmp(entry_key, key, key_size) == 0;
                }
                if (match) {
                    if (out_slot_bucket) {
                        *out_slot_bucket = bi;
                        *out_slot_pos    = si;
                    }
                    return di;
                }
            }
        }
        /* Wrap to start of bucket (slots before si_start). */
        for (si = 0; si < si_start; si++) {
            if (b->hash[si] == 0) {
                if (out_slot_bucket) {
                    if (tombstone_bucket != (size_t)-1) {
                        *out_slot_bucket = tombstone_bucket;
                        *out_slot_pos    = tombstone_slot;
                    } else {
                        *out_slot_bucket = bi;
                        *out_slot_pos    = si;
                    }
                }
                return -1;
            }
            if (b->hash[si] == 1) {
                if (tombstone_bucket == (size_t)-1) {
                    tombstone_bucket = bi;
                    tombstone_slot   = si;
                }
                continue;
            }
            if (b->hash[si] == hash) {
                ptrdiff_t di = b->index[si];
                const char *entry = (const char *)data + ((size_t)di + 1) * elem_size;
                const char *entry_key = entry + key_offset;
                int match;
                if (is_string) {
                    match = SDL_strcmp(*(const char *const *)entry_key,
                                      *(const char *const *)key) == 0;
                } else {
                    match = SDL_memcmp(entry_key, key, key_size) == 0;
                }
                if (match) {
                    if (out_slot_bucket) {
                        *out_slot_bucket = bi;
                        *out_slot_pos    = si;
                    }
                    return di;
                }
            }
        }

        /* Move to next bucket (quadratic probing). */
        pos = (pos + step) & mask;
        step += (size_t)FORGE_CONTAINERS_BUCKET_SIZE;
    }
}

/* Rebuild the hash index with a new slot count.  Reinserts all live entries.
 * The data array is unaffected — only the index changes. */
static forge_containers__hash_index *forge_containers__index_rebuild(
    forge_containers__hash_index *old_idx,
    size_t new_slot_count,
    const void *data, size_t elem_count, size_t elem_size,
    size_t key_offset, size_t key_size, int is_string)
{
    forge_containers__hash_index *new_idx;
    size_t i;

    new_idx = forge_containers__index_new(new_slot_count, old_idx->seed);
    if (!new_idx) return old_idx;

    /* Copy arena state. */
    new_idx->arena = old_idx->arena;

    /* Reinsert all live entries. */
    for (i = 0; i < elem_count; i++) {
        const char *entry = (const char *)data + (i + 1) * elem_size;
        const char *entry_key = entry + key_offset;
        size_t h;
        size_t sb, sp;

        if (is_string) {
            h = forge_containers__hash_clamp(
                    forge_containers__hash_string(
                        *(const char *const *)entry_key, new_idx->seed));
        } else {
            h = forge_containers__hash_clamp(
                    forge_containers__hash(entry_key, key_size, new_idx->seed));
        }

        forge_containers__hm_find(new_idx, entry_key, key_size, h,
                                   data, elem_size, key_offset, &sb, &sp,
                                   is_string);
        new_idx->buckets[sb].hash[sp]  = h;
        new_idx->buckets[sb].index[sp] = (ptrdiff_t)i;
        new_idx->used_count++;
    }

    /* Free the old index but NOT its arena blocks (transferred). */
    old_idx->arena.first_block = NULL;
    forge_containers__index_free(old_idx);

    return new_idx;
}

/* Put a key into the hash map.  `p` points to the user pointer (data array
 * base + 1).  Returns the user-visible index of the entry.  The entry
 * at that index has its key set but value is not yet written (the macro
 * layer does that). */
static ptrdiff_t forge_containers__hm_put_key(
    void **p, size_t elem_size, size_t key_offset, size_t key_size,
    const void *key, int is_string)
{
    forge_containers__header *hdr;
    forge_containers__hash_index *idx;
    size_t h;
    ptrdiff_t found;
    size_t sb, sp;
    ptrdiff_t new_di;
    void *raw_base;

    /* Lazy init: first put on a NULL map.
     * The user pointer points to element 0 of the raw array (the default
     * element).  Macros index as (a)[index+1] for real entries.
     * forge_hm_length returns raw_length - 1. */
    if (!*p) {
        forge_containers__arr_grow(p, 1, elem_size);
        if (!*p) return -1;
        hdr = forge_containers__hdr(*p);
        hdr->length = 1; /* element 0 = default value */
        SDL_memset(*p, 0, elem_size);

        idx = forge_containers__index_new(
                  (size_t)FORGE_CONTAINERS_BUCKET_SIZE,
                  forge_containers__advance_seed());
        if (!idx) {
            SDL_free(forge_containers__hdr(*p));
            *p = NULL;
            return -1;
        }
        hdr->hash_table = idx;
    }

    hdr = forge_containers__hdr(*p);
    idx = hdr->hash_table;

    /* ensure_default may have created the data array without a hash index. */
    if (!idx) {
        idx = forge_containers__index_new(
                  (size_t)FORGE_CONTAINERS_BUCKET_SIZE,
                  forge_containers__advance_seed());
        if (!idx) return -1;
        hdr->hash_table = idx;
    }

    /* Compute hash. */
    if (is_string) {
        h = forge_containers__hash_clamp(
                forge_containers__hash_string(*(const char *const *)key, idx->seed));
    } else {
        h = forge_containers__hash_clamp(
                forge_containers__hash(key, key_size, idx->seed));
    }

    /* Check if key already exists. */
    raw_base = *p;
    found = forge_containers__hm_find(idx, key, key_size, h,
                                       raw_base, elem_size, key_offset,
                                       &sb, &sp, is_string);
    if (found >= 0) {
        /* Key exists — return its index.  Value will be overwritten by macro. */
        hdr->temp = found;
        return found;
    }

    /* Check if we need to grow the hash index. */
    if (idx->used_count + idx->tombstone_count + 1 >= idx->grow_threshold) {
        idx = forge_containers__index_rebuild(
                  idx, idx->slot_count * 2,
                  raw_base, hdr->length - 1, elem_size,
                  key_offset, key_size, is_string);
        hdr->hash_table = idx;

        /* Re-find insertion point in new index. */
        forge_containers__hm_find(idx, key, key_size, h,
                                   raw_base, elem_size, key_offset,
                                   &sb, &sp, is_string);
    }

    /* Append new entry to data array. */
    forge_containers__arr_maybe_grow(p, 1, elem_size);
    hdr = forge_containers__hdr(*p);
    /* Check that grow succeeded (capacity > length). */
    if (hdr->length >= hdr->capacity) return -1;
    raw_base = *p;

    /* The new entry goes at raw index = hdr->length.
     * User-visible index = hdr->length - 1. */
    new_di = (ptrdiff_t)(hdr->length - 1);
    SDL_memset((char *)raw_base + hdr->length * elem_size, 0, elem_size);
    hdr->length++;

    /* If data array was reallocated, the hash index bucket search may have
     * used stale data pointers.  Re-find to be safe. */
    forge_containers__hm_find(idx, key, key_size, h,
                               raw_base, elem_size, key_offset,
                               &sb, &sp, is_string);

    /* Insert into hash index. */
    if (idx->buckets[sb].hash[sp] == 1) {
        /* Reclaiming a tombstone. */
        idx->tombstone_count--;
    }
    idx->buckets[sb].hash[sp]  = h;
    idx->buckets[sb].index[sp] = new_di;
    idx->used_count++;

    hdr->temp = new_di;
    return new_di;
}

/* Remove a key from the hash map.  Returns 1 if deleted, 0 if not found. */
static int forge_containers__hm_remove(
    void **p, size_t elem_size, size_t key_offset, size_t key_size,
    const void *key, int is_string)
{
    forge_containers__header *hdr;
    forge_containers__hash_index *idx;
    size_t h;
    ptrdiff_t found;
    size_t sb, sp;
    void *raw_base;
    size_t last_raw;
    ptrdiff_t last_user;

    if (!*p) return 0;
    /* Reject NULL string keys before they reach SDL_strcmp. */
    if (is_string && *(const char *const *)key == NULL) return 0;

    hdr = forge_containers__hdr(*p);
    idx = hdr->hash_table;
    if (!idx) return 0;

    if (is_string) {
        h = forge_containers__hash_clamp(
                forge_containers__hash_string(*(const char *const *)key, idx->seed));
    } else {
        h = forge_containers__hash_clamp(
                forge_containers__hash(key, key_size, idx->seed));
    }

    raw_base = *p;
    found = forge_containers__hm_find(idx, key, key_size, h,
                                       raw_base, elem_size, key_offset,
                                       &sb, &sp, is_string);
    if (found < 0) return 0;

    /* Free strdup key if needed. */
    if (is_string && idx->arena.mode == FORGE_CONTAINERS__MODE_STRDUP) {
        char **key_ptr = (char **)((char *)raw_base + (size_t)(found + 1) * elem_size + key_offset);
        SDL_free(*key_ptr);
        *key_ptr = NULL;
    }

    /* Validate the found slot is within the index bounds. */
    if (sb >= idx->slot_count / (size_t)FORGE_CONTAINERS_BUCKET_SIZE ||
        sp >= (size_t)FORGE_CONTAINERS_BUCKET_SIZE) {
        SDL_Log("forge_containers: remove slot out of bounds "
                "(bucket=%zu, slot=%zu)", sb, sp);
        return 0;
    }

    /* Mark slot as tombstone. */
    idx->buckets[sb].hash[sp]  = 1;
    idx->buckets[sb].index[sp] = -2;
    idx->tombstone_count++;

    /* Guard against used_count underflow. */
    if (idx->used_count == 0) {
        SDL_Log("forge_containers: used_count underflow in remove");
        return 0;
    }
    idx->used_count--;

    /* Swap-with-last in data array. */
    last_raw  = hdr->length - 1;
    last_user = (ptrdiff_t)(last_raw - 1);

    if (found != last_user) {
        /* Move last entry to deleted position. */
        char *dst = (char *)raw_base + (size_t)(found + 1) * elem_size;
        char *src = (char *)raw_base + last_raw * elem_size;
        SDL_memmove(dst, src, elem_size);

        /* Fix up hash index for the moved entry. */
        {
            const char *moved_key = dst + key_offset;
            size_t mh;
            size_t msb, msp;

            if (is_string) {
                mh = forge_containers__hash_clamp(
                         forge_containers__hash_string(
                             *(const char *const *)moved_key, idx->seed));
            } else {
                mh = forge_containers__hash_clamp(
                         forge_containers__hash(moved_key, key_size, idx->seed));
            }

            {
                ptrdiff_t moved_found = forge_containers__hm_find(
                    idx, moved_key, key_size, mh,
                    raw_base, elem_size, key_offset,
                    &msb, &msp, is_string);
                /* The moved entry must exist in the index at its old position. */
                if (moved_found != last_user) {
                    SDL_Log("forge_containers: remove fixup find failed "
                            "(expected %td, got %td)", last_user, moved_found);
                }
                /* Verify the bucket stores the expected index. */
                if (idx->buckets[msb].index[msp] != last_user) {
                    SDL_Log("forge_containers: remove fixup index mismatch "
                            "(expected %td, got %td)",
                            last_user, idx->buckets[msb].index[msp]);
                }
            }
            idx->buckets[msb].index[msp] = found;
        }
    }
    hdr->length--;

    /* Check for tombstone rebuild or shrink. */
    if (idx->tombstone_count > idx->tombstone_threshold) {
        idx = forge_containers__index_rebuild(
                  idx, idx->slot_count,
                  raw_base, hdr->length - 1, elem_size,
                  key_offset, key_size, is_string);
        hdr->hash_table = idx;
    } else if (idx->used_count < idx->shrink_threshold && idx->shrink_threshold > 0) {
        size_t new_sc = idx->slot_count / 2;
        if (new_sc < (size_t)FORGE_CONTAINERS_BUCKET_SIZE) {
            new_sc = (size_t)FORGE_CONTAINERS_BUCKET_SIZE;
        }
        if (new_sc < idx->slot_count) {
            idx = forge_containers__index_rebuild(
                      idx, new_sc,
                      raw_base, hdr->length - 1, elem_size,
                      key_offset, key_size, is_string);
            hdr->hash_table = idx;
        }
    }

    return 1;
}

/* Find a key's user-visible index.  Returns >= 0 if found, -1 otherwise. */
static ptrdiff_t forge_containers__hm_find_index(
    void *p, size_t elem_size, size_t key_offset, size_t key_size,
    const void *key, int is_string)
{
    forge_containers__header *hdr;
    forge_containers__hash_index *idx;
    size_t h;

    if (!p) return -1;
    /* Reject NULL string keys before they reach SDL_strcmp. */
    if (is_string && *(const char *const *)key == NULL) return -1;
    hdr = forge_containers__hdr(p);
    idx = hdr->hash_table;
    if (!idx) return -1;

    if (is_string) {
        h = forge_containers__hash_clamp(
                forge_containers__hash_string(*(const char *const *)key, idx->seed));
    } else {
        h = forge_containers__hash_clamp(
                forge_containers__hash(key, key_size, idx->seed));
    }

    return forge_containers__hm_find(idx, key, key_size, h,
                                      p, elem_size, key_offset,
                                      NULL, NULL, is_string);
}

/* Lazy-init the data array for get operations on NULL maps.
 * Creates the default element but no hash index. */
static void forge_containers__hm_ensure_default(void **p, size_t elem_size)
{
    if (!*p) {
        forge_containers__arr_grow(p, 1, elem_size);
        if (*p) {
            forge_containers__header *hdr = forge_containers__hdr(*p);
            hdr->length = 1;
            SDL_memset(*p, 0, elem_size);
        }
    }
}

/* Free a hash map: free strdup keys, arena blocks, hash index, data array. */
static void forge_containers__hm_free(void **p, size_t elem_size, size_t key_offset,
                                       int is_string)
{
    forge_containers__header *hdr;
    forge_containers__hash_index *idx;

    if (!*p) return;
    hdr = forge_containers__hdr(*p);
    idx = hdr->hash_table;

    if (idx && is_string && idx->arena.mode == FORGE_CONTAINERS__MODE_STRDUP) {
        /* Free all duplicated key strings. */
        size_t i;
        for (i = 1; i < hdr->length; i++) {
            char **kp = (char **)((char *)*p + i * elem_size + key_offset);
            SDL_free(*kp);
        }
    }

    forge_containers__index_free(idx);
    SDL_free(hdr);
    /* Pointer is set to NULL by the calling macro (forge_hm_free / forge_shm_free). */
}

/* ══════════════════════════════════════════════════════════════════════════
 * Hash map macros (forge_hm_*)
 *
 * Usage: declare a pointer to a struct with a `key` field:
 *   struct { int key; float value; } *map = NULL;
 *   forge_hm_put(map, 42, 3.14f);
 *   float v = forge_hm_get(map, 42);
 *   forge_hm_free(map);
 * ══════════════════════════════════════════════════════════════════════════ */

/* Helper: take the address of a key value.  On GCC/Clang we use a compound
 * literal so rvalues work.  On MSVC, keys must be lvalues. */
#if FORGE_CONTAINERS__HAS_TYPEOF
  #define FORGE_CONTAINERS__KEYPTR(a, k) \
      ((__typeof__((a)->key) []){(k)})
#else
  #define FORGE_CONTAINERS__KEYPTR(a, k) (&(k))
#endif

/* Put a key-value pair.  Returns the value.
 * If allocation fails, the put is skipped. */
#define forge_hm_put(a, k, v)                                                \
    (forge_containers__hm_put_key(                                           \
         (void **)&(a), sizeof(*(a)),                                        \
         offsetof(__typeof__(*(a)), key), sizeof((a)->key),                   \
         FORGE_CONTAINERS__KEYPTR(a, k), 0),                                 \
     ((a) && forge_containers__hdr(a)->temp >= 0)                            \
         ? ((a)[forge_containers__hdr(a)->temp + 1].key = (k),               \
            (a)[forge_containers__hdr(a)->temp + 1].value = (v))             \
         : (v))

/* Put a complete struct.  Returns the struct. */
#define forge_hm_put_struct(a, entry)                                        \
    (forge_containers__hm_put_key(                                           \
         (void **)&(a), sizeof(*(a)),                                        \
         offsetof(__typeof__(*(a)), key), sizeof((a)->key),                   \
         &(entry).key, 0),                                                   \
     ((a) && forge_containers__hdr(a)->temp >= 0)                            \
         ? (a)[forge_containers__hdr(a)->temp + 1] = (entry)                 \
         : (entry))

/* Get the value for a key.  Returns default value (or 0 on OOM). */
#define forge_hm_get(a, k)                                                   \
    (forge_containers__hm_ensure_default((void **)&(a), sizeof(*(a))),       \
     (a) ? (forge_containers__hdr(a)->temp =                                 \
                forge_containers__hm_find_index(                              \
                    (a), sizeof(*(a)),                                        \
                    offsetof(__typeof__(*(a)), key), sizeof((a)->key),        \
                    FORGE_CONTAINERS__KEYPTR(a, k), 0),                       \
            (a)[forge_containers__hdr(a)->temp + 1].value)                   \
         : (__typeof__((a)->value)){0})

/* Get the full struct for a key.  Returns default struct (or zeroed on OOM). */
#define forge_hm_get_struct(a, k)                                            \
    (forge_containers__hm_ensure_default((void **)&(a), sizeof(*(a))),       \
     (a) ? (forge_containers__hdr(a)->temp =                                 \
                forge_containers__hm_find_index(                              \
                    (a), sizeof(*(a)),                                        \
                    offsetof(__typeof__(*(a)), key), sizeof((a)->key),        \
                    FORGE_CONTAINERS__KEYPTR(a, k), 0),                       \
            (a)[forge_containers__hdr(a)->temp + 1])                         \
         : (__typeof__(*(a))){0})

/* Get a pointer to the entry.  Returns pointer to default (or NULL on OOM). */
#define forge_hm_get_ptr(a, k)                                               \
    (forge_containers__hm_ensure_default((void **)&(a), sizeof(*(a))),       \
     (a) ? (forge_containers__hdr(a)->temp =                                 \
                forge_containers__hm_find_index(                              \
                    (a), sizeof(*(a)),                                        \
                    offsetof(__typeof__(*(a)), key), sizeof((a)->key),        \
                    FORGE_CONTAINERS__KEYPTR(a, k), 0),                       \
            &(a)[forge_containers__hdr(a)->temp + 1])                        \
         : NULL)

/* Get a pointer, or NULL if not found (or on OOM). */
#define forge_hm_get_ptr_or_null(a, k)                                       \
    (forge_containers__hm_ensure_default((void **)&(a), sizeof(*(a))),       \
     (a) ? (forge_containers__hdr(a)->temp =                                 \
                forge_containers__hm_find_index(                              \
                    (a), sizeof(*(a)),                                        \
                    offsetof(__typeof__(*(a)), key), sizeof((a)->key),        \
                    FORGE_CONTAINERS__KEYPTR(a, k), 0),                       \
            forge_containers__hdr(a)->temp >= 0                              \
                ? &(a)[forge_containers__hdr(a)->temp + 1] : NULL)           \
         : NULL)

/* Find the user-visible index.  >= 0 if found, -1 if not. */
#define forge_hm_find_index(a, k)                                            \
    ((a) ? forge_containers__hm_find_index(                                  \
               (a), sizeof(*(a)),                                            \
               offsetof(__typeof__(*(a)), key), sizeof((a)->key),            \
               FORGE_CONTAINERS__KEYPTR(a, k), 0)                           \
         : (ptrdiff_t)-1)

/* Remove a key.  Returns 1 if deleted, 0 if not found. */
#define forge_hm_remove(a, k)                                                \
    ((a) ? forge_containers__hm_remove(                                      \
               (void **)&(a), sizeof(*(a)),                                  \
               offsetof(__typeof__(*(a)), key), sizeof((a)->key),            \
               FORGE_CONTAINERS__KEYPTR(a, k), 0)                           \
         : 0)

/* Number of entries (signed).  0 for NULL. */
#define forge_hm_length(a)                                                   \
    ((a) ? (ptrdiff_t)(forge_containers__hdr(a)->length - 1) : (ptrdiff_t)0)

/* Free the hash map.  Sets pointer to NULL. */
#define forge_hm_free(a)                                                     \
    (forge_containers__hm_free((void **)&(a), sizeof(*(a)),                  \
         offsetof(__typeof__(*(a)), key), 0),                                \
     (a) = NULL)

/* Set the default value (returned on missed lookups). */
#define forge_hm_set_default(a, v)                                           \
    (forge_containers__hm_ensure_default((void **)&(a), sizeof(*(a))),       \
     (a) ? ((a)[0].value = (v)) : (v))

/* Set the entire default struct. */
#define forge_hm_set_default_struct(a, entry)                                \
    (forge_containers__hm_ensure_default((void **)&(a), sizeof(*(a))),       \
     (a) ? ((a)[0] = (entry)) : (entry))

/* Thread-safe get: uses external temp variable instead of header temp. */
#define forge_hm_get_ts(a, k, tmp)                                           \
    (forge_containers__hm_ensure_default((void **)&(a), sizeof(*(a))),       \
     (a) ? ((tmp) = forge_containers__hm_find_index(                         \
                 (a), sizeof(*(a)),                                          \
                 offsetof(__typeof__(*(a)), key), sizeof((a)->key),          \
                 FORGE_CONTAINERS__KEYPTR(a, k), 0),                         \
            (a)[(tmp) + 1].value)                                            \
         : (__typeof__((a)->value)){0})

/* Iterate: entries are at indices 1..length in the raw array,
 * which means (a)[1] through (a)[hdr->length-1] in direct access.
 * The user-facing iteration macro uses 0-based indexing where
 * entry i is at (a)[i+1]. */
#define forge_hm_iter(a, i) \
    for ((i) = 0; (i) < forge_hm_length(a); (i)++)

/* ══════════════════════════════════════════════════════════════════════════
 * String arena allocator (for arena-mode string maps)
 * ══════════════════════════════════════════════════════════════════════════ */

/* Allocate a string in the arena.  Returns pointer to the copy. */
static char *forge_containers__arena_strdup(forge_containers__hash_index *idx,
                                             const char *str)
{
    size_t len = SDL_strlen(str) + 1;  /* include null terminator */
    forge_containers__arena_block *blk = idx->arena.first_block;
    size_t block_size;
    forge_containers__arena_block *new_blk;
    char *dest;

    /* Try to fit in current block (pack from high address toward low).
     * Increment used first, then compute dest from the new used value:
     * dest = block_data_start + (capacity - used).  This places each new
     * string immediately below the previous one. */
    if (blk && len <= blk->capacity - blk->used) {
        blk->used += len;
        dest = (char *)blk + sizeof(forge_containers__arena_block)
               + blk->capacity - blk->used;
        SDL_memcpy(dest, str, len);
        return dest;
    }

    /* Compute next block size: MIN_BLOCK << (counter >> 1), capped.
     * Cast to size_t and clamp shift amount to prevent UB from shifting
     * a signed int by >= its width. */
    {
        size_t shift = idx->arena.block_counter >> 1;
        if (shift >= sizeof(size_t) * 8) shift = sizeof(size_t) * 8 - 1;
        block_size = (size_t)FORGE_CONTAINERS__ARENA_MIN_BLOCK << shift;
    }
    if (block_size > FORGE_CONTAINERS__ARENA_MAX_BLOCK) {
        block_size = FORGE_CONTAINERS__ARENA_MAX_BLOCK;
    }

    /* Oversized string: allocate exact block. */
    if (len > block_size) {
        new_blk = (forge_containers__arena_block *)SDL_malloc(
                      sizeof(forge_containers__arena_block) + len);
        if (!new_blk) return NULL;
        new_blk->capacity = len;
        new_blk->used     = len;
        /* Insert after first block to not waste current block space. */
        if (blk) {
            new_blk->next = blk->next;
            blk->next = new_blk;
        } else {
            new_blk->next = NULL;
            idx->arena.first_block = new_blk;
        }
        idx->arena.block_counter++;
        dest = (char *)new_blk + sizeof(forge_containers__arena_block);
        SDL_memcpy(dest, str, len);
        return dest;
    }

    /* Allocate new regular block. */
    new_blk = (forge_containers__arena_block *)SDL_malloc(
                  sizeof(forge_containers__arena_block) + block_size);
    if (!new_blk) return NULL;
    new_blk->capacity = block_size;
    new_blk->used     = len;
    new_blk->next     = idx->arena.first_block;
    idx->arena.first_block = new_blk;
    idx->arena.block_counter++;

    dest = (char *)new_blk + sizeof(forge_containers__arena_block)
           + block_size - len;
    SDL_memcpy(dest, str, len);
    return dest;
}

/* ══════════════════════════════════════════════════════════════════════════
 * String map put key (handles strdup/arena allocation)
 * ══════════════════════════════════════════════════════════════════════════ */

static ptrdiff_t forge_containers__shm_put_key(
    void **p, size_t elem_size, size_t key_offset, const char *key)
{
    forge_containers__header *hdr;
    forge_containers__hash_index *idx;
    size_t h;
    ptrdiff_t found;
    size_t sb, sp;
    ptrdiff_t new_di;
    void *raw_base;
    char *stored_key;
    const char *key_ptr;

    if (!key) {
        SDL_Log("forge_containers: NULL key in string map put");
        return -1;
    }

    /* Lazy init. */
    if (!*p) {
        forge_containers__arr_grow(p, 1, elem_size);
        if (!*p) return -1;
        hdr = forge_containers__hdr(*p);
        hdr->length = 1;
        SDL_memset(*p, 0, elem_size);
        idx = forge_containers__index_new(
                  (size_t)FORGE_CONTAINERS_BUCKET_SIZE,
                  forge_containers__advance_seed());
        if (!idx) {
            SDL_free(forge_containers__hdr(*p));
            *p = NULL;
            return -1;
        }
        idx->arena.mode = FORGE_CONTAINERS__MODE_USER;
        hdr->hash_table = idx;
    }

    hdr = forge_containers__hdr(*p);
    idx = hdr->hash_table;

    /* Guard: init macro may have failed to create the hash index. */
    if (!idx) {
        idx = forge_containers__index_new(
                  (size_t)FORGE_CONTAINERS_BUCKET_SIZE,
                  forge_containers__advance_seed());
        if (!idx) return -1;
        idx->arena.mode = FORGE_CONTAINERS__MODE_USER;
        hdr->hash_table = idx;
    }

    h = forge_containers__hash_clamp(
            forge_containers__hash_string(key, idx->seed));

    raw_base = *p;
    key_ptr = key;
    found = forge_containers__hm_find(idx, &key_ptr, sizeof(char *), h,
                                       raw_base, elem_size, key_offset,
                                       &sb, &sp, 1);
    if (found >= 0) {
        /* Key exists.  For strdup/arena modes, we keep the existing key. */
        char **existing_key = (char **)((char *)raw_base + (size_t)(found + 1) * elem_size + key_offset);
        idx->temp_key = *existing_key;
        hdr->temp = found;
        return found;
    }

    /* Store the key string according to mode. */
    stored_key = (char *)key; /* default: user-managed */
    if (idx->arena.mode == FORGE_CONTAINERS__MODE_STRDUP) {
        size_t slen = SDL_strlen(key) + 1;
        stored_key = (char *)SDL_malloc(slen);
        if (!stored_key) return -1;
        SDL_memcpy(stored_key, key, slen);
    } else if (idx->arena.mode == FORGE_CONTAINERS__MODE_ARENA) {
        stored_key = forge_containers__arena_strdup(idx, key);
        if (!stored_key) return -1;
    }
    /* Grow hash index if needed (before setting temp_key, since rebuild
     * creates a new index and the old one is freed). */
    if (idx->used_count + idx->tombstone_count + 1 >= idx->grow_threshold) {
        idx = forge_containers__index_rebuild(
                  idx, idx->slot_count * 2,
                  raw_base, hdr->length - 1, elem_size,
                  key_offset, sizeof(char *), 1);
        hdr->hash_table = idx;
        forge_containers__hm_find(idx, &key_ptr, sizeof(char *), h,
                                   raw_base, elem_size, key_offset,
                                   &sb, &sp, 1);
    }
    idx->temp_key = stored_key;

    /* Append entry to data array. */
    forge_containers__arr_maybe_grow(p, 1, elem_size);
    hdr = forge_containers__hdr(*p);
    if (hdr->length >= hdr->capacity) {
        /* Grow failed — free the strdup key to avoid a leak. */
        if (idx->arena.mode == FORGE_CONTAINERS__MODE_STRDUP && stored_key != key) {
            SDL_free(stored_key);
        }
        return -1;
    }
    raw_base = *p;

    new_di = (ptrdiff_t)(hdr->length - 1);
    SDL_memset((char *)raw_base + hdr->length * elem_size, 0, elem_size);
    hdr->length++;

    /* Re-find after potential realloc. */
    forge_containers__hm_find(idx, &key_ptr, sizeof(char *), h,
                               raw_base, elem_size, key_offset,
                               &sb, &sp, 1);

    if (idx->buckets[sb].hash[sp] == 1) idx->tombstone_count--;
    idx->buckets[sb].hash[sp]  = h;
    idx->buckets[sb].index[sp] = new_di;
    idx->used_count++;

    hdr->temp = new_di;
    return new_di;
}

/* ══════════════════════════════════════════════════════════════════════════
 * String map macros (forge_shm_*)
 *
 * Usage: declare a pointer to a struct with `char *key`:
 *   struct { char *key; int value; } *smap = NULL;
 *   forge_shm_put(smap, "hello", 42);
 *   int v = forge_shm_get(smap, "hello");
 *   forge_shm_free(smap);
 * ══════════════════════════════════════════════════════════════════════════ */

/* Initialize in strdup mode (keys are duplicated).
 * Fails closed: if index allocation fails, frees the data array and
 * resets the pointer to NULL so callers don't get a half-init map. */
#define forge_shm_init_strdup(a)                                             \
    do {                                                                     \
        if (!(a)) {                                                          \
            forge_containers__arr_grow((void **)&(a), 1, sizeof(*(a)));      \
            if (a) {                                                         \
                forge_containers__header *h_ = forge_containers__hdr(a);     \
                h_->length = 1;                                              \
                SDL_memset((a), 0, sizeof(*(a)));                            \
                h_->hash_table = forge_containers__index_new(                \
                    (size_t)FORGE_CONTAINERS_BUCKET_SIZE,                    \
                    forge_containers__advance_seed());                       \
                if (h_->hash_table) {                                        \
                    h_->hash_table->arena.mode =                             \
                        FORGE_CONTAINERS__MODE_STRDUP;                       \
                } else {                                                     \
                    SDL_free(h_);                                             \
                    (a) = NULL;                                               \
                }                                                            \
            }                                                                \
        }                                                                    \
    } while (0)

/* Initialize in arena mode (keys arena-allocated, never individually freed).
 * Fails closed: same as forge_shm_init_strdup. */
#define forge_shm_init_arena(a)                                              \
    do {                                                                     \
        if (!(a)) {                                                          \
            forge_containers__arr_grow((void **)&(a), 1, sizeof(*(a)));      \
            if (a) {                                                         \
                forge_containers__header *h_ = forge_containers__hdr(a);     \
                h_->length = 1;                                              \
                SDL_memset((a), 0, sizeof(*(a)));                            \
                h_->hash_table = forge_containers__index_new(                \
                    (size_t)FORGE_CONTAINERS_BUCKET_SIZE,                    \
                    forge_containers__advance_seed());                       \
                if (h_->hash_table) {                                        \
                    h_->hash_table->arena.mode =                             \
                        FORGE_CONTAINERS__MODE_ARENA;                        \
                } else {                                                     \
                    SDL_free(h_);                                             \
                    (a) = NULL;                                               \
                }                                                            \
            }                                                                \
        }                                                                    \
    } while (0)

/* Put a key-value pair into a string map. */
#define forge_shm_put(a, k, v)                                               \
    (forge_containers__shm_put_key(                                          \
         (void **)&(a), sizeof(*(a)),                                        \
         offsetof(__typeof__(*(a)), key), (k)),                              \
     ((a) && forge_containers__hdr(a)->temp >= 0)                            \
         ? ((a)[forge_containers__hdr(a)->temp + 1].key =                    \
                forge_containers__hdr(a)->hash_table->temp_key,              \
            (a)[forge_containers__hdr(a)->temp + 1].value = (v))             \
         : (v))

/* Put a complete struct into a string map. */
#define forge_shm_put_struct(a, entry)                                       \
    (forge_containers__shm_put_key(                                          \
         (void **)&(a), sizeof(*(a)),                                        \
         offsetof(__typeof__(*(a)), key), (entry).key),                      \
     ((a) && forge_containers__hdr(a)->temp >= 0)                            \
         ? ((a)[forge_containers__hdr(a)->temp + 1] = (entry),               \
            (a)[forge_containers__hdr(a)->temp + 1].key =                    \
                forge_containers__hdr(a)->hash_table->temp_key,              \
            (entry).value)                                                   \
         : (entry).value)

/* Get value by string key.  Returns 0 on OOM. */
#define forge_shm_get(a, k)                                                  \
    (forge_containers__hm_ensure_default((void **)&(a), sizeof(*(a))),       \
     (a) ? (forge_containers__hdr(a)->temp =                                 \
                forge_containers__hm_find_index(                              \
                    (a), sizeof(*(a)),                                        \
                    offsetof(__typeof__(*(a)), key), sizeof(char *),          \
                    &(const char *){(k)}, 1),                                 \
            (a)[forge_containers__hdr(a)->temp + 1].value)                   \
         : (__typeof__((a)->value)){0})

/* Get full struct by string key.  Returns zeroed struct on OOM. */
#define forge_shm_get_struct(a, k)                                           \
    (forge_containers__hm_ensure_default((void **)&(a), sizeof(*(a))),       \
     (a) ? (forge_containers__hdr(a)->temp =                                 \
                forge_containers__hm_find_index(                              \
                    (a), sizeof(*(a)),                                        \
                    offsetof(__typeof__(*(a)), key), sizeof(char *),          \
                    &(const char *){(k)}, 1),                                 \
            (a)[forge_containers__hdr(a)->temp + 1])                         \
         : (__typeof__(*(a))){0})

/* Get pointer to entry, or pointer to default if not found. NULL on OOM. */
#define forge_shm_get_ptr(a, k)                                              \
    (forge_containers__hm_ensure_default((void **)&(a), sizeof(*(a))),       \
     (a) ? (forge_containers__hdr(a)->temp =                                 \
                forge_containers__hm_find_index(                              \
                    (a), sizeof(*(a)),                                        \
                    offsetof(__typeof__(*(a)), key), sizeof(char *),          \
                    &(const char *){(k)}, 1),                                 \
            &(a)[forge_containers__hdr(a)->temp + 1])                        \
         : NULL)

/* Get pointer, or NULL if not found (or on OOM). */
#define forge_shm_get_ptr_or_null(a, k)                                      \
    (forge_containers__hm_ensure_default((void **)&(a), sizeof(*(a))),       \
     (a) ? (forge_containers__hdr(a)->temp =                                 \
                forge_containers__hm_find_index(                              \
                    (a), sizeof(*(a)),                                        \
                    offsetof(__typeof__(*(a)), key), sizeof(char *),          \
                    &(const char *){(k)}, 1),                                 \
            forge_containers__hdr(a)->temp >= 0                              \
                ? &(a)[forge_containers__hdr(a)->temp + 1] : NULL)           \
         : NULL)

/* Find index by string key. */
#define forge_shm_find_index(a, k)                                           \
    ((a) ? forge_containers__hm_find_index(                                  \
               (a), sizeof(*(a)),                                            \
               offsetof(__typeof__(*(a)), key), sizeof(char *),              \
               &(const char *){(k)}, 1)                                      \
         : (ptrdiff_t)-1)

/* Remove by string key. */
#define forge_shm_remove(a, k)                                               \
    ((a) ? forge_containers__hm_remove(                                      \
               (void **)&(a), sizeof(*(a)),                                  \
               offsetof(__typeof__(*(a)), key), sizeof(char *),              \
               &(const char *){(k)}, 1)                                      \
         : 0)

/* Number of entries. */
#define forge_shm_length(a) forge_hm_length(a)

/* Free the string map. */
#define forge_shm_free(a)                                                    \
    (forge_containers__hm_free((void **)&(a), sizeof(*(a)),                  \
         offsetof(__typeof__(*(a)), key), 1),                                \
     (a) = NULL)

/* Set default value. */
#define forge_shm_set_default(a, v)  forge_hm_set_default(a, v)

/* Iterate string map entries. */
#define forge_shm_iter(a, i) forge_hm_iter(a, i)

#endif /* FORGE_CONTAINERS_H */
