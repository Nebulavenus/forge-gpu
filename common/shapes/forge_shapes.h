/*
 * forge_shapes.h — Procedural geometry library for forge-gpu
 *
 * Generates 3D meshes (positions, normals, UVs, indices) from mathematical
 * descriptions.  Every shape is unit-scale and centred at the origin —
 * use mat4_translate / mat4_scale to place and size it in your scene.
 *
 * Coordinate system: Right-handed, Y-up  (+X right, +Y up, +Z toward camera)
 * Winding order:     Counter-clockwise front faces (matches forge-gpu default)
 * UV origin:         Bottom-left  (U right, V up — matches SDL GPU UV space)
 * Index type:        uint32_t  (supports meshes up to 4 billion vertices)
 *
 * Usage pattern:
 *   ForgeShape sphere = forge_shapes_sphere(32, 16);
 *   // ... upload positions, normals, uvs, indices to GPU ...
 *   forge_shapes_free(&sphere);
 *
 * Memory: heap-allocated via SDL_malloc / SDL_free.  The caller owns the
 * ForgeShape and must call forge_shapes_free() when done.
 *
 * Dependencies: math/forge_math.h, SDL3/SDL.h (SDL_malloc, SDL_free,
 *               SDL_Log only — no GPU API calls)
 *
 * See common/shapes/README.md for full API reference.
 * See lessons/assets/04-procedural-geometry/ for a full walkthrough.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_SHAPES_H
#define FORGE_SHAPES_H

#include <SDL3/SDL.h>
#include "math/forge_math.h"

/* ── Constants ─────────────────────────────────────────────────────────── */

#ifndef FORGE_SHAPES_PI
#define FORGE_SHAPES_PI  3.14159265358979323846f
#endif
#ifndef FORGE_SHAPES_TAU
#define FORGE_SHAPES_TAU 6.28318530717958647692f
#endif

/* ── ForgeShape struct ─────────────────────────────────────────────────── */

/*
 * ForgeShape — a generated mesh ready for GPU upload.
 *
 * Struct-of-arrays layout: each field maps to one SDL_GPUBuffer with no
 * interleaving.  Upload positions to the position buffer, normals to the
 * normal buffer, etc.  This avoids stride arithmetic at upload time and
 * lets shaders that don't need UVs skip that buffer entirely.
 *
 * All arrays are flat and parallel — positions[i], normals[i], and uvs[i]
 * all describe vertex i.  indices[] contains triangle_count * 3 vertex
 * indices in CCW order.
 *
 *   vertex_count   — length of positions[], normals[], uvs[]
 *   index_count    — length of indices[] (always a multiple of 3)
 *   triangle_count — index_count / 3
 */
typedef struct {
    vec3     *positions;    /* [vertex_count]  XYZ positions             */
    vec3     *normals;      /* [vertex_count]  unit normals (may be NULL) */
    vec2     *uvs;          /* [vertex_count]  texture coords (may be NULL) */
    uint32_t *indices;      /* [index_count]   CCW triangle list          */
    int       vertex_count;
    int       index_count;
} ForgeShape;

/* ── Function declarations ─────────────────────────────────────────────── */

/*
 * forge_shapes_sphere — UV-parameterised sphere of radius 1.
 * Vertex count: (slices+1) * (stacks+1)
 * Index count:  slices * 2 * 3 + slices * (stacks-2) * 6  (poles use single triangles)
 */
ForgeShape forge_shapes_sphere(int slices, int stacks);

/*
 * forge_shapes_icosphere — subdivided icosahedron of radius 1.
 * Subdivision 0: 20 vertices (12 base + UV seam duplicates), 60 indices.
 * Subdivision 1: 56 vertices (42 base + UV seam duplicates), 240 indices.
 */
ForgeShape forge_shapes_icosphere(int subdivisions);

/*
 * forge_shapes_cylinder — open-ended cylinder, radius 1, height 2 (Y -1..+1).
 * Vertex count: (slices+1) * (stacks+1)
 * Index count:  slices * stacks * 6
 */
ForgeShape forge_shapes_cylinder(int slices, int stacks);

/*
 * forge_shapes_cone — open-ended cone, apex Y=+1, base radius 1 at Y=-1.
 * Vertex count: (slices+1) * (stacks+1)
 * Index count:  slices * stacks * 6
 */
ForgeShape forge_shapes_cone(int slices, int stacks);

/*
 * forge_shapes_torus — donut shape in the XZ plane.
 * Vertex count: (slices+1) * (stacks+1)
 * Index count:  slices * stacks * 6
 */
ForgeShape forge_shapes_torus(int slices, int stacks,
                               float major_radius, float tube_radius);

/*
 * forge_shapes_plane — flat XZ-plane quad, Y=0, extents [-1,+1].
 * Vertex count: (slices+1) * (stacks+1)
 * Index count:  slices * stacks * 6
 */
ForgeShape forge_shapes_plane(int slices, int stacks);

/*
 * forge_shapes_cube — axis-aligned box, extents [-1,+1] on all axes.
 * Vertex count: 6 * (slices+1) * (stacks+1)
 * Index count:  6 * slices * stacks * 6
 */
ForgeShape forge_shapes_cube(int slices, int stacks);

/*
 * forge_shapes_capsule — cylinder with hemisphere caps, radius 1.
 * Total height: 2 * half_height + 2.
 */
ForgeShape forge_shapes_capsule(int slices, int stacks, int cap_stacks,
                                 float half_height);

/*
 * forge_shapes_free — release all memory owned by a ForgeShape.
 * Sets all pointers to NULL and counts to 0 after freeing.
 * Passing a zeroed or already-freed ForgeShape is safe (no-op).
 */
void forge_shapes_free(ForgeShape *shape);

/*
 * forge_shapes_compute_flat_normals — replace per-vertex normals with
 * face normals.  Unwelds the mesh so every triangle gets 3 unique vertices.
 */
void forge_shapes_compute_flat_normals(ForgeShape *shape);

/*
 * forge_shapes_merge — combine multiple shapes into a single ForgeShape.
 * Index values are offset so each shape's indices remain valid.
 */
ForgeShape forge_shapes_merge(const ForgeShape *shapes, int count);

/* ── Implementation ────────────────────────────────────────────────────── */

#ifdef FORGE_SHAPES_IMPLEMENTATION

/* ── Helper: allocate a ForgeShape ─────────────────────────────────────── */

