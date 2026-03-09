/*
 * debug_box.frag.hlsl — Debug view fragment shader
 *
 * Colors each box green (visible to main camera) or red (culled) based on
 * the visibility flags written by the frustum cull compute shader. Applies
 * simple directional lighting for shape visibility.
 *
 * Read-only storage buffer:
 *   register(t0, space2) -> StructuredBuffer<uint> visibility flags
 *
 * SPDX-License-Identifier: Zlib
 */

StructuredBuffer<uint> visibility : register(t0, space2);

struct PSInput {
    float4 clip_pos  : SV_Position;
    float3 world_nrm : TEXCOORD0;
    nointerpolation uint object_id : TEXCOORD1;
};

float4 main(PSInput input) : SV_Target {
    float3 n = normalize(input.world_nrm);
    float light = max(dot(n, normalize(float3(0.6, 1.0, 0.4))), 0.0) * 0.7 + 0.3;

    bool visible = (visibility[input.object_id] != 0);
    float3 base_color = visible ? float3(0.2, 0.8, 0.2) : float3(0.8, 0.2, 0.2);

    return float4(base_color * light, 1.0);
}
