/*
 * forge_obj.h — Header-only Wavefront OBJ parser for forge-gpu
 *
 * Loads a .obj file into a flat array of de-indexed vertices ready for
 * GPU upload.  "De-indexed" means each triangle gets its own copy of
 * each vertex — no shared indices — because OBJ allows separate index
 * streams for position, UV, and normal, which can't map 1:1 to a GPU
 * index buffer without duplication.
 *
 * Supports:
 *   - Positions (v), texture coordinates (vt), normals (vn)
 *   - Triangular and quad faces (f) with v/vt/vn indices
 *   - Quads are automatically triangulated into two triangles
 *   - 1-based OBJ indices (converted internally)
 *   - Windows (\r\n) and Unix (\n) line endings
 *
 * Limitations (fine for a learning library):
 *   - Single-object files only (ignores g/o grouping)
 *   - No material library parsing (mtllib/usemtl ignored)
 *   - No negative (relative) indices
 *   - Faces must be triangles or quads (no n-gons with n > 4)
 *
 * Usage:
 *   #include "obj/forge_obj.h"
 *
 *   ForgeObjMesh mesh;
 *   if (forge_obj_load("model.obj", &mesh)) {
 *       // mesh.vertices[0..mesh.vertex_count-1] ready for GPU
 *       // Upload to a vertex buffer, then:
 *       forge_obj_free(&mesh);
 *   }
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef FORGE_OBJ_H
#define FORGE_OBJ_H

#include <SDL3/SDL.h>
#include "math/forge_math.h"

/* ── Vertex layout ────────────────────────────────────────────────────────── */
/* Position + normal + UV — the standard vertex format for textured 3D models.
 * This matches the vertex attributes we'll bind in the pipeline:
 *   location 0: float3 position  (TEXCOORD0)
 *   location 1: float3 normal    (TEXCOORD1)
 *   location 2: float2 uv        (TEXCOORD2) */

typedef struct ForgeObjVertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
} ForgeObjVertex;

/* ── Mesh result ──────────────────────────────────────────────────────────── */
/* A flat array of vertices — every 3 consecutive vertices form one triangle.
 * No index buffer needed; use SDL_DrawGPUPrimitives(pass, vertex_count, 1, 0, 0). */

typedef struct ForgeObjMesh {
    ForgeObjVertex *vertices;
    Uint32          vertex_count;
} ForgeObjMesh;

/* ── Public API ───────────────────────────────────────────────────────────── */

/* Load an OBJ file and fill out_mesh with de-indexed triangle vertices.
 * Returns true on success, false on error (logged via SDL_Log). */
static bool forge_obj_load(const char *path, ForgeObjMesh *out_mesh);

/* Free the vertex array allocated by forge_obj_load. */
static void forge_obj_free(ForgeObjMesh *mesh);

/* ══════════════════════════════════════════════════════════════════════════ */
/* ── Implementation ───────────────────────────────────────────────────────── */
/* ══════════════════════════════════════════════════════════════════════════ */

/* ── Line parsing helpers ─────────────────────────────────────────────────── */

/* Skip whitespace (spaces and tabs, not newlines). */
static const char *forge_obj__skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Parse a float and advance the pointer past it. */
static float forge_obj__parse_float(const char **pp)
{
    const char *p = forge_obj__skip_ws(*pp);
    float val = 0.0f;
    bool negative = false;

    if (*p == '-') { negative = true; p++; }
    else if (*p == '+') { p++; }

    /* Integer part */
    while (*p >= '0' && *p <= '9') {
        val = val * 10.0f + (float)(*p - '0');
        p++;
    }

    /* Fractional part */
    if (*p == '.') {
        p++;
        float frac = 0.1f;
        while (*p >= '0' && *p <= '9') {
            val += (float)(*p - '0') * frac;
            frac *= 0.1f;
            p++;
        }
    }

    /* Exponent (e.g. 1.5e-3) */
    if (*p == 'e' || *p == 'E') {
        p++;
        bool exp_neg = false;
        if (*p == '-') { exp_neg = true; p++; }
        else if (*p == '+') { p++; }
        int exp_val = 0;
        while (*p >= '0' && *p <= '9') {
            exp_val = exp_val * 10 + (*p - '0');
            p++;
        }
        float multiplier = 1.0f;
        for (int i = 0; i < exp_val; i++) {
            multiplier *= 10.0f;
        }
        if (exp_neg) val /= multiplier;
        else         val *= multiplier;
    }

    if (negative) val = -val;
    *pp = p;
    return val;
}

/* Parse an integer and advance the pointer past it. */
static int forge_obj__parse_int(const char **pp)
{
    const char *p = forge_obj__skip_ws(*pp);
    int val = 0;
    bool negative = false;

    if (*p == '-') { negative = true; p++; }
    else if (*p == '+') { p++; }

    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }

    if (negative) val = -val;
    *pp = p;
    return val;
}

/* Advance pointer to the next line (past \n or \r\n). */
static const char *forge_obj__next_line(const char *p)
{
    while (*p && *p != '\n' && *p != '\r') p++;
    if (*p == '\r') p++;
    if (*p == '\n') p++;
    return p;
}

