/* Auto-generated from shadow.frag.msl -- do not edit by hand. */
static const char shadow_frag_msl[] =
    "#include <metal_stdlib>\n"
    "#include <simd/simd.h>\n"
    "\n"
    "using namespace metal;\n"
    "\n"
    "struct main0_out\n"
    "{\n"
    "    float4 out_var_SV_Target [[color(0)]];\n"
    "};\n"
    "\n"
    "fragment main0_out main0()\n"
    "{\n"
    "    main0_out out = {};\n"
    "    out.out_var_SV_Target = float4(0.0);\n"
    "    return out;\n"
    "}\n"
    "\n"
;
static const unsigned int shadow_frag_msl_size = sizeof(shadow_frag_msl) - 1;
