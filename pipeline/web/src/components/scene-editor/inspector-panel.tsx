import { type Dispatch, useEffect, useMemo, useState } from "react"
import * as THREE from "three"
import { Input } from "@/components/ui/input"
import { Label } from "@/components/ui/label"
import { Switch } from "@/components/ui/switch"
import { Separator } from "@/components/ui/separator"
import type { SceneAction, SceneObject } from "./types"
import { isDescendantOf } from "./use-scene-store"

interface InspectorPanelProps {
  object: SceneObject | null
  allObjects: SceneObject[]
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
  step = 0.1,
  onChange,
}: {
  label: string
  value: [number, number, number]
  step?: number
  onChange: (v: [number, number, number]) => void
}) {
  const labels = ["X", "Y", "Z"]
  // Local state so we commit on blur/Enter, not every keystroke
  const [local, setLocal] = useState<[string, string, string]>([
    value[0].toFixed(3),
    value[1].toFixed(3),
    value[2].toFixed(3),
  ])

  // Sync from props when the value changes externally (e.g. gizmo drag)
  useEffect(() => {
    setLocal([value[0].toFixed(3), value[1].toFixed(3), value[2].toFixed(3)])
  }, [value[0], value[1], value[2]])

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

export function InspectorPanel({
  object,
  allObjects,
  dispatch,
}: InspectorPanelProps) {
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

  if (!object) {
    return (
      <div className="w-72 shrink-0 border-l border-border bg-card p-4 text-center text-xs text-muted-foreground">
        Select an object to inspect
      </div>
    )
  }

  const commitName = () => {
    if (localName !== object.name) {
      dispatch({ type: "RENAME_OBJECT", objectId: object.id, name: localName })
    }
  }

  const commitTransform = (
    position: [number, number, number],
    rotation: [number, number, number, number],
    scale: [number, number, number],
  ) => {
    dispatch({
      type: "UPDATE_TRANSFORM",
      objectId: object.id,
      position,
      rotation,
      scale,
    })
  }

  return (
    <div className="w-72 shrink-0 overflow-y-auto border-l border-border bg-card">
      <div className="border-b border-border px-3 py-2 text-xs font-medium text-muted-foreground">
        Inspector
      </div>
      <div className="space-y-4 p-3">
        {/* Name */}
        <div>
          <Label className="text-xs text-muted-foreground">Name</Label>
          <Input
            value={localName}
            className="mt-1 h-7 text-xs"
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
              commitTransform(pos, object.rotation, object.scale)
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
          <Label className="mt-2 text-xs text-muted-foreground">Parent</Label>
          <select
            className="mt-1 h-7 w-full rounded-md border border-input bg-background px-2 text-xs"
            value={object.parent_id ?? ""}
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
