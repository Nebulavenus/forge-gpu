/*
 * atmosphere_params.hlsli -- Shared atmosphere tuning parameters
 *
 * Included by both sky.frag.hlsl and multiscatter_lut.comp.hlsl so that
 * constants only need to be defined once.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef ATMOSPHERE_PARAMS_HLSLI
#define ATMOSPHERE_PARAMS_HLSLI

/* Horizon fade parameters for earth shadow smoothing.
 * Smooths the terminator (shadow boundary) around each sample point's
 * local horizon: saturate(cos_zenith * SCALE + BIAS).
 * Lower BIAS darkens the sky sooner as the sun approaches the horizon. */
static const float HORIZON_FADE_SCALE = 10.0;
static const float HORIZON_FADE_BIAS  = 0.1;

#endif /* ATMOSPHERE_PARAMS_HLSLI */
