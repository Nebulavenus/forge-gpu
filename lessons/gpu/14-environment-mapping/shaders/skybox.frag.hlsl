/*
 * skybox.frag.hlsl — Fragment shader for skybox cube
 *
 * Samples a cube map texture using the interpolated direction from the
 * vertex shader.  The direction vector does not need to be normalized —
 * TextureCube.Sample() uses only the direction, not the magnitude.
 *
 * SPDX-License-Identifier: Zlib
 */

TextureCube  skybox_tex : register(t0, space2);   /* Cube map texture */
SamplerState smp        : register(s0, space2);   /* Linear sampler */

struct PSInput
{
    float4 clip_pos  : SV_Position;  /* Not used in fragment shader */
    float3 direction : TEXCOORD0;    /* Interpolated cube map direction */
};

float4 main(PSInput input) : SV_Target
{
    /* Sample the cube map along the interpolated direction.
     * The GPU selects the correct face and UV automatically. */
    return skybox_tex.Sample(smp, input.direction);
}