static ForgeShape forge_shapes__alloc(int vertex_count, int index_count)
{
    ForgeShape s;
    s.vertex_count = 0;
    s.index_count  = 0;
    s.positions = (vec3 *)SDL_malloc((size_t)vertex_count * sizeof(vec3));
    s.normals   = (vec3 *)SDL_malloc((size_t)vertex_count * sizeof(vec3));
    s.uvs       = (vec2 *)SDL_malloc((size_t)vertex_count * sizeof(vec2));
    s.indices   = (uint32_t *)SDL_malloc((size_t)index_count * sizeof(uint32_t));
    if (!s.positions || !s.normals || !s.uvs || !s.indices) {
        SDL_Log("forge_shapes__alloc: out of memory");
        SDL_free(s.positions);
        SDL_free(s.normals);
        SDL_free(s.uvs);
        SDL_free(s.indices);
        s.positions = NULL;
        s.normals   = NULL;
        s.uvs       = NULL;
        s.indices   = NULL;
        return s;  /* vertex_count=0, index_count=0 */
    }
    s.vertex_count = vertex_count;
    s.index_count  = index_count;
    return s;
}

/* ── forge_shapes_free ─────────────────────────────────────────────────── */

void forge_shapes_free(ForgeShape *shape)
{
    if (!shape) return;
    SDL_free(shape->positions);
    SDL_free(shape->normals);
    SDL_free(shape->uvs);
    SDL_free(shape->indices);
    shape->positions    = NULL;
    shape->normals      = NULL;
    shape->uvs          = NULL;
    shape->indices      = NULL;
    shape->vertex_count = 0;
    shape->index_count  = 0;
}

/* ── forge_shapes_compute_flat_normals ─────────────────────────────────── */

void forge_shapes_compute_flat_normals(ForgeShape *shape)
{
    if (!shape || shape->index_count == 0) return;

    int tri_count  = shape->index_count / 3;
    int new_vcount = shape->index_count;  /* 3 unique vertices per triangle */

    vec3     *new_pos = (vec3 *)SDL_malloc((size_t)new_vcount * sizeof(vec3));
    vec3     *new_nor = (vec3 *)SDL_malloc((size_t)new_vcount * sizeof(vec3));
    vec2     *new_uvs = (vec2 *)SDL_malloc((size_t)new_vcount * sizeof(vec2));
    uint32_t *new_idx = (uint32_t *)SDL_malloc((size_t)new_vcount * sizeof(uint32_t));
    if (!new_pos || !new_nor || !new_uvs || !new_idx) {
        SDL_Log("forge_shapes_compute_flat_normals: out of memory");
        SDL_free(new_pos);
        SDL_free(new_nor);
        SDL_free(new_uvs);
        SDL_free(new_idx);
        return;  /* leave original shape unchanged */
    }

    for (int t = 0; t < tri_count; t++) {
        uint32_t i0 = shape->indices[t * 3 + 0];
        uint32_t i1 = shape->indices[t * 3 + 1];
        uint32_t i2 = shape->indices[t * 3 + 2];

        vec3 p0 = shape->positions[i0];
        vec3 p1 = shape->positions[i1];
        vec3 p2 = shape->positions[i2];

        /* Compute face normal from edge cross product */
        vec3 e1 = vec3_sub(p1, p0);
        vec3 e2 = vec3_sub(p2, p0);
        vec3 fn = vec3_normalize(vec3_cross(e1, e2));

        int base = t * 3;
        new_pos[base + 0] = p0;
        new_pos[base + 1] = p1;
        new_pos[base + 2] = p2;
        new_nor[base + 0] = fn;
        new_nor[base + 1] = fn;
        new_nor[base + 2] = fn;

        if (shape->uvs) {
            new_uvs[base + 0] = shape->uvs[i0];
            new_uvs[base + 1] = shape->uvs[i1];
            new_uvs[base + 2] = shape->uvs[i2];
        } else {
            new_uvs[base + 0] = vec2_create(0.0f, 0.0f);
            new_uvs[base + 1] = vec2_create(0.0f, 0.0f);
            new_uvs[base + 2] = vec2_create(0.0f, 0.0f);
        }

        new_idx[base + 0] = (uint32_t)(base + 0);
        new_idx[base + 1] = (uint32_t)(base + 1);
        new_idx[base + 2] = (uint32_t)(base + 2);
    }

    /* Replace old arrays */
    SDL_free(shape->positions);
    SDL_free(shape->normals);
    SDL_free(shape->uvs);
    SDL_free(shape->indices);

    shape->positions    = new_pos;
    shape->normals      = new_nor;
    shape->uvs          = new_uvs;
    shape->indices      = new_idx;
    shape->vertex_count = new_vcount;
    /* index_count stays the same */
}

/* ── forge_shapes_merge ────────────────────────────────────────────────── */

ForgeShape forge_shapes_merge(const ForgeShape *shapes, int count)
{
    if (!shapes || count <= 0) {
        SDL_Log("forge_shapes_merge: invalid input (shapes=%p, count=%d)", (const void *)shapes, count);
        ForgeShape empty = {0};
        return empty;
    }

    int total_verts = 0;
    int total_idx   = 0;
    for (int i = 0; i < count; i++) {
        total_verts += shapes[i].vertex_count;
        total_idx   += shapes[i].index_count;
    }

    ForgeShape merged = forge_shapes__alloc(total_verts, total_idx);
    if (merged.vertex_count == 0) return merged;  /* alloc failed */

    /* Zero-fill normals and UVs so sources with NULL leave safe defaults */
    SDL_memset(merged.normals, 0, (size_t)total_verts * sizeof(vec3));
    SDL_memset(merged.uvs, 0, (size_t)total_verts * sizeof(vec2));

    int v_offset = 0;
    int i_offset = 0;
    for (int s = 0; s < count; s++) {
        const ForgeShape *src = &shapes[s];

        SDL_memcpy(&merged.positions[v_offset], src->positions,
                   (size_t)src->vertex_count * sizeof(vec3));
        if (src->normals) {
            SDL_memcpy(&merged.normals[v_offset], src->normals,
                       (size_t)src->vertex_count * sizeof(vec3));
        }
        if (src->uvs) {
            SDL_memcpy(&merged.uvs[v_offset], src->uvs,
                       (size_t)src->vertex_count * sizeof(vec2));
        }

        /* Copy indices with vertex offset applied */
        for (int j = 0; j < src->index_count; j++) {
            merged.indices[i_offset + j] = src->indices[j] + (uint32_t)v_offset;
        }

        v_offset += src->vertex_count;
        i_offset += src->index_count;
    }

    return merged;
}

/* ── forge_shapes_plane ────────────────────────────────────────────────── */

/*
 * Flat XZ-plane quad in [-1,+1], Y=0, normal = +Y.
 * UV: (0,0) at (-X,-Z) corner, (1,1) at (+X,+Z) corner.
 * Higher tessellation enables vertex-shader displacement (terrain, water).
 */
