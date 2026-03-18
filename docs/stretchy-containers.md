# Fat-Pointer Stretchy Containers

A detailed technical description of a single-header C library pattern that
provides dynamic arrays and hash maps through pointer-compatible handles and
macro-driven APIs. All allocations and memory operations use SDL's
cross-platform wrappers (`SDL_realloc`, `SDL_free`, `SDL_memmove`, etc.).

## Core Idea: The Fat Pointer

Every container is represented as a **plain pointer to the element type.** The
user declares `int *numbers = NULL` and passes it to macros that grow, query,
and free the backing storage. From the user's perspective it looks and indexes
like a normal C array — `numbers[3]` works — but behind the scenes each
allocation carries a small metadata header **immediately before** the address
the user holds.

```text
               user pointer
               ↓
┌────────────────┬───────────────────────────────┐
│  header        │  element data ...              │
│  (length, cap, │  [0]  [1]  [2]  [3]  ...      │
│   hash_table,  │                                │
│   temp)        │                                │
└────────────────┴───────────────────────────────┘
```

When the library allocates or reallocates, it requests
`sizeof(header) + capacity * sizeof(element)` bytes, then returns a pointer
that is offset past the header. When it needs the metadata, it casts the user
pointer back and subtracts the header size to reach the hidden prefix. This
means:

- The pointer the user holds is a valid pointer to element 0.
- Standard C indexing, pointer arithmetic, and `SDL_memcpy` all work.
- The user never sees or manages the header directly.
- A `NULL` pointer is a valid empty container (length 0, capacity 0). Every
  operation checks for `NULL` before attempting to read the header.

The header contains four fields:

| Field        | Meaning                                              |
|--------------|------------------------------------------------------|
| `length`     | Number of elements currently stored                  |
| `capacity`   | Number of elements the allocation can hold            |
| `hash_table` | Pointer to a separate hash index (NULL for plain arrays) |
| `temp`       | Scratch space for passing state between macro stages  |

The `hash_table` and `temp` fields are unused by the dynamic array operations
but are present in every header because arrays and hash maps share the same
header type. This means a plain dynamic array carries two extra pointer-sized
words of overhead — a deliberate simplicity-over-space trade.

## Dynamic Arrays

### Growth

When the user appends an element, the macro checks whether
`length + added > capacity`. If so, it calls a growth function. The growth
policy is:

```text
new_capacity = max(minimum_needed, old_capacity * 2)
if new_capacity < 4 then
    new_capacity = 4
```

The first insertion on a `NULL` pointer triggers an initial allocation. Since
`old_capacity` is 0 for a `NULL` array, doubling it produces 0, so the floor
of 4 kicks in as the starting capacity.

The reallocation moves the header + data together in a single `SDL_realloc` call,
then adjusts the user pointer to point past the header in the new block. The
old pointer is invalidated — **the macro reassigns the user's pointer variable
in place**, which is why these operations are macros rather than functions.
A function could not update the caller's local variable.

### Supported Operations

The array API, expressed as pseudocode:

```ruby
# Append one element at the end. Grows if needed.
# Returns: the appended value
array.append(value)

# Read the current number of stored elements (signed).
# Returns: integer (ptrdiff_t; 0 for NULL)
array.length

# Read the current number of stored elements (unsigned).
# Returns: unsigned integer (size_t; 0 for NULL)
array.length_unsigned

# Read the current allocated capacity.
# Returns: unsigned integer (0 for NULL)
array.capacity

# Remove the last element and return it.
# Precondition: array must be non-empty. No bounds check.
# Returns: the removed element
array.pop

# Return the last element without removing it.
# Precondition: array must be non-empty. No bounds check.
# Returns: the last element
array.last

# Insert one element at index i, shifting subsequent elements right.
# Cost: O(n) due to SDL_memmove.
array.insert_at(i, value)

# Insert n uninitialized elements starting at index i, shifting
# subsequent elements right.
# Cost: O(n) due to SDL_memmove.
array.insert_n_at(i, n)

# Remove the element at index i, shifting subsequent elements left.
# Cost: O(n) due to SDL_memmove.
array.delete_at(i)

# Remove n elements starting at index i, shifting the rest left.
# Cost: O(n) due to SDL_memmove.
array.delete_n_at(i, n)

# Remove the element at index i by overwriting it with the last element,
# then decrementing length.
# Cost: O(1), but does not preserve order.
array.swap_remove(i)

# Append n uninitialized elements, growing if needed.
# Returns: pointer to the first new element
array.grow_by_ptr(n)

# Append n uninitialized elements, growing if needed.
# Returns: index of the first new element
array.grow_by_index(n)

# Set the length to n. If n > length, new slots are uninitialized.
# Grows the allocation if n > capacity. Never shrinks.
array.set_length(n)

# Set the capacity to at least n without changing length.
# Only grows, never shrinks.
array.set_capacity(n)

# Free all memory and reset the pointer to NULL.
array.free
```

