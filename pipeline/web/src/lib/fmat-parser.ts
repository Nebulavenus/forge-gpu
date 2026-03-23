/**
 * Parser for .fmat material sidecar files (JSON format).
 *
 * Validates and normalizes PBR material data from the JSON format
 * produced by forge-mesh-tool.  No Three.js dependency — the Three.js
 * integration is in fmat-loader.ts.
 *
 * The JSON schema matches common/pipeline/forge_pipeline.h
 * ForgePipelineMaterial fields.
 *
 * @module fmat-parser
 * SPDX-License-Identifier: Zlib
 */

// ── Constants (MUST match common/pipeline/forge_pipeline.h) ─────────────

/** FORGE_PIPELINE_FMAT_VERSION */
export const FMAT_VERSION = 1

/** FORGE_PIPELINE_MAX_MATERIALS */
export const FMAT_MAX_MATERIALS = 256

/** Valid alpha mode strings matching glTF 2.0 spec. */
const VALID_ALPHA_MODES = new Set(["OPAQUE", "MASK", "BLEND"])

// ── Types ───────────────────────────────────────────────────────────────

export interface FmatMaterial {
  /** Material name (for debugging and UI display). */
  name: string
  /** RGBA base color multiplier, sRGB space, clamped to [0, 1]. */
  baseColorFactor: [number, number, number, number]
  /** Relative path to base color texture, or null if none. */
  baseColorTexture: string | null
  /** Metallic factor: 0 = dielectric, 1 = metal. */
  metallicFactor: number
  /** Roughness factor: 0 = mirror, 1 = rough. */
  roughnessFactor: number
  /** Relative path to metallic-roughness texture (B=metallic, G=roughness). */
  metallicRoughnessTexture: string | null
  /** Relative path to tangent-space normal map, or null. */
  normalTexture: string | null
  /** Normal map XY multiplier (default 1.0). */
  normalScale: number
  /** Relative path to ambient occlusion texture (R channel), or null. */
  occlusionTexture: string | null
  /** AO blend strength: 0 = none, 1 = full (default 1.0). */
  occlusionStrength: number
  /** RGB emissive color multiplier, clamped to [0, 1]. */
  emissiveFactor: [number, number, number]
  /** Relative path to emissive texture, or null. */
  emissiveTexture: string | null
  /** Transparency mode per glTF 2.0 spec. */
  alphaMode: "OPAQUE" | "MASK" | "BLEND"
  /** Alpha cutoff threshold for MASK mode (default 0.5). */
  alphaCutoff: number
  /** Render both faces if true. */
  doubleSided: boolean
}

export interface FmatData {
  /** Format version (must be 1). */
  version: number
  /** Array of PBR metallic-roughness materials. */
  materials: FmatMaterial[]
}

// ── Error class ─────────────────────────────────────────────────────────

export class FmatParseError extends Error {
  readonly detail: string | undefined

  constructor(message: string, detail?: string) {
    super(message)
    this.name = "FmatParseError"
    this.detail = detail
  }
}

// ── Helpers ─────────────────────────────────────────────────────────────

function clamp01(v: number): number {
  if (!isFinite(v)) return 0
  return Math.max(0, Math.min(1, v))
}


/**
 * Validate that a texture path is safe (no directory traversal or
 * absolute paths).
 */
function validateTexturePath(path: string, context: string): void {
  if (path.includes("..")) {
    throw new FmatParseError(
      `Texture path contains "..": ${context} = "${path}"`,
    )
  }
  if (path.startsWith("/") || path.startsWith("\\")) {
    throw new FmatParseError(
      `Texture path is absolute: ${context} = "${path}"`,
    )
  }
  // Block Windows absolute paths like C:\...
  if (/^[a-zA-Z]:/.test(path)) {
    throw new FmatParseError(
      `Texture path is absolute: ${context} = "${path}"`,
    )
  }
}

function parseOptionalTexture(
  raw: unknown,
  fieldName: string,
  materialName: string,
): string | null {
  if (raw === null || raw === undefined || raw === "") return null
  if (typeof raw !== "string") {
    throw new FmatParseError(
      `Material "${materialName}": ${fieldName} must be a string or null, ` +
      `got ${typeof raw}`,
    )
  }
  validateTexturePath(raw, `${materialName}.${fieldName}`)
  return raw
}

function parseColorArray(
  raw: unknown,
  length: 3 | 4,
  fieldName: string,
  materialName: string,
): number[] {
  if (!Array.isArray(raw) || raw.length !== length) {
    throw new FmatParseError(
      `Material "${materialName}": ${fieldName} must be an array of ` +
      `${length} numbers, got ${JSON.stringify(raw)}`,
    )
  }
  return raw.map((v, i) => {
    if (typeof v !== "number" || !isFinite(v)) {
      throw new FmatParseError(
        `Material "${materialName}": ${fieldName}[${i}] must be a finite ` +
        `number, got ${JSON.stringify(v)}`,
      )
    }
    return clamp01(v)
  })
}

