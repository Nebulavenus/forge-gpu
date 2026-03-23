#!/usr/bin/env python
"""Generate golden binary fixtures for cross-language format contract tests.

Produces minimal .fmesh, .fmat, and .ftex files that both the C loader
(test_pipeline.c) and the TypeScript parser (fmesh-parser.test.ts, etc.)
validate against.  If the C tool changes the binary layout, regenerating
these fixtures will cause the TypeScript tests to fail — forcing both
sides to stay in sync.

Usage:
    python tests/pipeline/generate_fixtures.py            # generate fixtures
    python tests/pipeline/generate_fixtures.py --verify   # verify committed files match

The binary layouts here MUST match:
  - common/pipeline/forge_pipeline.h constants and structs
  - tests/pipeline/test_pipeline.c write_test_fmesh_ex()
  - tools/mesh/main.c .fmesh writer
  - tools/texture/main.c .ftex writer

SPDX-License-Identifier: Zlib
"""

from __future__ import annotations

import json
import struct
import sys
import typing
from pathlib import Path

# ── Constants (must match forge_pipeline.h) ──────────────────────────────

FMESH_MAGIC = b"FMSH"
FMESH_VERSION = 2
FMESH_HEADER_SIZE = 32
FMESH_STRIDE_NO_TAN = 32
FMESH_STRIDE_TAN = 48
FMESH_FLAG_TANGENTS = 1

FTEX_MAGIC = 0x58455446  # "FTEX" little-endian
FTEX_VERSION = 1
FTEX_HEADER_SIZE = 32
FTEX_MIP_ENTRY_SIZE = 16
FTEX_FORMAT_BC7_SRGB = 1
FTEX_FORMAT_BC7_UNORM = 2
FTEX_FORMAT_BC5_UNORM = 3

FMAT_VERSION = 1

FIXTURES_DIR = Path(__file__).parent / "fixtures"


# ── .fmesh generators ───────────────────────────────────────────────────


def _write_fmesh(
    *,
    with_tangents: bool,
    lod_count: int,
    submesh_count: int,
) -> bytes:
    """Build a minimal .fmesh v2 binary in memory.

    Creates a 3-vertex triangle with known vertex data so parsers can
    assert exact values.  Matches test_pipeline.c write_test_fmesh_ex().
    """
    vertex_count = 3
    stride = FMESH_STRIDE_TAN if with_tangents else FMESH_STRIDE_NO_TAN
    flags = FMESH_FLAG_TANGENTS if with_tangents else 0
    indices_per_submesh = 3
    total_indices = lod_count * submesh_count * indices_per_submesh

    # ── Header (32 bytes) ────────────────────────────────────────────
    header = struct.pack(
        "<4sIIIIIII",
        FMESH_MAGIC,  # magic
        FMESH_VERSION,  # version
        vertex_count,  # vertex_count
        stride,  # vertex_stride
        lod_count,  # lod_count
        flags,  # flags
        submesh_count,  # submesh_count
        0,  # reserved
    )
    assert len(header) == FMESH_HEADER_SIZE

    # ── LOD-submesh table ────────────────────────────────────────────
    lod_table = bytearray()
    running_offset = 0
    for lod in range(lod_count):
        # target_error: 0.0 for LOD 0, 0.01 for LOD 1, etc.
        lod_table += struct.pack("<f", lod * 0.01)
        for s in range(submesh_count):
            lod_table += struct.pack(
                "<IIi",
                indices_per_submesh,  # index_count
                running_offset,  # index_offset (bytes)
                s,  # material_index
            )
            running_offset += indices_per_submesh * 4  # sizeof(uint32)

    # ── Vertex data ──────────────────────────────────────────────────
    # Three vertices forming a right triangle in the XY plane.
    # Normal = +Z, UVs span [0,1].
    vertices = [
        # pos(x,y,z)   normal(x,y,z)   uv(u,v)        tangent(x,y,z,w)
        (0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0),
        (1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 1.0, 0.0, 0.0, 1.0),
        (0.0, 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0),
    ]

    vertex_data = bytearray()
    for v in vertices:
        # position + normal + uv = 8 floats
        vertex_data += struct.pack("<8f", *v[:8])
        if with_tangents:
            # tangent = 4 floats
            vertex_data += struct.pack("<4f", *v[8:12])

    assert len(vertex_data) == vertex_count * stride

    # ── Index data ───────────────────────────────────────────────────
    # (0, 1, 2) repeated for each LOD × submesh
    index_data = bytearray()
    for _ in range(lod_count * submesh_count):
        index_data += struct.pack("<3I", 0, 1, 2)

    assert len(index_data) == total_indices * 4

    return bytes(header) + bytes(lod_table) + bytes(vertex_data) + bytes(index_data)


def generate_triangle_fmesh() -> bytes:
    """3-vertex triangle, 1 LOD, 1 submesh, with tangents."""
    return _write_fmesh(with_tangents=True, lod_count=1, submesh_count=1)


def generate_multi_lod_fmesh() -> bytes:
    """3-vertex triangle, 2 LODs, 2 submeshes."""
    return _write_fmesh(with_tangents=True, lod_count=2, submesh_count=2)


# ── .fmat generator ─────────────────────────────────────────────────────


