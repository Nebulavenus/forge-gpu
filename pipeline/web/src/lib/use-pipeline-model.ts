/**
 * React hook for loading pipeline-processed 3D models.
 *
 * Orchestrates loading .fmesh + .fmat + textures and returns a
 * Three.js Group ready to render.  Caches the parsed binary data
 * so switching LOD levels only rebuilds geometry without re-fetching.
 *
 * @module use-pipeline-model
 * SPDX-License-Identifier: Zlib
 */

import { useCallback, useEffect, useMemo, useRef, useState } from "react"
import { useThree } from "@react-three/fiber"
import * as THREE from "three"
import { parseFmesh, type FmeshData } from "./fmesh-parser"
import { buildFmeshGeometry } from "./fmesh-geometry"
import { loadFmatMaterials, type TextureResolver } from "./fmat-loader"
import { loadFtexTexture } from "./ftex-loader"

// ── Types ───────────────────────────────────────────────────────────────

export interface PipelineModelResult {
  /** The Three.js scene graph object (Group containing meshes). */
  scene: THREE.Group | null
  /** Number of LOD levels available. */
  lodCount: number
  /** True while loading is in progress. */
  loading: boolean
  /** Error that occurred during loading, if any. */
  error: Error | null
  /** The parsed .fmesh data (for stats display, LOD info, etc.). */
  fmeshData: FmeshData | null
}

// ── Texture resolver factory ────────────────────────────────────────────

/**
 * Create a texture resolver that tries .ftex first, then falls back
 * to the source PNG through the companions endpoint.
 */
function createTextureResolver(
  assetId: string,
  renderer: THREE.WebGLRenderer,
  signal: AbortSignal,
): TextureResolver {
  return async (texturePath: string) => {
    const encoded = encodeURIComponent(assetId)

    // Bail early if the hook has already been unmounted / re-keyed
    if (signal.aborted) return null

    // Try .ftex compressed version first
    const ftexPath = texturePath.replace(/\.[^.]+$/, ".ftex")
    try {
      const ftexUrl = `/api/assets/${encoded}/companions?variant=processed&path=${encodeURIComponent(ftexPath)}`
      const resp = await fetch(ftexUrl, { signal })
      if (resp.ok) {
        const buffer = await resp.arrayBuffer()
        if (signal.aborted) return null
        return loadFtexTexture(buffer, renderer)
      }
    } catch {
      if (signal.aborted) return null
      // .ftex not available or not supported — fall back to source
    }

    // Fall back to source texture through companions endpoint.
    // Use fetch + Image for reliable cross-context loading.
    // Set flipY=false because .fmesh stores glTF UVs (V=0 at top).
    try {
      const srcUrl = `/api/assets/${encoded}/companions?path=${encodeURIComponent(texturePath)}`
      const imgResp = await fetch(srcUrl, { signal })
      if (!imgResp.ok) return null
      const blob = await imgResp.blob()
      if (signal.aborted) return null
      const objectUrl = URL.createObjectURL(blob)
      return await new Promise<THREE.Texture | null>((resolve) => {
        if (signal.aborted) {
          URL.revokeObjectURL(objectUrl)
          resolve(null)
          return
        }
        const img = new Image()
        img.onload = () => {
          if (signal.aborted) {
            URL.revokeObjectURL(objectUrl)
            resolve(null)
            return
          }
          const tex = new THREE.Texture(img)
          tex.flipY = false
          tex.needsUpdate = true
          URL.revokeObjectURL(objectUrl)
          resolve(tex)
        }
        img.onerror = () => {
          URL.revokeObjectURL(objectUrl)
          resolve(null)
        }
        img.src = objectUrl
      })
    } catch {
      return null
    }
  }
}

// ── Hook ────────────────────────────────────────────────────────────────

/**
 * Load a pipeline-processed 3D model (.fmesh + .fmat).
 *
 * @param assetId - Pipeline asset ID
 * @param lod - LOD level to render (default 0 = full detail)
 */
