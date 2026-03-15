#!/usr/bin/env python
"""
compile_shaders.py — Compile HLSL shaders to SPIRV, DXIL, and MSL with embedded C headers.

Finds .vert.hlsl, .frag.hlsl, and .comp.hlsl files and compiles them using dxc,
then cross-compiles SPIRV to MSL via spirv-cross for Metal support.

Usage:
    python scripts/compile_shaders.py                    # all lessons
    python scripts/compile_shaders.py 02                 # lesson 02 only
    python scripts/compile_shaders.py first-triangle     # by name
    python scripts/compile_shaders.py --dxc PATH         # override dxc path

The script auto-detects dxc and spirv-cross from:
  1. --dxc / --spirv-cross command-line flags
  2. VULKAN_SDK environment variable (for SPIRV via -spirv)
  3. System PATH
"""

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LESSONS_DIR = os.path.join(REPO_ROOT, "lessons", "gpu")

# All lesson tracks that may contain shaders
LESSON_TRACKS = ["gpu", "physics", "ui", "math", "engine", "assets"]

# Shader stage to DXC target profile mapping
STAGE_PROFILES = {
    ".vert.hlsl": "vs_6_0",
    ".frag.hlsl": "ps_6_0",
    ".comp.hlsl": "cs_6_0",
}


def find_dxc():
    """Auto-detect dxc compiler location."""
    # Check VULKAN_SDK first (has -spirv support)
    vulkan_sdk = os.environ.get("VULKAN_SDK")
    if vulkan_sdk:
        # Windows: Bin/dxc.exe, Linux/macOS: bin/dxc
        candidates = [
            os.path.join(vulkan_sdk, "Bin", "dxc.exe"),
            os.path.join(vulkan_sdk, "bin", "dxc"),
        ]
        for dxc_path in candidates:
            if os.path.isfile(dxc_path):
                return dxc_path

    # Fall back to PATH
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


def find_lesson_dirs(query=None):
    """Find lesson directories across all tracks, optionally filtered by query.

    Supports queries like:
      - "16"                          -> lessons/gpu/16-blending
      - "physics/01-point-particles"  -> lessons/physics/01-point-particles
      - "point-particles"             -> lessons/physics/01-point-particles
      - None                          -> all lessons in all tracks
    """
    # If query contains a slash, treat the prefix as a track filter
    track_filter = None
    lesson_query = query
    if query and "/" in query:
        parts = query.split("/", 1)
        track_filter = parts[0].lower()
        lesson_query = parts[1]

    tracks = LESSON_TRACKS if track_filter is None else [track_filter]

    dirs = []
    for track in tracks:
        track_dir = os.path.join(REPO_ROOT, "lessons", track)
        if not os.path.isdir(track_dir):
            continue

        for entry in sorted(os.listdir(track_dir)):
            full = os.path.join(track_dir, entry)
            if not os.path.isdir(full) or not entry[0].isdigit():
                continue
            if lesson_query is None:
                dirs.append(full)
            else:
                q = lesson_query.lower()
                num = entry.split("-", 1)[0]
                if num == q.zfill(2) or q in entry.lower():
                    dirs.append(full)
    return dirs


def find_shaders(lesson_dir):
    """Find all HLSL shader files in a lesson's shaders/ directory."""
    shader_dir = os.path.join(lesson_dir, "shaders")
    if not os.path.isdir(shader_dir):
        return []

    shaders = []
    for filename in sorted(os.listdir(shader_dir)):
        for suffix in STAGE_PROFILES:
            if filename.endswith(suffix):
                shaders.append(os.path.join(shader_dir, filename))
                break
    return shaders


def get_stage_suffix(shader_path):
    """Return the stage suffix (.vert.hlsl or .frag.hlsl) for a shader."""
    for suffix in STAGE_PROFILES:
        if shader_path.endswith(suffix):
            return suffix
    return None


