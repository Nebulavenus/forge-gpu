# Asset Lesson 06 — Loading Processed Assets in C

## Type

Type C — C library lesson. Produces `common/pipeline/forge_pipeline.h`
(header-only) and `tests/pipeline/test_pipeline.c`.

## What it teaches

- The `.fmesh` binary format produced by `forge-mesh-tool` (header, vertex
  layout with tangents, index buffer, LOD table)
- Reading `.meta.json` sidecars to discover what the pipeline produced
- A header-only C loader: `common/pipeline/forge_pipeline.h`
- Loading BC7 compressed textures (albedo/diffuse — high-quality RGBA)
- Loading BC5 compressed textures (normal maps — two-channel RG, reconstruct
  Z in shader via `z = sqrt(1 - x² - y²)`)
- Mipmap loading from pipeline-generated mip chains
- How this loader bridges the Python pipeline (Asset Lessons 01-05) and GPU
  lessons that consume processed assets (Lesson 39+)

## Deliverables

1. `common/pipeline/forge_pipeline.h` — header-only C library
2. `common/pipeline/README.md` — API reference
3. `tests/pipeline/test_pipeline.c` — comprehensive test suite
4. `tests/pipeline/CMakeLists.txt` — test build
5. `lessons/assets/06-loading-processed-assets/README.md` — lesson walkthrough

## API Design

```c
/* ── Mesh loading ─────────────────────────────────────── */

typedef struct ForgePipelineVertex {
    float position[3];
    float normal[3];
    float uv[2];
} ForgePipelineVertex;

typedef struct ForgePipelineVertexTan {
    float position[3];
    float normal[3];
    float uv[2];
    float tangent[4];  /* xyz = direction, w = handedness */
} ForgePipelineVertexTan;

typedef struct ForgePipelineLod {
    uint32_t index_count;
    uint32_t index_offset;  /* byte offset into index data */
    float    target_error;
} ForgePipelineLod;

typedef struct ForgePipelineMesh {
    void        *vertices;       /* ForgePipelineVertex or ForgePipelineVertexTan */
    uint32_t    *indices;        /* all LOD indices concatenated */
    uint32_t     vertex_count;
    uint32_t     vertex_stride;  /* 32 or 48 */
    ForgePipelineLod *lods;
    uint32_t     lod_count;
    uint32_t     flags;          /* FORGE_PIPELINE_FLAG_TANGENTS etc */
} ForgePipelineMesh;

bool forge_pipeline_load_mesh(const char *path, ForgePipelineMesh *mesh);
void forge_pipeline_free_mesh(ForgePipelineMesh *mesh);

/* ── Texture loading ──────────────────────────────────── */

typedef enum ForgePipelineTextureFormat {
    FORGE_PIPELINE_TEX_RGBA8,   /* uncompressed RGBA */
    FORGE_PIPELINE_TEX_RGB8,    /* uncompressed RGB */
    FORGE_PIPELINE_TEX_BC7,     /* BC7 compressed (albedo) */
    FORGE_PIPELINE_TEX_BC5,     /* BC5 compressed (normal maps) */
} ForgePipelineTextureFormat;

typedef struct ForgePipelineMipLevel {
    void    *data;
    uint32_t width;
    uint32_t height;
    uint32_t size;   /* data size in bytes */
} ForgePipelineMipLevel;

typedef struct ForgePipelineTexture {
    ForgePipelineMipLevel *mips;
    uint32_t               mip_count;
    uint32_t               width;      /* base level */
    uint32_t               height;     /* base level */
    ForgePipelineTextureFormat format;
} ForgePipelineTexture;

bool forge_pipeline_load_texture(const char *path, ForgePipelineTexture *tex);
void forge_pipeline_free_texture(ForgePipelineTexture *tex);

/* ── Utility ──────────────────────────────────────────── */

bool forge_pipeline_has_tangents(const ForgePipelineMesh *mesh);
uint32_t forge_pipeline_lod_index_count(const ForgePipelineMesh *mesh, uint32_t lod);
const uint32_t *forge_pipeline_lod_indices(const ForgePipelineMesh *mesh, uint32_t lod);
```

## Binary format reference (.fmesh)

```text
Offset  Size   Field
──────  ────   ──────────────
  0       4    Magic "FMSH"
  4       4    Version (uint32, currently 1)
  8       4    vertex_count (uint32)
 12       4    vertex_stride (uint32, 32 or 48)
 16       4    lod_count (uint32)
 20       4    flags (uint32, bit 0 = tangents)
 24       8    reserved (zero-filled)
 32    12*N    LOD entries (N = lod_count)
               - index_count  (uint32)
               - index_offset (uint32, byte offset into index section)
               - target_error (float)
32+12*N  V*S   Vertex data (V = vertex_count, S = vertex_stride)
               - position  float[3]
               - normal    float[3]
               - uv        float[2]
               - tangent   float[4]  (if stride == 48)
  ...    I*4   Index data (I = total indices across all LODs, uint32)
```

## Test plan (test_pipeline.c)

### Mesh tests

1. **Load valid .fmesh** — create a minimal .fmesh in /tmp, load it, verify
   vertex/index counts, stride, flags, LOD data
1. **Load mesh with tangents** — stride 48, TANGENTS flag set
1. **Load mesh without tangents** — stride 32, no TANGENTS flag
1. **Multiple LOD levels** — verify each LOD's index_count and indices
1. **LOD accessor functions** — forge_pipeline_lod_index_count, lod_indices
1. **Invalid magic** — reject non-FMSH files
1. **Invalid version** — reject unsupported versions
1. **Invalid stride** — reject stride != 32 and != 48
1. **Truncated file** — reject files shorter than header
1. **NULL path** — handle gracefully
1. **Nonexistent file** — handle gracefully
1. **Free NULL mesh** — no crash
1. **Double free** — zeroed-out struct after free

### Texture tests

1. **Load PNG texture** — load a valid PNG, verify dimensions and pixel data
1. **Load texture with mipmaps** — verify mip chain dimensions
1. **Nonexistent texture** — handle gracefully
1. **NULL path** — handle gracefully
1. **Free NULL texture** — no crash

### Helper function tests

1. **has_tangents** — true when flag set, false otherwise
1. **LOD out of range** — returns 0/NULL for invalid LOD index

## Implementation notes

- The .fmesh loader reads the entire file with `SDL_LoadFile`, then parses
  the in-memory buffer. This is simpler and faster than stream-based reading.
- Texture loading uses `SDL_LoadFile` + stb_image or SDL_image. For this
  initial version, we load uncompressed PNG/JPG textures (the formats
  currently output by the texture plugin). BC7/BC5 KTX2 loading will be
  added when the texture plugin gains real GPU compression output.
- The loader validates all header fields before allocating memory.
- All allocations use `SDL_malloc`/`SDL_calloc`/`SDL_free`.
- The free functions zero out the struct to prevent use-after-free.

## Files to update

- `CMakeLists.txt` — add `add_subdirectory(tests/pipeline)`
- `PLAN.md` — check off Asset Lesson 06
- `lessons/assets/README.md` — add lesson 06 row
