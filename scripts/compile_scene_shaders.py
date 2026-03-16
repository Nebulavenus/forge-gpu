#!/usr/bin/env python
"""
compile_scene_shaders.py — Compile forge_scene.h HLSL shaders to SPIRV + DXIL + MSL.

Compiles all .vert.hlsl, .frag.hlsl, and .comp.hlsl files in common/scene/shaders/
to SPIRV and DXIL bytecode, then cross-compiles SPIRV to MSL via spirv-cross.
Generates C byte-array headers (SPIRV, DXIL) and C string headers (MSL) for embedding.

Usage:
    python scripts/compile_scene_shaders.py              # all scene shaders
    python scripts/compile_scene_shaders.py scene_model   # by name fragment
    python scripts/compile_scene_shaders.py --dxc PATH    # override dxc path
    python scripts/compile_scene_shaders.py -v            # verbose output

The script auto-detects dxc and spirv-cross from:
  1. --dxc / --spirv-cross command-line flags
  2. VULKAN_SDK environment variable (for dxc with -spirv support)
  3. System PATH
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SHADER_DIR = os.path.join(REPO_ROOT, "common", "scene", "shaders")
COMPILED_DIR = os.path.join(SHADER_DIR, "compiled")

# Shader stage to DXC target profile mapping
STAGE_PROFILES = {
    ".vert.hlsl": "vs_6_0",
    ".frag.hlsl": "ps_6_0",
    ".comp.hlsl": "cs_6_0",
}


def find_dxc():
    """Auto-detect dxc compiler location."""
    vulkan_sdk = os.environ.get("VULKAN_SDK")
    if vulkan_sdk:
        candidates = [
            os.path.join(vulkan_sdk, "Bin", "dxc.exe"),
            os.path.join(vulkan_sdk, "bin", "dxc"),
        ]
        for dxc_path in candidates:
            if os.path.isfile(dxc_path):
                return dxc_path

    dxc_path = shutil.which("dxc")
    if dxc_path:
        return dxc_path

    return None


def find_spirv_cross():
    """Auto-detect spirv-cross location."""
    sc = shutil.which("spirv-cross")
    if sc:
        return sc
    return None


def get_stage_suffix(shader_path):
    """Return the stage suffix (.vert.hlsl, .frag.hlsl, etc.) for a shader."""
    for suffix in STAGE_PROFILES:
        if shader_path.endswith(suffix):
            return suffix
    return None


def find_shaders(query=None):
    """Find HLSL shader files in common/scene/shaders/, optionally filtered."""
    if not os.path.isdir(SHADER_DIR):
        return []

    shaders = []
    for filename in sorted(os.listdir(SHADER_DIR)):
        if get_stage_suffix(filename) is None:
            continue
        if query and query.lower() not in filename.lower():
            continue
        shaders.append(os.path.join(SHADER_DIR, filename))
    return shaders


def generate_header(binary_path, array_name, output_path):
    """Convert a binary file to a C byte-array header."""
    with open(binary_path, "rb") as f:
        data = f.read()

    basename = os.path.basename(binary_path)
    tmp_path = output_path + ".tmp"

    with open(tmp_path, "w", newline="\n") as f:
        f.write(f"/* Auto-generated from {basename} -- do not edit by hand. */\n")
        f.write(f"static const unsigned char {array_name}[] = {{\n")
        for i in range(0, len(data), 12):
            chunk = data[i : i + 12]
            hex_values = ", ".join(f"0x{b:02x}" for b in chunk)
            f.write(f"    {hex_values},\n")
        f.write("};\n")
        f.write(
            f"static const unsigned int {array_name}_size = sizeof({array_name});\n"
        )
    os.replace(tmp_path, output_path)


def generate_msl_header(msl_path, var_name, output_path):
    """Convert an MSL source file to a C string literal header."""
    with open(msl_path) as f:
        msl_source = f.read()

    basename = os.path.basename(msl_path)
    tmp_path = output_path + ".tmp"

    with open(tmp_path, "w", newline="\n") as f:
        f.write(f"/* Auto-generated from {basename} -- do not edit by hand. */\n")
        f.write(f"static const char {var_name}[] =\n")
        for line in msl_source.splitlines():
            escaped = line.replace("\\", "\\\\").replace('"', '\\"')
            f.write(f'    "{escaped}\\n"\n')
        f.write(";\n")
        f.write(
            f"static const unsigned int {var_name}_size = sizeof({var_name}) - 1;\n"
        )
    os.replace(tmp_path, output_path)


def compile_shader(dxc_path, spirv_cross_path, shader_path, verbose=False):
    """Compile a single HLSL shader to SPIRV, DXIL, and MSL, then generate C headers."""
    suffix = get_stage_suffix(shader_path)
    if suffix is None:
        print(f"  Unknown shader stage: {os.path.basename(shader_path)}")
        return False

    profile = STAGE_PROFILES[suffix]
    basename = os.path.basename(shader_path[: -len(suffix)])
    stage = suffix.removeprefix(".").removesuffix(".hlsl")

    os.makedirs(COMPILED_DIR, exist_ok=True)

    spirv_out = os.path.join(COMPILED_DIR, f"{basename}.{stage}.spv")
    dxil_out = os.path.join(COMPILED_DIR, f"{basename}.{stage}.dxil")
    msl_out = os.path.join(COMPILED_DIR, f"{basename}.{stage}.msl")

    success = True

    # Compile SPIRV
    spirv_tmp = spirv_out + ".tmp"
    spirv_cmd = [
        dxc_path,
        "-spirv",
        "-I",
        SHADER_DIR,
        "-T",
        profile,
        "-E",
        "main",
        shader_path,
        "-Fo",
        spirv_tmp,
    ]
    if verbose:
        print(f"  $ {' '.join(spirv_cmd)}")
    result = subprocess.run(spirv_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  SPIRV compilation failed for {os.path.basename(shader_path)}:")
        print(f"    {result.stderr.strip()}")
        Path(spirv_tmp).unlink(missing_ok=True)
        success = False
    else:
        os.replace(spirv_tmp, spirv_out)
        array_name = f"{basename}_{stage}_spirv"
        header_path = os.path.join(COMPILED_DIR, f"{array_name}.h")
        generate_header(spirv_out, array_name, header_path)
        if verbose:
            size = os.path.getsize(spirv_out)
            print(f"  SPIRV: {size} bytes -> compiled/{array_name}.h")

    # Compile DXIL
    dxil_tmp = dxil_out + ".tmp"
    dxil_cmd = [
        dxc_path,
        "-I",
        SHADER_DIR,
        "-T",
        profile,
        "-E",
        "main",
        shader_path,
        "-Fo",
        dxil_tmp,
    ]
    if verbose:
        print(f"  $ {' '.join(dxil_cmd)}")
    result = subprocess.run(dxil_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  DXIL compilation failed for {os.path.basename(shader_path)}:")
        print(f"    {result.stderr.strip()}")
        Path(dxil_tmp).unlink(missing_ok=True)
        success = False
    else:
        os.replace(dxil_tmp, dxil_out)
        array_name = f"{basename}_{stage}_dxil"
        header_path = os.path.join(COMPILED_DIR, f"{array_name}.h")
        generate_header(dxil_out, array_name, header_path)
        if verbose:
            size = os.path.getsize(dxil_out)
            print(f"  DXIL:  {size} bytes -> compiled/{array_name}.h")

    # Cross-compile SPIRV → MSL (requires successful SPIRV compilation)
    if spirv_cross_path and os.path.isfile(spirv_out):
        msl_tmp = msl_out + ".tmp"
        msl_cmd = [
            spirv_cross_path,
            "--msl",
            "--msl-version",
            "20100",
            spirv_out,
            "--output",
            msl_tmp,
        ]
        if verbose:
            print(f"  $ {' '.join(msl_cmd)}")
        result = subprocess.run(msl_cmd, capture_output=True, text=True)
        if result.returncode != 0:
            print(
                f"  MSL cross-compilation failed for {os.path.basename(shader_path)}:"
            )
            print(f"    {result.stderr.strip()}")
            Path(msl_tmp).unlink(missing_ok=True)
            success = False
        else:
            os.replace(msl_tmp, msl_out)
            var_name = f"{basename}_{stage}_msl"
            header_path = os.path.join(COMPILED_DIR, f"{var_name}.h")
            generate_msl_header(msl_out, var_name, header_path)
            if verbose:
                size = os.path.getsize(msl_out)
                print(f"  MSL:   {size} bytes -> compiled/{var_name}.h")
    elif not spirv_cross_path:
        print("  MSL:   skipped (spirv-cross not found)")

    return success


def main():
    parser = argparse.ArgumentParser(
        description="Compile forge_scene.h HLSL shaders to SPIRV, DXIL, and MSL."
    )
    parser.add_argument(
        "shader",
        nargs="?",
        help="Shader name fragment to filter (e.g. 'scene_model', 'grid')",
    )
    parser.add_argument("--dxc", help="Path to dxc compiler (auto-detected if not set)")
    parser.add_argument(
        "--spirv-cross",
        dest="spirv_cross",
        help="Path to spirv-cross (auto-detected if not set)",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Show compilation commands"
    )
    args = parser.parse_args()

    dxc_path = args.dxc or find_dxc()
    if dxc_path is None:
        print("Could not find dxc compiler.")
        print("Set VULKAN_SDK environment variable or pass --dxc PATH.")
        return 1

    spirv_cross_path = args.spirv_cross or find_spirv_cross()
    if spirv_cross_path is None:
        print("Warning: spirv-cross not found — MSL shaders will not be generated.")
        print(
            "Install via: brew install spirv-cross (macOS), apt install spirv-cross (Linux),"
        )
        print("or the Vulkan SDK (all platforms).")

    print(f"Using dxc: {dxc_path}")
    if spirv_cross_path:
        print(f"Using spirv-cross: {spirv_cross_path}")

    shaders = find_shaders(args.shader)
    if not shaders:
        if args.shader:
            print(f"No scene shaders matching '{args.shader}' found.")
        else:
            print("No scene shaders found.")
        return 1

    print("\ncommon/scene/shaders/")

    total = 0
    failed = 0
    for shader in shaders:
        total += 1
        print(f"  {os.path.basename(shader)}")
        if not compile_shader(dxc_path, spirv_cross_path, shader, verbose=args.verbose):
            failed += 1

    print(f"\nCompiled {total - failed}/{total} shaders.")
    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
