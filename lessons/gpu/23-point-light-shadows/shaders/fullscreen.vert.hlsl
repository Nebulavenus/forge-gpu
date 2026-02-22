/*
 * Fullscreen vertex shader — generates a fullscreen triangle from SV_VertexID.
 *
 * Draws a single triangle that covers the entire screen. No vertex buffer
 * needed — the vertex positions and UVs are computed from the vertex index.
 * Used by bloom downsample, upsample, and tone mapping passes.
 */

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

VSOutput main(uint id : SV_VertexID)
{
    VSOutput output;

    /* Triangle covering the full screen:
     *   id=0: (-1, -1) uv=(0, 1)
     *   id=1: ( 3, -1) uv=(2, 1)
     *   id=2: (-1,  3) uv=(0,-1)
     * The GPU clips to the viewport, so the oversized triangle works. */
    float2 pos = float2((id == 1) ? 3.0 : -1.0,
                        (id == 2) ? 3.0 : -1.0);
    output.pos = float4(pos, 0.0, 1.0);

    /* UV: map NDC [-1,1] to [0,1], flip Y for texture coordinates */
    output.uv = float2(pos.x * 0.5 + 0.5, 1.0 - (pos.y * 0.5 + 0.5));
    return output;
}