// ── Parser ──────────────────────────────────────────────────────────────

/**
 * Parse .fmat JSON text into validated material data.
 *
 * @throws {FmatParseError} on any validation failure
 */
export function parseFmat(jsonText: string): FmatData {
  let parsed: unknown
  try {
    parsed = JSON.parse(jsonText)
  } catch (e) {
    throw new FmatParseError(
      `Invalid JSON in .fmat file: ${e instanceof Error ? e.message : String(e)}`,
    )
  }

  if (typeof parsed !== "object" || parsed === null || Array.isArray(parsed)) {
    throw new FmatParseError(
      "Invalid .fmat: root must be a JSON object",
    )
  }

  const root = parsed as Record<string, unknown>

  // Version
  if (root.version !== FMAT_VERSION) {
    throw new FmatParseError(
      `Unsupported .fmat version: expected ${FMAT_VERSION}, got ${JSON.stringify(root.version)}`,
    )
  }

  // Materials array
  if (!Array.isArray(root.materials)) {
    throw new FmatParseError(
      `Invalid .fmat: "materials" must be an array, got ${typeof root.materials}`,
    )
  }

  if (root.materials.length > FMAT_MAX_MATERIALS) {
    throw new FmatParseError(
      `Invalid .fmat: ${root.materials.length} materials exceeds maximum ${FMAT_MAX_MATERIALS}`,
    )
  }

  const materials: FmatMaterial[] = root.materials.map(
    (raw: unknown, i: number) => {
      if (typeof raw !== "object" || raw === null || Array.isArray(raw)) {
        throw new FmatParseError(
          `Invalid .fmat: materials[${i}] must be an object`,
        )
      }

      const m = raw as Record<string, unknown>
      const name = typeof m.name === "string" ? m.name : `material_${i}`

      // Parse color factors with clamping
      const baseColorFactor = parseColorArray(
        m.base_color_factor ?? [1, 1, 1, 1],
        4,
        "base_color_factor",
        name,
      ) as [number, number, number, number]

      const emissiveFactor = parseColorArray(
        m.emissive_factor ?? [0, 0, 0],
        3,
        "emissive_factor",
        name,
      ) as [number, number, number]

      // Parse numeric factors
      const metallicFactor = clamp01(
        typeof m.metallic_factor === "number" ? m.metallic_factor : 1.0,
      )
      const roughnessFactor = clamp01(
        typeof m.roughness_factor === "number" ? m.roughness_factor : 1.0,
      )
      // normalScale can be negative (flips the normal map direction per glTF spec)
      const rawNormalScale = typeof m.normal_scale === "number" ? m.normal_scale : 1.0
      const normalScale = isFinite(rawNormalScale) ? rawNormalScale : 1.0
      const occlusionStrength = clamp01(
        typeof m.occlusion_strength === "number" ? m.occlusion_strength : 1.0,
      )
      const alphaCutoff = clamp01(
        typeof m.alpha_cutoff === "number" ? m.alpha_cutoff : 0.5,
      )

      // Parse alpha mode
      const rawAlphaMode = typeof m.alpha_mode === "string"
        ? m.alpha_mode.toUpperCase()
        : "OPAQUE"
      if (!VALID_ALPHA_MODES.has(rawAlphaMode)) {
        throw new FmatParseError(
          `Material "${name}": invalid alpha_mode "${m.alpha_mode}" ` +
          `(must be OPAQUE, MASK, or BLEND)`,
        )
      }
      const alphaMode = rawAlphaMode as "OPAQUE" | "MASK" | "BLEND"

      // Parse textures
      const baseColorTexture = parseOptionalTexture(
        m.base_color_texture,
        "base_color_texture",
        name,
      )
      const metallicRoughnessTexture = parseOptionalTexture(
        m.metallic_roughness_texture,
        "metallic_roughness_texture",
        name,
      )
      const normalTexture = parseOptionalTexture(
        m.normal_texture,
        "normal_texture",
        name,
      )
      const occlusionTexture = parseOptionalTexture(
        m.occlusion_texture,
        "occlusion_texture",
        name,
      )
      const emissiveTexture = parseOptionalTexture(
        m.emissive_texture,
        "emissive_texture",
        name,
      )

      // Parse double_sided
      const doubleSided = typeof m.double_sided === "boolean"
        ? m.double_sided
        : false

      return {
        name,
        baseColorFactor,
        baseColorTexture,
        metallicFactor,
        roughnessFactor,
        metallicRoughnessTexture,
        normalTexture,
        normalScale,
        occlusionTexture,
        occlusionStrength,
        emissiveFactor,
        emissiveTexture,
        alphaMode,
        alphaCutoff,
        doubleSided,
      }
    },
  )

  return { version: FMAT_VERSION, materials }
}
