/**
 * Pure binary parser for .ftex files (pre-transcoded GPU-ready textures).
 *
 * Reads the binary format produced by forge-texture-tool and returns
 * typed data structures.  No Three.js dependency — the Three.js
 * integration is in ftex-loader.ts.
 *
 * The binary layout matches common/pipeline/forge_pipeline.h.
 *
 * @module ftex-parser
 * SPDX-License-Identifier: Zlib
 */

// ── Constants (MUST match common/pipeline/forge_pipeline.h) ─────────────

/** FORGE_PIPELINE_FTEX_MAGIC — 0x58455446 = "FTEX" little-endian */
export const FTEX_MAGIC = 0x58455446

/** FORGE_PIPELINE_FTEX_VERSION */
export const FTEX_VERSION = 1

/** FORGE_PIPELINE_FTEX_HEADER_SIZE — 8 × uint32 fields */
export const FTEX_HEADER_SIZE = 32

/** FORGE_PIPELINE_FTEX_MIP_ENTRY_SIZE — 4 × uint32 per mip */
export const FTEX_MIP_ENTRY_SIZE = 16

/** FORGE_PIPELINE_FTEX_MAX_MIP_LEVELS */
export const FTEX_MAX_MIP_LEVELS = 32

/** FORGE_PIPELINE_COMPRESSED_BC7_SRGB */
export const FTEX_FORMAT_BC7_SRGB = 1

/** FORGE_PIPELINE_COMPRESSED_BC7_UNORM */
export const FTEX_FORMAT_BC7_UNORM = 2

/** FORGE_PIPELINE_COMPRESSED_BC5_UNORM */
export const FTEX_FORMAT_BC5_UNORM = 3

/** Maximum supported texture dimension (prevents integer overflow in block math) */
export const FTEX_MAX_DIMENSION = 32768

/** Bytes per 4×4 block for BC7 and BC5 formats */
const BLOCK_SIZE = 16

// ── Types ───────────────────────────────────────────────────────────────

export interface FtexMip {
  /** Byte offset of this mip's block data within the file. */
  dataOffset: number
  /** Byte count of compressed block data for this mip level. */
  dataSize: number
  /** Mip level width in pixels. */
  width: number
  /** Mip level height in pixels. */
  height: number
  /** Slice of the source ArrayBuffer containing the GPU-ready block data. */
  data: ArrayBuffer
}

export interface FtexData {
  /** Compressed format: 1=BC7_SRGB, 2=BC7_UNORM, 3=BC5_UNORM. */
  format: number
  /** Base (mip 0) width in pixels. */
  width: number
  /** Base (mip 0) height in pixels. */
  height: number
  /** Number of mip levels in the chain. */
  mipCount: number
  /** Per-mip block data, ordered from largest to smallest. */
  mips: FtexMip[]
}

// ── Error class ─────────────────────────────────────────────────────────

export class FtexParseError extends Error {
  readonly offset: number | undefined
  readonly detail: string | undefined

  constructor(
    message: string,
    options?: { offset?: number; detail?: string },
  ) {
    super(message)
    this.name = "FtexParseError"
    this.offset = options?.offset
    this.detail = options?.detail
  }
}

// ── Parser ──────────────────────────────────────────────────────────────

/**
 * Parse a .ftex binary buffer into structured data.
 *
 * @throws {FtexParseError} on any validation failure
 */
