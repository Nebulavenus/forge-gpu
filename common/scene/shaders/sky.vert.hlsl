/*
 * Sky vertex shader — fullscreen triangle from SV_VertexID.
 *
 * Part of forge_scene.h — renders a vertical gradient sky background.
 * No vertex buffer needed; three vertices generate a screen-filling triangle.
 *
 * SPDX-License-Identifier: Zlib
 */

struct VSOutput {
    float4 clip_pos : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOutput main(uint vertex_id : SV_VertexID) {
    VSOutput output;

    /* Generate fullscreen triangle UVs from vertex ID.
     * ID 0 -> (0, 0), ID 1 -> (2, 0), ID 2 -> (0, 2) */
    float2 uv = float2((vertex_id << 1) & 2, vertex_id & 2);

    /* Map [0,1] UV to [-1,1] clip space.  z = 0.9999 renders behind
     * all scene geometry.  Flip Y so uv.y=0 is screen top. */
    output.clip_pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0),
                             0.9999, 1.0);
    output.uv = uv;

    return output;
}
