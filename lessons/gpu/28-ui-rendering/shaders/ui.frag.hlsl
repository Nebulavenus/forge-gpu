/*
 * ui.frag.hlsl -- UI fragment shader for Lesson 28 (UI Rendering)
 *
 * Samples the font atlas and multiplies the result with the per-vertex
 * color to produce the final fragment output.
 *
 * The atlas is a single-channel R8_UNORM texture.  When the GPU samples
 * it, the return value is (r, 0, 0, 1) -- the alpha coverage is stored
 * in the red channel.  We extract .r and multiply it into the vertex
 * color's alpha to control per-pixel opacity.
 *
 * sRGB color handling:
 *   UI theme colors are authored as sRGB values (hex codes like #1a1a2e
 *   divided by 255).  The swapchain framebuffer is typically an sRGB
 *   format (e.g. B8G8R8A8_SRGB), which applies the linear-to-sRGB OETF
 *   on write.  To avoid double gamma encoding, the shader must linearize
 *   the sRGB vertex colors before output using the IEC 61966-2-1 EOTF
 *   (piecewise: linear segment below 0.04045, gamma 2.4 above).  The
 *   framebuffer's automatic encoding then recovers the original sRGB
 *   values on screen.  Alpha is not affected by sRGB.
 *
 * White-pixel technique:
 *   The font atlas contains a small 2x2 region of solid white pixels
 *   (coverage = 1.0) at a known UV rect (ForgeUiFontAtlas.white_uv).
 *   Solid-colored rectangles (buttons, slider tracks, panel backgrounds,
 *   checkboxes) set their UVs to point at this white region.  Because
 *   the sampled coverage is 1.0, the fragment alpha equals the vertex
 *   alpha unchanged -- so the vertex color controls both tint and
 *   transparency.  This avoids needing a separate shader or pipeline
 *   for solid geometry; everything goes through one draw call.
 *
 * Fragment samplers:
 *   register(t0, space2) -> atlas texture (R8_UNORM single-channel)
 *   register(s0, space2) -> atlas sampler (linear, clamp-to-edge)
 *
 * Fragment uniforms: none
 *
 * SPDX-License-Identifier: Zlib
 */

/* Font atlas texture -- single-channel (R8_UNORM) alpha coverage.
 * Bound at fragment sampler slot 0 in the SDL GPU pipeline.
 * Using R8 instead of RGBA8 saves 4x GPU memory and matches the
 * atlas pixel data format (one byte per pixel, no expansion needed).   */
Texture2D    atlas_tex : register(t0, space2);

/* Atlas sampler -- linear filtering smooths glyph edges at non-integer
 * positions; clamp-to-edge prevents sampling outside the atlas bounds
 * from pulling in garbage texels or bleeding between packed glyphs.    */
SamplerState atlas_smp : register(s0, space2);

/* Input from the rasterizer -- interpolated values from the vertex
 * shader output.  SV_Position is the screen-space fragment coordinate
 * (not used in this shader but required by the pipeline stage).        */
struct PSInput
{
    float4 position : SV_Position; /* screen-space frag coord (unused)  */
    float2 uv       : TEXCOORD0;   /* interpolated atlas UV coordinate  */
    float4 color    : TEXCOORD1;   /* interpolated per-vertex RGBA color*/
};

/* sRGB EOTF (IEC 61966-2-1): convert a single sRGB-encoded channel to
 * linear light.  The piecewise function has a linear segment near zero
 * (slope 1/12.92) to avoid an infinite derivative at the origin, and a
 * gamma-2.4 curve above the 0.04045 threshold.                         */
float srgb_to_linear(float c)
{
    return (c <= 0.04045) ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4);
}

float4 main(PSInput input) : SV_Target0
{
    /* Sample the single-channel atlas.  For R8_UNORM textures the GPU
     * returns (r, 0, 0, 1).  The .r component holds the glyph's alpha
     * coverage: 1.0 inside the glyph shape, 0.0 outside, and fractional
     * values along anti-aliased edges.                                  */
    float coverage = atlas_tex.Sample(atlas_smp, input.uv).r;

    /* Linearize the sRGB vertex color so that the framebuffer's
     * automatic linear-to-sRGB encoding on write recovers the original
     * authored sRGB values.  Without this, sRGB colors would be
     * double-encoded (sRGB → sRGB) producing washed-out output.
     * Alpha is unaffected — sRGB conversion applies only to RGB.        */
    float4 result;
    result.r = srgb_to_linear(input.color.r);
    result.g = srgb_to_linear(input.color.g);
    result.b = srgb_to_linear(input.color.b);
    result.a = input.color.a;

    /* Multiply the linearized alpha by the atlas coverage:
     *   - Text glyphs: coverage varies 0..1 along edges, producing
     *     smooth anti-aliased text when combined with alpha blending.
     *   - Solid rects: UVs point to the white-pixel region where
     *     coverage = 1.0, so result.a equals input.color.a unchanged.
     * The RGB channels are not modified -- the atlas only controls
     * opacity, not color.  Color comes entirely from the vertex data.   */
    result.a *= coverage;

    return result;
}
