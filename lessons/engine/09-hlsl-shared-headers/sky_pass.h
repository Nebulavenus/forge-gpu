/*
 * sky_pass.h -- Public interface for the sky pass module
 *
 * This header declares functions that sky_pass.c implements.  Both main.c
 * and sky_pass.c include shared_params.h, mirroring how sky.frag.hlsl and
 * multiscatter_lut.comp.hlsl both include atmosphere_params.hlsli.
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef SKY_PASS_H
#define SKY_PASS_H

/* Apply the horizon fade formula: saturate(cos_zenith * SCALE + BIAS).
 * Returns a value in [0, 1] indicating how much sunlight reaches
 * the sample point (1 = fully lit, 0 = in earth shadow). */
float sky_pass_horizon_fade(float cos_zenith);

/* Print the shared parameters used by the sky pass.
 * Demonstrates that sky_pass.c reads the same constants as main.c. */
void sky_pass_print_params(void);

#endif /* SKY_PASS_H */