Indexing is plain C: `array[i]`. There is no bounds checking — the user is
responsible for staying within `0..length-1`.

### Type Safety

Because the array is a typed pointer (`float *`, `MyStruct *`, etc.), the
compiler enforces element types at every use site. The macros use
`sizeof(*array)` internally, so they adapt to whatever type the pointer
carries. No `void *` casting is needed by the user.

## Hash Maps

The hash map is built on top of the dynamic array. The user declares a pointer
to a struct containing at minimum a `key` field:

```c
struct { int key; float value; } *lookup = NULL;
```

The field **must** be named `key`. The macros reference `.key` by name through
C's member-access syntax. The value field is conventionally named `value` (and
some convenience macros require it), but the struct can contain any number of
additional fields. Omitting `value` makes the map a hash set — `key` is both
the lookup key and the stored data.

**Struct key gotcha:** For non-string keys, the library hashes and compares
the raw bytes of the key field using `sizeof(key)` and byte-wise comparison.
This means **padding bytes in struct keys affect both hashing and equality.**
Two structs with identical field values but different padding bytes will hash
differently and compare as unequal. Callers using struct keys must ensure the
key is fully initialized (e.g., via `SDL_memset` before setting fields) so that
padding bytes are deterministic.

### Dual-Structure Architecture

Unlike the dynamic array, a hash map uses **two separate allocations**:

1. **A data array** — the same hidden-header stretchy array as described above.
   Element 0 is reserved as the **default value** (explained below). Elements
   1 through N hold the inserted key-value structs in roughly insertion order.
   The user pointer is offset so that `map[0]` points to element 1 of the
   underlying array (the first real entry). This means `map[-1]` reaches the
   default value at element 0.

2. **A hash index** — a separate allocation containing:
   - Metadata: slot count, used count, various thresholds, the hash seed, a
     string arena (for string maps), and a pointer to the bucket storage.
   - A flat array of **buckets**, aligned to cache-line boundaries (64 bytes).

The data array's header holds a pointer to the hash index. When the hash map
is freed, both allocations are released.

```text
Data array (hidden-header stretchy array):
┌─────────┬──────────┬──────────┬──────────┬──────────┬────┐
│ header   │ default  │ entry 0  │ entry 1  │ entry 2  │... │
│          │ (map[-1])│ (map[0]) │ (map[1]) │ (map[2]) │    │
└─────────┴──────────┴──────────┴──────────┴──────────┴────┘
                      ↑ user pointer

Hash index (separate allocation):
┌────────────────────────┬────────────────────────────────┐
│ metadata               │ bucket storage (cache-aligned) │
│ (slot_count, seed,     │ [bucket 0][bucket 1][bucket 2] │
│  thresholds, arena)    │ ...                            │
└────────────────────────┴────────────────────────────────┘
```

### Bucket Structure

Each bucket holds a fixed number of slots — typically **8** (tunable to 4 at
compile time). A bucket contains two parallel arrays:

| Array     | Contents                                             |
|-----------|------------------------------------------------------|
| `hash[]`  | The cached hash value for each slot                  |
| `index[]` | The index into the data array for each slot           |

