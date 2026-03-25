import { type Dispatch, useEffect, useMemo, useState } from "react"
import * as THREE from "three"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Switch } from "@/components/ui/switch"
import { Separator } from "@/components/ui/separator"
import type { SceneAction, SceneObject, SnapSize } from "./types"
import { isDescendantOf } from "./use-scene-store"

interface InspectorPanelProps {
  /** Single selected object (for single-select mode). */
  object: SceneObject | null
  /** All currently selected objects (for multi-select inspector). */
  selectedObjects: SceneObject[]
  allObjects: SceneObject[]
  snapEnabled: boolean
  snapSize: SnapSize
  dispatch: Dispatch<SceneAction>
}

// ── Euler ↔ Quaternion helpers ──────────────────────────────────────────

function quatToEulerDeg(q: [number, number, number, number]): [number, number, number] {
  const euler = new THREE.Euler().setFromQuaternion(
    new THREE.Quaternion(q[0], q[1], q[2], q[3]),
  )
  const RAD2DEG = 180 / Math.PI
  return [euler.x * RAD2DEG, euler.y * RAD2DEG, euler.z * RAD2DEG]
}

function eulerDegToQuat(deg: [number, number, number]): [number, number, number, number] {
  const DEG2RAD = Math.PI / 180
  const euler = new THREE.Euler(
    deg[0] * DEG2RAD,
    deg[1] * DEG2RAD,
    deg[2] * DEG2RAD,
  )
  const q = new THREE.Quaternion().setFromEuler(euler)
  return [q.x, q.y, q.z, q.w]
}

// ── Number input row ────────────────────────────────────────────────────

function Vec3Input({
  label,
  value,
  mixed,
  step = 0.1,
  onChange,
}: {
  label: string
  /** Current value. Used as the base for editing. */
  value: [number, number, number]
  /** Per-axis mixed indicators. When true, that axis has different values
   *  across the multi-selection and shows a placeholder instead. */
  mixed?: [boolean, boolean, boolean]
  step?: number
  onChange: (v: [number, number, number]) => void
}) {
  const labels = ["X", "Y", "Z"]
  // Local state so we commit on blur/Enter, not every keystroke.
  // For mixed axes, start as empty string so the placeholder shows until the user types.
  const [local, setLocal] = useState<[string, string, string]>([
    mixed?.[0] ? "" : value[0].toFixed(3),
    mixed?.[1] ? "" : value[1].toFixed(3),
    mixed?.[2] ? "" : value[2].toFixed(3),
  ])

  // Sync from props when the value changes externally (e.g. gizmo drag)
  useEffect(() => {
    setLocal([
      mixed?.[0] ? "" : value[0].toFixed(3),
      mixed?.[1] ? "" : value[1].toFixed(3),
      mixed?.[2] ? "" : value[2].toFixed(3),
    ])
  }, [value[0], value[1], value[2], mixed?.[0], mixed?.[1], mixed?.[2]])

  const commit = () => {
    const parse = (s: string) => {
      const n = parseFloat(s)
      return Number.isFinite(n) ? n : 0
    }
    const parsed: [number, number, number] = [
      parse(local[0]),
      parse(local[1]),
      parse(local[2]),
    ]
    if (
      parsed[0] !== value[0] ||
      parsed[1] !== value[1] ||
      parsed[2] !== value[2]
    ) {
      onChange(parsed)
    }
  }

  return (
    <div>
      <Label className="text-xs text-muted-foreground">{label}</Label>
      <div className="mt-1 grid grid-cols-3 gap-1">
        {labels.map((axis, i) => (
          <div key={axis} className="relative">
            <span className="absolute left-2 top-1/2 -translate-y-1/2 text-[10px] text-muted-foreground">
              {axis}
            </span>
            <Input
              type="number"
              step={step}
              value={local[i]}
              placeholder={mixed?.[i] && local[i] === "" ? "\u2014" : undefined}
              aria-label={`${label} ${axis}`}
              className="h-7 pl-6 text-xs"
              onChange={(e) => {
                const next = [...local] as [string, string, string]
                next[i] = e.target.value
                setLocal(next)
              }}
              onBlur={commit}
              onKeyDown={(e) => {
                if (e.key === "Enter") commit()
              }}
            />
          </div>
        ))}
      </div>
    </div>
  )
}

// ── Inspector ───────────────────────────────────────────────────────────

/** Snap a value to the nearest grid increment. */
function snapToGrid(value: number, gridSize: number): number {
  if (gridSize === 0) return value
  return Math.round(value / gridSize) * gridSize
}

/** Check if all values in an array are the same. */
function allEqual<T>(values: T[]): boolean {
  if (values.length <= 1) return true
  return values.every((v) => v === values[0])
}

/** Check if vec3 arrays are all equal per-axis. */
function vec3AllEqual(
  values: [number, number, number][],
): [boolean, boolean, boolean] {
  return [
    allEqual(values.map((v) => v[0])),
    allEqual(values.map((v) => v[1])),
    allEqual(values.map((v) => v[2])),
  ]
}

