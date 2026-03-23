/**
 * Three.js material loader for .fmat JSON sidecar files.
 *
 * Creates THREE.MeshStandardMaterial instances from parsed .fmat data.
 * Supports a texture resolver callback for loading textures through
 * the pipeline API (with .ftex → PNG fallback).
 *
 * @module fmat-loader
 * SPDX-License-Identifier: Zlib
 */

import * as THREE from "three"
import type { FmatMaterial } from "./fmat-parser"
import { parseFmat } from "./fmat-parser"

// ── Types ───────────────────────────────────────────────────────────────

/**
 * Callback that resolves a texture path to a Three.js Texture.
 * Returns null if the texture cannot be loaded (caller uses defaults).
 */
export type TextureResolver = (
  path: string,
) => Promise<THREE.Texture | null>

// ── Helpers ─────────────────────────────────────────────────────────────

function mapAlphaMode(
  mode: "OPAQUE" | "MASK" | "BLEND",
): { transparent: boolean; alphaTest: number; depthWrite: boolean } {
  switch (mode) {
    case "BLEND":
      return { transparent: true, alphaTest: 0, depthWrite: false }
    case "MASK":
      return { transparent: false, alphaTest: 0.5, depthWrite: true }
    case "OPAQUE":
    default:
      return { transparent: false, alphaTest: 0, depthWrite: true }
  }
}

// ── Material builder ────────────────────────────────────────────────────

/**
 * Build a Three.js MeshStandardMaterial from a parsed FmatMaterial.
 */
async function buildMaterial(
  mat: FmatMaterial,
  resolveTexture: TextureResolver,
): Promise<THREE.MeshStandardMaterial> {
  const alpha = mapAlphaMode(mat.alphaMode)

  // glTF 2.0 specifies baseColorFactor and emissiveFactor as linear values.
  // Pass them directly — do not apply sRGB-to-linear conversion, which would
  // double-decode.  Three.js color management handles the display transform.
  const material = new THREE.MeshStandardMaterial({
    color: new THREE.Color(
      mat.baseColorFactor[0],
      mat.baseColorFactor[1],
      mat.baseColorFactor[2],
    ),
    opacity: mat.baseColorFactor[3],
    metalness: mat.metallicFactor,
    roughness: mat.roughnessFactor,
    normalScale: new THREE.Vector2(mat.normalScale, mat.normalScale),
    aoMapIntensity: mat.occlusionStrength,
    emissive: new THREE.Color(
      mat.emissiveFactor[0],
      mat.emissiveFactor[1],
      mat.emissiveFactor[2],
    ),
    transparent: alpha.transparent,
    alphaTest: mat.alphaMode === "MASK" ? mat.alphaCutoff : alpha.alphaTest,
    depthWrite: alpha.depthWrite,
    side: mat.doubleSided ? THREE.DoubleSide : THREE.FrontSide,
    name: mat.name,
  })

  // Load textures in parallel
  const texturePromises: Promise<void>[] = []

  if (mat.baseColorTexture) {
    texturePromises.push(
      resolveTexture(mat.baseColorTexture).then((tex) => {
        if (tex) {
          tex.colorSpace = THREE.SRGBColorSpace
          material.map = tex
          material.needsUpdate = true
        }
      }),
    )
  }

  if (mat.metallicRoughnessTexture) {
    texturePromises.push(
      resolveTexture(mat.metallicRoughnessTexture).then((tex) => {
        if (tex) {
          material.metalnessMap = tex
          material.roughnessMap = tex
          material.needsUpdate = true
        }
      }),
    )
  }

  if (mat.normalTexture) {
    texturePromises.push(
      resolveTexture(mat.normalTexture).then((tex) => {
        if (tex) {
          material.normalMap = tex
          material.needsUpdate = true
        }
      }),
    )
  }

  if (mat.occlusionTexture) {
    texturePromises.push(
      resolveTexture(mat.occlusionTexture).then((tex) => {
        if (tex) {
          material.aoMap = tex
          material.needsUpdate = true
        }
      }),
    )
  }

  if (mat.emissiveTexture) {
    texturePromises.push(
      resolveTexture(mat.emissiveTexture).then((tex) => {
        if (tex) {
          tex.colorSpace = THREE.SRGBColorSpace
          material.emissiveMap = tex
          material.needsUpdate = true
        }
      }),
    )
  }

  await Promise.all(texturePromises)

  return material
}

// ── Public API ──────────────────────────────────────────────────────────

/**
 * Load materials from .fmat JSON text.
 *
 * @param jsonText - Raw .fmat file contents
 * @param resolveTexture - Callback to load textures by path
 * @returns Array of Three.js materials, one per .fmat material entry
 */
export async function loadFmatMaterials(
  jsonText: string,
  resolveTexture: TextureResolver,
): Promise<THREE.MeshStandardMaterial[]> {
  const data = parseFmat(jsonText)
  return Promise.all(
    data.materials.map((mat) => buildMaterial(mat, resolveTexture)),
  )
}