On 64-bit platforms with 8-slot buckets, the `hash[]` array is one 64-byte
cache line and the `index[]` array is another 64-byte cache line. On 32-bit
platforms, the entire bucket (both arrays) fits in one 64-byte cache line.

Slot states are encoded directly in these arrays:

| State    | `hash[]` value | `index[]` value |
|----------|----------------|-----------------|
| Empty    | 0              | -1              |
| Occupied | ≥ 2            | ≥ 0 (data index)|
| Deleted  | 1              | -2              |

Hash values 0 and 1 are reserved as sentinels. Any real hash that computes to
less than 2 is bumped up to 2. This bumping happens at the call site (in the
put and find functions), not inside the hash functions themselves.

The total number of slots (across all buckets) is always a power of two. The
starting position for a key is computed as `hash & (slot_count - 1)`, which
is a fast bitmask instead of modulo. The hash index also stores
`log2(slot_count)` for potential use in probe calculations, though the
current implementation uses only the bitmask.

### Hashing

The library uses different hash functions depending on key size and
configuration:

**For 4-byte keys** (common case: `int` keys), a specialized bit-mixing
function derived from Bob Jenkins' and Thomas Wang's work. The input is
read as a little-endian 32-bit integer from the raw key bytes:

```text
hash ^= seed
hash = (hash ^ 61) ^ (hash >> 16)
hash = hash + (hash << 3)
hash = hash ^ (hash >> 4)
hash = hash * 0x27d4eb2d
hash ^= seed
hash = hash ^ (hash >> 15)
```

On 64-bit platforms, the 32-bit result is extended to `size_t` by
duplicating it into both halves and XORing with the seed:

```text
return ((hash << 32) | hash) ^ seed
```

**For 8-byte keys** on 64-bit platforms, a Thomas Wang 64-bit mix:

```text
hash ^= seed
hash = (~hash) + (hash << 21)
hash ^= rotate_right(hash, 24)
hash *= 265
hash ^= rotate_right(hash, 14)
hash ^= seed
hash *= 21
hash ^= rotate_right(hash, 28)
hash += (hash << 31)
hash = (~hash) + (hash << 18)
```

**For all other key sizes** (and for 4/8-byte keys when the strong-hash
compile option is enabled), a variant of SipHash. The state is four
`size_t` variables initialized from fixed constants XORed with the seed:

```text
v0 = 0x736f6d6570736575 ^  seed
v1 = 0x646f72616e646f6d ^ ~seed
v2 = 0x6c7967656e657261 ^  seed
v3 = 0x7465646279746573 ^ ~seed
```

(On 32-bit platforms, these are truncated to 32 bits via the shift-and-or
loading technique — the algorithm produces different hashes on 32-bit vs
64-bit.)

The round function (SIPROUND) is:

```text
v0 += v1;  v1 = rotate_left(v1, 13);  v1 ^= v0;  v0 = rotate_left(v0, SIZE_BITS/2)
v2 += v3;  v3 = rotate_left(v3, 16);  v3 ^= v2
v2 += v1;  v1 = rotate_left(v1, 17);  v1 ^= v2;  v2 = rotate_left(v2, SIZE_BITS/2)
v0 += v3;  v3 = rotate_left(v3, 21);  v3 ^= v0
```

where `SIZE_BITS` is `sizeof(size_t) * 8` (32 or 64).

Input is consumed in `size_t`-sized words (little-endian). The final word
is padded with the message length in the high byte, matching SipHash
convention. By default the library uses 1 compression round and 1
finalization round (SipHash-1-1). A compile-time option enables
specification-compliant SipHash-2-4 for stronger security at the cost of
~10-20% slower insertion.

In the default (weakened) mode, the final result is `v1 ^ v2 ^ v3` (omitting
`v0`). In SipHash-2-4 mode, the result is `v0 ^ v1 ^ v2 ^ v3` per the
specification. The author noted that omitting `v0` may be slightly stronger
for the weakened variant because `v0 ^ v3` in the standard finalization
cancels out part of the last round's work.

