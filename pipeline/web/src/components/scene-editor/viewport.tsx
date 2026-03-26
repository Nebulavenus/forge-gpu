/**
 * 3D viewport for the scene editor.
 *
 * Renders placed scene objects with react-three-fiber. The selected object
 * gets TransformControls (translate/rotate/scale gizmo from drei).
 * Transforms are committed to the store on mouse release, not every frame.
 *
 * Multi-select: Shift+click adds to selection, Ctrl/Cmd+click toggles.
 * Drag-select (box select) draws a rubber-band rectangle to select objects
 * whose screen-space positions fall within the box.
 */

import { type Dispatch, Suspense, useCallback, useEffect, useMemo, useRef, useState } from "react"
import { Canvas, useFrame, useThree } from "@react-three/fiber"
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
import type { CameraBookmark, GizmoMode, MaterialOverrides, SceneAction, SceneObject, SnapSize } from "./types"
import { ASSET_DRAG_MIME } from "./asset-shelf"
import { computeWorldPosition } from "./scene-utils"
import { EMPTY_STATS, SceneStatsCollector, SceneStatsOverlay, type SceneStats } from "./scene-stats"

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

// ── Material override applicator ─────────────────────────────────────────

/**
 * Traverses a Three.js group and applies material overrides (color tint,
 * opacity, wireframe) to every mesh material. Restores original values
 * when the overrides change or the component unmounts.
 */
function useMaterialOverrides(
  groupRef: React.RefObject<THREE.Group | null>,
  overrides: MaterialOverrides | undefined | null,
) {
  // Track which meshes had their materials cloned, mapping mesh → original material.
  // On cleanup, we restore the original shared material to avoid leaking clones.
  const clonedRef = useRef<Map<THREE.Mesh, THREE.Material | THREE.Material[]>>(new Map())

  // Track subtree identity via mesh UUIDs so the effect re-runs when async
  // models finish loading OR when a fallback (1 mesh) is swapped for a real
  // model that also has 1 mesh (same count but different identity).
  const [subtreeId, setSubtreeId] = useState("")
  const prevIdRef = useRef("")

  // Gate: only traverse when overrides are active or clones are still attached.
  // This avoids O(n) per-frame walks for objects with no material overrides.
  const hasActiveOverrides = overrides != null && (
    overrides.color != null || overrides.opacity != null || overrides.wireframe != null
  )

  useFrame(() => {
    if (!hasActiveOverrides && clonedRef.current.size === 0) return
    const group = groupRef.current
    if (!group) return
    const ids: string[] = []
    group.traverse((child) => {
      if (child instanceof THREE.Mesh) ids.push(child.uuid)
    })
    const id = ids.join(",")
    if (id !== prevIdRef.current) {
      prevIdRef.current = id
      setSubtreeId(id)
    }
  })

  useEffect(() => {
    const group = groupRef.current
    if (!group) return

    // Restore previously cloned meshes to their original shared materials
    const cloned = clonedRef.current
    for (const [mesh, origMat] of cloned) {
      // Dispose cloned materials to avoid memory leaks
      const current = Array.isArray(mesh.material) ? mesh.material : [mesh.material]
      for (const mat of current) mat.dispose()
      mesh.material = origMat
    }
    cloned.clear()

    if (!overrides) return

    // Normalize nulls to undefined (backend may send null for unset fields)
    const color = overrides.color ?? undefined
    const opacity = overrides.opacity ?? undefined
    const wireframe = overrides.wireframe ?? undefined

    const hasColor = color !== undefined
    const hasOpacity = opacity !== undefined
    const hasWireframe = wireframe !== undefined

    if (!hasColor && !hasOpacity && !hasWireframe) return

    const parsedColor = hasColor ? new THREE.Color(color) : null

    group.traverse((child) => {
      if (!(child instanceof THREE.Mesh)) return

      // Clone materials so we don't mutate shared instances (three.js clone()
      // shares material references between cloned scenes).
      const isArray = Array.isArray(child.material)
      const origMaterials = isArray ? child.material : child.material
      cloned.set(child, origMaterials)

      if (isArray) {
        child.material = (child.material as THREE.Material[]).map((mat) => {
          const clone = mat.clone()
          applyOverride(clone, parsedColor, hasColor, opacity, hasOpacity, wireframe, hasWireframe)
          return clone
        })
      } else {
        const clone = (child.material as THREE.Material).clone()
        applyOverride(clone, parsedColor, hasColor, opacity, hasOpacity, wireframe, hasWireframe)
        child.material = clone
      }
    })

    return () => {
      // Cleanup: restore original shared materials, dispose clones
      for (const [mesh, origMat] of cloned) {
        const current = Array.isArray(mesh.material) ? mesh.material : [mesh.material]
        for (const mat of current) mat.dispose()
        mesh.material = origMat
      }
      cloned.clear()
    }
  }, [groupRef, overrides?.color, overrides?.opacity, overrides?.wireframe, subtreeId])
}

