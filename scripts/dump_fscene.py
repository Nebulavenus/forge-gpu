#!/usr/bin/env python
"""
dump_fscene.py — Dump .fscene binary file contents for debugging.

Reads the binary scene hierarchy format written by forge-scene-tool
and prints all node data: names, hierarchy, TRS, local transforms,
and optionally computed world transforms.

Usage:
    python scripts/dump_fscene.py <path/to/file.fscene>

SPDX-License-Identifier: Zlib
"""

import struct
import sys


def read_fscene(path: str) -> dict:
    with open(path, "rb") as f:
        data = f.read()

    pos = 0

    # Header (24 bytes)
    magic = data[pos : pos + 4]
    pos += 4
    if magic != b"FSCN":
        raise ValueError(f"Bad magic: {magic!r} (expected b'FSCN')")

    version, node_count, mesh_count, root_count, reserved = struct.unpack_from(
        "<5I", data, pos
    )
    pos += 20

    print(f"=== .fscene dump: {path} ===")
    print(f"Version:    {version}")
    print(f"Nodes:      {node_count}")
    print(f"Meshes:     {mesh_count}")
    print(f"Roots:      {root_count}")
    if reserved != 0:
        print(f"Reserved:   {reserved} (expected 0)")
    print()

    # Root indices
    roots = []
    for _ in range(root_count):
        (r,) = struct.unpack_from("<I", data, pos)
        pos += 4
        roots.append(r)
    print(f"Root indices: {roots}")
    print()

    # Mesh table
    meshes = []
    for i in range(mesh_count):
        first_sub, sub_count = struct.unpack_from("<II", data, pos)
        pos += 8
        meshes.append((first_sub, sub_count))
        print(f"Mesh {i}: first_submesh={first_sub}, submesh_count={sub_count}")
    print()

    # Node table (192 bytes each)
    nodes = []
    for _i in range(node_count):
        node_start = pos

        # Name: 64 bytes null-terminated
        name_bytes = data[pos : pos + 64]
        pos += 64
        name = name_bytes.split(b"\x00", 1)[0].decode("utf-8", errors="replace")

        # parent(i32), mesh_index(i32), skin_index(i32)
        parent, mesh_idx, skin_idx = struct.unpack_from("<3i", data, pos)
        pos += 12

        # first_child(u32), child_count(u32), has_trs(u32)
        first_child, child_count, has_trs = struct.unpack_from("<3I", data, pos)
        pos += 12

        # translation(3f), rotation(4f), scale(3f)
        tx, ty, tz = struct.unpack_from("<3f", data, pos)
        pos += 12
        rx, ry, rz, rw = struct.unpack_from("<4f", data, pos)
        pos += 16
        sx, sy, sz = struct.unpack_from("<3f", data, pos)
        pos += 12

        # local_transform (16 floats, column-major)
        local = list(struct.unpack_from("<16f", data, pos))
        pos += 64

        assert pos - node_start == 192, f"Node size mismatch: {pos - node_start}"

        nodes.append(
            {
                "name": name,
                "parent": parent,
                "mesh_index": mesh_idx,
                "skin_index": skin_idx,
                "first_child": first_child,
                "child_count": child_count,
                "has_trs": has_trs,
                "translation": (tx, ty, tz),
                "rotation_xyzw": (rx, ry, rz, rw),
                "scale": (sx, sy, sz),
                "local_transform": local,
            }
        )

    # Children array
    total_children = sum(n["child_count"] for n in nodes)
    children = []
    for _ in range(total_children):
        (c,) = struct.unpack_from("<I", data, pos)
        pos += 4
        children.append(c)

    return {
        "roots": roots,
        "meshes": meshes,
        "nodes": nodes,
        "children": children,
    }


def mat4_str(m: list[float]) -> str:
    """Format a column-major 4x4 matrix as rows."""
    lines = []
    for row in range(4):
        vals = [m[col * 4 + row] for col in range(4)]
        lines.append("  [{:9.5f} {:9.5f} {:9.5f} {:9.5f}]".format(*vals))
    return "\n".join(lines)


def mat4_multiply(a: list[float], b: list[float]) -> list[float]:
    """Column-major 4x4 matrix multiply: result = a * b."""
    result = [0.0] * 16
    for col in range(4):
        for row in range(4):
            s = 0.0
            for k in range(4):
                s += a[k * 4 + row] * b[col * 4 + k]
            result[col * 4 + row] = s
    return result


