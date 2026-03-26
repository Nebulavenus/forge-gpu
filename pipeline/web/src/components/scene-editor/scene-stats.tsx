/**
 * Scene statistics overlay for the viewport.
 *
 * Traverses the Three.js scene graph to accumulate geometry stats
 * (object count, triangle count, texture memory estimate) and
 * displays them in a semi-transparent overlay.
 */

import { useEffect, useRef } from "react"
import { useThree } from "@react-three/fiber"
import * as THREE from "three"

export interface SceneStats {
  objects: number
  triangles: number
  vertices: number
  textures: number
  textureMemory: number
}

export const EMPTY_STATS: SceneStats = {
  objects: 0,
  triangles: 0,
  vertices: 0,
  textures: 0,
  textureMemory: 0,
}

function statsEqual(a: SceneStats, b: SceneStats): boolean {
  return a.objects === b.objects
    && a.triangles === b.triangles
    && a.vertices === b.vertices
    && a.textures === b.textures
    && a.textureMemory === b.textureMemory
}

/**
 * Estimate GPU memory for a texture based on dimensions and format.
 * Accounts for mipmaps (adds ~33% overhead).
 *
 * Note: this assumes uncompressed textures. Compressed formats (BC7,
 * BC5, ASTC, ETC2) used by the asset pipeline have lower effective BPP,
 * so this estimate may overcount for processed assets.
 */
function estimateTextureMemory(texture: THREE.Texture): number {
  const image = texture.image
  if (!image || !image.width || !image.height) return 0
  const bpp = texture.format === THREE.RGBAFormat ? 4 : 3
  const baseSize = image.width * image.height * bpp
  // Mipmap chain adds ~33% to base size
  const mipmapFactor = texture.generateMipmaps ? 1.33 : 1.0
  return Math.ceil(baseSize * mipmapFactor)
}

// Module-scoped Set reused across collection ticks to avoid allocation.
// Safe for a single viewport; move into the effect closure if multiple
// SceneStatsCollector instances are ever needed simultaneously.
const seenTextures = new Set<number>()

// Texture map property names to inspect on any material
const TEXTURE_KEYS = ["map", "normalMap", "roughnessMap", "metalnessMap", "aoMap", "emissiveMap"] as const

/**
 * Inner component that runs inside the Canvas context.
 * Traverses the scene graph at ~4 Hz and reports stats via the
 * onStats callback.
 */
export function SceneStatsCollector({
  onStats,
}: {
  onStats: (stats: SceneStats) => void
}) {
  const { scene } = useThree()
  const prevRef = useRef<SceneStats>(EMPTY_STATS)
  // Callback ref so the rAF loop does not depend on onStats identity
  const onStatsRef = useRef(onStats)
  onStatsRef.current = onStats

  useEffect(() => {
    let frameId: number
    let running = true
    let elapsed = 0
    let lastTime = performance.now()

    const collect = () => {
      if (!running) return
      frameId = requestAnimationFrame(collect)

      // Throttle to ~4 Hz to avoid per-frame traversal overhead
      const now = performance.now()
      // Clamp delta to avoid a burst after background-tab resume
      elapsed += Math.min(now - lastTime, 1000)
      lastTime = now
      if (elapsed < 250) return
      elapsed = 0

      let objects = 0
      let triangles = 0
      let vertices = 0
      seenTextures.clear()
      let textureMemory = 0

      scene.traverse((child) => {
        if (!(child instanceof THREE.Mesh)) return
        // Skip scene helpers (grid, gizmos, transform controls) — only
        // count user-placed objects. Helpers are identified by:
        // - isHelper flag (Three.js convention for helpers)
        // - isTransformControlsPlane (drei transform gizmo)
        // - userData.isHelper (set by viewport on helper meshes)
        // - frustumCulled === false with no name (drei Grid pattern)
        const obj = child as unknown as Record<string, unknown>
        if (obj.isTransformControlsPlane || obj.isHelper) return
        if (child.userData?.isHelper) return
        // drei Grid: detected via implementation details (unnamed mesh, frustumCulled=false,
        // ShaderMaterial). This heuristic may need updating if drei's Grid changes.
        const childMat = child.material
        if (!child.name && !child.frustumCulled
            && !Array.isArray(childMat) && childMat instanceof THREE.ShaderMaterial) return
        objects++
        const geo = child.geometry
        if (geo) {
          const pos = geo.getAttribute("position")
          if (geo.index) {
            triangles += Math.floor(geo.index.count / 3)
          } else if (pos) {
            triangles += Math.floor(pos.count / 3)
          }
          if (pos) vertices += pos.count
        }

        // Collect unique textures from materials (duck-typed to handle
        // any material class, not just MeshStandardMaterial)
        const materials = Array.isArray(child.material)
          ? child.material
          : [child.material]
        for (const mat of materials) {
          if (!mat) continue
          const m = mat as Record<string, unknown>
          for (const key of TEXTURE_KEYS) {
            const tex = m[key]
            if (tex instanceof THREE.Texture && !seenTextures.has(tex.id)) {
              seenTextures.add(tex.id)
              textureMemory += estimateTextureMemory(tex)
            }
          }
        }
      })

      const stats: SceneStats = {
        objects,
        triangles,
        vertices,
        textures: seenTextures.size,
        textureMemory,
      }

      // Only invoke callback when stats actually change
      if (!statsEqual(stats, prevRef.current)) {
        prevRef.current = stats
        onStatsRef.current(stats)
      }
    }

    frameId = requestAnimationFrame(collect)
    return () => {
      running = false
      cancelAnimationFrame(frameId)
    }
  }, [scene])

  return null
}

/**
 * Format bytes into a human-readable string (KB / MB).
 */
function formatMemory(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`
}

/**
 * HTML overlay that displays scene statistics.
 * Positioned absolutely over the viewport Canvas.
 */
export function SceneStatsOverlay({ stats }: { stats: SceneStats }) {
  if (statsEqual(stats, EMPTY_STATS)) return null

  return (
    <div
      role="status"
      aria-label="Scene statistics"
      className="absolute bottom-3 left-3 rounded-md border border-border bg-card/80 px-2.5 py-1.5 text-xs text-muted-foreground backdrop-blur-sm"
    >
      <dl className="flex flex-col gap-0.5">
        <div className="flex justify-between gap-4">
          <dt>Objects</dt>
          <dd className="font-mono tabular-nums">{stats.objects.toLocaleString()}</dd>
        </div>
        <div className="flex justify-between gap-4">
          <dt>Triangles</dt>
          <dd className="font-mono tabular-nums">{stats.triangles.toLocaleString()}</dd>
        </div>
        <div className="flex justify-between gap-4">
          <dt>Vertices</dt>
          <dd className="font-mono tabular-nums">{stats.vertices.toLocaleString()}</dd>
        </div>
        {stats.textures > 0 && (
          <div className="flex justify-between gap-4">
            <dt>Textures</dt>
            <dd className="font-mono tabular-nums">
              {stats.textures} ({formatMemory(stats.textureMemory)})
            </dd>
          </div>
        )}
      </dl>
    </div>
  )
}