ForgeShape forge_shapes_plane(int slices, int stacks)
{
    if (slices < 1 || stacks < 1) {
        SDL_Log("forge_shapes_plane: slices (%d) must be >= 1, stacks (%d) must be >= 1", slices, stacks);
        ForgeShape empty = {0};
        return empty;
    }

    int cols = slices + 1;
    int rows = stacks + 1;
    int vertex_count = cols * rows;
    int index_count  = slices * stacks * 6;

    ForgeShape s = forge_shapes__alloc(vertex_count, index_count);
    if (s.vertex_count == 0) return s;  /* alloc failed */

    /* Generate vertices */
    int v = 0;
    for (int row = 0; row < rows; row++) {
        float fz = (float)row / (float)stacks;
        float z  = -1.0f + 2.0f * fz;
        for (int col = 0; col < cols; col++) {
            float fx = (float)col / (float)slices;
            float x  = -1.0f + 2.0f * fx;

            s.positions[v] = vec3_create(x, 0.0f, z);
            s.normals[v]   = vec3_create(0.0f, 1.0f, 0.0f);
            s.uvs[v]       = vec2_create(fx, fz);
            v++;
        }
    }

    /* Generate indices */
    int idx = 0;
    for (int row = 0; row < stacks; row++) {
        for (int col = 0; col < slices; col++) {
            int tl = row * cols + col;
            int tr = tl + 1;
            int bl = tl + cols;
            int br = bl + 1;

            /* Two triangles per quad, CCW winding */
            s.indices[idx++] = (uint32_t)tl;
            s.indices[idx++] = (uint32_t)bl;
            s.indices[idx++] = (uint32_t)tr;

            s.indices[idx++] = (uint32_t)tr;
            s.indices[idx++] = (uint32_t)bl;
            s.indices[idx++] = (uint32_t)br;
        }
    }

    return s;
}

/* ── forge_shapes_cube ─────────────────────────────────────────────────── */

/*
 * Axis-aligned box, extents [-1,+1] on all axes.
 * Six faces, each tessellated into a grid.  Each face has its own normal
 * and UV set — vertices are NOT shared across faces because adjacent faces
 * need different normals.
 *
 * Vertex count: 6 * (slices+1) * (stacks+1)
 * Index count:  6 * slices * stacks * 6
 */
ForgeShape forge_shapes_cube(int slices, int stacks)
{
    if (slices < 1 || stacks < 1) {
        SDL_Log("forge_shapes_cube: slices (%d) must be >= 1, stacks (%d) must be >= 1", slices, stacks);
        ForgeShape empty = {0};
        return empty;
    }

    int face_verts = (slices + 1) * (stacks + 1);
    int face_idx   = slices * stacks * 6;
    int vertex_count = 6 * face_verts;
    int index_count  = 6 * face_idx;

    ForgeShape s = forge_shapes__alloc(vertex_count, index_count);
    if (s.vertex_count == 0) return s;  /* alloc failed */

    /*
     * Face definitions: normal direction, and two tangent axes (u_axis, v_axis)
     * that define the face's local 2D coordinate system.
     * The face is generated at distance +1 along the normal axis.
     *
     * Tangent directions are chosen so that cross(u_axis, v_axis) == normal,
     * giving counter-clockwise winding for outward-facing triangles.
     */
    struct { vec3 normal; vec3 u_axis; vec3 v_axis; } faces[6] = {
        /* +Y (top)    — U along +X, V along +Z */
        { {0, 1, 0},  {1, 0, 0},  {0, 0, 1} },
        /* -Y (bottom) — U along +X, V along -Z */
        { {0,-1, 0},  {1, 0, 0},  {0, 0,-1} },
        /* +X (right)  — U along +Z, V along +Y */
        { {1, 0, 0},  {0, 0, 1},  {0, 1, 0} },
        /* -X (left)   — U along -Z, V along +Y */
        { {-1, 0, 0}, {0, 0,-1},  {0, 1, 0} },
        /* +Z (front)  — U along -X, V along +Y */
        { {0, 0, 1},  {-1, 0, 0}, {0, 1, 0} },
        /* -Z (back)   — U along +X, V along +Y */
        { {0, 0,-1},  {1, 0, 0},  {0, 1, 0} },
    };

    int v_off = 0;
    int i_off = 0;
    int cols  = slices + 1;

    for (int f = 0; f < 6; f++) {
        vec3 n = faces[f].normal;
        vec3 u = faces[f].u_axis;
        vec3 va = faces[f].v_axis;

        /* Generate face vertices */
        for (int row = 0; row <= stacks; row++) {
            float fv = (float)row / (float)stacks;
            float tv = -1.0f + 2.0f * fv;  /* [-1, +1] */
            for (int col = 0; col <= slices; col++) {
                float fu = (float)col / (float)slices;
                float tu = -1.0f + 2.0f * fu;  /* [-1, +1] */

                int vi = v_off + row * cols + col;
                /* Position = normal + tu * u_axis + tv * v_axis */
                s.positions[vi] = vec3_create(
                    n.x + tu * u.x + tv * va.x,
                    n.y + tu * u.y + tv * va.y,
                    n.z + tu * u.z + tv * va.z
                );
                s.normals[vi] = n;
                s.uvs[vi]     = vec2_create(fu, fv);
            }
        }

        /* Generate face indices */
        for (int row = 0; row < stacks; row++) {
            for (int col = 0; col < slices; col++) {
                int tl = v_off + row * cols + col;
                int tr = tl + 1;
                int bl = tl + cols;
                int br = bl + 1;

                s.indices[i_off++] = (uint32_t)tl;
                s.indices[i_off++] = (uint32_t)bl;
                s.indices[i_off++] = (uint32_t)tr;

                s.indices[i_off++] = (uint32_t)tr;
                s.indices[i_off++] = (uint32_t)bl;
                s.indices[i_off++] = (uint32_t)br;
            }
        }

        v_off += face_verts;
    }

    return s;
}

/* ── forge_shapes_sphere ────────────────────────────────────────────────── */

/*
 * UV-parameterised sphere of radius 1, centred at the origin.
 *
 * Uses the standard spherical parameterisation:
 *   x = cos(phi) * sin(theta)    phi   in [0, TAU]  (longitude)
 *   y = cos(theta)               theta in [0, PI]   (latitude, Y-up)
 *   z = sin(phi) * sin(theta)
 *
 * slices  — longitudinal divisions (columns, min 3)
 * stacks  — latitudinal divisions  (rows,    min 2)
 *
 * UV mapping: U = phi / TAU (0..1 west-to-east)
 *             V = 1 - theta / PI  (1 = north pole, 0 = south pole)
 *
 * Seam duplication: the first and last columns of vertices share the same
 * 3D position but have different U coordinates (0 and 1).  Without this
 * duplication, triangles crossing the seam would have UVs that wrap from
 * ~1.0 back to ~0.0, causing the entire texture to compress into a thin
 * strip.  This is why vertex_count is (slices+1)*(stacks+1) rather than
 * slices*(stacks+1).
 *
 * Normals: for a unit sphere, the outward normal at any point equals the
 * normalised position vector — no extra computation needed.
 *
 * Vertex count: (slices+1) * (stacks+1)
 * Index count:  slices * 2 * 3 + slices * (stacks-2) * 6  (poles use single triangles)
 */