/** Apply override values to a cloned material instance. */
function applyOverride(
  mat: THREE.Material,
  parsedColor: THREE.Color | null,
  hasColor: boolean,
  opacity: number | undefined,
  hasOpacity: boolean,
  wireframe: boolean | undefined,
  hasWireframe: boolean,
) {
  if (hasColor && parsedColor && "color" in mat) {
    ;(mat as THREE.MeshStandardMaterial).color.copy(parsedColor)
  }
  if (hasOpacity && opacity !== undefined) {
    mat.opacity = opacity
    // Only force transparent on — never force it off.
    // At opacity === 1 the cloned material keeps the source asset's
    // transparency flag (e.g. alpha-mapped textures stay transparent).
    if (opacity < 1) {
      mat.transparent = true
    }
  }
  if (hasWireframe && wireframe !== undefined && "wireframe" in mat) {
    ;(mat as THREE.MeshStandardMaterial).wireframe = wireframe
  }
}

// ── Single scene object ─────────────────────────────────────────────────

interface SceneObjectMeshProps {
  obj: SceneObject
  /** Whether this specific object should show the gizmo (primary selection). */
  showGizmo: boolean
  gizmoMode: GizmoMode
  snapEnabled: boolean
  snapSize: SnapSize
  dispatch: Dispatch<SceneAction>
  orbitRef: React.RefObject<any>
  children?: React.ReactNode
}

function SceneObjectMesh({
  obj,
  showGizmo,
  gizmoMode,
  snapEnabled,
  snapSize,
  dispatch,
  orbitRef,
  children,
}: SceneObjectMeshProps) {
  const groupRef = useRef<THREE.Group>(null!)
  const modelRef = useRef<THREE.Group>(null!)
  const transformRef = useRef<any>(null)

  // Apply material overrides to the model subtree only (not recursive children)
  useMaterialOverrides(modelRef, obj.material_overrides)

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
  }, [showGizmo, orbitRef])

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
          // Shift+click → add to selection, Ctrl/Cmd+click → toggle
          const nativeEvent = e.nativeEvent as PointerEvent
          if (nativeEvent.shiftKey) {
            dispatch({ type: "SELECT", objectId: obj.id, mode: "add" })
          } else if (nativeEvent.ctrlKey || nativeEvent.metaKey) {
            dispatch({ type: "SELECT", objectId: obj.id, mode: "toggle" })
          } else {
            dispatch({ type: "SELECT", objectId: obj.id })
          }
        }}
      >
        <group ref={modelRef}>
          <Suspense fallback={<FallbackBox />}>
            {obj.asset_id ? (
              <LoadedModel assetId={obj.asset_id} />
            ) : (
              <FallbackBox />
            )}
          </Suspense>
        </group>
        {children}
      </group>
      {showGizmo && groupRef.current && (
        <TransformControls
          ref={transformRef}
          object={groupRef.current}
          mode={gizmoMode}
          translationSnap={snapEnabled ? snapSize : null}
          rotationSnap={snapEnabled ? (Math.PI / 180) * 15 : null}
          scaleSnap={snapEnabled ? snapSize : null}
          onMouseUp={commitTransform}
        />
      )}
    </>
  )
}

// ── Recursive hierarchy renderer ─────────────────────────────────────────

interface RenderHierarchyProps {
  obj: SceneObject
  childrenMap: Map<string | null, SceneObject[]>
  /** The single object that gets the transform gizmo. */
  gizmoTargetId: string | null
  gizmoMode: GizmoMode
  snapEnabled: boolean
  snapSize: SnapSize
  dispatch: Dispatch<SceneAction>
  orbitRef: React.RefObject<any>
}

function renderHierarchy(props: RenderHierarchyProps): React.ReactNode {
  const { obj, childrenMap, gizmoTargetId, gizmoMode, snapEnabled, snapSize, dispatch, orbitRef } = props
  const children = childrenMap.get(obj.id) ?? []
  return (
    <SceneObjectMesh
      key={obj.id}
      obj={obj}
      showGizmo={obj.id === gizmoTargetId}
      gizmoMode={gizmoMode}
      snapEnabled={snapEnabled}
      snapSize={snapSize}
      dispatch={dispatch}
      orbitRef={orbitRef}
    >
      {children.map((child) =>
        renderHierarchy({ ...props, obj: child })
      )}
    </SceneObjectMesh>
  )
}

