/*
 * Shadow fragment shader — depth-only pass.
 *
 * The GPU writes depth automatically from SV_Position.z. No color
 * output is needed — we only need the depth buffer for shadow testing.
 *
 * SPDX-License-Identifier: Zlib
 */

void main()
{
    /* Hardware depth write — nothing to do here. */
}