/* Check if a line starts with a given prefix (followed by whitespace). */
static bool forge_obj__starts_with(const char *line, const char *prefix)
{
    while (*prefix) {
        if (*line != *prefix) return false;
        line++;
        prefix++;
    }
    /* The prefix must be followed by whitespace or end of line. */
    return (*line == ' ' || *line == '\t');
}

/* ── Face index parsing ───────────────────────────────────────────────────── */
/* OBJ face vertices are in the form: v/vt/vn, v//vn, or v/vt, or just v.
 * All indices are 1-based in the file; we convert to 0-based here. */

typedef struct ForgeObjFaceIndex {
    int v;   /* position index (0-based, -1 if missing) */
    int vt;  /* texcoord index (0-based, -1 if missing) */
    int vn;  /* normal index   (0-based, -1 if missing) */
} ForgeObjFaceIndex;

/* Parse one v/vt/vn index group and advance the pointer. */
static ForgeObjFaceIndex forge_obj__parse_face_index(const char **pp)
{
    ForgeObjFaceIndex idx;
    idx.v  = -1;
    idx.vt = -1;
    idx.vn = -1;

    const char *p = forge_obj__skip_ws(*pp);

    /* Position index (required) */
    idx.v = forge_obj__parse_int(&p) - 1;

    if (*p == '/') {
        p++;
        if (*p != '/') {
            /* Texture coordinate index */
            idx.vt = forge_obj__parse_int(&p) - 1;
        }
        if (*p == '/') {
            p++;
            /* Normal index */
            idx.vn = forge_obj__parse_int(&p) - 1;
        }
    }

    *pp = p;
    return idx;
}

/* ── Main loader ──────────────────────────────────────────────────────────── */