A per-table **random seed** is mixed into every hash. The global seed
defaults to `0x31415926` and is advanceable by the caller. Each time a new
hash index is created (not a resize — resizes reuse the old seed), the
library derives the table's seed from the global seed and then advances the
global seed using a linear congruential generator:

```text
-- 64-bit constants:
a = 2862933555777941757   (0x27BB2EE687B0B0FD)
b = 3037000493            (0x00000000B504F32D)

-- 32-bit constants:
a = 2147001325
b =  715136305

global_seed = global_seed * a + b
```

The constants are loaded portably using a shift-and-or pattern that works
on both 32-bit and 64-bit `size_t` without compiler warnings. This defends
against hash-flooding attacks by giving each independently created table a
unique seed.

**For string keys**, a separate hash function processes the string byte by
byte using a rotate-and-add loop, followed by Thomas Wang's 64-to-32-bit
avalanche mix:

```text
hash = seed
for each byte b in string:
    hash = rotate_left(hash, 9) + b

hash ^= seed
hash = (~hash) + (hash << 18)
hash ^= rotate_right(hash, 31)
hash *= 21
hash ^= rotate_right(hash, 11)
hash += (hash << 6)
hash ^= rotate_right(hash, 22)
return hash + seed
```

### Collision Resolution: Bucketed Quadratic Probing

When a key's starting bucket is full, the library probes forward using
**quadratic probing** with a step size that increases by the bucket width on
each iteration:

```text
pos   = hash & (slot_count - 1)
step  = BUCKET_SIZE       -- e.g. 8

loop:
    search all slots in the bucket at pos
    if found or empty slot → done
    pos   = (pos + step) & (slot_count - 1)
    step += BUCKET_SIZE
```

Within each bucket, the search proceeds from the starting slot
(`pos & BUCKET_MASK`) to the end of the bucket, then wraps around to the
beginning. This means the probe sequence visits every slot in the starting
bucket before moving to the next one.

The **cached hash** in each slot enables fast rejection: the library compares
the stored hash against the probe hash first. Only on a hash match does it
dereference the index into the data array and compare the full key. An empty
hash (0) terminates the probe — the key is not in the table.

During insertion, if a tombstone slot is encountered before an empty slot, the
library remembers the tombstone's position. If the key is ultimately not found,
the new entry is placed in the tombstone slot (reclaiming it) rather than in
the first empty slot. This keeps entries closer to their ideal positions.

### Data Array Index Convention

The bucket's `index[]` values are offsets into the **user-visible** array, not
the raw underlying array. Since element 0 of the raw array is the default
value and the user pointer starts at element 1, a bucket index of `i` refers
to raw array element `i + 1`, which the user sees as `map[i]`.

When a new entry is appended to the data array, the raw array grows by one
element (at raw index `length`). The bucket stores `length - 1` as the index,
because that is where the entry sits in user-visible coordinates. This off-by-
one pervades the implementation: all index values stored in the hash index are
in user-visible space, and all raw-array operations must add 1.

### Deletion

Deletion uses **tombstones**, not backward-shift deletion. When a key is
removed:

1. The bucket slot is marked as deleted (hash = 1, index = -2). This
   tombstone allows subsequent probes to continue past the deleted slot —
   an empty slot (hash = 0) would incorrectly terminate a probe for a key
   that was inserted after the now-deleted one.

2. For **strdup-mode string maps**, the key string copy is freed.

3. In the **data array**, the deleted entry is replaced by the **last entry**
   (swap-with-last via `SDL_memmove`). This keeps the data array dense with no
   gaps. The library must then **fix up the hash index** for the moved
   element: it re-probes the hash table using the moved element's key to
   find its bucket slot, then updates that slot's index to the deleted
   element's former position. If the deleted element was already the last
   element, no swap or fixup is needed.

4. When the **tombstone count** exceeds a threshold (~18.75% of slot count),
   the entire hash index is **rebuilt** — a new index of the same size is
   allocated, all live entries are reinserted, and all tombstones are
   cleared. This amortizes the cost of tombstone cleanup.

