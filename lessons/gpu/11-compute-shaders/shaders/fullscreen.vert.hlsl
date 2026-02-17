/*
 * fullscreen.vert.hlsl — Vertex shader that generates a fullscreen triangle.
 *
 * Uses SV_VertexID to produce three vertices that cover the entire screen.
 * No vertex buffer needed — the positions and UVs are computed from the
 * vertex index alone.
 *
 * The triangle extends beyond the screen bounds:
 *
 *       (-1, 3)
 *         *
 *         |\
 *         | \
 *   (-1,1)|__\_______(3,1)   <- top edge of screen
 *         |  |\      |
 *         |  | \ vis |        vis = visible portion (the screen)
 *         |  |  \ible|
 *   (-1,-1)__|___\___|(1,-1)  <- bottom edge of screen
 *            |    \  |
 *            |_____\*|
 *                  (3,-1)
 *
 * This is more efficient than a fullscreen quad (two triangles) because
 * it avoids the diagonal seam where the two triangles meet, and the GPU
 * only processes one triangle instead of two.
 *
 * Register layout (SDL3 GPU vertex shader — DXIL):
 *   No vertex inputs — everything derived from SV_VertexID.
 *   No uniform buffers — pure geometry generation.
 *
 * SPDX-License-Identifier: Zlib
 */

struct VSOutput
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(uint id : SV_VertexID)
{
    VSOutput output;

    /* Generate positions using bit manipulation of the vertex index:
     *   id=0: (0&1)*2-1 = -1,  (0&2)*2-1 = -1  → (-1, -1)
     *   id=1: (1&1)*2-1 =  1,  (1&2)*2-1 = -1  → ( 3, -1)  after *2+(-1)
     *   id=2: (2&1)*2-1 = -1,  (2&2)*2-1 =  1  → (-1,  3)  after *2+(-1)
     *
     * We multiply by 2 so the triangle overshoots the [-1,1] clip bounds,
     * and the rasterizer clips it to exactly fill the screen. */
    float2 pos;
    pos.x = (float)((id & 1u) << 1) - 1.0f;           /* -1 or 1 */
    pos.y = (float)((id & 2u))       - 1.0f;           /* -1 or 1 */

    /* Scale to overshoot: -1 stays -1, 1 becomes 3 */
    output.position = float4(pos.x * 2.0f + 1.0f,      /* -1, 3, -1 */
                             pos.y * 2.0f + 1.0f,      /* -1, -1, 3 */
                             0.0f, 1.0f);

    /* UVs: map clip space [-1,1] to texture space [0,1].
     * Only the visible portion [-1,1] matters; the overshot parts
     * are clipped away before the fragment shader runs. */
    output.uv = float2((output.position.x + 1.0f) * 0.5f,
                        1.0f - (output.position.y + 1.0f) * 0.5f);

    return output;
}
