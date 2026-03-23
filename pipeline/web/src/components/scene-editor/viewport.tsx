/**
 * 3D viewport for the scene editor.
 *
 * Renders placed scene objects with react-three-fiber. The selected object
 * gets TransformControls (translate/rotate/scale gizmo from drei).
 * Transforms are committed to the store on mouse release, not every frame.
 */

import { type Dispatch, Suspense, useEffect, useMemo, useRef } from "react"
import { Canvas } from "@react-three/fiber"
import {
  Grid,
  OrbitControls,
  TransformControls,
  useGLTF,
} from "@react-three/drei"
import { useQuery } from "@tanstack/react-query"
import * as THREE from "three"
import { useCompanionManager } from "@/lib/companion-manager"
import { fetchAsset } from "@/lib/api"
import { usePipelineModel } from "@/lib/use-pipeline-model"
import type { GizmoMode, SceneAction, SceneObject } from "./types"

// ── Fallback box for objects without an asset ───────────────────────────

function FallbackBox() {
  return (
    <mesh>
      <boxGeometry args={[1, 1, 1]} />
      <meshStandardMaterial color="#666" wireframe />
    </mesh>
  )
}

// ── Loaded glTF model (source format fallback) ──────────────────────────

function GltfModel({ assetId }: { assetId: string }) {
  const url = `/api/assets/${encodeURIComponent(assetId)}/file`
  const manager = useCompanionManager(assetId)
  const { scene } = useGLTF(url, undefined, undefined, (loader) => {
    loader.manager = manager
  })
  return <primitive object={scene.clone()} />
}

// ── Pipeline model (.fmesh format) ──────────────────────────────────────

function PipelineModelView({ assetId }: { assetId: string }) {
  const { scene, error } = usePipelineModel(assetId, 0)
  if (error || !scene) {
    return <FallbackBox />
  }
  return <primitive object={scene} />
}

// ── Smart model loader — uses pipeline format when available ────────────

function LoadedModel({ assetId }: { assetId: string }) {
  const { data: asset, isLoading, isError } = useQuery({
    queryKey: ["asset", assetId],
    queryFn: () => fetchAsset(assetId),
    staleTime: 60_000,
  })

  // Show fallback while asset info is loading or if the fetch failed,
  // to avoid premature glTF rendering that crashes when companion
  // files are missing
  if (isLoading || isError) {
    return <FallbackBox />
  }

  const hasFmesh = asset?.output_path?.endsWith(".fmesh") ?? false

  if (hasFmesh) {
    return <PipelineModelView assetId={assetId} />
  }
  return <GltfModel assetId={assetId} />
}

// ── Single scene object ─────────────────────────────────────────────────

interface SceneObjectMeshProps {
  obj: SceneObject
  isSelected: boolean
  gizmoMode: GizmoMode
  dispatch: Dispatch<SceneAction>
  orbitRef: React.RefObject<any>
  children?: React.ReactNode
}

function SceneObjectMesh({
  obj,
  isSelected,
  gizmoMode,
  dispatch,
  orbitRef,
  children,
}: SceneObjectMeshProps) {
  const groupRef = useRef<THREE.Group>(null!)
  const transformRef = useRef<any>(null)

  // Sync group transform from store state
  useEffect(() => {
    if (groupRef.current) {
      groupRef.current.position.set(...obj.position)
      groupRef.current.quaternion.set(...obj.rotation)
      groupRef.current.scale.set(...obj.scale)
    }
  }, [obj.position, obj.rotation, obj.scale])

  // Disable OrbitControls while dragging the gizmo
  useEffect(() => {
    const controls = transformRef.current
    if (!controls) return

    const onDragChange = (event: { value: boolean }) => {
      if (orbitRef.current) {
        orbitRef.current.enabled = !event.value
      }
    }
    controls.addEventListener("dragging-changed", onDragChange)
    return () => {
      controls.removeEventListener("dragging-changed", onDragChange)
      // Re-enable orbit controls on cleanup to avoid stuck state
      if (orbitRef.current) {
        orbitRef.current.enabled = true
      }
    }
  }, [isSelected, orbitRef])

  const commitTransform = () => {
    if (!groupRef.current) return
    const p = groupRef.current.position
    const q = groupRef.current.quaternion
    const s = groupRef.current.scale
    dispatch({
      type: "UPDATE_TRANSFORM",
      objectId: obj.id,
      position: [p.x, p.y, p.z],
      rotation: [q.x, q.y, q.z, q.w],
      scale: [s.x, s.y, s.z],
    })
  }

  return (
    <>
      <group
        ref={groupRef}
        visible={obj.visible}
        onClick={(e) => {
          e.stopPropagation()
          dispatch({ type: "SELECT", objectId: obj.id })
        }}
      >
        <Suspense fallback={<FallbackBox />}>
          {obj.asset_id ? (
            <LoadedModel assetId={obj.asset_id} />
          ) : (
            <FallbackBox />
          )}
        </Suspense>
        {children}
      </group>
      {isSelected && groupRef.current && (
        <TransformControls
          ref={transformRef}
          object={groupRef.current}
          mode={gizmoMode}
          onMouseUp={commitTransform}
        />
      )}
    </>
  )
}