5. When the **used count** drops below a shrink threshold (25% of slot
   count), the hash index is rebuilt at half size.

Shrink and tombstone-rebuild are mutually exclusive — whichever condition
is met first triggers the rebuild. Both allocate a new hash index and free
the old one.

### Load Factor and Resizing

The thresholds are computed using bit-shift arithmetic to avoid overflow:

| Threshold          | Formula                                    | Approximate % |
|--------------------|--------------------------------------------|---------------|
| Grow               | `slot_count - (slot_count >> 2)`           | 75%           |
| Tombstone rebuild  | `(slot_count >> 3) + (slot_count >> 4)`    | ~18.75%       |
| Shrink             | `slot_count >> 2`                          | 25%           |

The shrink threshold is clamped to 0 when the slot count equals one bucket
(the minimum table size), to prevent the table from shrinking below its
minimum.

The library also asserts that `grow_threshold + tombstone_threshold < slot_count`,
guaranteeing that at least one slot is always empty to terminate probe
sequences.

Resizing allocates a new hash index at double the slot count and reinserts
all live entries. The data array is unaffected — only the index changes. The
old seed is reused so that existing hash values remain valid, avoiding
re-hashing each key.

### First Insertion on a NULL Map

When the first put operation is called on a `NULL` pointer:

1. A data array is allocated with capacity for 1 element. This element is
   zeroed and becomes the default value. The raw array length is set to 1.
2. The user pointer is offset to point past the default element.
3. A hash index is allocated with the minimum slot count (one bucket, i.e.,
   8 slots). For string maps, the string mode is set based on whether the
   map was pre-initialized with an arena/strdup mode selector.
4. The put then proceeds normally — growing the data array and inserting
   into the hash index.

A get operation on a `NULL` map follows a similar lazy-initialization path:
it allocates the data array with the zeroed default but does **not** create
a hash index. The lookup returns -1 (not found) immediately.

### The Default Value Mechanism

Hash map lookups need a way to signal "key not found." Rather than returning
a pointer (which complicates the API), the library stores a **default value**
at position -1 in the user-visible array (element 0 of the underlying data
array). When a lookup misses, the returned index is -1, which means the
caller reads `map[-1]` — the default entry.

The default is initially zeroed. The user can **set the default** to a custom
sentinel before performing lookups. For example, setting the default value
field to `-1` lets the caller distinguish "not found" from "found with value
0."

```ruby
# Set the default value returned on lookup miss
map.set_default(sentinel_value)

# Set the entire default struct (key and all fields)
map.set_default_struct(sentinel_entry)
```

A subtlety: if the user writes `map.get(key).value = 5` and the key is absent,
this silently overwrites the default value. The correct pattern is to check
for presence first via the index lookup, or use the pointer variant that
returns null on miss.

### Supported Operations

```ruby
# Insert or update a key-value pair.
# If the key exists, its value is overwritten.
# Returns: the value
map.put(key, value)

# Insert or update using a complete struct (key + all fields).
# Returns: the struct
map.put_struct(entry)

# Look up a key and return the associated value.
# If not found, returns the default value.
# Requires the struct to have a field named 'value'.
# Returns: the value type
map.get(key)

# Look up a key and return the complete struct.
# If not found, returns the default struct.
# Returns: the full struct
map.get_struct(key)

# Look up a key and return a pointer to the entry.
# If not found, returns a pointer to the default entry.
# Returns: pointer to the struct
map.get_ptr(key)

# Look up a key and return a pointer to the entry.
# If not found, returns NULL.
# Returns: pointer to the struct, or NULL
map.get_ptr_or_null(key)

# Look up a key and return its index in the data array.
# Returns: index (>= 0) if found, or -1 if not found.
# The index can be used as: map[index].key, map[index].value, etc.
map.find_index(key)

# Remove a key from the map.
# Returns: 1 if the key was present and deleted, 0 otherwise.
map.remove(key)

# Return the number of stored entries.
# Returns: signed integer (0 for NULL)
map.length

# Free all memory and reset the pointer to NULL.
map.free
```

### Hash Map Length

