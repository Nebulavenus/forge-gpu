/*
 * Emissive fragment shader — constant HDR color output.
 *
 * Used for the visible point light spheres. Outputs a high-intensity
 * color that drives the bloom effect.
 */

cbuffer FragUniforms : register(b0, space3)
{
    float3 emission_color; /* HDR emission RGB */
    float  _pad;           /* pad to 16 bytes  */
};

float4 main(float4 clip_pos : SV_Position,
            float3 world_pos : TEXCOORD0,
            float3 world_nrm : TEXCOORD1,
            float2 uv        : TEXCOORD2) : SV_Target
{
    return float4(emission_color, 1.0);
}