// ── Multi-select inspector ──────────────────────────────────────────────

interface MultiInspectorProps {
  objects: SceneObject[]
  snapEnabled: boolean
  snapSize: SnapSize
  dispatch: Dispatch<SceneAction>
}

function MultiSelectInspector({
  objects,
  snapEnabled,
  snapSize,
  dispatch,
}: MultiInspectorProps) {
  // Compute shared/mixed state for each property
  const positions = objects.map((o) => o.position)
  const scales = objects.map((o) => o.scale)
  const visibles = objects.map((o) => o.visible)

  const posMixed = vec3AllEqual(positions).map((eq) => !eq) as [boolean, boolean, boolean]
  const scaleMixed = vec3AllEqual(scales).map((eq) => !eq) as [boolean, boolean, boolean]
  const visibleShared = allEqual(visibles)

  // Use first object's values as display base — caller guarantees objects.length > 1
  const first = objects[0]!

  const commitPosition = (pos: [number, number, number]) => {
    const finalPos: [number, number, number] = snapEnabled
      ? [snapToGrid(pos[0], snapSize), snapToGrid(pos[1], snapSize), snapToGrid(pos[2], snapSize)]
      : pos
    // Compute delta from first object and apply to all in a single undo step
    const delta: [number, number, number] = [
      finalPos[0] - first.position[0],
      finalPos[1] - first.position[1],
      finalPos[2] - first.position[2],
    ]
    dispatch({
      type: "UPDATE_TRANSFORMS_BATCH",
      updates: objects.map((obj) => ({
        objectId: obj.id,
        position: [
          obj.position[0] + delta[0],
          obj.position[1] + delta[1],
          obj.position[2] + delta[2],
        ] as [number, number, number],
        rotation: obj.rotation,
        scale: obj.scale,
      })),
    })
  }

  const commitScale = (scl: [number, number, number]) => {
    const delta: [number, number, number] = [
      scl[0] - first.scale[0],
      scl[1] - first.scale[1],
      scl[2] - first.scale[2],
    ]
    dispatch({
      type: "UPDATE_TRANSFORMS_BATCH",
      updates: objects.map((obj) => ({
        objectId: obj.id,
        position: obj.position,
        rotation: obj.rotation,
        scale: [
          obj.scale[0] + delta[0],
          obj.scale[1] + delta[1],
          obj.scale[2] + delta[2],
        ] as [number, number, number],
      })),
    })
  }

  return (
    <div
      className="w-72 shrink-0 overflow-y-auto border-l border-border bg-card"
      role="region"
      aria-label="Inspector panel — multiple objects selected"
    >
      <div className="border-b border-border px-3 py-2 text-xs font-medium text-muted-foreground">
        Inspector
      </div>
      <div className="space-y-4 p-3">
        <p className="text-xs text-muted-foreground" aria-live="polite">
          {objects.length} objects selected
        </p>

        <Separator />

        {/* Transform — position and scale (rotation is omitted for multi-select
            because relative deltas on quaternions are complex) */}
        <div className="space-y-3">
          <p className="text-xs font-medium text-muted-foreground">Transform</p>

          <Vec3Input
            label="Position"
            value={first.position}
            mixed={posMixed}
            onChange={commitPosition}
          />

          <Vec3Input
            label="Scale"
            value={first.scale}
            mixed={scaleMixed}
            onChange={commitScale}
          />
        </div>

        <Separator />

        {/* Visibility */}
        <div className="flex items-center justify-between">
          <Label className="text-xs text-muted-foreground">
            Visible{!visibleShared && " (\u2014)"}
          </Label>
          <Switch
            checked={visibleShared ? first.visible : true}
            aria-label={`Set visibility for ${objects.length} objects`}
            onCheckedChange={(checked) => {
              dispatch({
                type: "SET_VISIBILITY_BATCH",
                objectIds: objects.map((o) => o.id),
                visible: checked,
              })
            }}
          />
        </div>
      </div>
    </div>
  )
}

// ── Single-select inspector ─────────────────────────────────────────────

export function InspectorPanel({
  object,
  selectedObjects,
  allObjects,
  snapEnabled,
  snapSize,
  dispatch,
}: InspectorPanelProps) {
  // Multi-select: show shared-property inspector
  if (selectedObjects.length > 1) {
    return (
      <MultiSelectInspector
        objects={selectedObjects}
        snapEnabled={snapEnabled}
        snapSize={snapSize}
        dispatch={dispatch}
      />
    )
  }

  // No selection
  if (!object) {
    return (
      <div
        className="w-72 shrink-0 border-l border-border bg-card p-4 text-center text-xs text-muted-foreground"
        role="region"
        aria-label="Inspector panel — no selection"
      >
        Select an object to inspect
      </div>
    )
  }

  // Single selection — full inspector
  return (
    <SingleObjectInspector
      object={object}
      allObjects={allObjects}
      snapEnabled={snapEnabled}
      snapSize={snapSize}
      dispatch={dispatch}
    />
  )
}

