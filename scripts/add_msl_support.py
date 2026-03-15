#!/usr/bin/env python3
"""Add MSL shader support to all GPU lesson main.c files (02-39).

This script performs three mechanical transformations:
1. Adds #include for each *_msl.h header alongside existing *_spirv.h / *_dxil.h
2. Adds MSL parameters to create_shader() and create_compute_pipeline() functions
3. Adds the MSL format branch in the body
4. Updates all call sites to pass MSL data

Run from repo root: python scripts/add_msl_support.py
"""

import re
import sys
from pathlib import Path


def add_msl_includes(content: str) -> str:
    """Add *_msl.h includes after each group of *_dxil.h includes."""
    lines = content.split("\n")
    result = []
    i = 0
    while i < len(lines):
        line = lines[i]
        result.append(line)

        # After a _dxil.h include, add matching _msl.h if not already present
        m = re.match(r'^#include\s+"(shaders/compiled/(\w+)_dxil\.h)"', line)
        if m:
            msl_header = f'shaders/compiled/{m.group(2)}_msl.h'
            msl_include = f'#include "{msl_header}"'
            # Check if next line already has the MSL include
            if i + 1 < len(lines) and "_msl.h" in lines[i + 1]:
                pass  # Already present
            else:
                result.append(msl_include)
        i += 1

    return "\n".join(result)


def add_msl_to_create_shader(content: str) -> str:
    """Add MSL params to create_shader() signature and body."""

    # Pattern 1: Simple signature (lesson 02) - no num_samplers params
    # const unsigned char *dxil_code,   unsigned int dxil_size)
    content = re.sub(
        r"(const unsigned char \*dxil_code,\s*unsigned int dxil_size)\)",
        r"\1,\n    const char          *msl_code,   unsigned int msl_size)",
        content,
    )

    # Pattern 2: Extended signature - dxil_size, followed by newline then num_/int/Uint32
    # const unsigned char *dxil_code,   unsigned int dxil_size,
    # Works for both "unsigned char" and "Uint8" types
    content = re.sub(
        r"(const (?:unsigned char|Uint8) \*dxil_code,\s*(?:unsigned int|size_t) dxil_size,)\n(\s*)((?:int|Uint32)\s+num_)",
        r"\1\n\2const char *msl_code, unsigned int msl_size,\n\2\3",
        content,
    )

    # Add MSL branch to the format selection in create_shader
    # Match: } else {\n        SDL_Log("No supported shader format (need SPIRV or DXIL)");
    content = re.sub(
        r'(\} else if \(formats & SDL_GPU_SHADERFORMAT_DXIL\) \{[^}]+\})'
        r'(\s*else\s*\{\s*\n\s*SDL_Log\("No supported shader format \(need SPIRV or DXIL\)"\);)',
        r"""\1 else if ((formats & SDL_GPU_SHADERFORMAT_MSL) && msl_code) {
        info.format     = SDL_GPU_SHADERFORMAT_MSL;
        info.entrypoint = "main0";
        info.code       = (const unsigned char *)msl_code;
        info.code_size  = msl_size;
    }\2""",
        content,
    )

    # Update error message to include MSL
    content = content.replace(
        'SDL_Log("No supported shader format (need SPIRV or DXIL)")',
        'SDL_Log("No supported shader format (need SPIRV, DXIL, or MSL)")',
    )

    return content


def add_msl_to_create_compute(content: str) -> str:
    """Add MSL params to create_compute_pipeline() signature and body."""

    # Match the compute pipeline signature: dxil_size, then newline + int num_samplers
    content = re.sub(
        r"(static SDL_GPUComputePipeline \*create_compute_pipeline\([^)]*"
        r"const unsigned char \*dxil_code,\s*unsigned int dxil_size,)\n"
        r"(\s*)(int\s+num_samplers)",
        r"\1\n\2const char          *msl_code,   unsigned int msl_size,\n\2\3",
        content,
    )

    return content