The length reported to the user is `header(raw_array)->length - 1`. The
raw array's length includes the default entry at element 0, so subtracting
1 gives the count of actual inserted entries. For a `NULL` map, length is 0.

### Iteration

The data array holds all live entries as a dense, contiguous sequence. The
user iterates with a simple index loop from 0 to `length - 1`:

```ruby
for i in 0 .. map.length - 1:
    process(map[i].key, map[i].value)
```

This works because the data array **is** a plain stretchy array. No
bucket-scanning or tombstone-skipping is needed — the data array contains
only live entries (tombstones exist only in the hash index, not in the data).

If no deletions have occurred, iteration order matches insertion order. After
deletions, the swap-with-last removal may reorder entries.

### Thread Safety

Most operations are **not thread-safe**. However, the library provides special
**thread-safe lookup variants** (`_ts` suffix) that accept an external
temporary variable instead of writing to the shared temp field in the header.
These are safe to call concurrently from multiple reader threads, provided
no writer is active. All mutating operations (put, remove) require external
synchronization.

## String Map Variant

The string-keyed map is a separate set of macros. The key field is always a
`char *`. Besides using the string hash function, the main difference is how
string keys are stored. The library supports **three string ownership modes**,
selected at initialization time:

### Mode 1: User-Managed (default)

The library stores the `char *` pointer as-is. The user is responsible for
keeping the string alive for the lifetime of the entry. This is the default
if the user does not explicitly select a mode. Suitable when keys are string
literals or otherwise have a known long lifetime.

### Mode 2: Duplicate (strdup)

The library allocates a copy of each key string using `SDL_realloc`. When an
entry is deleted, the library frees the copy. When the map is destroyed, all
copies are freed. This is the safest mode — the user can free or reuse the
original string immediately after insertion.

### Mode 3: Arena

The library allocates key strings from a **block arena allocator**. The arena
uses a series of growing blocks whose sizes follow the pattern
`MIN_BLOCK << (block_counter >> 1)` — that is, the size doubles every
**two** allocations: 512, 512, 1024, 1024, 2048, 2048, ... up to a cap of
1 MB. This means there are O(log(total_size)) blocks to free on destruction.

Strings are packed into the current block from the **end** (high address
downward). When a block fills, a new larger one is allocated and pushed onto
the front of a singly-linked list. Strings that exceed the current block
size get their own individual allocation, inserted after the first block in
the list so as not to waste the remaining space in the current block.

The block counter advances even for oversized strings, so the regular block
size continues to grow and will eventually accommodate them.

Arena mode is faster than strdup mode (no per-key free) but **never reclaims
memory for deleted keys** — the string data persists in the arena until the
entire map is destroyed. This makes it ideal for maps that only grow (no
deletions), such as symbol tables or configuration registries.

### String Mode Storage

The string ownership mode is stored in a `mode` byte inside the arena struct,
which is itself embedded in the hash index metadata. This field serves double
duty: it tells the arena allocator nothing (the arena ignores it), and it
tells the put/delete operations how to handle key strings. Non-string maps
set mode to 0.

The mode-selection initializers (arena or strdup) allocate the data array
and hash index eagerly — even before any entries are inserted — so that
the mode is recorded. This is why mode selection must happen before the
first put.

### String Comparison

In all string modes, key comparison during lookup uses `SDL_strcmp` (byte-by-byte
comparison). There is no pointer interning or deduplication — duplicate keys
inserted separately occupy separate memory. (The hash comparison serves as
the fast-rejection path, so full string comparison only happens on hash
collisions.)

## Memory Management

All allocations go through `SDL_realloc` and `SDL_free`. A context pointer is threaded through
the allocator interface but is currently always NULL — a placeholder for
future per-container allocator contexts.

**Dynamic arrays** use a single allocation: one block holding the header and
the element data. Growth is a single `SDL_realloc`. Destruction is a single
`SDL_free`.

**Hash maps** use two allocations:

1. The data array (header + default entry + element data) — same as a
   dynamic array.