// ── Recursive hierarchy renderer ─────────────────────────────────────────

function renderHierarchy(
  obj: SceneObject,
  childrenMap: Map<string | null, SceneObject[]>,
  selectedId: string | null,
  gizmoMode: GizmoMode,
  dispatch: Dispatch<SceneAction>,
  orbitRef: React.RefObject<any>,
): React.ReactNode {
  const children = childrenMap.get(obj.id) ?? []
  return (
    <SceneObjectMesh
      key={obj.id}
      obj={obj}
      isSelected={obj.id === selectedId}
      gizmoMode={gizmoMode}
      dispatch={dispatch}
      orbitRef={orbitRef}
    >
      {children.map((child) =>
        renderHierarchy(child, childrenMap, selectedId, gizmoMode, dispatch, orbitRef)
      )}
    </SceneObjectMesh>
  )
}

// ── Scene contents (inside Canvas) ──────────────────────────────────────

interface SceneContentsProps {
  objects: SceneObject[]
  selectedId: string | null
  gizmoMode: GizmoMode
  dispatch: Dispatch<SceneAction>
}

function SceneContents({
  objects,
  selectedId,
  gizmoMode,
  dispatch,
}: SceneContentsProps) {
  const orbitRef = useRef<any>(null)

  const { childrenMap, roots } = useMemo(() => {
    const map = new Map<string | null, SceneObject[]>()
    for (const obj of objects) {
      const pid = obj.parent_id ?? null
      const list = map.get(pid) ?? []
      list.push(obj)
      map.set(pid, list)
    }
    return { childrenMap: map, roots: map.get(null) ?? [] }
  }, [objects])

  return (
    <>
      <ambientLight intensity={0.4} />
      <directionalLight position={[5, 8, 5]} intensity={1.5} />
      <Grid
        args={[20, 20]}
        cellSize={1}
        cellThickness={0.5}
        cellColor="#333"
        sectionSize={5}
        sectionThickness={1}
        sectionColor="#555"
        fadeDistance={30}
        infiniteGrid
      />
      <OrbitControls ref={orbitRef} makeDefault />
      {roots.map((obj) =>
        renderHierarchy(obj, childrenMap, selectedId, gizmoMode, dispatch, orbitRef)
      )}
    </>
  )
}

// ── Viewport wrapper ────────────────────────────────────────────────────

interface ViewportProps {
  objects: SceneObject[]
  selectedId: string | null
  gizmoMode: GizmoMode
  dispatch: Dispatch<SceneAction>
}

export function Viewport({
  objects,
  selectedId,
  gizmoMode,
  dispatch,
}: ViewportProps) {
  return (
    <div className="flex-1 min-h-0">
      <Canvas
        style={{ background: "#1a1a1a" }}
        camera={{ position: [12, 10, 12], fov: 50 }}
        onPointerMissed={() => dispatch({ type: "SELECT", objectId: null })}
      >
        <SceneContents
          objects={objects}
          selectedId={selectedId}
          gizmoMode={gizmoMode}
          dispatch={dispatch}
        />
      </Canvas>
    </div>
  )
}