def compile_shader(dxc_path, spirv_cross_path, shader_path, verbose=False):
    """Compile a single HLSL shader to SPIRV, DXIL, and MSL, then generate C headers.

    Generated files (.spv, .dxil, .msl, and C headers) are placed in a compiled/
    subdirectory so that the shaders/ directory contains only HLSL source.
    """
    suffix = get_stage_suffix(shader_path)
    if suffix is None:
        print(f"  Unknown shader stage: {os.path.basename(shader_path)}")
        return False
    profile = STAGE_PROFILES[suffix]
    basename = os.path.basename(shader_path[: -len(suffix)])  # e.g. triangle
    shader_dir = os.path.dirname(shader_path)

    # Output compiled artifacts to a compiled/ subdirectory
    compiled_dir = os.path.join(shader_dir, "compiled")
    os.makedirs(compiled_dir, exist_ok=True)

    # Derive short stage name from the validated suffix
    stage = suffix.removeprefix(".").removesuffix(
        ".hlsl"
    )  # e.g. ".vert.hlsl" -> "vert"

    spirv_out = os.path.join(compiled_dir, f"{basename}.{stage}.spv")
    dxil_out = os.path.join(compiled_dir, f"{basename}.{stage}.dxil")
    msl_out = os.path.join(compiled_dir, f"{basename}.{stage}.msl")

    success = True

    # Compile SPIRV (using Vulkan SDK dxc with -spirv flag)
    spirv_cmd = [
        dxc_path,
        "-spirv",
        "-I",
        shader_dir,
        "-T",
        profile,
        "-E",
        "main",
        shader_path,
        "-Fo",
        spirv_out,
    ]
    if verbose:
        print(f"  $ {' '.join(spirv_cmd)}")
    result = subprocess.run(spirv_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  SPIRV compilation failed for {os.path.basename(shader_path)}:")
        print(f"    {result.stderr.strip()}")
        success = False
    else:
        # Generate C header from SPIRV
        array_name = f"{basename}_{stage}_spirv"
        header_path = os.path.join(compiled_dir, f"{array_name}.h")
        generate_header(spirv_out, array_name, header_path)
        if verbose:
            size = os.path.getsize(spirv_out)
            print(f"  SPIRV: {size} bytes -> compiled/{array_name}.h")

    # Compile DXIL (plain dxc, no -spirv)
    dxil_cmd = [
        dxc_path,
        "-I",
        shader_dir,
        "-T",
        profile,
        "-E",
        "main",
        shader_path,
        "-Fo",
        dxil_out,
    ]
    if verbose:
        print(f"  $ {' '.join(dxil_cmd)}")
    result = subprocess.run(dxil_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  DXIL compilation failed for {os.path.basename(shader_path)}:")
        print(f"    {result.stderr.strip()}")
        success = False
    else:
        # Generate C header from DXIL
        array_name = f"{basename}_{stage}_dxil"
        header_path = os.path.join(compiled_dir, f"{array_name}.h")
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
            header_path = os.path.join(compiled_dir, f"{var_name}.h")
            generate_msl_header(msl_out, var_name, header_path)
            if verbose:
                size = os.path.getsize(msl_out)
                print(f"  MSL:   {size} bytes -> compiled/{var_name}.h")
    elif not spirv_cross_path:
        print("  MSL:   skipped (spirv-cross not found)")

    return success


def generate_header(binary_path, array_name, output_path):
    """Convert a binary file to a C byte-array header (same format as bin_to_header.py)."""
    with open(binary_path, "rb") as f:
        data = f.read()

    basename = os.path.basename(binary_path)

    with open(output_path, "w") as f:
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


def main():
    parser = argparse.ArgumentParser(
        description="Compile HLSL shaders to SPIRV, DXIL, and MSL with C headers."
    )
    parser.add_argument(
        "lesson",
        nargs="?",
        help="Lesson number or name fragment (default: all lessons)",
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

    # Find dxc
    dxc_path = args.dxc or find_dxc()
    if dxc_path is None:
        print("Could not find dxc compiler.")
        print("Set VULKAN_SDK environment variable or pass --dxc PATH.")
        return 1

    spirv_cross_path = args.spirv_cross or find_spirv_cross()
    if spirv_cross_path is None:
        print("Warning: spirv-cross not found — MSL shaders will not be generated.")
        print("Install via: brew install spirv-cross (macOS), apt install spirv-cross (Linux),")
        print("or the Vulkan SDK (all platforms).")

    print(f"Using dxc: {dxc_path}")
    if spirv_cross_path:
        print(f"Using spirv-cross: {spirv_cross_path}")

    # Find lessons
    lesson_dirs = find_lesson_dirs(args.lesson)
    if not lesson_dirs:
        if args.lesson:
            print(f"No lesson matching '{args.lesson}' found.")
        else:
            print("No lessons found.")
        return 1

    # Compile shaders in each lesson
    total_shaders = 0
    failed = 0
    for lesson_dir in lesson_dirs:
        shaders = find_shaders(lesson_dir)
        if not shaders:
            continue

        lesson_name = os.path.basename(lesson_dir)
        print(f"\n{lesson_name}/")

        for shader in shaders:
            total_shaders += 1
            shader_name = os.path.basename(shader)
            print(f"  {shader_name}")
            if not compile_shader(dxc_path, spirv_cross_path, shader, verbose=args.verbose):
                failed += 1

    if total_shaders == 0:
        print("No shaders found to compile.")
        return 0

    print(f"\nCompiled {total_shaders - failed}/{total_shaders} shaders.")
    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
