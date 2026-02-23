/*
 * shared_params.h -- Shared atmosphere parameters (C mirror)
 *
 * This file mirrors atmosphere_params.hlsli from GPU Lesson 26 line-by-line.
 * Both the C host code and the HLSL shaders use the same constants, defined
 * once in a shared header.  If you update one, update the other.
 *
 * In C:    #include "shared_params.h"
 * In HLSL: #include "atmosphere_params.hlsli"
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef SHARED_PARAMS_H
#define SHARED_PARAMS_H

/* Horizon fade parameters for earth shadow smoothing.
 * Smooths the terminator (shadow boundary) around each sample point's
 * local horizon: saturate(cos_zenith * SCALE + BIAS).
 * Lower BIAS darkens the sky sooner as the sun approaches the horizon. */
static const float HORIZON_FADE_SCALE = 10.0f;
static const float HORIZON_FADE_BIAS  = 0.1f;

#endif /* SHARED_PARAMS_H */