ForgeShape forge_shapes_sphere(int slices, int stacks)
{
    if (slices < 3 || stacks < 2) {
        SDL_Log("forge_shapes_sphere: slices (%d) must be >= 3, stacks (%d) must be >= 2", slices, stacks);
        ForgeShape empty = {0};
        return empty;
    }

    int cols = slices + 1;
    int rows = stacks + 1;
    int vertex_count = cols * rows;
    int index_count  = slices * stacks * 6;

    ForgeShape s = forge_shapes__alloc(vertex_count, index_count);
    if (s.vertex_count == 0) return s;  /* alloc failed */

    /* Generate vertices */
    int v = 0;
    for (int row = 0; row < rows; row++) {
        float theta = FORGE_SHAPES_PI * (float)row / (float)stacks;
        float sin_theta = sinf(theta);
        float cos_theta = cosf(theta);

        for (int col = 0; col < cols; col++) {
            float phi = FORGE_SHAPES_TAU * (float)col / (float)slices;
            float sin_phi = sinf(phi);
            float cos_phi = cosf(phi);

            float x = cos_phi * sin_theta;
            float y = cos_theta;
            float z = sin_phi * sin_theta;

            s.positions[v] = vec3_create(x, y, z);
            s.normals[v]   = vec3_create(x, y, z);  /* unit sphere: normal = position */
            s.uvs[v]       = vec2_create(
                (float)col / (float)slices,          /* U = phi / TAU */
                1.0f - (float)row / (float)stacks    /* V = 1 - theta / PI */
            );
            v++;
        }
    }

    /* Generate indices.
     *
     * At the north pole (row 0) all tl/tr vertices are at (0,1,0), so
     * only one triangle per column is valid: (tl, bl, br).
     * At the south pole (last row) all bl/br vertices are at (0,-1,0),
     * so only one triangle is valid: (tl, bl, tr).
     * Middle rows get the standard two-triangle quad.
     */
    int idx = 0;
    for (int row = 0; row < stacks; row++) {
        for (int col = 0; col < slices; col++) {
            int tl = row * cols + col;
            int tr = tl + 1;
            int bl = tl + cols;
            int br = bl + 1;

            if (row == 0) {
                /* North pole fan: tl is the pole vertex, bl/br are on the
                 * first non-pole ring.  CCW when viewed from outside. */
                s.indices[idx++] = (uint32_t)tl;
                s.indices[idx++] = (uint32_t)br;
                s.indices[idx++] = (uint32_t)bl;
            } else if (row == stacks - 1) {
                /* South pole fan: bl is the pole vertex, tl/tr are on the
                 * last non-pole ring.  CCW when viewed from outside. */
                s.indices[idx++] = (uint32_t)bl;
                s.indices[idx++] = (uint32_t)tl;
                s.indices[idx++] = (uint32_t)tr;
            } else {
                /* Middle rows: two triangles per quad, CCW winding
                 * when viewed from outside the sphere */
                s.indices[idx++] = (uint32_t)tl;
                s.indices[idx++] = (uint32_t)tr;
                s.indices[idx++] = (uint32_t)bl;

                s.indices[idx++] = (uint32_t)tr;
                s.indices[idx++] = (uint32_t)br;
                s.indices[idx++] = (uint32_t)bl;
            }
        }
    }

    /* Actual index count may be less than allocated (pole rows emit
     * one triangle per column instead of two) */
    s.index_count = idx;

    return s;
}

/* ── forge_shapes_icosphere ────────────────────────────────────────────── */

/*
 * Subdivided icosahedron of radius 1, centred at the origin.
 *
 * Starts from the 12-vertex, 20-face regular icosahedron and repeatedly
 * subdivides every triangle into 4 smaller triangles by inserting midpoints
 * on each edge, then normalising those midpoints back onto the unit sphere.
 *
 * This produces a much more even triangle distribution than a UV sphere —
 * no dense poles, no sparse equator.
 *
 * subdivisions — number of subdivision passes:
 *   0 = raw icosahedron (20 vertices after UV seam fix, 60 indices)
 *   1 = 56 vertices after UV seam fix, 240 indices
 *   2 = ~200 vertices after UV seam fix, 960 indices
 *
 * Normals: for a unit sphere, normal = normalised position.
 * UVs: spherical projection from normalised position.
 */
