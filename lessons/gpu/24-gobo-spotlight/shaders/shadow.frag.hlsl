/*
 * Shadow fragment shader — depth-only pass.
 *
 * For a spotlight shadow map we use hardware depth (D32_FLOAT) with no
 * color target. The GPU writes depth automatically from SV_Position.z,
 * so this shader is essentially a no-op. We still need it to create
 * a valid pipeline.
 */

void main()
{
    /* Hardware depth write — nothing to do here. */
}
