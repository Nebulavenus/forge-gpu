#!/usr/bin/env python
"""
dump_fmesh.py -- Dump .fmesh binary file contents for debugging.

Reads the binary mesh format written by forge-mesh-tool and prints
header info, submesh ranges, and per-submesh vertex data (positions,
normals, UVs, tangents).

Usage:
    python scripts/dump_fmesh.py <path/to/file.fmesh>
    python scripts/dump_fmesh.py <path/to/file.fmesh> --uvs        # UV ranges per submesh
    python scripts/dump_fmesh.py <path/to/file.fmesh> --verts 0    # first 20 verts of submesh 0
    python scripts/dump_fmesh.py <path/to/file.fmesh> --all-verts  # all vertex data

SPDX-License-Identifier: Zlib
"""

import struct
import sys


def read_u32(data, pos):
    return struct.unpack_from("<I", data, pos)[0], pos + 4


def read_i32(data, pos):
    return struct.unpack_from("<i", data, pos)[0], pos + 4


def read_f32(data, pos):
    return struct.unpack_from("<f", data, pos)[0], pos + 4


def parse_vertex(data, offset, stride, has_tangents=False):
    """Parse a single vertex at the given byte offset."""
    pos = offset
    px, py, pz = struct.unpack_from("<3f", data, pos)
    pos += 12
    nx, ny, nz = struct.unpack_from("<3f", data, pos)
    pos += 12
    u, v = struct.unpack_from("<2f", data, pos)
    pos += 8
    tx, ty, tz, tw = (0, 0, 0, 0)
    if has_tangents:
        tx, ty, tz, tw = struct.unpack_from("<4f", data, pos)
    return {
        "pos": (px, py, pz),
        "nrm": (nx, ny, nz),
        "uv": (u, v),
        "tan": (tx, ty, tz, tw),
    }


