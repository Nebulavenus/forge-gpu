/*
 * sky_pass.c -- Second translation unit that includes shared_params.h
 *
 * This file parallels how multiscatter_lut.comp.hlsl includes
 * atmosphere_params.hlsli.  Both sky_pass.c and main.c include
 * shared_params.h, proving that the same constants are visible
 * everywhere — just like two HLSL shaders including the same .hlsli.
 *
 * SPDX-License-Identifier: Zlib
 */

#include <SDL3/SDL.h>
#include "shared_params.h"  /* HORIZON_FADE_SCALE, HORIZON_FADE_BIAS */
#include "sky_pass.h"

float sky_pass_horizon_fade(float cos_zenith)
{
    /* saturate(cos_zenith * SCALE + BIAS) — clamp result to [0, 1].
     * This is the same formula used in sky.frag.hlsl. */
    float fade = cos_zenith * HORIZON_FADE_SCALE + HORIZON_FADE_BIAS;
    if (fade < 0.0f) fade = 0.0f;
    if (fade > 1.0f) fade = 1.0f;
    return fade;
}

void sky_pass_print_params(void)
{
    SDL_Log("  sky_pass.c sees HORIZON_FADE_SCALE = %.1f", HORIZON_FADE_SCALE);
    SDL_Log("  sky_pass.c sees HORIZON_FADE_BIAS  = %.1f", HORIZON_FADE_BIAS);
}