// ── Box select helper (inside Canvas) ────────────────────────────────────

interface BoxSelectHelperProps {
  objects: SceneObject[]
  /** Stable ref set by the outer component with the current selection rect. */
  boxRef: React.MutableRefObject<{
    active: boolean
    startX: number
    startY: number
    endX: number
    endY: number
  }>
  /** Callback that receives the IDs of objects inside the box. */
  onBoxSelect: (ids: string[]) => void
}

/**
 * A component that lives inside the Canvas to access the Three.js camera.
 * When a box select completes (active transitions false), it projects all
 * object world-space positions to screen space and returns matching IDs.
 */
function BoxSelectProjector({ objects, boxRef, onBoxSelect }: BoxSelectHelperProps) {
  const { camera, gl } = useThree()
  const prevActive = useRef(false)

  useEffect(() => {
    let cancelled = false
    const check = () => {
      if (cancelled) return
      // Detect end of box select
      if (prevActive.current && !boxRef.current.active) {
        const { startX, startY, endX, endY } = boxRef.current
        const rect = gl.domElement.getBoundingClientRect()

        // Normalize to canvas-relative pixels
        const x1 = Math.min(startX, endX) - rect.left
        const x2 = Math.max(startX, endX) - rect.left
        const y1 = Math.min(startY, endY) - rect.top
        const y2 = Math.max(startY, endY) - rect.top

        const objectMap = new Map(objects.map((o) => [o.id, o]))
        const vec = new THREE.Vector3()
        const ids: string[] = []

        for (const obj of objects) {
          const worldPos = computeWorldPosition(obj, objectMap)
          vec.set(worldPos[0], worldPos[1], worldPos[2])
          vec.project(camera)
          // Convert from NDC (-1..1) to pixel coordinates
          const sx = ((vec.x + 1) / 2) * rect.width
          const sy = ((-vec.y + 1) / 2) * rect.height

          if (sx >= x1 && sx <= x2 && sy >= y1 && sy <= y2 && vec.z < 1) {
            ids.push(obj.id)
          }
        }

        onBoxSelect(ids)
      }
      prevActive.current = boxRef.current.active
      requestAnimationFrame(check)
    }
    requestAnimationFrame(check)
    return () => {
      cancelled = true
    }
  }, [objects, camera, gl, boxRef, onBoxSelect])

  return null
}

// ── Scene contents (inside Canvas) ──────────────────────────────────────

/** Functions exposed by the viewport for reading/restoring camera state. */
export interface CameraHandle {
  /** Return the current camera position and orbit target. */
  getState: () => { position: [number, number, number]; target: [number, number, number] } | null
  /** Restore the camera to a saved bookmark position/target. */
  restore: (bookmark: CameraBookmark) => void
}

interface SceneContentsProps {
  objects: SceneObject[]
  gizmoTargetId: string | null
  gizmoMode: GizmoMode
  snapEnabled: boolean
  snapSize: SnapSize
  dispatch: Dispatch<SceneAction>
  boxRef: React.MutableRefObject<{
    active: boolean
    startX: number
    startY: number
    endX: number
    endY: number
  }>
  onBoxSelect: (ids: string[]) => void
  onStats: (stats: SceneStats) => void
  /** Ref that receives camera read/restore functions. */
  cameraHandleRef: React.MutableRefObject<CameraHandle | null>
}

function SceneContents({
  objects,
  gizmoTargetId,
  gizmoMode,
  snapEnabled,
  snapSize,
  dispatch,
  boxRef,
  onBoxSelect,
  onStats,
  cameraHandleRef,
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

  const { camera } = useThree()

  // Expose camera read/restore functions to the parent Viewport component
  useEffect(() => {
    cameraHandleRef.current = {
      getState: () => {
        const orbit = orbitRef.current
        if (!orbit) return null
        const p = camera.position
        const t = orbit.target
        return {
          position: [p.x, p.y, p.z],
          target: [t.x, t.y, t.z],
        }
      },
      restore: (bookmark: CameraBookmark) => {
        const orbit = orbitRef.current
        if (!orbit) return
        camera.position.set(...bookmark.position)
        orbit.target.set(...bookmark.target)
        orbit.update()
      },
    }
    return () => {
      cameraHandleRef.current = null
    }
  }, [camera, cameraHandleRef])

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
        renderHierarchy({ obj, childrenMap, gizmoTargetId, gizmoMode, snapEnabled, snapSize, dispatch, orbitRef })
      )}
      <BoxSelectProjector
        objects={objects}
        boxRef={boxRef}
        onBoxSelect={onBoxSelect}
      />
      <SceneStatsCollector onStats={onStats} />
    </>
  )
}