def generate_simple_fmat() -> bytes:
    """2-material JSON sidecar with PBR properties and texture paths."""
    data = {
        "version": FMAT_VERSION,
        "materials": [
            {
                "name": "Wood",
                "base_color_factor": [0.8, 0.6, 0.3, 1.0],
                "base_color_texture": "textures/wood_albedo.png",
                "metallic_factor": 0.0,
                "roughness_factor": 0.8,
                "metallic_roughness_texture": "textures/wood_mr.png",
                "normal_texture": "textures/wood_normal.png",
                "normal_scale": 1.0,
                "occlusion_texture": None,
                "occlusion_strength": 1.0,
                "emissive_factor": [0.0, 0.0, 0.0],
                "emissive_texture": None,
                "alpha_mode": "OPAQUE",
                "alpha_cutoff": 0.5,
                "double_sided": False,
            },
            {
                "name": "Glass",
                "base_color_factor": [0.9, 0.9, 1.0, 0.5],
                "base_color_texture": None,
                "metallic_factor": 0.0,
                "roughness_factor": 0.1,
                "metallic_roughness_texture": None,
                "normal_texture": None,
                "normal_scale": 1.0,
                "occlusion_texture": None,
                "occlusion_strength": 1.0,
                "emissive_factor": [0.0, 0.0, 0.0],
                "emissive_texture": None,
                "alpha_mode": "BLEND",
                "alpha_cutoff": 0.5,
                "double_sided": True,
            },
        ],
    }
    return json.dumps(data, indent=2).encode("utf-8") + b"\n"


# ── .ftex generator ─────────────────────────────────────────────────────


def generate_checkerboard_ftex() -> bytes:
    """4×4 single-mip BC7 texture (one 16-byte block).

    BC7 compresses 4×4 pixel blocks into 16 bytes each.  A 4×4 texture
    is exactly one block.  The block data here is a valid BC7 mode 6
    encoding of a solid mid-grey colour (not visually meaningful, but
    structurally correct for parser testing).
    """
    width = 4
    height = 4
    mip_count = 1
    fmt = FTEX_FORMAT_BC7_SRGB

    # BC7 block data: 16 bytes.  Mode 6 (bit 6 set = 0x40), rest zeros
    # produces a valid decompressible block.
    block_data = bytes([0x40]) + bytes(15)
    assert len(block_data) == 16

    # Data offset is after header + mip entries
    data_offset = FTEX_HEADER_SIZE + mip_count * FTEX_MIP_ENTRY_SIZE

    # Header (32 bytes)
    header = struct.pack(
        "<IIIIIIII",
        FTEX_MAGIC,  # magic
        FTEX_VERSION,  # version
        fmt,  # format
        width,  # width
        height,  # height
        mip_count,  # mip_count
        0,  # reserved
        0,  # reserved
    )
    assert len(header) == FTEX_HEADER_SIZE

    # Mip entry (16 bytes)
    mip_entry = struct.pack(
        "<IIII",
        data_offset,  # data_offset
        len(block_data),  # data_size
        width,  # width
        height,  # height
    )
    assert len(mip_entry) == FTEX_MIP_ENTRY_SIZE

    return header + mip_entry + block_data


# ── Main ─────────────────────────────────────────────────────────────────

FIXTURES: dict[str, typing.Callable[[], bytes]] = {
    "triangle.fmesh": generate_triangle_fmesh,
    "multi_lod.fmesh": generate_multi_lod_fmesh,
    "simple.fmat": generate_simple_fmat,
    "checkerboard.ftex": generate_checkerboard_ftex,
}


def generate_all() -> dict[str, bytes]:
    """Generate all fixture files and return {name: bytes}."""
    return {name: gen() for name, gen in FIXTURES.items()}


def write_fixtures() -> None:
    """Write all fixtures to disk."""
    FIXTURES_DIR.mkdir(parents=True, exist_ok=True)
    for name, data in generate_all().items():
        path = FIXTURES_DIR / name
        path.write_bytes(data)
        print(f"  wrote {path} ({len(data)} bytes)")


def verify_fixtures() -> bool:
    """Verify that committed fixtures match freshly generated ones.

    Returns True if all match, False if any differ.
    """
    ok = True
    for name, expected in generate_all().items():
        path = FIXTURES_DIR / name
        if not path.exists():
            print(f"  MISSING: {path}")
            ok = False
            continue

        actual = path.read_bytes()
        if actual != expected:
            print(
                f"  MISMATCH: {path} "
                f"(expected {len(expected)} bytes, got {len(actual)} bytes)"
            )
            ok = False
        else:
            print(f"  OK: {path} ({len(actual)} bytes)")

    return ok


def main() -> None:
    verify_mode = "--verify" in sys.argv

    if verify_mode:
        print("Verifying golden fixtures...")
        if verify_fixtures():
            print("All fixtures match.")
        else:
            print(
                "\nFixture mismatch!  Regenerate with:\n"
                "  python tests/pipeline/generate_fixtures.py\n"
                "\nThen update TypeScript parser tests if the format changed."
            )
            sys.exit(1)
    else:
        print("Generating golden fixtures...")
        write_fixtures()
        print("Done.")


if __name__ == "__main__":
    main()
