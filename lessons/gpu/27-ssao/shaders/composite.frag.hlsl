/*
 * Composite fragment shader — combines lit scene color with the SSAO factor.
 *
 * Three display modes controlled by a uniform:
 *   mode 0: AO only — white surface modulated by the AO factor
 *   mode 1: Full render with AO applied (color * AO)
 *   mode 2: Full render without AO (comparison view)
 *
 * Optionally applies Interleaved Gradient Noise dithering (Jimenez 2014)
 * to the output to reduce 8-bit banding in the AO gradients.
 *
 * SPDX-License-Identifier: Zlib
 */

/* Scene color from the geometry pass (slot 0). */
Texture2D    color_tex : register(t0, space2);
SamplerState color_smp : register(s0, space2);

/* Blurred SSAO factor (slot 1). */
Texture2D    ao_tex    : register(t1, space2);
SamplerState ao_smp    : register(s1, space2);

cbuffer CompositeParams : register(b0, space3)
{
    int  display_mode;   /* 0=AO only, 1=with AO, 2=no AO */
    int  use_dither;     /* 1 = apply IGN dithering         */
    float2 _pad;
};

/* ── Interleaved Gradient Noise (Jimenez 2014) ──────────────────────── */

float ign(float2 screen_pos)
{
    float3 ign_coeffs = float3(0.06711056, 0.00583715, 52.9829189);
    return frac(ign_coeffs.z * frac(dot(screen_pos, ign_coeffs.xy)));
}

float4 main(float4 clip_pos : SV_Position,
            float2 uv       : TEXCOORD0) : SV_Target
{
    float3 scene_color = color_tex.Sample(color_smp, uv).rgb;
    float  ao          = ao_tex.Sample(ao_smp, uv).r;

    float3 final_color;

    if (display_mode == 0)
    {
        /* AO only — show occlusion on a white background. */
        final_color = float3(ao, ao, ao);
    }
    else if (display_mode == 1)
    {
        /* Full render with AO applied. */
        final_color = scene_color * ao;
    }
    else
    {
        /* Full render without AO — comparison view. */
        final_color = scene_color;
    }

    /* Optional IGN dithering to reduce 8-bit banding in AO gradients. */
    if (use_dither)
    {
        float dither = (ign(clip_pos.xy) - 0.5) / 255.0;
        final_color += dither;
    }

    return float4(final_color, 1.0);
}