def add_msl_to_call_sites(content: str) -> str:
    """Add MSL arguments to create_shader() and create_compute_pipeline() call sites."""

    # Find all call sites and add MSL args after dxil args
    # Pattern: _dxil, _dxil_size followed by ) or , (for next arg)
    # We need to match the variable name prefix to construct the MSL variable name
    #
    # Example:
    #   triangle_vert_spirv, triangle_vert_spirv_size,
    #   triangle_vert_dxil,  triangle_vert_dxil_size);
    # becomes:
    #   triangle_vert_spirv, triangle_vert_spirv_size,
    #   triangle_vert_dxil,  triangle_vert_dxil_size,
    #   triangle_vert_msl,   triangle_vert_msl_size);
    #
    # Or for extended signatures:
    #   triangle_vert_dxil,  triangle_vert_dxil_size,
    #   1, 0, 0, 1);
    # becomes:
    #   triangle_vert_dxil,  triangle_vert_dxil_size,
    #   triangle_vert_msl,   triangle_vert_msl_size,
    #   1, 0, 0, 1);

    # Match: prefix_dxil, prefix_dxil_size) — simple signature (closing paren)
    content = re.sub(
        r"(\w+)_dxil,(\s+)\1_dxil_size\)",
        lambda m: (
            f"{m.group(1)}_dxil,{m.group(2)}{m.group(1)}_dxil_size,\n"
            f"        {m.group(1)}_msl,{m.group(2)} {m.group(1)}_msl_size)"
        ),
        content,
    )

    # Match: prefix_dxil, prefix_dxil_size,\n        <next arg> — extended sig
    # Only do this where we haven't already added MSL (no _msl on next line)
    def add_msl_extended(m):
        prefix = m.group(1)
        spacing = m.group(2)
        indent = m.group(3)
        rest_line = m.group(4)
        if rest_line.startswith(f"{prefix}_msl"):
            return m.group(0)
        return (
            f"{prefix}_dxil,{spacing}{prefix}_dxil_size,\n"
            f"{indent}{prefix}_msl,   {prefix}_msl_size,\n"
            f"{indent}{rest_line}"
        )

    content = re.sub(
        r"(\w+)_dxil,(\s+)\1_dxil_size,\n(\s+)(\S+)",
        add_msl_extended,
        content,
    )

    # Match sizeof() pattern: prefix_dxil, sizeof(prefix_dxil),\n  <next arg>
    def add_msl_sizeof(m):
        prefix = m.group(1)
        spacing = m.group(2)
        indent = m.group(3)
        rest_line = m.group(4)
        if rest_line.startswith(f"{prefix}_msl"):
            return m.group(0)
        return (
            f"{prefix}_dxil,{spacing}sizeof({prefix}_dxil),\n"
            f"{indent}{prefix}_msl,   {prefix}_msl_size,\n"
            f"{indent}{rest_line}"
        )

    content = re.sub(
        r"(\w+)_dxil,(\s+)sizeof\(\1_dxil\),\n(\s+)(\S+)",
        add_msl_sizeof,
        content,
    )

    # Match sizeof() pattern on same line: sizeof(prefix_dxil), <num args>);
    # e.g. scene_vert_dxil, sizeof(scene_vert_dxil), 0, 1);
    def add_msl_sizeof_inline(m):
        prefix = m.group(1)
        spacing = m.group(2)
        rest = m.group(3)
        if f"{prefix}_msl" in rest:
            return m.group(0)
        return (
            f"{prefix}_dxil,{spacing}sizeof({prefix}_dxil),\n"
            f"        {prefix}_msl, {prefix}_msl_size, {rest}"
        )

    content = re.sub(
        r"(\w+)_dxil,(\s+)sizeof\(\1_dxil\), (\d[\d, ]*\);)",
        add_msl_sizeof_inline,
        content,
    )

    return content


def process_lesson(main_c: Path) -> bool:
    """Process a single lesson main.c file. Returns True if modified."""
    content = main_c.read_text(encoding="utf-8")

    # Skip if already has MSL support
    if "SDL_GPU_SHADERFORMAT_MSL" in content:
        print(f"  {main_c.parent.name}: already has MSL support, skipping")
        return False

    # Skip if no create_shader function
    if "create_shader(" not in content:
        print(f"  {main_c.parent.name}: no create_shader function, skipping")
        return False

    # Skip lesson 15 — multi-line dxil args need manual handling
    if "15-cascaded-shadow-maps" in str(main_c):
        print(f"  {main_c.parent.name}: skipping (handle manually)")
        return False

    original = content

    # Step 1: Add MSL includes
    content = add_msl_includes(content)

    # Step 2: Add MSL to create_shader signature and body
    content = add_msl_to_create_shader(content)

    # Step 3: Add MSL to create_compute_pipeline if present
    if "create_compute_pipeline(" in content:
        content = add_msl_to_create_compute(content)

    # Step 4: Update call sites
    content = add_msl_to_call_sites(content)

    if content == original:
        print(f"  {main_c.parent.name}: no changes needed")
        return False

    main_c.write_text(content, encoding="utf-8", newline="\n")
    print(f"  {main_c.parent.name}: updated")
    return True


def main():
    repo_root = Path(__file__).parent.parent
    lessons_gpu = repo_root / "lessons" / "gpu"

    if not lessons_gpu.exists():
        print(f"Error: {lessons_gpu} not found", file=sys.stderr)
        sys.exit(1)

    # Find all lesson directories with a main.c
    lesson_dirs = sorted(
        d for d in lessons_gpu.iterdir()
        if d.is_dir() and (d / "main.c").exists()
    )

    print(f"Found {len(lesson_dirs)} GPU lessons with main.c")
    modified = 0
    for d in lesson_dirs:
        main_c = d / "main.c"
        if process_lesson(main_c):
            modified += 1

    print(f"\nModified {modified} files")


if __name__ == "__main__":
    main()
