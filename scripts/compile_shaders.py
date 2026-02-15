#!/usr/bin/env python
"""
compile_shaders.py — Compile HLSL shaders to SPIRV and DXIL with embedded C headers.

Finds .vert.hlsl and .frag.hlsl files and compiles them using dxc.

Usage:
    python scripts/compile_shaders.py                    # all lessons
    python scripts/compile_shaders.py 02                 # lesson 02 only
    python scripts/compile_shaders.py first-triangle     # by name
    python scripts/compile_shaders.py --dxc PATH         # override dxc path

The script auto-detects dxc from:
  1. --dxc command-line flag
  2. VULKAN_SDK environment variable (for SPIRV via -spirv)
  3. System PATH
"""

import argparse
import os
import shutil
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LESSONS_DIR = os.path.join(REPO_ROOT, "lessons", "gpu")

# Shader stage to DXC target profile mapping
STAGE_PROFILES = {
    ".vert.hlsl": "vs_6_0",
    ".frag.hlsl": "ps_6_0",
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


def find_lesson_dirs(query=None):
    """Find lesson directories, optionally filtered by query."""
    if not os.path.isdir(LESSONS_DIR):
        return []

    dirs = []
    for entry in sorted(os.listdir(LESSONS_DIR)):
        full = os.path.join(LESSONS_DIR, entry)
        if not os.path.isdir(full) or not entry[0].isdigit():
            continue
        if query is None:
            dirs.append(full)
        else:
            q = query.lower()
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


def compile_shader(dxc_path, shader_path, verbose=False):
    """Compile a single HLSL shader to SPIRV and DXIL, then generate C headers."""
    suffix = get_stage_suffix(shader_path)
    if suffix is None:
        print(f"  Unknown shader stage: {os.path.basename(shader_path)}")
        return False
    profile = STAGE_PROFILES[suffix]
    base = shader_path[: -len(suffix)]  # e.g. .../shaders/triangle
    basename = os.path.basename(base)  # e.g. triangle
    shader_dir = os.path.dirname(shader_path)

    # Determine short stage name for naming output files
    stage = "vert" if ".vert." in shader_path else "frag"

    spirv_out = f"{base}.{stage}.spv"
    dxil_out = f"{base}.{stage}.dxil"

    success = True

    # Compile SPIRV (using Vulkan SDK dxc with -spirv flag)
    spirv_cmd = [
        dxc_path,
        "-spirv",
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
        header_path = os.path.join(shader_dir, f"{array_name}.h")
        generate_header(spirv_out, array_name, header_path)
        if verbose:
            size = os.path.getsize(spirv_out)
            print(f"  SPIRV: {size} bytes -> {os.path.basename(header_path)}")

    # Compile DXIL (plain dxc, no -spirv)
    dxil_cmd = [dxc_path, "-T", profile, "-E", "main", shader_path, "-Fo", dxil_out]
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
        header_path = os.path.join(shader_dir, f"{array_name}.h")
        generate_header(dxil_out, array_name, header_path)
        if verbose:
            size = os.path.getsize(dxil_out)
            print(f"  DXIL:  {size} bytes -> {os.path.basename(header_path)}")

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


def main():
    parser = argparse.ArgumentParser(
        description="Compile HLSL shaders to SPIRV and DXIL with C headers."
    )
    parser.add_argument(
        "lesson",
        nargs="?",
        help="Lesson number or name fragment (default: all lessons)",
    )
    parser.add_argument("--dxc", help="Path to dxc compiler (auto-detected if not set)")
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

    print(f"Using dxc: {dxc_path}")

    # Find lessons
    lesson_dirs = find_lesson_dirs(args.lesson)
    if not lesson_dirs:
        if args.lesson:
            print(f"No lesson matching '{args.lesson}' found.")
        else:
            print("No GPU lessons found.")
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
            if not compile_shader(dxc_path, shader, verbose=args.verbose):
                failed += 1

    if total_shaders == 0:
        print("No shaders found to compile.")
        return 0

    print(f"\nCompiled {total_shaders - failed}/{total_shaders} shaders.")
    return 1 if failed > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
