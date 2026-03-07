/*
 * Debug overlay fragment shader — samples a debug texture as a semi-transparent
 * overlay.
 *
 * Used to visualize the stencil buffer or other debug textures on screen.
 * Samples the bound texture and outputs it with reduced alpha so the scene
 * remains partially visible underneath.
 *
 * Fragment samplers (space2):
 *   slot 0 -> debug texture + linear sampler
 *
 * No uniform buffer — alpha is hardcoded for simplicity.
 *
 * Pipeline: debug_overlay (fullscreen triangle, alpha blending enabled)
 *
 * SPDX-License-Identifier: Zlib
 */

/* Debug texture (slot 0). */
Texture2D    debug_tex : register(t0, space2);
SamplerState debug_smp : register(s0, space2);

float4 main(float4 clip_pos : SV_Position,
             float2 uv       : TEXCOORD0) : SV_Target
{
    /* Sample the debug texture (typically a grayscale stencil visualization). */
    float4 texel = debug_tex.Sample(debug_smp, uv);

    /* Output with reduced alpha for semi-transparent overlay.
     * 0.7 keeps the debug info readable while the scene shows through. */
    return float4(texel.rgb, texel.a * 0.7);
}
