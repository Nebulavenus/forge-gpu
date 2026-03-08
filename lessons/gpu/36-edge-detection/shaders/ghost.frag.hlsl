/*
 * Ghost (X-ray) fragment shader — Fresnel rim-light effect.
 *
 * Computes a view-dependent rim glow using the Fresnel approximation:
 * surfaces facing away from the camera glow brighter, creating a
 * translucent silhouette effect for occluded geometry.
 *
 * The stencil buffer (written by xray_mark) ensures this shader only
 * runs on fragments that are behind other geometry.
 *
 * Uniform buffers:
 *   register(b0, space3) -> slot 0: ghost color and intensity (32 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer GhostFragUniforms : register(b0, space3)
{
    float3 ghost_color;      /* RGB tint for the rim glow                   */
    float  ghost_power;      /* Fresnel exponent — higher = thinner rim     */
    float  ghost_brightness; /* overall intensity multiplier                */
    float3 _pad0;
};

struct PSInput
{
    float4 clip_pos   : SV_Position;
    float3 normal_eye : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target0
{
    /* Compute how much the surface faces the camera in view space.
     * In view space the camera looks down -Z, so the forward vector is (0,0,1).
     * The facing factor is 1.0 when the normal points directly at the camera
     * and 0.0 when it is perpendicular (edge-on). */
    float3 N = normalize(input.normal_eye);
    float facing = abs(dot(N, float3(0.0, 0.0, 1.0)));

    /* Fresnel rim factor — edges glow brightest, facing surfaces are transparent. */
    float rim = pow(1.0 - facing, ghost_power);

    /* Output with premultiplied alpha for additive blending. */
    float3 color = ghost_color * rim * ghost_brightness;

    return float4(color, rim);
}