def main():
    args = sys.argv[1:]
    if not args:
        print(f"Usage: {sys.argv[0]} <file.fmesh> [--uvs] [--verts N] [--all-verts]")
        sys.exit(1)

    path = args[0]
    show_uvs = "--uvs" in args
    show_all_verts = "--all-verts" in args
    show_verts_submesh = None
    if "--verts" in args:
        idx = args.index("--verts")
        show_verts_submesh = int(args[idx + 1]) if idx + 1 < len(args) else 0

    with open(path, "rb") as f:
        data = f.read()

    pos = 0

    # Header (32 bytes)
    magic = data[pos : pos + 4]
    pos += 4
    if magic != b"FMSH":
        raise ValueError(f"Bad magic: {magic!r}")

    version, pos = read_u32(data, pos)
    vertex_count, pos = read_u32(data, pos)
    vertex_stride, pos = read_u32(data, pos)
    lod_count, pos = read_u32(data, pos)
    flags, pos = read_u32(data, pos)
    submesh_count, pos = read_u32(data, pos)
    _reserved, pos = read_u32(data, pos)  # 4 bytes padding

    has_tangents = (flags & 1) != 0
    has_skin = (flags & 2) != 0

    print(f"=== .fmesh dump: {path} ===")
    print(f"Version:      {version}")
    print(f"Vertices:     {vertex_count}")
    print(f"Stride:       {vertex_stride} bytes")
    print(f"LODs:         {lod_count}")
    print(f"Submeshes:    {submesh_count}")
    print(f"Flags:        0x{flags:x} (tangents={has_tangents}, skin={has_skin})")
    print()

    # LOD-submesh table
    submeshes = []
    for lod in range(lod_count):
        target_error, pos = read_f32(data, pos)
        print(f"LOD {lod}: target_error={target_error:.6f}")
        for s in range(submesh_count):
            idx_count, pos = read_u32(data, pos)
            idx_offset, pos = read_u32(data, pos)
            mat_idx, pos = read_i32(data, pos)
            submeshes.append(
                {
                    "lod": lod,
                    "submesh": s,
                    "index_count": idx_count,
                    "index_offset": idx_offset,
                    "material_index": mat_idx,
                }
            )
            print(
                f"  Submesh {s}: {idx_count} indices, "
                f"offset={idx_offset}, material={mat_idx}"
            )
    print()

    # Vertex data section
    vertex_section_start = pos
    vertex_section_size = vertex_count * vertex_stride
    pos += vertex_section_size

    # Index data section
    index_section_start = pos

    # Compute per-submesh vertex ranges (LOD 0 only)
    lod0_submeshes = [s for s in submeshes if s["lod"] == 0]
    lod0_base_offset = lod0_submeshes[0]["index_offset"] if lod0_submeshes else 0

    for sub in lod0_submeshes:
        # Read indices for this submesh
        first_index = (sub["index_offset"] - lod0_base_offset) // 4
        idx_start = index_section_start + first_index * 4
        indices = []
        for i in range(sub["index_count"]):
            idx_val = struct.unpack_from("<I", data, idx_start + i * 4)[0]
            indices.append(idx_val)

        if not indices:
            continue

        min_idx = min(indices)
        max_idx = max(indices)

        # Gather UV stats
        uv_min_u = float("inf")
        uv_max_u = float("-inf")
        uv_min_v = float("inf")
        uv_max_v = float("-inf")
        pos_min = [float("inf")] * 3
        pos_max = [float("-inf")] * 3

        unique_verts = set(indices)
        for vi in unique_verts:
            vert = parse_vertex(
                data,
                vertex_section_start + vi * vertex_stride,
                vertex_stride,
                has_tangents,
            )
            u, v = vert["uv"]
            uv_min_u = min(uv_min_u, u)
            uv_max_u = max(uv_max_u, u)
            uv_min_v = min(uv_min_v, v)
            uv_max_v = max(uv_max_v, v)
            for ax in range(3):
                pos_min[ax] = min(pos_min[ax], vert["pos"][ax])
                pos_max[ax] = max(pos_max[ax], vert["pos"][ax])

        sub_idx = sub["submesh"]
        print(f"--- Submesh {sub_idx} (material {sub['material_index']}) ---")
        print(f"  Triangles: {sub['index_count'] // 3}")
        print(f"  Vertex range: [{min_idx}, {max_idx}] ({len(unique_verts)} unique)")
        print(f"  Position X: [{pos_min[0]:.4f}, {pos_max[0]:.4f}]")
        print(f"  Position Y: [{pos_min[1]:.4f}, {pos_max[1]:.4f}]")
        print(f"  Position Z: [{pos_min[2]:.4f}, {pos_max[2]:.4f}]")
        print(f"  UV U: [{uv_min_u:.6f}, {uv_max_u:.6f}]")
        print(f"  UV V: [{uv_min_v:.6f}, {uv_max_v:.6f}]")

        # Show individual vertices if requested
        if (
            show_verts_submesh is not None and show_verts_submesh == sub_idx
        ) or show_all_verts:
            sorted_verts = sorted(unique_verts)
            limit = 20 if not show_all_verts else len(sorted_verts)
            print(f"  First {min(limit, len(sorted_verts))} vertices:")
            for vi in sorted_verts[:limit]:
                vert = parse_vertex(
                    data,
                    vertex_section_start + vi * vertex_stride,
                    vertex_stride,
                    has_tangents,
                )
                p = vert["pos"]
                uv = vert["uv"]
                t = vert["tan"]
                n = vert["nrm"]
                print(
                    f"    [{vi:5d}] pos=({p[0]:8.4f},{p[1]:8.4f},{p[2]:8.4f})"
                    f"  nrm=({n[0]:7.4f},{n[1]:7.4f},{n[2]:7.4f})"
                    f"  uv=({uv[0]:.5f},{uv[1]:.5f})"
                    f"  tan=({t[0]:7.4f},{t[1]:7.4f},{t[2]:7.4f},{t[3]:5.1f})"
                )
            if len(sorted_verts) > limit:
                print(f"    ... {len(sorted_verts) - limit} more vertices")
        print()

    # Show UV ranges summary if requested
    if show_uvs:
        print("=== UV range summary (LOD 0) ===")
        for sub in lod0_submeshes:
            print(
                f"  Submesh {sub['submesh']} (mat {sub['material_index']}): see above"
            )


if __name__ == "__main__":
    main()
