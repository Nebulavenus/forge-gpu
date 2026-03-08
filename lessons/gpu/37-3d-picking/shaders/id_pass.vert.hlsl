/*
 * ID pass vertex shader — simple MVP transform for color-ID picking.
 *
 * Each object in the scene is rendered with a unique flat color (the "ID
 * color") into an offscreen render target.  After rendering, the CPU reads
 * back the pixel under the cursor to determine which object was clicked.
 *
 * This shader only needs to project vertices to clip space — no lighting,
 * no shadow coordinates, no world-space outputs.  However, it must accept
 * the same vertex layout as the scene shader (position + normal) because
 * both pipelines share the same vertex buffer bindings.  The normal
 * attribute is declared but unused.
 *
 * CRITICAL: The output is ONLY SV_Position — no TEXCOORD outputs.  The
 * id_pass fragment shader accepts only SV_Position as input.  Adding
 * extra outputs here would cause a D3D12 I/O mismatch error.
 *
 * Vertex attributes:
 *   TEXCOORD0 -> float3 position  (location 0)
 *   TEXCOORD1 -> float3 normal    (location 1, unused but must match layout)
 *
 * Uniform buffers:
 *   register(b0, space1) -> slot 0: MVP matrix (64 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer IdVertUniforms : register(b0, space1)
{
    column_major float4x4 mvp;   /* model-view-projection matrix */
};

struct VSInput
{
    float3 position : TEXCOORD0;   /* vertex attribute location 0 */
    float3 normal   : TEXCOORD1;   /* unused — must match vertex layout */
};

float4 main(VSInput input) : SV_Position
{
    return mul(mvp, float4(input.position, 1.0));
}