ForgeShape forge_shapes_icosphere(int subdivisions)
{
    if (subdivisions < 0) {
        SDL_Log("forge_shapes_icosphere: subdivisions (%d) must be >= 0", subdivisions);
        ForgeShape empty = {0};
        return empty;
    }

    /* Golden ratio for icosahedron vertex positions */
    const float t = (1.0f + sqrtf(5.0f)) / 2.0f;
    const float len = sqrtf(1.0f + t * t);
    const float a = 1.0f / len;
    const float b = t / len;

    /* 12 icosahedron vertices (already on unit sphere after normalisation) */
    vec3 ico_verts[12] = {
        {-a,  b,  0}, { a,  b,  0}, {-a, -b,  0}, { a, -b,  0},
        { 0, -a,  b}, { 0,  a,  b}, { 0, -a, -b}, { 0,  a, -b},
        { b,  0, -a}, { b,  0,  a}, {-b,  0, -a}, {-b,  0,  a},
    };

    /* 20 icosahedron faces (CCW winding) */
    uint32_t ico_faces[60] = {
         0, 11,  5,    0,  5,  1,    0,  1,  7,    0,  7, 10,    0, 10, 11,
         1,  5,  9,    5, 11,  4,   11, 10,  2,   10,  7,  6,    7,  1,  8,
         3,  9,  4,    3,  4,  2,    3,  2,  6,    3,  6,  8,    3,  8,  9,
         4,  9,  5,    2,  4, 11,    6,  2, 10,    8,  6,  7,    9,  8,  1,
    };

    /* Work with dynamic arrays — start from icosahedron, subdivide */
    int max_verts = 12;
    int max_idx   = 60;
    for (int i = 0; i < subdivisions; i++) {
        /* Each subdivision: faces * 4, verts roughly doubles */
        max_idx *= 4;
        max_verts = max_idx / 3 + 2;  /* conservative upper bound */
    }

    vec3     *verts   = (vec3 *)SDL_malloc((size_t)max_verts * sizeof(vec3));
    uint32_t *indices = (uint32_t *)SDL_malloc((size_t)max_idx * sizeof(uint32_t));
    if (!verts || !indices) {
        SDL_Log("forge_shapes_icosphere: out of memory");
        SDL_free(verts);
        SDL_free(indices);
        ForgeShape empty = {0};
        return empty;
    }

    int num_verts = 12;
    int num_idx   = 60;
    SDL_memcpy(verts, ico_verts, sizeof(ico_verts));
    SDL_memcpy(indices, ico_faces, sizeof(ico_faces));

    /* Subdivision loop */
    for (int sub = 0; sub < subdivisions; sub++) {
        int new_num_idx = num_idx * 4;
        uint32_t *new_indices = (uint32_t *)SDL_malloc((size_t)new_num_idx * sizeof(uint32_t));
        if (!new_indices) {
            SDL_Log("forge_shapes_icosphere: out of memory during subdivision");
            SDL_free(verts);
            SDL_free(indices);
            ForgeShape empty = {0};
            return empty;
        }

        /*
         * Midpoint cache: for each edge (a,b) where a < b, store the index
         * of the midpoint vertex.  Use a simple linear scan — for the
         * tessellation levels we support this is fast enough.
         */
        int cache_cap = num_idx;  /* at most one midpoint per edge */
        uint32_t *cache_a = (uint32_t *)SDL_malloc((size_t)cache_cap * sizeof(uint32_t));
        uint32_t *cache_b = (uint32_t *)SDL_malloc((size_t)cache_cap * sizeof(uint32_t));
        uint32_t *cache_m = (uint32_t *)SDL_malloc((size_t)cache_cap * sizeof(uint32_t));
        if (!cache_a || !cache_b || !cache_m) {
            SDL_Log("forge_shapes_icosphere: out of memory for midpoint cache");
            SDL_free(cache_a);
            SDL_free(cache_b);
            SDL_free(cache_m);
            SDL_free(new_indices);
            SDL_free(verts);
            SDL_free(indices);
            ForgeShape empty = {0};
            return empty;
        }
        int cache_count = 0;

        /* Helper: get or create midpoint vertex index */
        #define MIDPOINT(ia, ib) do { \
            uint32_t lo = (ia) < (ib) ? (ia) : (ib); \
            uint32_t hi = (ia) < (ib) ? (ib) : (ia); \
            int found = -1; \
            for (int ci = 0; ci < cache_count; ci++) { \
                if (cache_a[ci] == lo && cache_b[ci] == hi) { found = ci; break; } \
            } \
            if (found >= 0) { \
                mid = cache_m[found]; \
            } else { \
                vec3 mp = vec3_normalize(vec3_create( \
                    (verts[lo].x + verts[hi].x) * 0.5f, \
                    (verts[lo].y + verts[hi].y) * 0.5f, \
                    (verts[lo].z + verts[hi].z) * 0.5f  \
                )); \
                /* Grow verts array if needed */ \
                if (num_verts >= max_verts) { \
                    max_verts *= 2; \
                    vec3 *tmp_v = (vec3 *)SDL_realloc(verts, (size_t)max_verts * sizeof(vec3)); \
                    if (!tmp_v) { \
                        SDL_Log("forge_shapes_icosphere: realloc failed in subdivision"); \
                        SDL_free(cache_a); SDL_free(cache_b); SDL_free(cache_m); \
                        SDL_free(new_indices); SDL_free(verts); SDL_free(indices); \
                        ForgeShape oom = {0}; return oom; \
                    } \
                    verts = tmp_v; \
                } \
                mid = (uint32_t)num_verts; \
                verts[num_verts++] = mp; \
                cache_a[cache_count] = lo; \
                cache_b[cache_count] = hi; \
                cache_m[cache_count] = mid; \
                cache_count++; \
            } \
        } while (0)

        int ni = 0;
        int tri_count = num_idx / 3;
        for (int tri = 0; tri < tri_count; tri++) {
            uint32_t v0 = indices[tri * 3 + 0];
            uint32_t v1 = indices[tri * 3 + 1];
            uint32_t v2 = indices[tri * 3 + 2];

            uint32_t mid;
            MIDPOINT(v0, v1); uint32_t m01 = mid;
            MIDPOINT(v1, v2); uint32_t m12 = mid;
            MIDPOINT(v2, v0); uint32_t m20 = mid;

            /* 4 sub-triangles, preserving CCW winding */
            new_indices[ni++] = v0;  new_indices[ni++] = m01; new_indices[ni++] = m20;
            new_indices[ni++] = v1;  new_indices[ni++] = m12; new_indices[ni++] = m01;
            new_indices[ni++] = v2;  new_indices[ni++] = m20; new_indices[ni++] = m12;
            new_indices[ni++] = m01; new_indices[ni++] = m12; new_indices[ni++] = m20;
        }

        #undef MIDPOINT

        SDL_free(cache_a);
        SDL_free(cache_b);
        SDL_free(cache_m);
        SDL_free(indices);
        indices = new_indices;
        num_idx = new_num_idx;
    }

    /* Build final ForgeShape */
    ForgeShape s = forge_shapes__alloc(num_verts, num_idx);
    if (s.vertex_count == 0) {
        SDL_free(verts);
        SDL_free(indices);
        return s;  /* alloc failed */
    }
    SDL_memcpy(s.positions, verts, (size_t)num_verts * sizeof(vec3));
    SDL_memcpy(s.indices, indices, (size_t)num_idx * sizeof(uint32_t));

    /* Normals = normalised position (unit sphere property) */
    /* UVs = spherical projection from position */
    for (int i = 0; i < num_verts; i++) {
        s.normals[i] = verts[i];

        /* Spherical UV from position */
        float u = 0.5f + atan2f(verts[i].z, verts[i].x) / FORGE_SHAPES_TAU;
        float v_coord = 0.5f + asinf(verts[i].y) / FORGE_SHAPES_PI;
        s.uvs[i] = vec2_create(u, v_coord);
    }

    /*
     * UV seam fix: triangles crossing the anti-meridian (where atan2 wraps
     * from +PI to -PI) have some vertices with U near 0 and others near 1.
     * We detect these triangles and duplicate the low-U vertices with U+1
     * so the interpolation is correct.  Pole vertices (Y near ±1) get their
     * U set to the average of the other two vertices in the triangle.
     */
    {
        /* Allocate extra space for duplicated vertices */
        int extra_cap = num_idx / 3;  /* upper bound: one dup per triangle */
        int extra_count = 0;
        vec3 *extra_pos = (vec3 *)SDL_malloc((size_t)extra_cap * sizeof(vec3));
        vec3 *extra_nrm = (vec3 *)SDL_malloc((size_t)extra_cap * sizeof(vec3));
        vec2 *extra_uv  = (vec2 *)SDL_malloc((size_t)extra_cap * sizeof(vec2));
        if (!extra_pos || !extra_nrm || !extra_uv) {
            SDL_free(extra_pos);
            SDL_free(extra_nrm);
            SDL_free(extra_uv);
            /* Fall through with uncorrected UVs */
            goto ico_seam_done;
        }

        /* Build a remap table: for each seam-crossing vertex, record
         * which triangle index slot needs updating and what the new
         * vertex index will be, but do NOT modify s.indices yet. */
        int remap_cap = num_idx / 3;
        struct { int tri_slot; uint32_t new_idx; } *remap = NULL;
        int remap_count = 0;
        remap = (void *)SDL_malloc((size_t)remap_cap * sizeof(*remap));
        if (!remap) goto ico_seam_done;

        int tri_count = num_idx / 3;
        for (int tri = 0; tri < tri_count; tri++) {
            uint32_t *ti = &s.indices[tri * 3];
            float u0 = s.uvs[ti[0]].x, u1 = s.uvs[ti[1]].x, u2 = s.uvs[ti[2]].x;

            float du01 = fabsf(u0 - u1);
            float du12 = fabsf(u1 - u2);
            float du20 = fabsf(u2 - u0);
            if (du01 <= 0.5f && du12 <= 0.5f && du20 <= 0.5f)
                continue;

            for (int k = 0; k < 3; k++) {
                if (s.uvs[ti[k]].x < 0.5f) {
                    if (extra_count >= extra_cap) {
                        extra_cap *= 2;
                        vec3 *tp = (vec3 *)SDL_realloc(extra_pos, (size_t)extra_cap * sizeof(vec3));
                        vec3 *tn = (vec3 *)SDL_realloc(extra_nrm, (size_t)extra_cap * sizeof(vec3));
                        vec2 *tu = (vec2 *)SDL_realloc(extra_uv,  (size_t)extra_cap * sizeof(vec2));
                        if (!tp || !tn || !tu) {
                            /* At least one realloc failed — keep whichever succeeded */
                            if (tp) extra_pos = tp;
                            if (tn) extra_nrm = tn;
                            if (tu) extra_uv  = tu;
                            SDL_free(remap);
                            goto ico_seam_done;
                        }
                        extra_pos = tp;
                        extra_nrm = tn;
                        extra_uv  = tu;
                    }
                    if (remap_count >= remap_cap) {
                        remap_cap *= 2;
                        void *tmp = SDL_realloc(remap, (size_t)remap_cap * sizeof(*remap));
                        if (!tmp) { SDL_free(remap); goto ico_seam_done; }
                        remap = tmp;
                    }
                    uint32_t old = ti[k];
                    uint32_t new_vi = (uint32_t)(num_verts + extra_count);
                    extra_pos[extra_count] = s.positions[old];
                    extra_nrm[extra_count] = s.normals[old];
                    extra_uv[extra_count]  = vec2_create(s.uvs[old].x + 1.0f, s.uvs[old].y);
                    remap[remap_count].tri_slot = tri * 3 + k;
                    remap[remap_count].new_idx  = new_vi;
                    remap_count++;
                    extra_count++;
                }
            }
        }

        /* All duplicates collected — now try to grow the shape arrays */
        if (extra_count > 0) {
            int new_vert_count = num_verts + extra_count;
            vec3 *tp = (vec3 *)SDL_realloc(s.positions, (size_t)new_vert_count * sizeof(vec3));
            vec3 *tn = (vec3 *)SDL_realloc(s.normals,   (size_t)new_vert_count * sizeof(vec3));
            vec2 *tu = (vec2 *)SDL_realloc(s.uvs,       (size_t)new_vert_count * sizeof(vec2));
            if (tp && tn && tu) {
                s.positions = tp;
                s.normals   = tn;
                s.uvs       = tu;
                SDL_memcpy(&s.positions[num_verts], extra_pos, (size_t)extra_count * sizeof(vec3));
                SDL_memcpy(&s.normals[num_verts],   extra_nrm, (size_t)extra_count * sizeof(vec3));
                SDL_memcpy(&s.uvs[num_verts],       extra_uv,  (size_t)extra_count * sizeof(vec2));
                s.vertex_count = new_vert_count;

                /* Apply index remapping — safe because reallocs all succeeded */
                for (int r = 0; r < remap_count; r++) {
                    s.indices[remap[r].tri_slot] = remap[r].new_idx;
                }
            } else {
                /* Realloc failed — keep original pointers where possible */
                if (tp) s.positions = tp;
                if (tn) s.normals   = tn;
                if (tu) s.uvs       = tu;
            }
        }

        SDL_free(remap);
        SDL_free(extra_pos);
        SDL_free(extra_nrm);
        SDL_free(extra_uv);
    }
