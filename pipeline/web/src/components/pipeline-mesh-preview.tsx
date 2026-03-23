/**
 * 3D preview component for pipeline-processed .fmesh models.
 *
 * Renders a processed mesh with orbit controls, LOD selector,
 * wireframe toggle, and vertex/triangle count display.
 */

import { Suspense, useEffect, useRef, useState } from "react"
import { Canvas } from "@react-three/fiber"
import { OrbitControls, Bounds, Center, useBounds } from "@react-three/drei"
import * as THREE from "three"
import { usePipelineModel } from "@/lib/use-pipeline-model"
import { getFmeshLodStats } from "@/lib/fmesh-geometry"
import { type FmeshData } from "@/lib/fmesh-parser"

// ── Inner model component (must be inside Canvas) ───────────────────────

function PipelineModel({
  assetId,
  lod,
  wireframe,
  onMeshData,
  onError,
}: {
  assetId: string
  lod: number
  wireframe: boolean
  onMeshData?: (data: FmeshData | null) => void
  onError?: (error: Error | null) => void
}) {
  const { scene, fmeshData, loading, error } = usePipelineModel(assetId, lod)
  const bounds = useBounds()

  // Stable ref for callbacks to avoid re-render loops with non-stable props
  const onMeshDataRef = useRef(onMeshData)
  onMeshDataRef.current = onMeshData
  const onErrorRef = useRef(onError)
  onErrorRef.current = onError

  useEffect(() => {
    if (scene) {
      bounds.refresh(scene).fit()
    }
  }, [scene, bounds])

  // Apply wireframe
  useEffect(() => {
    if (!scene) return
    scene.traverse((child) => {
      if (child instanceof THREE.Mesh && child.material) {
        const materials = Array.isArray(child.material)
          ? child.material
          : [child.material]
        for (const mat of materials) {
          mat.wireframe = wireframe
        }
      }
    })
  }, [scene, wireframe])

  // Report mesh data back to parent for stats display
  useEffect(() => {
    onMeshDataRef.current?.(fmeshData)
  }, [fmeshData])

  // Report errors to parent so it can show a proper failure UI outside the Canvas
  useEffect(() => {
    onErrorRef.current?.(error)
  }, [error])

  if (loading) {
    return (
      <mesh>
        <boxGeometry args={[1, 1, 1]} />
        <meshStandardMaterial color="#444" wireframe />
      </mesh>
    )
  }

  if (error || !scene) {
    // Signal the parent that loading failed — report via onMeshData(null)
    // so the outer component can show a proper error message.  Returning
    // null hides the Canvas contents; the parent's error boundary or
    // fallback handles the UI.
    return null
  }

  return <primitive object={scene} />
}

// ── Loading fallback ────────────────────────────────────────────────────

function LoadingFallback() {
  return (
    <mesh>
      <boxGeometry args={[1, 1, 1]} />
      <meshStandardMaterial color="#444" wireframe />
    </mesh>
  )
}

// ── Main component ──────────────────────────────────────────────────────

interface PipelineMeshPreviewProps {
  assetId: string
}

export function PipelineMeshPreview({ assetId }: PipelineMeshPreviewProps) {
  const [wireframe, setWireframe] = useState(false)
  const [lod, setLod] = useState(0)
  const [meshData, setMeshData] = useState<FmeshData | null>(null)
  const [loadError, setLoadError] = useState<Error | null>(null)

  const lodCount = meshData?.lods.length ?? 0
  const stats = meshData && lod < meshData.lods.length
    ? getFmeshLodStats(meshData, lod)
    : null

  if (loadError) {
    return (
      <div className="flex h-[400px] w-full items-center justify-center rounded-lg border border-border bg-card text-sm text-muted-foreground">
        Processed preview failed to load.
      </div>
    )
  }

  return (
    <div className="relative h-[400px] w-full rounded-lg border border-border bg-card overflow-hidden">
      <Canvas
        style={{ background: "#1a1a1a" }}
        camera={{ position: [200, 150, 200], fov: 50, near: 0.1, far: 10000 }}
      >
        <ambientLight intensity={0.8} />
        <directionalLight position={[5, 8, 5]} intensity={2.0} />
        <directionalLight position={[-3, 4, -3]} intensity={0.6} />
        <Suspense fallback={<LoadingFallback />}>
          <Bounds observe margin={2.5}>
            <Center>
              <PipelineModel
                assetId={assetId}
                lod={lod}
                wireframe={wireframe}
                onMeshData={setMeshData}
                onError={setLoadError}
              />
            </Center>
          </Bounds>
        </Suspense>
        <OrbitControls makeDefault />
      </Canvas>

      {/* Toolbar */}
      <div className="absolute bottom-3 right-3 flex items-center gap-2">
        {/* Stats */}
        {stats && (
          <span className="rounded-md border border-border bg-card px-2 py-1 text-xs text-muted-foreground">
            {stats.vertexCount.toLocaleString()} verts
            {" · "}
            {stats.triangleCount.toLocaleString()} tris
          </span>
        )}

        {/* LOD selector */}
        {lodCount > 1 && (
          <select
            aria-label="Level of detail"
            value={lod}
            onChange={(e) => setLod(Number(e.target.value))}
            className="rounded-md border border-border bg-card px-2 py-1 text-xs text-muted-foreground"
          >
            {Array.from({ length: lodCount }, (_, i) => (
              <option key={i} value={i}>
                LOD {i}
              </option>
            ))}
          </select>
        )}

        {/* Wireframe toggle */}
        <button
          type="button"
          aria-pressed={wireframe}
          onClick={() => setWireframe((v) => !v)}
          className={
            "rounded-md border px-3 py-1.5 text-xs font-medium transition-colors " +
            (wireframe
              ? "border-primary bg-primary text-primary-foreground"
              : "border-border bg-card text-muted-foreground hover:bg-accent")
          }
        >
          Wireframe
        </button>
      </div>
    </div>
  )
}
