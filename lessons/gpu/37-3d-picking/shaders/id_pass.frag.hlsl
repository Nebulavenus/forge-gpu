/*
 * ID pass fragment shader — flat object-ID color for color-based 3D picking.
 *
 * Every object in the scene is assigned a unique color (e.g., (1,0,0) for
 * object 0, (0,1,0) for object 1).  This shader outputs that color with no
 * lighting, shading, or blending — just a flat, unambiguous identifier.
 *
 * After the ID pass renders the entire scene, the CPU reads back the single
 * pixel under the mouse cursor.  The color value maps directly to an object
 * index, enabling accurate 3D picking without raycasting or bounding-box
 * intersection tests.
 *
 * Input: Only SV_Position (from the rasterizer).  No TEXCOORD inputs —
 * this must match the id_pass vertex shader which outputs only SV_Position.
 *
 * Uniform buffers:
 *   register(b0, space3) -> slot 0: object ID color (16 bytes)
 *
 * SPDX-License-Identifier: Zlib
 */

cbuffer IdFragUniforms : register(b0, space3)
{
    float4 id_color;   /* unique flat color identifying this object */
};

float4 main(float4 pos : SV_Position) : SV_Target
{
    return id_color;
}