2. The hash index (metadata + cache-aligned bucket storage) — a separate
   block. The bucket storage is not a separate allocation; it is part of
   the hash index block, placed at a 64-byte-aligned offset after the
   metadata.

On destruction, both blocks are freed. For string maps in strdup mode, each
key copy is individually freed — iterating from element 1 (skipping the
default at element 0, which has no allocated key). For arena mode, the
arena's chain of blocks is walked and freed.

Because the user pointer changes on any growth operation, the macros
**reassign the pointer variable** provided by the caller. This is the reason
the API must be macros, not functions — a C function cannot reassign the
caller's local variable. The user must always use the macro's result or pass
the pointer variable by name, never a temporary.

### Gotcha: Pointer Invalidation

Any operation that may grow the container (append, put, set-length, etc.)
potentially reallocates. After such an operation, **all prior pointers and
indices into the container are invalid** — the entire block may have moved.
This matches the semantics of `SDL_realloc`.

## Implementation Technique: Expression Macros

The macros are written to be usable as expressions — they evaluate to a
value where appropriate (e.g., the length query evaluates to an integer).
This is achieved through the **comma operator** and **ternary expressions**
in C. A typical pattern:

```text
APPEND(array, val) →
    ( maybe_grow(array, 1),
      array[header(array).length++] = val )
```

The `maybe_grow` call may reallocate and update `array`. Then the comma
operator sequences the assignment, which stores the value and increments
length. The whole expression evaluates to the assigned value.

For hash map operations, the macros are more elaborate. A put operation, for
example:

1. Calls the put-key function, which may reallocate the data array and/or
   rebuild the hash index. This function stores the index of the
   new-or-existing entry in the header's `temp` field.
2. Uses the comma operator to then assign the key and value into
   `array[temp]`.

The `temp` field exists precisely for this cross-step communication. The
thread-safe lookup variants avoid writing to the shared `temp` field by
accepting an external variable from the caller.

For **string maps**, there is an additional cross-step communication channel:
`temp_key`. This is a `char *` stored as the **first field** of the hash
index structure. The macros access it by casting `header->hash_table` to
`char **` and dereferencing. When a put-key function stores a string (via
strdup or arena), it writes the actual stored pointer into `temp_key`. The
macro layer then uses this value to set the key field in the data array.

This is critical for the put_struct operation on string maps: the macro first
writes the entire user-provided struct into the data array slot (overwriting
all fields including the key), then **rewrites the key field** with the value
from `temp_key`. Without this rewrite step, the interned/duplicated key
pointer would be lost, replaced by the user's original (possibly transient)
pointer.

On compilers that support `typeof` (GCC, Clang), the library uses a
compound-literal technique to take the address of rvalue keys — constructing
a single-element array literal of the key type that decays to a pointer.
On other compilers (MSVC), keys must be lvalues so the macro can use `&`.

C++ compatibility is handled through template wrapper functions that perform
the `void *` to `T *` casts that C handles implicitly.

## Summary of Characteristics

| Property                  | Dynamic Arrays       | Hash Maps                       |
|---------------------------|----------------------|---------------------------------|
| Backing store             | Contiguous block     | Data array + hash index         |
| Growth policy             | Double capacity (min 4) | Double slot count at 75% load |
| Index access              | O(1)                 | O(1) amortized                  |
| Append / Insert           | O(1) amortized       | O(1) amortized                  |
| Ordered removal           | O(n) shift           | N/A (swap-with-last)            |
| Unordered removal         | O(1) swap            | O(1) amortized (swap + tombstone) |
| Lookup                    | By index             | By key (hash + bucket probe)    |
| Memory allocations        | 1 per container      | 2 per container (+ string copies) |
| Probing strategy          | N/A                  | Bucketed quadratic probing      |
| Deletion strategy         | Shift or swap        | Tombstones (periodic rebuild)   |
| Thread safety             | None                 | Read-only `_ts` variants        |
| Pointer stability on grow | No                   | No                              |
| Iteration                 | Index 0..length-1    | Index 0..length-1 (same)        |
| Shrinks on delete         | No                   | Yes (below 25% load)            |