ico_seam_done:

    SDL_free(verts);
    SDL_free(indices);
    return s;
}

/* ── forge_shapes_cylinder ──────────────────────────────────────────────── */

/*
 * Open-ended cylinder along the Y axis, radius 1, height 2 (Y from -1 to +1).
 * No caps — use forge_shapes_plane with appropriate transform for caps.
 *
 * slices — longitudinal divisions around the circumference (min 3)
 * stacks — height divisions along the Y axis (min 1)
 *
 * Normal: radially outward — (cos(phi), 0, sin(phi)) at angle phi.
 * UV: U = phi / TAU (wraps around), V = (y + 1) / 2 (0 = bottom, 1 = top).
 *
 * Seam duplication: same approach as the sphere — (slices+1) columns so
 * the first and last share position but have U=0 and U=1 respectively.
 *
 * Vertex count: (slices+1) * (stacks+1)
 * Index count:  slices * stacks * 6
 */
ForgeShape forge_shapes_cylinder(int slices, int stacks)
{
    if (slices < 3 || stacks < 1) {
        SDL_Log("forge_shapes_cylinder: slices (%d) must be >= 3, stacks (%d) must be >= 1", slices, stacks);
        ForgeShape empty = {0};
        return empty;
    }

    int cols = slices + 1;
    int rows = stacks + 1;
    int vertex_count = cols * rows;
    int index_count  = slices * stacks * 6;

    ForgeShape s = forge_shapes__alloc(vertex_count, index_count);
    if (s.vertex_count == 0) return s;  /* alloc failed */

    /* Generate vertices */
    int v = 0;
    for (int row = 0; row < rows; row++) {
        float fv = (float)row / (float)stacks;
        float y  = -1.0f + 2.0f * fv;

        for (int col = 0; col < cols; col++) {
            float fu  = (float)col / (float)slices;
            float phi = FORGE_SHAPES_TAU * fu;
            float cp  = cosf(phi);
            float sp  = sinf(phi);

            s.positions[v] = vec3_create(cp, y, sp);
            s.normals[v]   = vec3_create(cp, 0.0f, sp);
            s.uvs[v]       = vec2_create(fu, fv);
            v++;
        }
    }

    /* Generate indices */
    int idx = 0;
    for (int row = 0; row < stacks; row++) {
        for (int col = 0; col < slices; col++) {
            int tl = row * cols + col;
            int tr = tl + 1;
            int bl = tl + cols;
            int br = bl + 1;

            s.indices[idx++] = (uint32_t)tl;
            s.indices[idx++] = (uint32_t)bl;
            s.indices[idx++] = (uint32_t)tr;

            s.indices[idx++] = (uint32_t)tr;
            s.indices[idx++] = (uint32_t)bl;
            s.indices[idx++] = (uint32_t)br;
        }
    }

    return s;
}

