/**
 * Pure binary parser for .fmesh v2/v3 files.
 *
 * Reads the binary mesh format produced by forge-mesh-tool and returns
 * typed data structures suitable for building Three.js BufferGeometry.
 * No Three.js dependency — this module works in any TypeScript environment.
 *
 * The binary layout matches common/pipeline/forge_pipeline.h.
 * Constants here are annotated with the corresponding C define name.
 *
 * @module fmesh-parser
 * SPDX-License-Identifier: Zlib
 */

// ── Constants (MUST match common/pipeline/forge_pipeline.h) ─────────────

/** FORGE_PIPELINE_FMESH_MAGIC — 4-byte ASCII identifier */
export const FMESH_MAGIC = "FMSH"

/** FORGE_PIPELINE_HEADER_SIZE — total header size in bytes */
export const FMESH_HEADER_SIZE = 32

/** FORGE_PIPELINE_FMESH_VERSION — standard mesh format version */
export const FMESH_VERSION_V2 = 2

/** FORGE_PIPELINE_FMESH_VERSION_SKIN — skinned/morph format version */
export const FMESH_VERSION_V3 = 3

/** FORGE_PIPELINE_MAX_LODS — upper bound for LOD count validation */
export const FMESH_MAX_LODS = 8

/** FORGE_PIPELINE_MAX_SUBMESHES — upper bound for submesh count validation */
export const FMESH_MAX_SUBMESHES = 64

/** FORGE_PIPELINE_VERTEX_STRIDE_NO_TAN — vertex without tangent data */
export const FMESH_STRIDE_NO_TAN = 32

/** FORGE_PIPELINE_VERTEX_STRIDE_TAN — vertex with tangent data */
export const FMESH_STRIDE_TAN = 48

/** FORGE_PIPELINE_FLAG_TANGENTS — bit 0 of flags field */
export const FMESH_FLAG_TANGENTS = 1 << 0

/** FORGE_PIPELINE_FLAG_SKINNED — bit 1 of flags field */
export const FMESH_FLAG_SKINNED = 1 << 1

/** FORGE_PIPELINE_FLAG_MORPHS — bit 2 of flags field */
export const FMESH_FLAG_MORPHS = 1 << 2

/** Sanity upper bound for vertex count to prevent OOM on corrupt files */
const MAX_VERTEX_COUNT = 16 * 1024 * 1024 // 16M vertices

/** Sanity upper bound for total index count to prevent OOM on corrupt files */
const MAX_INDEX_COUNT = 64 * 1024 * 1024 // 64M indices

// ── Types ───────────────────────────────────────────────────────────────

export interface FmeshHeader {
  /** Format version: 2 = standard, 3 = skinned/morph. */
  version: number
  /** Total number of vertices in the vertex buffer. */
  vertexCount: number
  /** Bytes per vertex: 32 (no tangent) or 48 (with tangent). */
  vertexStride: number
  /** Number of LOD levels (1 = full detail only, up to 8). */
  lodCount: number
  /** Bit field: bit 0 = tangents, bit 1 = skinned, bit 2 = morphs. */
  flags: number
  /** Number of submeshes (material groups) per LOD level. */
  submeshCount: number
}

export interface FmeshSubmesh {
  /** Number of indices in this submesh. */
  indexCount: number
  /** Byte offset into the index data section. */
  indexOffset: number
  /** Index into the .fmat material array, or -1 for no material. */
  materialIndex: number
}

export interface FmeshLod {
  /** Meshoptimizer simplification error metric (0.0 = full detail). */
  targetError: number
  /** Per-submesh index ranges for this LOD level. */
  submeshes: FmeshSubmesh[]
}

export interface FmeshData {
  /** Parsed header fields. */
  header: FmeshHeader
  /** LOD levels with per-submesh index ranges. */
  lods: FmeshLod[]
  /** Raw vertex bytes — use stride to interpret as interleaved attributes. */
  vertexBuffer: ArrayBuffer
  /** Byte offset into the source buffer where vertex data begins. */
  vertexBufferOffset: number
  /** All indices concatenated across all LODs and submeshes. */
  indices: Uint32Array
  /** True when vertex stride is 48 (tangent data present). */
  hasTangents: boolean
}

// ── Error class ─────────────────────────────────────────────────────────

export class FmeshParseError extends Error {
  /** Byte offset in the buffer where the error was detected, if applicable. */
  readonly offset: number | undefined
  /** Additional diagnostic context. */
  readonly detail: string | undefined

  constructor(
    message: string,
    options?: { offset?: number; detail?: string },
  ) {
    super(message)
    this.name = "FmeshParseError"
    this.offset = options?.offset
    this.detail = options?.detail
  }
}

// ── Parser ──────────────────────────────────────────────────────────────