// ── Ground-plane drop raycaster ─────────────────────────────────────────
// Exposed via a ref callback so the outer HTML drop handler can invoke it.

// Module-scoped scratch objects reused across drop raycasts to avoid
// per-call allocations. Safe for a single Viewport instance.
const groundPlane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0)
const dropRaycaster = new THREE.Raycaster()
const dropNdc = new THREE.Vector2()
const dropHit = new THREE.Vector3()

/** Reject ground-plane hits beyond this distance from the camera. */
const MAX_DROP_DISTANCE = 200

interface DropRaycastProps {
  /** Ref that receives a function: (clientX, clientY) → world position | null */
  raycastRef: React.MutableRefObject<((cx: number, cy: number) => THREE.Vector3 | null) | null>
}

function DropRaycaster({ raycastRef }: DropRaycastProps) {
  const { camera, gl } = useThree()

  useEffect(() => {
    raycastRef.current = (clientX: number, clientY: number) => {
      const rect = gl.domElement.getBoundingClientRect()
      dropNdc.set(
        ((clientX - rect.left) / rect.width) * 2 - 1,
        -((clientY - rect.top) / rect.height) * 2 + 1,
      )
      dropRaycaster.setFromCamera(dropNdc, camera)
      const intersected = dropRaycaster.ray.intersectPlane(groundPlane, dropHit)
      if (!intersected) return null
      // Clamp extremely distant hits (near-horizon camera angles)
      if (dropHit.distanceTo(camera.position) > MAX_DROP_DISTANCE) return null
      return dropHit.clone()
    }
    return () => {
      raycastRef.current = null
    }
  }, [camera, gl]) // raycastRef is a stable useRef — omitted intentionally

  return null
}

// ── Box select rubber band overlay ──────────────────────────────────────

/** Minimum drag distance (px) before box select activates. */
const BOX_SELECT_THRESHOLD = 5

// ── Viewport wrapper ────────────────────────────────────────────────────

interface ViewportProps {
  objects: SceneObject[]
  selectedIds: Set<string>
  gizmoMode: GizmoMode
  snapEnabled: boolean
  snapSize: SnapSize
  dispatch: Dispatch<SceneAction>
  /** Called when a mesh asset is dropped on the viewport. */
  onAssetDrop?: (assetId: string, position: [number, number, number]) => void
  /** Ref that receives camera read/restore functions. */
  cameraHandleRef?: React.MutableRefObject<CameraHandle | null>
}