/* ── forge_shapes_cone ─────────────────────────────────────────────────── */

/*
 * Open-ended cone along the Y axis.  Apex at Y=+1, base circle of radius 1
 * at Y=-1, centred at origin.  Base cap is NOT included.
 *
 * slices — longitudinal divisions around the base (min 3)
 * stacks — height divisions along the slope (min 1)
 *
 * The radius at height fraction t is: r(t) = 1 - t  (1 at base, 0 at apex).
 *
 * Normals: the cone's slant normal requires the half-angle of the cone.
 * For a unit cone with base radius 1 and height 2, the half-angle is
 * atan2(1, 2).  The normal at angle phi is:
 *   n = normalize(cos(phi) * sin(half_angle), cos(half_angle),
 *                 sin(phi) * sin(half_angle))
 * This tilts the flat radial direction upward by the slant angle.
 *
 * UV: U = phi / TAU, V = height fraction (0 at base, 1 at apex).
 *
 * Vertex count: (slices+1) * (stacks+1)
 * Index count:  slices * stacks * 6
 */
ForgeShape forge_shapes_cone(int slices, int stacks)
{
    if (slices < 3 || stacks < 1) {
        SDL_Log("forge_shapes_cone: slices (%d) must be >= 3, stacks (%d) must be >= 1", slices, stacks);
        ForgeShape empty = {0};
        return empty;
    }

    int cols = slices + 1;
    int rows = stacks + 1;
    int vertex_count = cols * rows;
    int index_count  = slices * stacks * 6;

    ForgeShape s = forge_shapes__alloc(vertex_count, index_count);
    if (s.vertex_count == 0) return s;  /* alloc failed */

    /* Slant normal: half-angle of the cone */
    float half_angle = atan2f(1.0f, 2.0f);
    float sin_ha = sinf(half_angle);
    float cos_ha = cosf(half_angle);

    /* Generate vertices */
    int v = 0;
    for (int row = 0; row < rows; row++) {
        float fv = (float)row / (float)stacks;  /* 0 at base, 1 at apex */
        float y  = -1.0f + 2.0f * fv;
        float r  = 1.0f - fv;  /* radius shrinks to 0 at apex */

        for (int col = 0; col < cols; col++) {
            float fu  = (float)col / (float)slices;
            float phi = FORGE_SHAPES_TAU * fu;
            float cp  = cosf(phi);
            float sp  = sinf(phi);

            s.positions[v] = vec3_create(r * cp, y, r * sp);

            /* Slant normal — tilted outward and upward */
            s.normals[v] = vec3_create(cp * cos_ha, sin_ha, sp * cos_ha);

            s.uvs[v] = vec2_create(fu, fv);
            v++;
        }
    }

    /* Generate indices */
    int idx = 0;
    for (int row = 0; row < stacks; row++) {
        for (int col = 0; col < slices; col++) {
            int tl = row * cols + col;
            int tr = tl + 1;
            int bl = tl + cols;
            int br = bl + 1;

            s.indices[idx++] = (uint32_t)tl;
            s.indices[idx++] = (uint32_t)bl;
            s.indices[idx++] = (uint32_t)tr;

            s.indices[idx++] = (uint32_t)tr;
            s.indices[idx++] = (uint32_t)bl;
            s.indices[idx++] = (uint32_t)br;
        }
    }

    return s;
}

/* ── forge_shapes_capsule ──────────────────────────────────────────────── */

/*
 * Cylinder of radius 1 with hemisphere caps at each end.
 * Total height = 2 * half_height + 2 (cylinder body + two hemisphere radii).
 *
 * slices     — divisions around the circumference (min 3)
 * stacks     — divisions along the cylinder body (min 1)
 * cap_stacks — latitudinal divisions in each hemisphere cap (min 1)
 * half_height — half the length of the cylindrical section
 *
 * The capsule is built in three sections, bottom to top:
 *   1. Bottom hemisphere (south pole at Y = -(half_height + 1))
 *   2. Cylinder body
 *   3. Top hemisphere (north pole at Y = +(half_height + 1))
 *
 * UV: V runs continuously from 0 (south pole) to 1 (north pole).
 * The V range is split proportionally:
 *   bottom cap: [0, cap_fraction]
 *   cylinder:   [cap_fraction, 1 - cap_fraction]
 *   top cap:    [1 - cap_fraction, 1]
 * where cap_fraction = cap_stacks / total_rows.
 */
