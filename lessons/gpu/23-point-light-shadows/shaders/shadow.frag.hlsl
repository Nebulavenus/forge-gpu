/*
 * Shadow fragment shader — outputs linear depth to an R32_FLOAT cube map face.
 *
 * Computes the distance from the light to the fragment in world space,
 * then normalizes it by the far plane to produce a [0, 1] value. This
 * linear depth is stored as the red channel of the R32_FLOAT color target.
 *
 * Why linear depth instead of hardware depth:
 *   Hardware depth (z/w) is non-linear — precision concentrates near the
 *   camera (the light, in this case) and becomes coarse far away. By storing
 *   distance/far_plane explicitly, we get uniform precision across the
 *   entire shadow range, which produces more consistent shadow comparisons.
 */

cbuffer FragUniforms : register(b0, space3)
{
    float3 light_pos;  /* world-space light position */
    float  far_plane;  /* shadow far plane distance  */
};

float4 main(float4 clip_pos : SV_Position,
            float3 world_pos : TEXCOORD0) : SV_Target
{
    float dist = length(world_pos - light_pos);
    return float4(dist / far_plane, 0.0, 0.0, 1.0);
}
