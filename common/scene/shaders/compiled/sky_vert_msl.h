/* Auto-generated from sky.vert.msl -- do not edit by hand. */
static const char sky_vert_msl[] =
    "#include <metal_stdlib>\n"
    "#include <simd/simd.h>\n"
    "\n"
    "using namespace metal;\n"
    "\n"
    "struct main0_out\n"
    "{\n"
    "    float2 out_var_TEXCOORD0 [[user(locn0)]];\n"
    "    float4 gl_Position [[position]];\n"
    "};\n"
    "\n"
    "vertex main0_out main0(uint gl_VertexIndex [[vertex_id]])\n"
    "{\n"
    "    main0_out out = {};\n"
    "    float2 _30 = float2(float((gl_VertexIndex << 1u) & 2u), float(gl_VertexIndex & 2u));\n"
    "    out.gl_Position = float4((_30 * float2(2.0, -2.0)) + float2(-1.0, 1.0), 0.99989998340606689453125, 1.0);\n"
    "    out.out_var_TEXCOORD0 = _30;\n"
    "    return out;\n"
    "}\n"
    "\n"
;
static const unsigned int sky_vert_msl_size = sizeof(sky_vert_msl) - 1;