ForgeShape forge_shapes_capsule(int slices, int stacks, int cap_stacks,
                                 float half_height)
{
    if (slices < 3 || stacks < 1 || cap_stacks < 1 || half_height < 0.0f) {
        SDL_Log("forge_shapes_capsule: invalid params (slices=%d, stacks=%d, cap_stacks=%d, half_height=%.2f)",
                slices, stacks, cap_stacks, (double)half_height);
        ForgeShape empty = {0};
        return empty;
    }

    int cols = slices + 1;
    int total_rows = cap_stacks + stacks + cap_stacks + 1;
    int vertex_count = cols * total_rows;
    int index_count  = slices * (total_rows - 1) * 6;

    ForgeShape s = forge_shapes__alloc(vertex_count, index_count);
    if (s.vertex_count == 0) return s;  /* alloc failed */

    int v = 0;

    /* Bottom hemisphere: from south pole to equator */
    for (int row = 0; row < cap_stacks; row++) {
        /* theta goes from PI (south pole) to PI/2 (equator) */
        float frac = (float)row / (float)cap_stacks;
        float theta = FORGE_SHAPES_PI - frac * (FORGE_SHAPES_PI * 0.5f);
        float sin_t = sinf(theta);
        float cos_t = cosf(theta);
        float y_off = -half_height;

        float fv = (float)row / (float)(total_rows - 1);

        for (int col = 0; col < cols; col++) {
            float fu  = (float)col / (float)slices;
            float phi = FORGE_SHAPES_TAU * fu;
            float cp  = cosf(phi);
            float sp  = sinf(phi);

            s.positions[v] = vec3_create(sin_t * cp, cos_t + y_off, sin_t * sp);
            s.normals[v]   = vec3_create(sin_t * cp, cos_t, sin_t * sp);
            s.uvs[v]       = vec2_create(fu, fv);
            v++;
        }
    }

    /* Cylinder body */
    for (int row = 0; row <= stacks; row++) {
        float frac = (float)row / (float)stacks;
        float y = -half_height + 2.0f * half_height * frac;

        float fv = (float)(cap_stacks + row) / (float)(total_rows - 1);

        for (int col = 0; col < cols; col++) {
            float fu  = (float)col / (float)slices;
            float phi = FORGE_SHAPES_TAU * fu;
            float cp  = cosf(phi);
            float sp  = sinf(phi);

            s.positions[v] = vec3_create(cp, y, sp);
            s.normals[v]   = vec3_create(cp, 0.0f, sp);
            s.uvs[v]       = vec2_create(fu, fv);
            v++;
        }
    }

    /* Top hemisphere: from equator to north pole */
    for (int row = 1; row <= cap_stacks; row++) {
        float frac = (float)row / (float)cap_stacks;
        float theta = (FORGE_SHAPES_PI * 0.5f) - frac * (FORGE_SHAPES_PI * 0.5f);
        float sin_t = sinf(theta);
        float cos_t = cosf(theta);
        float y_off = half_height;

        float fv = (float)(cap_stacks + stacks + row) / (float)(total_rows - 1);

        for (int col = 0; col < cols; col++) {
            float fu  = (float)col / (float)slices;
            float phi = FORGE_SHAPES_TAU * fu;
            float cp  = cosf(phi);
            float sp  = sinf(phi);

            s.positions[v] = vec3_create(sin_t * cp, cos_t + y_off, sin_t * sp);
            s.normals[v]   = vec3_create(sin_t * cp, cos_t, sin_t * sp);
            s.uvs[v]       = vec2_create(fu, fv);
            v++;
        }
    }

    /* Generate indices — same grid pattern for all rows */
    int idx = 0;
    for (int row = 0; row < total_rows - 1; row++) {
        for (int col = 0; col < slices; col++) {
            int tl = row * cols + col;
            int tr = tl + 1;
            int bl = tl + cols;
            int br = bl + 1;

            s.indices[idx++] = (uint32_t)tl;
            s.indices[idx++] = (uint32_t)bl;
            s.indices[idx++] = (uint32_t)tr;

            s.indices[idx++] = (uint32_t)tr;
            s.indices[idx++] = (uint32_t)bl;
            s.indices[idx++] = (uint32_t)br;
        }
    }

    /* Adjust counts to actual values */
    s.vertex_count = v;
    s.index_count  = idx;

    return s;
}

/* ── forge_shapes_torus ─────────────────────────────────────────────────── */

/*
 * Torus (donut) lying in the XZ plane — the hole is along the Y axis.
 *
 * Two radii define the shape:
 *   major_radius — distance from the torus centre to the tube centre
 *   tube_radius  — radius of the tube itself
 *
 * For a "fat donut": major_radius=1.0, tube_radius=0.4
 * For a "thin ring":  major_radius=1.0, tube_radius=0.1
 *
 * Parameterisation:
 *   phi   = angle around the ring  [0, TAU]  (the big circle)
 *   theta = angle around the tube  [0, TAU]  (the small circle)
 *
 *   x = (major_radius + tube_radius * cos(theta)) * cos(phi)
 *   y =  tube_radius * sin(theta)
 *   z = (major_radius + tube_radius * cos(theta)) * sin(phi)
 *
 * Normal direction: the vector from the tube centre to the surface point,
 * normalised.  The tube centre at angle phi is (major_radius*cos(phi), 0,
 * major_radius*sin(phi)).  Subtracting this from the surface point gives the
 * outward direction: (cos(theta)*cos(phi), sin(theta), cos(theta)*sin(phi)).
 *
 * UV: U = phi / TAU (around the ring), V = theta / TAU (around the tube).
 *
 * slices — divisions around the ring (min 3)
 * stacks — divisions around the tube (min 3)
 *
 * Vertex count: (slices+1) * (stacks+1)
 * Index count:  slices * stacks * 6
 */
ForgeShape forge_shapes_torus(int slices, int stacks,
                               float major_radius, float tube_radius)
{
    if (slices < 3 || stacks < 3 || major_radius <= 0.0f || tube_radius <= 0.0f) {
        SDL_Log("forge_shapes_torus: invalid params (slices=%d, stacks=%d, major=%.2f, tube=%.2f)",
                slices, stacks, (double)major_radius, (double)tube_radius);
        ForgeShape empty = {0};
        return empty;
    }

    int cols = slices + 1;
    int rows = stacks + 1;
    int vertex_count = cols * rows;
    int index_count  = slices * stacks * 6;

    ForgeShape s = forge_shapes__alloc(vertex_count, index_count);
    if (s.vertex_count == 0) return s;  /* alloc failed */

    /* Generate vertices */
    int v = 0;
    for (int row = 0; row < rows; row++) {
        float fv    = (float)row / (float)stacks;
        float theta = FORGE_SHAPES_TAU * fv;
        float cos_t = cosf(theta);
        float sin_t = sinf(theta);

        for (int col = 0; col < cols; col++) {
            float fu  = (float)col / (float)slices;
            float phi = FORGE_SHAPES_TAU * fu;
            float cos_p = cosf(phi);
            float sin_p = sinf(phi);

            float r = major_radius + tube_radius * cos_t;

            s.positions[v] = vec3_create(
                r * cos_p,
                tube_radius * sin_t,
                r * sin_p
            );

            /* Normal: outward from tube centre */
            s.normals[v] = vec3_create(
                cos_t * cos_p,
                sin_t,
                cos_t * sin_p
            );

            s.uvs[v] = vec2_create(fu, fv);
            v++;
        }
    }

    /* Generate indices */
    int idx = 0;
    for (int row = 0; row < stacks; row++) {
        for (int col = 0; col < slices; col++) {
            int tl = row * cols + col;
            int tr = tl + 1;
            int bl = tl + cols;
            int br = bl + 1;

            s.indices[idx++] = (uint32_t)tl;
            s.indices[idx++] = (uint32_t)bl;
            s.indices[idx++] = (uint32_t)tr;

            s.indices[idx++] = (uint32_t)tr;
            s.indices[idx++] = (uint32_t)bl;
            s.indices[idx++] = (uint32_t)br;
        }
    }

    return s;
}

#endif /* FORGE_SHAPES_IMPLEMENTATION */

#endif /* FORGE_SHAPES_H */
