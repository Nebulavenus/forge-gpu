/*
 * instanced.vert.hlsl — Vertex shader for instanced rendering
 *
 * The key difference from previous lessons: the model matrix is NOT in a
 * uniform buffer.  Instead, it arrives as a per-instance vertex attribute
 * (VERTEXINPUTRATE_INSTANCE).  This means the GPU reads a different model
 * matrix for each instance, allowing one draw call to place many objects
 * at different positions/rotations/scales.
 *
 * The model matrix is passed as 4 separate float4 columns (TEXCOORD3-6)
 * because vertex attributes are limited to float4 (16 bytes) each.
 * We reconstruct the full 4x4 matrix inside the shader.
 *
 * The uniform buffer holds only the View-Projection matrix (64 bytes),
 * shared by ALL instances in a single draw call.
 *
 * Normal transformation uses the adjugate transpose (same method as L10/L12)
 * to correctly handle non-uniform scaling.
 *
 * Per-vertex attributes (slot 0, VERTEX input rate):
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1)
 *   TEXCOORD2 -> float2 uv        (location 2)
 *
 * Per-instance attributes (slot 1, INSTANCE input rate):
 *   TEXCOORD3 -> float4 model_c0  (location 3) — model matrix column 0
 *   TEXCOORD4 -> float4 model_c1  (location 4) — model matrix column 1
 *   TEXCOORD5 -> float4 model_c2  (location 5) — model matrix column 2
 *   TEXCOORD6 -> float4 model_c3  (location 6) — model matrix column 3
 *
 * Uniform buffer:
 *   register(b0, space1) -> vertex shader uniform slot 0
 *   Contains the combined View-Projection matrix (64 bytes).
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer VertUniforms : register(b0, space1)
{
    column_major float4x4 vp;   /* combined view-projection matrix */
};

struct VSInput
{
    /* Per-vertex data (from the mesh vertex buffer, slot 0) */
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
    float3 normal   : TEXCOORD1;   /* vertex attribute location 1 */
    float2 uv       : TEXCOORD2;   /* vertex attribute location 2 */

    /* Per-instance data (from the instance buffer, slot 1).
     * Each float4 is one column of the 4x4 model matrix.
     * Our math library (forge_math.h) stores mat4 as 4 contiguous vec4
     * columns, so the memory layout matches directly. */
    float4 model_c0 : TEXCOORD3;   /* vertex attribute location 3 */
    float4 model_c1 : TEXCOORD4;   /* vertex attribute location 4 */
    float4 model_c2 : TEXCOORD5;   /* vertex attribute location 5 */
    float4 model_c3 : TEXCOORD6;   /* vertex attribute location 6 */
};

struct VSOutput
{
    float4 clip_pos   : SV_Position; /* clip-space position for rasterizer */
    float2 uv         : TEXCOORD0;   /* texture coordinates               */
    float3 world_norm : TEXCOORD1;   /* world-space normal (not normalized)*/
    float3 world_pos  : TEXCOORD2;   /* world-space position               */
};

VSOutput main(VSInput input)
{
    VSOutput output;

    /* Reconstruct the 4x4 model matrix from the 4 per-instance columns.
     * float4x4(c0, c1, c2, c3) in HLSL treats arguments as ROWS, but
     * our data is stored column-major.  We transpose to get the correct
     * column-major matrix.  This matches how forge_math.h stores mat4:
     * columns[0] through columns[3] laid out contiguously in memory. */
    float4x4 model = transpose(float4x4(
        input.model_c0,
        input.model_c1,
        input.model_c2,
        input.model_c3
    ));

    /* World-space position — multiply the model matrix by the vertex position.
     * Each instance gets its own model matrix, placing it at a unique
     * position/rotation/scale in the world. */
    float4 wp = mul(model, float4(input.position, 1.0));
    output.world_pos = wp.xyz;

    /* Clip-space position — apply the shared VP matrix. */
    output.clip_pos = mul(vp, wp);

    /* World-space normal — transform by the adjugate transpose of the
     * model matrix's upper-left 3x3 (same method as L10/L12).
     * This preserves perpendicularity even under non-uniform scale.
     *
     * The adjugate transpose rows are cross products of the matrix rows:
     *   adj_t row 0 = cross(row 1, row 2)
     *   adj_t row 1 = cross(row 2, row 0)
     *   adj_t row 2 = cross(row 0, row 1)
     *
     * NOT normalized here — the fragment shader normalizes per-pixel. */
    float3x3 m = (float3x3)model;
    float3x3 adj_t;
    adj_t[0] = cross(m[1], m[2]);
    adj_t[1] = cross(m[2], m[0]);
    adj_t[2] = cross(m[0], m[1]);
    output.world_norm = mul(adj_t, input.normal);

    /* Pass UVs through for texture sampling. */
    output.uv = input.uv;

    return output;
}