/**
 * Parse a .fmesh binary buffer into structured data.
 *
 * Performs thorough validation at every step:
 * - Magic, version, stride, flag consistency
 * - Bounds checks before every DataView read
 * - Max-value guards to prevent OOM on corrupt data
 *
 * @throws {FmeshParseError} on any validation or bounds failure
 */
export function parseFmesh(buffer: ArrayBuffer): FmeshData {
  const byteLength = buffer.byteLength

  // ── Header validation ──────────────────────────────────────────────

  if (byteLength < FMESH_HEADER_SIZE) {
    throw new FmeshParseError(
      `Buffer too small for .fmesh header: need ${FMESH_HEADER_SIZE} bytes, got ${byteLength}`,
      { offset: 0 },
    )
  }

  const view = new DataView(buffer)

  // Magic (bytes 0-3)
  const magic = String.fromCharCode(
    view.getUint8(0),
    view.getUint8(1),
    view.getUint8(2),
    view.getUint8(3),
  )
  if (magic !== FMESH_MAGIC) {
    throw new FmeshParseError(
      `Invalid .fmesh magic: expected "${FMESH_MAGIC}", got "${magic}"`,
      { offset: 0 },
    )
  }

  // Version (bytes 4-7)
  const version = view.getUint32(4, true)
  if (version !== FMESH_VERSION_V2 && version !== FMESH_VERSION_V3) {
    throw new FmeshParseError(
      `Unsupported .fmesh version: expected ${FMESH_VERSION_V2} or ${FMESH_VERSION_V3}, got ${version}`,
      { offset: 4 },
    )
  }

  // Vertex count (bytes 8-11)
  const vertexCount = view.getUint32(8, true)
  if (vertexCount === 0) {
    throw new FmeshParseError("Invalid .fmesh: vertex_count is 0", {
      offset: 8,
    })
  }
  if (vertexCount > MAX_VERTEX_COUNT) {
    throw new FmeshParseError(
      `Invalid .fmesh: vertex_count ${vertexCount} exceeds maximum ${MAX_VERTEX_COUNT}`,
      { offset: 8 },
    )
  }

  // Vertex stride (bytes 12-15)
  const vertexStride = view.getUint32(12, true)
  if (vertexStride !== FMESH_STRIDE_NO_TAN && vertexStride !== FMESH_STRIDE_TAN) {
    throw new FmeshParseError(
      `Invalid .fmesh vertex_stride: expected ${FMESH_STRIDE_NO_TAN} or ${FMESH_STRIDE_TAN}, got ${vertexStride}`,
      { offset: 12 },
    )
  }

  // LOD count (bytes 16-19)
  const lodCount = view.getUint32(16, true)
  if (lodCount === 0) {
    throw new FmeshParseError("Invalid .fmesh: lod_count is 0", {
      offset: 16,
    })
  }
  if (lodCount > FMESH_MAX_LODS) {
    throw new FmeshParseError(
      `Invalid .fmesh: lod_count ${lodCount} exceeds maximum ${FMESH_MAX_LODS}`,
      { offset: 16 },
    )
  }

  // Flags (bytes 20-23)
  const flags = view.getUint32(20, true)
  const knownFlags = FMESH_FLAG_TANGENTS | FMESH_FLAG_SKINNED | FMESH_FLAG_MORPHS
  if ((flags & ~knownFlags) !== 0) {
    throw new FmeshParseError(
      `Invalid .fmesh flags: unknown bits set (0x${flags.toString(16)})`,
      { offset: 20 },
    )
  }
  if (
    version === FMESH_VERSION_V2 &&
    (flags & (FMESH_FLAG_SKINNED | FMESH_FLAG_MORPHS)) !== 0
  ) {
    throw new FmeshParseError(
      "Invalid .fmesh: v2 format cannot set skinned/morph flags",
      { offset: 20, detail: `flags=0x${flags.toString(16)}` },
    )
  }

  // Submesh count (bytes 24-27)
  const submeshCount = view.getUint32(24, true)
  if (submeshCount === 0) {
    throw new FmeshParseError("Invalid .fmesh: submesh_count is 0", {
      offset: 24,
    })
  }
  if (submeshCount > FMESH_MAX_SUBMESHES) {
    throw new FmeshParseError(
      `Invalid .fmesh: submesh_count ${submeshCount} exceeds maximum ${FMESH_MAX_SUBMESHES}`,
      { offset: 24 },
    )
  }

  // Reserved (bytes 28-31) — ignored

  // ── Flag/stride consistency ────────────────────────────────────────

  const hasTangents = (flags & FMESH_FLAG_TANGENTS) !== 0
  if (hasTangents && vertexStride === FMESH_STRIDE_NO_TAN) {
    throw new FmeshParseError(
      "Flag/stride mismatch: FLAG_TANGENTS is set but vertex_stride is 32 (no tangent data)",
      { offset: 20, detail: `flags=0x${flags.toString(16)}, stride=${vertexStride}` },
    )
  }
  if (!hasTangents && vertexStride === FMESH_STRIDE_TAN) {
    throw new FmeshParseError(
      "Flag/stride mismatch: FLAG_TANGENTS is not set but vertex_stride is 48 (tangent data present)",
      { offset: 20, detail: `flags=0x${flags.toString(16)}, stride=${vertexStride}` },
    )
  }

  // ── LOD-submesh table ──────────────────────────────────────────────

  // Each LOD entry: 4 bytes (target_error) + submeshCount * 12 bytes
  const perLodSize = 4 + submeshCount * 12
  const lodTableSize = lodCount * perLodSize
  const lodTableEnd = FMESH_HEADER_SIZE + lodTableSize

  if (lodTableEnd > byteLength) {
    throw new FmeshParseError(
      `Buffer too small for LOD-submesh table: need ${lodTableEnd} bytes, got ${byteLength}`,
      { offset: FMESH_HEADER_SIZE },
    )
  }

  const lods: FmeshLod[] = []
  let pos = FMESH_HEADER_SIZE

  for (let lod = 0; lod < lodCount; lod++) {
    const targetError = view.getFloat32(pos, true)
    pos += 4

    const submeshes: FmeshSubmesh[] = []
    for (let s = 0; s < submeshCount; s++) {
      const indexCount = view.getUint32(pos, true)
      const indexOffset = view.getUint32(pos + 4, true)
      const materialIndex = view.getInt32(pos + 8, true)
      submeshes.push({ indexCount, indexOffset, materialIndex })
      pos += 12
    }

    lods.push({ targetError, submeshes })
  }

  // ── Vertex data section ────────────────────────────────────────────

  const vertexDataSize = vertexCount * vertexStride
  const vertexSectionStart = pos
  const vertexSectionEnd = vertexSectionStart + vertexDataSize

  if (vertexSectionEnd > byteLength) {
    throw new FmeshParseError(
      `Buffer too small for vertex data: need ${vertexSectionEnd} bytes, got ${byteLength}`,
      { offset: vertexSectionStart },
    )
  }

  // ── Index data section ─────────────────────────────────────────────

  // Compute required index byte span from explicit offsets and counts.
  // This catches malformed/sparse/overlapping submesh references that
  // a simple sum(indexCount) would miss.
  let totalIndexCount = 0
  let requiredIndexBytes = 0
  for (const lod of lods) {
    for (const sub of lod.submeshes) {
      if (sub.indexOffset % 4 !== 0) {
        throw new FmeshParseError(
          `Submesh index_offset ${sub.indexOffset} is not 4-byte aligned`,
          { offset: pos },
        )
      }
      const subBytes = sub.indexCount * 4
      const subEnd = sub.indexOffset + subBytes
      if (subEnd > requiredIndexBytes) requiredIndexBytes = subEnd
      totalIndexCount += sub.indexCount
    }
  }

  if (totalIndexCount > MAX_INDEX_COUNT) {
    throw new FmeshParseError(
      `Invalid .fmesh: total index count ${totalIndexCount} exceeds maximum ${MAX_INDEX_COUNT}`,
      { offset: vertexSectionEnd },
    )
  }

  const indexSectionStart = vertexSectionEnd
  const indexDataSize = requiredIndexBytes
  const indexSectionEnd = indexSectionStart + indexDataSize

  if (indexSectionEnd > byteLength) {
    throw new FmeshParseError(
      `Buffer too small for index data: need ${indexSectionEnd} bytes, got ${byteLength}`,
      { offset: indexSectionStart },
    )
  }

  // Verify 4-byte alignment required by Uint32Array
  if (indexSectionStart % 4 !== 0) {
    throw new FmeshParseError(
      `Index section offset ${indexSectionStart} is not 4-byte aligned`,
      { offset: indexSectionStart },
    )
  }

  // Create a Uint32Array view over the index data
  const indexCountInSection = indexDataSize / 4
  const indices = new Uint32Array(buffer, indexSectionStart, indexCountInSection)

  // Validate index values per-submesh — no index should exceed vertexCount
  for (const lod of lods) {
    for (const sub of lod.submeshes) {
      const start = sub.indexOffset / 4
      const end = start + sub.indexCount
      for (let i = start; i < end; i++) {
        if (indices[i]! >= vertexCount) {
          throw new FmeshParseError(
            `Index out of bounds at position ${i}: value ${indices[i]!} >= vertex_count ${vertexCount}`,
            { offset: indexSectionStart + i * 4 },
          )
        }
      }
    }
  }

  const header: FmeshHeader = {
    version,
    vertexCount,
    vertexStride,
    lodCount,
    flags,
    submeshCount,
  }

  return {
    header,
    lods,
    vertexBuffer: buffer,
    vertexBufferOffset: vertexSectionStart,
    indices,
    hasTangents,
  }
}