def compute_world_transforms(nodes, roots, children):
    """Compute world transforms by traversing the hierarchy."""
    world = [[0.0] * 16 for _ in range(len(nodes))]
    identity = [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1]
    visited = set()

    def visit(node_idx, parent_world):
        if node_idx < 0 or node_idx >= len(nodes):
            print(f"  WARNING: node index {node_idx} out of range [0, {len(nodes)})")
            return
        if node_idx in visited:
            print(f"  WARNING: cycle detected at node {node_idx}")
            return
        visited.add(node_idx)
        node = nodes[node_idx]
        world[node_idx] = mat4_multiply(parent_world, node["local_transform"])
        for ci in range(node["child_count"]):
            child_slot = node["first_child"] + ci
            if child_slot < 0 or child_slot >= len(children):
                print(
                    f"  WARNING: child slot {child_slot} out of range "
                    f"[0, {len(children)}) for node {node_idx}"
                )
                continue
            child_idx = children[child_slot]
            visit(child_idx, world[node_idx])

    for r in roots:
        if r < 0 or r >= len(nodes):
            print(f"  WARNING: root index {r} out of range [0, {len(nodes)})")
            continue
        visit(r, identity)

    return world


def quat_expected_matrix(rx, ry, rz, rw):
    """Compute the expected rotation matrix from quaternion (x,y,z,w)."""
    x, y, z, w = rx, ry, rz, rw
    xx = x * x
    yy = y * y
    zz = z * z
    xy = x * y
    xz = x * z
    yz = y * z
    wx = w * x
    wy = w * y
    wz = w * z

    # Column-major
    return [
        1 - 2 * (yy + zz),
        2 * (xy + wz),
        2 * (xz - wy),
        0,  # col 0
        2 * (xy - wz),
        1 - 2 * (xx + zz),
        2 * (yz + wx),
        0,  # col 1
        2 * (xz + wy),
        2 * (yz - wx),
        1 - 2 * (xx + yy),
        0,  # col 2
        0,
        0,
        0,
        1,  # col 3
    ]


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <file.fscene>")
        sys.exit(1)

    scene = read_fscene(sys.argv[1])
    nodes = scene["nodes"]
    children = scene["children"]

    # Compute world transforms
    world = compute_world_transforms(nodes, scene["roots"], children)

    # Print each node
    for i, node in enumerate(nodes):
        print(f'--- Node {i}: "{node["name"]}" ---')
        print(
            f"  parent={node['parent']}  mesh={node['mesh_index']}  "
            f"skin={node['skin_index']}  children={node['child_count']}"
        )

        if node["has_trs"]:
            t = node["translation"]
            r = node["rotation_xyzw"]
            s = node["scale"]
            print("  TRS:")
            print(f"    T = ({t[0]:.6f}, {t[1]:.6f}, {t[2]:.6f})")
            print(
                f"    R = quat(x={r[0]:.6f}, y={r[1]:.6f}, z={r[2]:.6f}, w={r[3]:.6f})"
            )
            print(f"    S = ({s[0]:.6f}, {s[1]:.6f}, {s[2]:.6f})")

            # Verify: compute expected matrix from TRS and compare
            expected_r = quat_expected_matrix(*r)
            # Apply scale
            for col in range(3):
                scale_val = s[col]
                for row in range(3):
                    expected_r[col * 4 + row] *= scale_val
            # Apply translation
            expected_r[12] = t[0]
            expected_r[13] = t[1]
            expected_r[14] = t[2]

            # Compare with stored local_transform
            local = node["local_transform"]
            max_diff = max(abs(expected_r[j] - local[j]) for j in range(16))
            if max_diff > 1e-5:
                print(f"  *** TRS->matrix MISMATCH (max diff = {max_diff:.8f}) ***")
                print("  Expected from TRS:")
                print(mat4_str(expected_r))
                print("  Stored local_transform:")
                print(mat4_str(local))
            else:
                print(f"  TRS->matrix: OK (max diff = {max_diff:.2e})")

        print("  Local transform:")
        print(mat4_str(node["local_transform"]))
        print("  World transform (computed):")
        print(mat4_str(world[i]))
        print()


if __name__ == "__main__":
    main()