interface SingleInspectorProps {
  object: SceneObject
  allObjects: SceneObject[]
  snapEnabled: boolean
  snapSize: SnapSize
  dispatch: Dispatch<SceneAction>
}

function SingleObjectInspector({
  object,
  allObjects,
  snapEnabled,
  snapSize,
  dispatch,
}: SingleInspectorProps) {
  // Local name state for controlled input
  const [localName, setLocalName] = useState("")

  useEffect(() => {
    if (object) setLocalName(object.name)
  }, [object?.id, object?.name])

  // Euler angles for rotation display
  const eulerDeg = useMemo<[number, number, number]>(
    () => (object ? quatToEulerDeg(object.rotation) : [0, 0, 0]),
    [object?.rotation[0], object?.rotation[1], object?.rotation[2], object?.rotation[3]],
  )

  // Parent candidates: all objects except self and descendants
  const parentCandidates = useMemo(() => {
    if (!object) return []
    return allObjects.filter(
      (o) =>
        o.id !== object.id &&
        !isDescendantOf(allObjects, o.id, object.id),
    )
  }, [object, allObjects])

  const commitName = () => {
    if (localName !== object.name) {
      dispatch({ type: "RENAME_OBJECT", objectId: object.id, name: localName })
    }
  }

  const commitTransform = (
    position: [number, number, number],
    rotation: [number, number, number, number],
    scale: [number, number, number],
    snapPosition = false,
  ) => {
    const finalPosition: [number, number, number] =
      snapEnabled && snapPosition
        ? [
            snapToGrid(position[0], snapSize),
            snapToGrid(position[1], snapSize),
            snapToGrid(position[2], snapSize),
          ]
        : position
    dispatch({
      type: "UPDATE_TRANSFORM",
      objectId: object.id,
      position: finalPosition,
      rotation,
      scale,
    })
  }

  return (
    <div
      className="w-72 shrink-0 overflow-y-auto border-l border-border bg-card"
      role="region"
      aria-label={`Inspector panel — ${object.name}`}
    >
      <div className="border-b border-border px-3 py-2 text-xs font-medium text-muted-foreground">
        Inspector
      </div>
      <div className="space-y-4 p-3">
        {/* Name */}
        <div>
          <Label htmlFor="inspector-name" className="text-xs text-muted-foreground">Name</Label>
          <Input
            id="inspector-name"
            value={localName}
            className="mt-1 h-7 text-xs"
            aria-label="Object name"
            onChange={(e) => setLocalName(e.target.value)}
            onBlur={commitName}
            onKeyDown={(e) => {
              if (e.key === "Enter") commitName()
            }}
          />
        </div>

        <Separator />

        {/* Transform */}
        <div className="space-y-3">
          <p className="text-xs font-medium text-muted-foreground">Transform</p>

          <Vec3Input
            label="Position"
            value={object.position}
            onChange={(pos) =>
              commitTransform(pos, object.rotation, object.scale, true)
            }
          />

          <Vec3Input
            label="Rotation"
            value={eulerDeg}
            step={1}
            onChange={(deg) =>
              commitTransform(
                object.position,
                eulerDegToQuat(deg),
                object.scale,
              )
            }
          />

          <Vec3Input
            label="Scale"
            value={object.scale}
            onChange={(scl) =>
              commitTransform(object.position, object.rotation, scl)
            }
          />
        </div>

        <Separator />

        {/* Hierarchy */}
        <div>
          <p className="text-xs font-medium text-muted-foreground">Hierarchy</p>
          <Label htmlFor="inspector-parent" className="mt-2 text-xs text-muted-foreground">Parent</Label>
          <select
            id="inspector-parent"
            className="mt-1 h-7 w-full rounded-md border border-input bg-background px-2 text-xs"
            value={object.parent_id ?? ""}
            aria-label="Parent object"
            onChange={(e) => {
              const newParentId = e.target.value || null
              dispatch({
                type: "REPARENT_OBJECT",
                objectId: object.id,
                newParentId,
              })
            }}
          >
            <option value="">None (root)</option>
            {parentCandidates.map((o) => (
              <option key={o.id} value={o.id}>
                {o.name}
              </option>
            ))}
          </select>
        </div>

        <Separator />

        {/* Display */}
        <div className="flex items-center justify-between">
          <Label className="text-xs text-muted-foreground">Visible</Label>
          <Switch
            checked={object.visible}
            aria-label={`Toggle visibility for ${object.name}`}
            onCheckedChange={(checked) =>
              dispatch({
                type: "SET_VISIBILITY",
                objectId: object.id,
                visible: checked,
              })
            }
          />
        </div>
      </div>
    </div>
  )
}