static bool forge_obj_load(const char *path, ForgeObjMesh *out_mesh)
{
    out_mesh->vertices     = NULL;
    out_mesh->vertex_count = 0;

    /* ── Load the entire file into memory ─────────────────────────────
     * SDL_LoadFile reads a file and returns a null-terminated string.
     * This is simpler than line-by-line I/O and fast enough for the
     * model sizes we deal with in these lessons. */
    size_t file_size = 0;
    char *file_data = (char *)SDL_LoadFile(path, &file_size);
    if (!file_data) {
        SDL_Log("forge_obj_load: failed to load '%s': %s", path, SDL_GetError());
        return false;
    }

    /* ── First pass: count elements ───────────────────────────────────
     * We scan through the file once to count how many positions, UVs,
     * normals, and face vertices we'll need.  This lets us allocate
     * all arrays up front instead of using dynamic resizing. */
    int num_positions = 0;
    int num_texcoords = 0;
    int num_normals   = 0;
    int num_triangles = 0;  /* total triangles after quad triangulation */

    const char *p = file_data;
    while (*p) {
        if (p[0] == 'v' && p[1] == 't' && (p[2] == ' ' || p[2] == '\t')) {
            num_texcoords++;
        } else if (p[0] == 'v' && p[1] == 'n' && (p[2] == ' ' || p[2] == '\t')) {
            num_normals++;
        } else if (p[0] == 'v' && (p[1] == ' ' || p[1] == '\t')) {
            num_positions++;
        } else if (p[0] == 'f' && (p[1] == ' ' || p[1] == '\t')) {
            /* Count vertices in this face to determine triangle count.
             * A face with N vertices produces (N - 2) triangles:
             *   3 vertices = 1 triangle
             *   4 vertices = 2 triangles (quad split into two tris) */
            const char *fp = p + 1;
            int verts_in_face = 0;
            fp = forge_obj__skip_ws(fp);
            while (*fp && *fp != '\n' && *fp != '\r') {
                if (*fp >= '0' && *fp <= '9') {
                    verts_in_face++;
                    /* Skip this index group (digits, slashes) */
                    while (*fp && *fp != ' ' && *fp != '\t' &&
                           *fp != '\n' && *fp != '\r') {
                        fp++;
                    }
                }
                fp = forge_obj__skip_ws(fp);
            }
            if (verts_in_face >= 3) {
                num_triangles += (verts_in_face - 2);
            }
        }
        p = forge_obj__next_line(p);
    }

    SDL_Log("OBJ '%s': %d positions, %d texcoords, %d normals, %d triangles",
            path, num_positions, num_texcoords, num_normals, num_triangles);

    if (num_positions == 0 || num_triangles == 0) {
        SDL_Log("forge_obj_load: no geometry found in '%s'", path);
        SDL_free(file_data);
        return false;
    }

    /* ── Allocate temporary arrays for raw OBJ data ───────────────────
     * These hold the v/vt/vn values as parsed.  The face pass will
     * index into these to build the final de-indexed vertex array. */
    vec3 *positions = (vec3 *)SDL_malloc(sizeof(vec3) * (size_t)num_positions);
    vec2 *texcoords = num_texcoords > 0
        ? (vec2 *)SDL_malloc(sizeof(vec2) * (size_t)num_texcoords)
        : NULL;
    vec3 *normals = num_normals > 0
        ? (vec3 *)SDL_malloc(sizeof(vec3) * (size_t)num_normals)
        : NULL;

    /* Output: 3 vertices per triangle, fully de-indexed. */
    Uint32 total_verts = (Uint32)num_triangles * 3;
    ForgeObjVertex *vertices = (ForgeObjVertex *)SDL_malloc(
        sizeof(ForgeObjVertex) * total_verts);

    if (!positions || (num_texcoords > 0 && !texcoords) ||
        (num_normals > 0 && !normals) || !vertices) {
        SDL_Log("forge_obj_load: allocation failed");
        SDL_free(positions);
        SDL_free(texcoords);
        SDL_free(normals);
        SDL_free(vertices);
        SDL_free(file_data);
        return false;
    }

    /* ── Second pass: parse data ──────────────────────────────────────
     * Now we actually read the numbers and build vertices. */
    int pi = 0, ti = 0, ni = 0;
    Uint32 vi = 0;  /* output vertex index */

    p = file_data;
    while (*p) {
        if (p[0] == 'v' && (p[1] == ' ' || p[1] == '\t')) {
            /* Position: "v x y z" */
            const char *lp = p + 1;
            float x = forge_obj__parse_float(&lp);
            float y = forge_obj__parse_float(&lp);
            float z = forge_obj__parse_float(&lp);
            positions[pi++] = vec3_create(x, y, z);

        } else if (p[0] == 'v' && p[1] == 't' &&
                   (p[2] == ' ' || p[2] == '\t')) {
            /* Texture coordinate: "vt u v" */
            const char *lp = p + 2;
            float u = forge_obj__parse_float(&lp);
            float v = forge_obj__parse_float(&lp);
            texcoords[ti++] = vec2_create(u, v);

        } else if (p[0] == 'v' && p[1] == 'n' &&
                   (p[2] == ' ' || p[2] == '\t')) {
            /* Normal: "vn x y z" */
            const char *lp = p + 2;
            float x = forge_obj__parse_float(&lp);
            float y = forge_obj__parse_float(&lp);
            float z = forge_obj__parse_float(&lp);
            normals[ni++] = vec3_create(x, y, z);

        } else if (p[0] == 'f' && (p[1] == ' ' || p[1] == '\t')) {
            /* Face: "f v1/vt1/vn1 v2/vt2/vn2 v3/vt3/vn3 [v4/vt4/vn4]"
             *
             * We parse all vertex indices in the face, then triangulate:
             *   Triangle: (0, 1, 2)
             *   Quad:     (0, 1, 2) and (0, 2, 3)
             *   N-gon:    fan triangulation (0, i, i+1) for i in 1..N-2 */
            const char *lp = p + 1;
            ForgeObjFaceIndex face_indices[8];  /* max 8 verts per face */
            int face_count = 0;

            lp = forge_obj__skip_ws(lp);
            while (*lp && *lp != '\n' && *lp != '\r' && face_count < 8) {
                if (*lp >= '0' && *lp <= '9') {
                    face_indices[face_count++] = forge_obj__parse_face_index(&lp);
                } else {
                    lp++;
                }
            }

            /* Fan triangulation: generate (N-2) triangles from the polygon. */
            for (int f = 1; f + 1 < face_count; f++) {
                ForgeObjFaceIndex idx[3];
                idx[0] = face_indices[0];
                idx[1] = face_indices[f];
                idx[2] = face_indices[f + 1];

                for (int k = 0; k < 3; k++) {
                    ForgeObjVertex vert;
                    SDL_zero(vert);

                    /* Position (always present) */
                    if (idx[k].v >= 0 && idx[k].v < num_positions) {
                        vert.position = positions[idx[k].v];
                    }

                    /* Normal (optional) */
                    if (idx[k].vn >= 0 && idx[k].vn < num_normals && normals) {
                        vert.normal = normals[idx[k].vn];
                    }

                    /* Texture coordinate (optional) — flip V for OpenGL-style
                     * OBJ files where V=0 is at the bottom.  Most OBJ exporters
                     * use this convention, while GPUs expect V=0 at the top. */
                    if (idx[k].vt >= 0 && idx[k].vt < num_texcoords && texcoords) {
                        vert.uv = vec2_create(
                            texcoords[idx[k].vt].x,
                            1.0f - texcoords[idx[k].vt].y);
                    }

                    if (vi < total_verts) {
                        vertices[vi++] = vert;
                    }
                }
            }
        }

        p = forge_obj__next_line(p);
    }

    /* ── Clean up temporary data ──────────────────────────────────────── */
    SDL_free(positions);
    SDL_free(texcoords);
    SDL_free(normals);
    SDL_free(file_data);

    out_mesh->vertices     = vertices;
    out_mesh->vertex_count = vi;

    SDL_Log("OBJ loaded: %u vertices (%u triangles)",
            vi, vi / 3);

    return true;
}

static void forge_obj_free(ForgeObjMesh *mesh)
{
    if (mesh) {
        SDL_free(mesh->vertices);
        mesh->vertices     = NULL;
        mesh->vertex_count = 0;
    }
}

#endif /* FORGE_OBJ_H */
