/*
 * particle.vert.hlsl — Billboard vertex shader with vertex pulling
 *
 * No vertex buffer input.  Each particle is rendered as a camera-facing
 * quad (2 triangles, 6 vertices).  The vertex shader reads particle data
 * from a StructuredBuffer using SV_VertexID:
 *
 *   particle_index = SV_VertexID / 6
 *   corner_index   = SV_VertexID % 6
 *
 * The billboard quad is constructed by offsetting the particle's world
 * position along the camera's right and up vectors, scaled by the
 * particle's size.  Dead particles (lifetime <= 0) are collapsed to
 * degenerate triangles at the origin.
 *
 * UV coordinates index into a 4x4 texture atlas.  The atlas frame is
 * chosen based on the particle's age ratio (0 = newborn, 15 = dying).
 *
 * Register layout (SDL GPU vertex storage conventions):
 *   t0, space0 — StructuredBuffer<Particle>  (vertex storage buffer)
 *   b0, space1 — cbuffer BillboardUniforms   (uniform buffer)
 *
 * SPDX-License-Identifier: Zlib
 */

/* ── Particle data layout (must match compute shader) ─────────────────── */

struct Particle {
    float4 pos_lifetime;   /* xyz = position, w = lifetime remaining */
    float4 vel_size;       /* xyz = velocity, w = billboard size     */
    float4 color;          /* rgba                                   */
    float4 max_life_type;  /* x = max lifetime, y = type, z = orig size */
};

/* ── GPU resources ────────────────────────────────────────────────────── */

StructuredBuffer<Particle> particles : register(t0, space0);

cbuffer BillboardUniforms : register(b0, space1) {
    float4x4 view_proj;   /* camera view-projection matrix */
    float4   cam_right;   /* xyz = camera right vector     */
    float4   cam_up;      /* xyz = camera up vector        */
};

/* ── Atlas constants ──────────────────────────────────────────────────── */

#define ATLAS_CELLS    4       /* 4x4 grid = 16 frames */
#define ATLAS_INV_SIZE (1.0 / ATLAS_CELLS)  /* 0.25 */

/* ── Quad corner positions and UVs ────────────────────────────────────── */
/* Two triangles forming a quad, wound counter-clockwise.
 * Corner positions are in [-0.5, 0.5] range, centered at the particle. */

static const float2 CORNER_POS[6] = {
    float2(-0.5, -0.5),   /* tri 0: bottom-left  */
    float2( 0.5, -0.5),   /* tri 0: bottom-right */
    float2( 0.5,  0.5),   /* tri 0: top-right    */
    float2(-0.5, -0.5),   /* tri 1: bottom-left  */
    float2( 0.5,  0.5),   /* tri 1: top-right    */
    float2(-0.5,  0.5)    /* tri 1: top-left     */
};

/* UV coordinates within a single atlas cell [0,1] */
static const float2 CORNER_UV[6] = {
    float2(0.0, 1.0),     /* bottom-left  (V flipped: 1 = bottom) */
    float2(1.0, 1.0),     /* bottom-right */
    float2(1.0, 0.0),     /* top-right    */
    float2(0.0, 1.0),     /* bottom-left  */
    float2(1.0, 0.0),     /* top-right    */
    float2(0.0, 0.0)      /* top-left     */
};

/* ── Vertex output ────────────────────────────────────────────────────── */

struct VSOutput {
    float4 clip_pos : SV_Position;
    float4 color    : TEXCOORD0;
    float2 uv       : TEXCOORD1;
};

/* ── Main ─────────────────────────────────────────────────────────────── */

VSOutput main(uint vid : SV_VertexID) {
    VSOutput output;

    uint particle_idx = vid / 6;
    uint corner_idx   = vid % 6;

    Particle p = particles[particle_idx];

    /* Dead particles: collapse to degenerate triangle */
    if (p.pos_lifetime.w <= 0.0) {
        output.clip_pos = float4(0, 0, 0, 0);
        output.color    = float4(0, 0, 0, 0);
        output.uv       = float2(0, 0);
        return output;
    }

    /* ── Billboard quad expansion ─────────────────────────────── */
    float3 center = p.pos_lifetime.xyz;
    float  size   = p.vel_size.w;

    float2 corner = CORNER_POS[corner_idx];
    float3 offset = cam_right.xyz * (corner.x * size)
                  + cam_up.xyz    * (corner.y * size);
    float3 world_pos = center + offset;

    output.clip_pos = mul(view_proj, float4(world_pos, 1.0));

    /* ── Atlas UV calculation ─────────────────────────────────── */
    /* age_ratio: 0 = just born, 1 = about to die */
    float max_life  = max(p.max_life_type.x, 0.001);
    float age_ratio = 1.0 - (p.pos_lifetime.w / max_life);
    age_ratio = clamp(age_ratio, 0.0, 0.999);

    /* Map age ratio to atlas frame index (0..15) */
    uint frame = (uint)(age_ratio * (ATLAS_CELLS * ATLAS_CELLS));
    uint row = frame / ATLAS_CELLS;
    uint col = frame % ATLAS_CELLS;

    /* Offset UV into the correct atlas cell */
    float2 cell_origin = float2(col, row) * ATLAS_INV_SIZE;
    output.uv = cell_origin + CORNER_UV[corner_idx] * ATLAS_INV_SIZE;

    /* ── Pass color to fragment shader ────────────────────────── */
    output.color = p.color;

    return output;
}