export function parseFtex(buffer: ArrayBuffer): FtexData {
  const byteLength = buffer.byteLength

  if (byteLength < FTEX_HEADER_SIZE) {
    throw new FtexParseError(
      `Buffer too small for .ftex header: need ${FTEX_HEADER_SIZE} bytes, got ${byteLength}`,
      { offset: 0 },
    )
  }

  const view = new DataView(buffer)

  // Magic (bytes 0-3)
  const magic = view.getUint32(0, true)
  if (magic !== FTEX_MAGIC) {
    throw new FtexParseError(
      `Invalid .ftex magic: expected 0x${FTEX_MAGIC.toString(16)}, got 0x${magic.toString(16)}`,
      { offset: 0 },
    )
  }

  // Version (bytes 4-7)
  const version = view.getUint32(4, true)
  if (version !== FTEX_VERSION) {
    throw new FtexParseError(
      `Unsupported .ftex version: expected ${FTEX_VERSION}, got ${version}`,
      { offset: 4 },
    )
  }

  // Format (bytes 8-11)
  const format = view.getUint32(8, true)
  if (
    format !== FTEX_FORMAT_BC7_SRGB &&
    format !== FTEX_FORMAT_BC7_UNORM &&
    format !== FTEX_FORMAT_BC5_UNORM
  ) {
    throw new FtexParseError(
      `Unknown .ftex format: ${format} (expected 1=BC7_SRGB, 2=BC7_UNORM, or 3=BC5_UNORM)`,
      { offset: 8 },
    )
  }

  // Dimensions (bytes 12-19)
  const width = view.getUint32(12, true)
  const height = view.getUint32(16, true)
  if (width === 0 || height === 0) {
    throw new FtexParseError(
      `Invalid .ftex dimensions: ${width}×${height}`,
      { offset: 12 },
    )
  }
  if (width > FTEX_MAX_DIMENSION || height > FTEX_MAX_DIMENSION) {
    throw new FtexParseError(
      `Invalid .ftex dimensions: ${width}×${height} exceeds maximum ${FTEX_MAX_DIMENSION}`,
      { offset: 12 },
    )
  }

  // Mip count (bytes 20-23)
  const mipCount = view.getUint32(20, true)
  if (mipCount === 0 || mipCount > FTEX_MAX_MIP_LEVELS) {
    throw new FtexParseError(
      `Invalid .ftex mip_count: ${mipCount} (max ${FTEX_MAX_MIP_LEVELS})`,
      { offset: 20 },
    )
  }

  // Reserved (bytes 24-31) — ignored

  // ── Mip entry table ────────────────────────────────────────────────

  const mipTableEnd = FTEX_HEADER_SIZE + mipCount * FTEX_MIP_ENTRY_SIZE
  if (mipTableEnd > byteLength) {
    throw new FtexParseError(
      `Buffer too small for mip table: need ${mipTableEnd} bytes, got ${byteLength}`,
      { offset: FTEX_HEADER_SIZE },
    )
  }

  const mips: FtexMip[] = []
  let expectedWidth = width
  let expectedHeight = height

  for (let i = 0; i < mipCount; i++) {
    const entryOffset = FTEX_HEADER_SIZE + i * FTEX_MIP_ENTRY_SIZE
    const dataOffset = view.getUint32(entryOffset, true)
    const dataSize = view.getUint32(entryOffset + 4, true)
    const mipWidth = view.getUint32(entryOffset + 8, true)
    const mipHeight = view.getUint32(entryOffset + 12, true)

    // Validate mip dimensions match expected halving sequence
    if (mipWidth !== expectedWidth || mipHeight !== expectedHeight) {
      throw new FtexParseError(
        `Mip ${i} dimensions ${mipWidth}×${mipHeight} do not match ` +
        `expected ${expectedWidth}×${expectedHeight}`,
        { offset: entryOffset + 8 },
      )
    }

    // Validate block data size is consistent with dimensions
    const blocksX = Math.ceil(mipWidth / 4)
    const blocksY = Math.ceil(mipHeight / 4)
    const expectedDataSize = blocksX * blocksY * BLOCK_SIZE
    if (dataSize !== expectedDataSize) {
      throw new FtexParseError(
        `Mip ${i} data_size ${dataSize} does not match expected ` +
        `${expectedDataSize} (${blocksX}×${blocksY} blocks × ${BLOCK_SIZE} bytes)`,
        { offset: entryOffset + 4 },
      )
    }

    // Validate data fits within buffer
    if (dataOffset + dataSize > byteLength) {
      throw new FtexParseError(
        `Mip ${i} data extends beyond buffer: offset ${dataOffset} + ` +
        `size ${dataSize} > ${byteLength}`,
        { offset: entryOffset },
      )
    }

    mips.push({
      dataOffset,
      dataSize,
      width: mipWidth,
      height: mipHeight,
      data: buffer.slice(dataOffset, dataOffset + dataSize),
    })

    // Next mip is half the size (minimum 1)
    expectedWidth = Math.max(1, Math.floor(expectedWidth / 2))
    expectedHeight = Math.max(1, Math.floor(expectedHeight / 2))
  }

  return { format, width, height, mipCount, mips }
}