export function usePipelineModel(
  assetId: string,
  lod: number = 0,
): PipelineModelResult {
  const { gl: renderer } = useThree()
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<Error | null>(null)
  const [fmeshData, setFmeshData] = useState<FmeshData | null>(null)
  const [materials, setMaterials] = useState<THREE.MeshStandardMaterial[] | null>(null)

  // Stable refs for cleanup
  const abortRef = useRef<AbortController | null>(null)
  const prevMaterialsRef = useRef<THREE.MeshStandardMaterial[] | null>(null)

  // ── Fetch and parse .fmesh + .fmat ──────────────────────────────────

  useEffect(() => {
    const abort = new AbortController()
    abortRef.current = abort

    async function load() {
      setLoading(true)
      setError(null)
      setFmeshData(null)
      setMaterials(null)

      const encoded = encodeURIComponent(assetId)

      try {
        // Fetch .fmesh binary
        const meshResp = await fetch(
          `/api/assets/${encoded}/file?variant=processed`,
          { signal: abort.signal },
        )
        if (!meshResp.ok) {
          throw new Error(`Failed to fetch .fmesh: ${meshResp.status} ${meshResp.statusText}`)
        }
        const meshBuffer = await meshResp.arrayBuffer()
        const parsed = parseFmesh(meshBuffer)

        if (abort.signal.aborted) return
        setFmeshData(parsed)

        // Derive .fmat companion path from the asset ID.
        // Asset IDs strip the source extension (Duck/Duck.gltf → Duck--Duck),
        // so the last segment is already the stem. Append .fmat directly.
        const stem = assetId.split("--").pop()
        const fmatPath = stem ? `${stem}.fmat` : null
        if (fmatPath) {
          const fmatResp = await fetch(
            `/api/assets/${encoded}/companions?variant=processed&path=${encodeURIComponent(fmatPath)}`,
            { signal: abort.signal },
          )
          if (fmatResp.ok) {
            const fmatText = await fmatResp.text()
            const resolver = createTextureResolver(
              assetId,
              renderer,
              abort.signal,
            )
            const mats = await loadFmatMaterials(fmatText, resolver)
            if (!abort.signal.aborted) {
              prevMaterialsRef.current = mats
              setMaterials(mats)
            }
          } else if (fmatResp.status !== 404) {
            // Only fall back to default material when .fmat does not exist.
            // Any other error (5xx, parse failure) is a real problem.
            throw new Error(`.fmat fetch failed: ${fmatResp.status} ${fmatResp.statusText}`)
          }
          // 404 — no .fmat companion, use default material (no throw)
        }
      } catch (e) {
        if (!abort.signal.aborted) {
          setError(e instanceof Error ? e : new Error(String(e)))
        }
      } finally {
        if (!abort.signal.aborted) {
          setLoading(false)
        }
      }
    }

    load()

    return () => {
      abort.abort()
      // Dispose previous materials and their textures when asset changes
      if (prevMaterialsRef.current) {
        for (const mat of prevMaterialsRef.current) {
          // Collect unique textures to avoid double-dispose (metallic-roughness
          // maps share the same texture object for metalnessMap and roughnessMap)
          const textures = new Set<THREE.Texture>()
          if (mat.map) textures.add(mat.map)
          if (mat.normalMap) textures.add(mat.normalMap)
          if (mat.roughnessMap) textures.add(mat.roughnessMap)
          if (mat.metalnessMap) textures.add(mat.metalnessMap)
          if (mat.aoMap) textures.add(mat.aoMap)
          if (mat.emissiveMap) textures.add(mat.emissiveMap)
          for (const tex of textures) tex.dispose()
          mat.dispose()
        }
        prevMaterialsRef.current = null
      }
    }
  }, [assetId, renderer])

  // Stable default material — lazily created on first use
  const defaultMaterialRef = useRef<THREE.MeshStandardMaterial | null>(null)
  const getDefaultMaterial = useCallback(() => {
    if (!defaultMaterialRef.current) {
      defaultMaterialRef.current = new THREE.MeshStandardMaterial({
        color: 0x888888,
        roughness: 0.7,
        metalness: 0.0,
      })
    }
    return defaultMaterialRef.current
  }, [])

  // Track previous geometry for disposal
  const prevGeometryRef = useRef<THREE.BufferGeometry | null>(null)
  const prevGroupRef = useRef<THREE.Group | null>(null)

  // ── Build Three.js scene from cached data + current LOD ─────────────

  const scene = useMemo(() => {
    // Dispose previous geometry to prevent GPU memory leaks
    if (prevGeometryRef.current) {
      prevGeometryRef.current.dispose()
      prevGeometryRef.current = null
    }

    if (!fmeshData) return null

    const clampedLod = Math.max(0, Math.min(lod, fmeshData.lods.length - 1))
    const geometry = buildFmeshGeometry(fmeshData, clampedLod)
    prevGeometryRef.current = geometry

    const group = new THREE.Group()
    prevGroupRef.current = group

    if (materials && materials.length > 0) {
      // Multi-material mesh: geometry groups map to material array indices
      const mesh = new THREE.Mesh(geometry, materials)
      group.add(mesh)
    } else {
      const mesh = new THREE.Mesh(geometry, getDefaultMaterial())
      group.add(mesh)
    }

    return group
  }, [fmeshData, materials, lod, getDefaultMaterial])

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (prevGeometryRef.current) {
        prevGeometryRef.current.dispose()
      }
      if (defaultMaterialRef.current) {
        defaultMaterialRef.current.dispose()
        defaultMaterialRef.current = null
      }
    }
  }, [])

  return {
    scene,
    lodCount: fmeshData?.lods.length ?? 0,
    loading,
    error,
    fmeshData,
  }
}
