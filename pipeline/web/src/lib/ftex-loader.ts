/**
 * Three.js loader for .ftex compressed textures.
 *
 * Loads .ftex binary files as THREE.CompressedTexture.  Probes WebGL2
 * extensions for BC7 and BC5 support.  Throws when the required
 * extension is unavailable — callers should catch and fall back to
 * source PNG textures.
 *
 * @module ftex-loader
 * SPDX-License-Identifier: Zlib
 */

import * as THREE from "three"
import {
  parseFtex,
  FTEX_FORMAT_BC7_SRGB,
  FTEX_FORMAT_BC7_UNORM,
  FTEX_FORMAT_BC5_UNORM,
} from "./ftex-parser"

// ── Extension support detection ─────────────────────────────────────────

export interface CompressedTextureSupport {
  bc7: boolean
  bc5: boolean
}

/**
 * Check which compressed texture formats the renderer supports.
 *
 * BC7 requires EXT_texture_compression_bptc (WebGL2 desktop).
 * BC5 requires EXT_texture_compression_rgtc (WebGL2 desktop).
 */
export function checkCompressedTextureSupport(
  renderer: THREE.WebGLRenderer,
): CompressedTextureSupport {
  const gl = renderer.getContext()
  return {
    bc7: gl.getExtension("EXT_texture_compression_bptc") !== null,
    bc5: gl.getExtension("EXT_texture_compression_rgtc") !== null,
  }
}

// ── Format mapping ──────────────────────────────────────────────────────

/**
 * Map .ftex format enum to Three.js compressed texture format constant.
 * Returns null if the format is not supported by the renderer.
 */
function getThreeFormat(
  ftexFormat: number,
  support: CompressedTextureSupport,
): number | null {
  switch (ftexFormat) {
    case FTEX_FORMAT_BC7_SRGB:
      // THREE.RGBA_BPTC_Format with sRGB encoding
      return support.bc7 ? THREE.RGBA_BPTC_Format : null
    case FTEX_FORMAT_BC7_UNORM:
      return support.bc7 ? THREE.RGBA_BPTC_Format : null
    case FTEX_FORMAT_BC5_UNORM: {
      // RED_GREEN_RGTC2_Format for two-channel normal maps
      const format = (THREE as Record<string, unknown>).RED_GREEN_RGTC2_Format
      return support.bc5 && typeof format === "number" ? format : null
    }
    default:
      return null
  }
}

// ── Loader ──────────────────────────────────────────────────────────────

/**
 * Load a .ftex binary as a Three.js CompressedTexture.
 *
 * @param buffer - The raw .ftex file bytes
 * @param renderer - WebGL renderer for extension probing
 * @returns CompressedTexture ready for GPU upload
 * @throws {Error} if the required compression extension is unavailable
 */
export function loadFtexTexture(
  buffer: ArrayBuffer,
  renderer: THREE.WebGLRenderer,
): THREE.CompressedTexture {
  const data = parseFtex(buffer)
  const support = checkCompressedTextureSupport(renderer)
  const threeFormat = getThreeFormat(data.format, support)

  if (threeFormat === null) {
    const formatName =
      data.format === FTEX_FORMAT_BC7_SRGB ? "BC7_SRGB" :
      data.format === FTEX_FORMAT_BC7_UNORM ? "BC7_UNORM" :
      data.format === FTEX_FORMAT_BC5_UNORM ? "BC5_UNORM" :
      `unknown(${data.format})`
    throw new Error(
      `Compressed texture format ${formatName} is not supported by this ` +
      `renderer. BC7 requires EXT_texture_compression_bptc, ` +
      `BC5 requires EXT_texture_compression_rgtc.`,
    )
  }

  // Build mipmaps array for CompressedTexture
  const mipmaps: THREE.CompressedTextureMipmap[] = data.mips.map((mip) => ({
    data: new Uint8Array(mip.data),
    width: mip.width,
    height: mip.height,
  }))

  const texture = new THREE.CompressedTexture(
    mipmaps,
    data.width,
    data.height,
    threeFormat as THREE.CompressedPixelFormat,
  )

  // Set color space based on format
  if (data.format === FTEX_FORMAT_BC7_SRGB) {
    texture.colorSpace = THREE.SRGBColorSpace
  } else {
    texture.colorSpace = THREE.LinearSRGBColorSpace
  }

  // Enable mipmaps if multiple levels are present
  texture.minFilter = data.mipCount > 1
    ? THREE.LinearMipmapLinearFilter
    : THREE.LinearFilter
  texture.magFilter = THREE.LinearFilter
  texture.wrapS = THREE.RepeatWrapping
  texture.wrapT = THREE.RepeatWrapping
  texture.needsUpdate = true

  return texture
}
