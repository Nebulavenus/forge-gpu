/**
 * Converts parsed .fmesh data into Three.js BufferGeometry.
 *
 * Builds geometry for a specific LOD level with per-submesh groups
 * so materials can be applied per-group.
 *
 * @module fmesh-geometry
 * SPDX-License-Identifier: Zlib
 */

import * as THREE from "three"
import type { FmeshData } from "./fmesh-parser"
import { FMESH_STRIDE_TAN } from "./fmesh-parser"

/**
 * Build a Three.js BufferGeometry from parsed .fmesh data for a given LOD.
 *
 * Creates separate Float32Array attribute buffers extracted from the
 * interleaved vertex data.  Each submesh in the LOD becomes a group
 * (via `addGroup`) so a material array can be applied.
 *
 * @param data - Parsed .fmesh data from `parseFmesh()`
 * @param lodIndex - Which LOD level to build geometry for (0 = full detail)
 * @returns A BufferGeometry with position, normal, uv, and optionally tangent
 *          attributes, plus one group per submesh.
 * @throws {RangeError} if lodIndex is out of bounds
 */
export function buildFmeshGeometry(
  data: FmeshData,
  lodIndex: number = 0,
): THREE.BufferGeometry {
  const { header, lods, vertexBuffer, vertexBufferOffset, hasTangents } = data

  if (lodIndex < 0 || lodIndex >= lods.length) {
    throw new RangeError(
      `LOD index ${lodIndex} out of range [0, ${lods.length - 1}]`,
    )
  }

  const { vertexCount, vertexStride } = header
  const lod = lods[lodIndex]!
  const geometry = new THREE.BufferGeometry()

  // ── Extract vertex attributes from interleaved buffer ─────────────
  //
  // Layout per vertex:
  //   position: 3 floats at offset 0   (12 bytes)
  //   normal:   3 floats at offset 12  (12 bytes)
  //   uv:       2 floats at offset 24  (8 bytes)
  //   tangent:  4 floats at offset 32  (16 bytes, stride-48 only)

  const positions = new Float32Array(vertexCount * 3)
  const normals = new Float32Array(vertexCount * 3)
  const uvs = new Float32Array(vertexCount * 2)
  const tangents = hasTangents ? new Float32Array(vertexCount * 4) : null

  const srcView = new DataView(vertexBuffer)

  for (let i = 0; i < vertexCount; i++) {
    const base = vertexBufferOffset + i * vertexStride

    // Position (offset 0)
    positions[i * 3 + 0] = srcView.getFloat32(base + 0, true)
    positions[i * 3 + 1] = srcView.getFloat32(base + 4, true)
    positions[i * 3 + 2] = srcView.getFloat32(base + 8, true)

    // Normal (offset 12)
    normals[i * 3 + 0] = srcView.getFloat32(base + 12, true)
    normals[i * 3 + 1] = srcView.getFloat32(base + 16, true)
    normals[i * 3 + 2] = srcView.getFloat32(base + 20, true)

    // UV (offset 24)
    uvs[i * 2 + 0] = srcView.getFloat32(base + 24, true)
    uvs[i * 2 + 1] = srcView.getFloat32(base + 28, true)

    // Tangent (offset 32, stride-48 only)
    if (tangents && vertexStride === FMESH_STRIDE_TAN) {
      tangents[i * 4 + 0] = srcView.getFloat32(base + 32, true)
      tangents[i * 4 + 1] = srcView.getFloat32(base + 36, true)
      tangents[i * 4 + 2] = srcView.getFloat32(base + 40, true)
      tangents[i * 4 + 3] = srcView.getFloat32(base + 44, true)
    }
  }

  geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3))
  geometry.setAttribute("normal", new THREE.BufferAttribute(normals, 3))
  geometry.setAttribute("uv", new THREE.BufferAttribute(uvs, 2))
  // Duplicate UVs as uv2 so Three.js MeshStandardMaterial can sample aoMap
  geometry.setAttribute("uv2", new THREE.BufferAttribute(uvs, 2))
  if (tangents) {
    geometry.setAttribute("tangent", new THREE.BufferAttribute(tangents, 4))
  }

  // ── Build index buffer for this LOD ───────────────────────────────
  //
  // Submesh index_offset is a byte offset.  We need to convert to an
  // element index into the flat indices array.  The first LOD's first
  // submesh offset serves as the base.

  const firstSubmesh = lods[0]?.submeshes[0]
  if (!firstSubmesh) {
    throw new RangeError("LOD 0 has no submeshes")
  }
  const baseOffset = firstSubmesh.indexOffset
  let totalLodIndices = 0
  for (const sub of lod.submeshes) {
    totalLodIndices += sub.indexCount
  }

  const lodIndices = new Uint32Array(totalLodIndices)
  let destPos = 0

  for (const sub of lod.submeshes) {
    // Convert byte offset to element index relative to the index array start
    const srcStart = (sub.indexOffset - baseOffset) / 4
    for (let i = 0; i < sub.indexCount; i++) {
      lodIndices[destPos++] = data.indices[srcStart + i]!
    }
  }

  geometry.setIndex(new THREE.BufferAttribute(lodIndices, 1))

  // ── Per-submesh groups for multi-material rendering ────────────────

  let groupStart = 0
  for (let s = 0; s < lod.submeshes.length; s++) {
    const sub = lod.submeshes[s]!
    geometry.addGroup(groupStart, sub.indexCount, sub.materialIndex >= 0 ? sub.materialIndex : 0)
    groupStart += sub.indexCount
  }

  geometry.computeBoundingSphere()
  geometry.computeBoundingBox()

  return geometry
}

/**
 * Compute mesh statistics for a given LOD.
 */
export function getFmeshLodStats(
  data: FmeshData,
  lodIndex: number,
): { vertexCount: number; triangleCount: number; submeshCount: number } {
  if (lodIndex < 0 || lodIndex >= data.lods.length) {
    throw new RangeError(
      `LOD index ${lodIndex} out of range [0, ${data.lods.length - 1}]`,
    )
  }
  const lod = data.lods[lodIndex]!
  let totalIndices = 0
  for (const sub of lod.submeshes) {
    totalIndices += sub.indexCount
  }
  return {
    vertexCount: data.header.vertexCount,
    triangleCount: Math.floor(totalIndices / 3),
    submeshCount: lod.submeshes.length,
  }
}
