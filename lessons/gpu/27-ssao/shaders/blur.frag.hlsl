/*
 * Blur fragment shader — 4x4 box blur for SSAO.
 *
 * Averages the raw SSAO output over a 4x4 texel region. This matches
 * the 4x4 noise texture tile size, effectively smoothing out one
 * complete rotation pattern per blur sample.
 *
 * A box blur is used instead of a Gaussian because:
 *   - The noise pattern repeats every 4 texels in both directions
 *   - Averaging exactly one tile removes the tiling pattern
 *   - The result is a smooth, artifact-free AO factor
 *
 * SPDX-License-Identifier: Zlib
 */

/* Raw SSAO texture (slot 0). */
Texture2D    ssao_tex : register(t0, space2);
SamplerState ssao_smp : register(s0, space2);

cbuffer BlurParams : register(b0, space3)
{
    float2 texel_size; /* 1/width, 1/height of the SSAO texture */
    float2 _pad;
};

float4 main(float4 clip_pos : SV_Position,
            float2 uv       : TEXCOORD0) : SV_Target
{
    float result = 0.0;

    /* 4x4 box blur centered on the current texel.
     * Offsets range from -1.5 to +1.5 to cover 4 texels in each axis. */
    for (int x = -2; x < 2; x++)
    {
        for (int y = -2; y < 2; y++)
        {
            float2 offset = float2((float)x + 0.5, (float)y + 0.5) * texel_size;
            result += ssao_tex.Sample(ssao_smp, uv + offset).r;
        }
    }

    result /= 16.0; /* 4x4 = 16 samples */
    return float4(result, result, result, 1.0);
}