export function Viewport({
  objects,
  selectedIds,
  gizmoMode,
  snapEnabled,
  snapSize,
  dispatch,
  onAssetDrop,
  cameraHandleRef: externalCameraRef,
}: ViewportProps) {
  const raycastRef = useRef<((cx: number, cy: number) => THREE.Vector3 | null) | null>(null)
  const internalCameraRef = useRef<CameraHandle | null>(null)
  const cameraHandleRef = externalCameraRef ?? internalCameraRef
  const [sceneStats, setSceneStats] = useState<SceneStats>(EMPTY_STATS)
  const handleStats = useCallback((stats: SceneStats) => setSceneStats(stats), [])

  // Box select state
  const [boxSelect, setBoxSelect] = useState<{
    startX: number
    startY: number
    endX: number
    endY: number
  } | null>(null)
  const boxRef = useRef({
    active: false,
    startX: 0,
    startY: 0,
    endX: 0,
    endY: 0,
  })
  const pointerDownRef = useRef<{ x: number; y: number; button: number } | null>(null)

  // Determine which object gets the gizmo — last in the selected set
  const gizmoTargetId = useMemo(() => {
    if (selectedIds.size === 0) return null
    // Return the last selected ID (last in iteration order)
    let last: string | null = null
    for (const id of selectedIds) last = id
    return last
  }, [selectedIds])

  const handlePointerDown = useCallback((e: React.PointerEvent) => {
    // Only start box select on left button from an empty-space drag.
    // Allow the event when it originates on the wrapper div itself or on
    // the <canvas> element (which is a direct child of the wrapper). Clicks
    // on TransformControls overlays or other UI children are rejected.
    // R3F handles 3D object hit-testing internally — DOM pointer events
    // always see the <canvas> as the target regardless of which 3D object
    // was clicked. The onPointerMissed callback on <Canvas> handles the
    // empty-space deselect case separately.
    if (e.button !== 0) return
    const target = e.target as HTMLElement
    if (target !== e.currentTarget && target.tagName !== "CANVAS") return
    pointerDownRef.current = { x: e.clientX, y: e.clientY, button: e.button }
    // Capture pointer so we receive move/up events even if the drag leaves
    // the viewport bounds. Without this, dragging outside the element loses
    // pointer events and leaves box select stuck in an active state.
    e.currentTarget.setPointerCapture(e.pointerId)
  }, [])

  const handlePointerMove = useCallback((e: React.PointerEvent) => {
    if (!pointerDownRef.current) return
    const dx = e.clientX - pointerDownRef.current.x
    const dy = e.clientY - pointerDownRef.current.y
    const dist = Math.sqrt(dx * dx + dy * dy)

    if (dist >= BOX_SELECT_THRESHOLD) {
      const box = {
        startX: pointerDownRef.current.x,
        startY: pointerDownRef.current.y,
        endX: e.clientX,
        endY: e.clientY,
      }
      setBoxSelect(box)
      boxRef.current = { active: true, ...box }
    }
  }, [])

  const handlePointerUp = useCallback((e: React.PointerEvent) => {
    if (boxSelect) {
      // End box select — the BoxSelectProjector will detect active→false
      boxRef.current.active = false
    }
    setBoxSelect(null)
    pointerDownRef.current = null
    // Release pointer capture if held
    if (e.currentTarget.hasPointerCapture(e.pointerId)) {
      e.currentTarget.releasePointerCapture(e.pointerId)
    }
  }, [boxSelect])

  // Fallback for lost pointer capture (e.g. browser-level interruption)
  const handlePointerCancel = useCallback(() => {
    if (boxSelect) {
      boxRef.current.active = false
    }
    setBoxSelect(null)
    pointerDownRef.current = null
  }, [boxSelect])

  const handleBoxSelect = useCallback(
    (ids: string[]) => {
      // Single dispatch replaces the entire selection
      dispatch({ type: "SELECT_SET", objectIds: ids })
    },
    [dispatch],
  )

  const handleDragOver = useCallback((e: React.DragEvent) => {
    if (e.dataTransfer.types.includes(ASSET_DRAG_MIME)) {
      e.preventDefault()
      e.dataTransfer.dropEffect = "copy"
    }
  }, [])

  const handleDrop = useCallback(
    (e: React.DragEvent) => {
      e.preventDefault()
      const assetId = e.dataTransfer.getData(ASSET_DRAG_MIME)
      if (!assetId || !onAssetDrop) return

      const worldPos = raycastRef.current?.(e.clientX, e.clientY)
      const position: [number, number, number] = worldPos
        ? [worldPos.x, worldPos.y, worldPos.z]
        : [0, 0, 0]

      onAssetDrop(assetId, position)
    },
    [onAssetDrop],
  )

  // Compute rubber band rectangle style
  const rubberBandStyle = boxSelect
    ? {
        left: Math.min(boxSelect.startX, boxSelect.endX),
        top: Math.min(boxSelect.startY, boxSelect.endY),
        width: Math.abs(boxSelect.endX - boxSelect.startX),
        height: Math.abs(boxSelect.endY - boxSelect.startY),
      }
    : null

  return (
    <div
      className="relative flex-1 min-h-0"
      onDragOver={handleDragOver}
      onDrop={handleDrop}
      onPointerDown={handlePointerDown}
      onPointerMove={handlePointerMove}
      onPointerUp={handlePointerUp}
      onPointerCancel={handlePointerCancel}
      role="application"
      aria-label={`3D viewport. ${selectedIds.size} object${selectedIds.size !== 1 ? "s" : ""} selected`}
      aria-roledescription="3D scene viewport"
    >
      <Canvas
        style={{ background: "#1a1a1a" }}
        camera={{ position: [12, 10, 12], fov: 50 }}
        onPointerMissed={() => dispatch({ type: "SELECT", objectId: null })}
      >
        <DropRaycaster raycastRef={raycastRef} />
        <SceneContents
          objects={objects}
          gizmoTargetId={gizmoTargetId}
          gizmoMode={gizmoMode}
          snapEnabled={snapEnabled}
          snapSize={snapSize}
          dispatch={dispatch}
          boxRef={boxRef}
          onBoxSelect={handleBoxSelect}
          onStats={handleStats}
          cameraHandleRef={cameraHandleRef}
        />
      </Canvas>
      {/* Rubber band overlay for box select */}
      {rubberBandStyle && (
        <div
          className="fixed pointer-events-none border border-primary/60 bg-primary/10"
          style={rubberBandStyle}
          aria-hidden="true"
        />
      )}
      <SceneStatsOverlay stats={sceneStats} />
    </div>
  )
}
