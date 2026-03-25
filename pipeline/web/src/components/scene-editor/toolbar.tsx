import { type Dispatch, useMemo } from "react"
import {
  Move,
  RotateCcw,
  Maximize2,
  Plus,
  Copy,
  Trash2,
  Undo2,
  Redo2,
  Save,
  Grid3x3,
  Group,
  Ungroup,
} from "lucide-react"
import { Button } from "@/components/ui/button"
import { Separator } from "@/components/ui/separator"
import type { SceneAction, SceneState, SnapSize } from "./types"
import { SNAP_SIZES } from "./types"

interface ToolbarProps {
  state: SceneState
  dispatch: Dispatch<SceneAction>
  onSave: () => void
  onAdd: () => void
}

const GIZMO_MODES = [
  { mode: "translate" as const, icon: Move, label: "Move" },
  { mode: "rotate" as const, icon: RotateCcw, label: "Rotate" },
  { mode: "scale" as const, icon: Maximize2, label: "Scale" },
] as const

export function Toolbar({ state, dispatch, onSave, onAdd }: ToolbarProps) {
  const selCount = state.selectedIds.size
  const hasSelection = selCount > 0
  const hasMultiSelection = selCount >= 2

  // For group: get the selected IDs as an array
  const selectedIdsArray = [...state.selectedIds]

  // For ungroup: check if the single selected object is a group (empty parent
  // with children). Only enabled when exactly one group-like object is selected.
  const canUngroup = useMemo(() => {
    if (selCount !== 1 || state.scene === null) return false
    const id = selectedIdsArray[0]
    const obj = state.scene.objects.find((o) => o.id === id)
    if (!obj || obj.asset_id !== null) return false
    return state.scene.objects.some((o) => o.parent_id === id)
  }, [selCount, state.scene, selectedIdsArray])

  return (
    <div
      className="flex items-center gap-2 border-b border-border bg-card px-4 py-2"
      role="toolbar"
      aria-label="Scene editor toolbar"
    >
      {/* Gizmo mode toggle */}
      <div className="flex gap-1" role="radiogroup" aria-label="Transform mode">
        {GIZMO_MODES.map(({ mode, icon: Icon, label }) => (
          <Button
            key={mode}
            size="sm"
            variant={state.gizmoMode === mode ? "default" : "ghost"}
            title={label}
            role="radio"
            aria-checked={state.gizmoMode === mode}
            aria-label={label}
            onClick={() => dispatch({ type: "SET_GIZMO_MODE", mode })}
          >
            <Icon className="h-4 w-4" />
          </Button>
        ))}
      </div>

      <Separator orientation="vertical" className="h-6" />

      {/* Add / Duplicate / Delete */}
      <Button size="sm" variant="ghost" onClick={onAdd} title="Add object" aria-label="Add object">
        <Plus className="h-4 w-4 mr-1" />
        Add
      </Button>
      <Button
        size="sm"
        variant="ghost"
        disabled={!hasSelection}
        title={`Duplicate selected (Ctrl+D)${selCount > 1 ? ` — ${selCount} objects` : ""}`}
        aria-label={`Duplicate ${selCount} selected object${selCount !== 1 ? "s" : ""}`}
        onClick={() => {
          if (selCount === 1 && selectedIdsArray[0]) {
            dispatch({ type: "DUPLICATE_OBJECT", objectId: selectedIdsArray[0] })
          } else if (selCount > 1) {
            dispatch({ type: "DUPLICATE_OBJECTS", objectIds: selectedIdsArray })
          }
        }}
      >
        <Copy className="h-4 w-4" />
      </Button>
      <Button
        size="sm"
        variant="ghost"
        disabled={!hasSelection}
        title={`Delete selected${selCount > 1 ? ` (${selCount} objects)` : ""}`}
        aria-label={`Delete ${selCount} selected object${selCount !== 1 ? "s" : ""}`}
        onClick={() => {
          dispatch({ type: "REMOVE_OBJECTS", objectIds: selectedIdsArray })
        }}
      >
        <Trash2 className="h-4 w-4" />
      </Button>

      <Separator orientation="vertical" className="h-6" />

      {/* Group / Ungroup */}
      <Button
        size="sm"
        variant="ghost"
        disabled={!hasMultiSelection}
        title="Group selected objects"
        aria-label={`Group ${selCount} selected objects`}
        onClick={() => {
          dispatch({ type: "GROUP_OBJECTS", objectIds: selectedIdsArray })
        }}
      >
        <Group className="h-4 w-4 mr-1" />
        Group
      </Button>
      <Button
        size="sm"
        variant="ghost"
        disabled={!canUngroup}
        title="Ungroup selected group"
        aria-label="Ungroup selected group"
        onClick={() => {
          if (selectedIdsArray[0]) {
            dispatch({ type: "UNGROUP_OBJECT", groupId: selectedIdsArray[0] })
          }
        }}
      >
        <Ungroup className="h-4 w-4 mr-1" />
        Ungroup
      </Button>

      <Separator orientation="vertical" className="h-6" />

      {/* Undo / Redo */}
      <Button
        size="sm"
        variant="ghost"
        disabled={state.undoStack.length === 0}
        title="Undo (Ctrl+Z)"
        aria-label="Undo"
        onClick={() => dispatch({ type: "UNDO" })}
      >
        <Undo2 className="h-4 w-4" />
      </Button>
      <Button
        size="sm"
        variant="ghost"
        disabled={state.redoStack.length === 0}
        title="Redo (Ctrl+Shift+Z)"
        aria-label="Redo"
        onClick={() => dispatch({ type: "REDO" })}
      >
        <Redo2 className="h-4 w-4" />
      </Button>

      <Separator orientation="vertical" className="h-6" />

      {/* Snap to grid */}
      <Button
        size="sm"
        variant={state.snapEnabled ? "default" : "ghost"}
        title="Snap to grid"
        aria-pressed={state.snapEnabled}
        aria-label="Snap to grid toggle"
        onClick={() =>
          dispatch({ type: "SET_SNAP", enabled: !state.snapEnabled })
        }
      >
        <Grid3x3 className="h-4 w-4 mr-1" />
        Snap
      </Button>
      {state.snapEnabled && (
        <select
          className="h-7 rounded-md border border-input bg-background px-1 text-xs"
          value={state.snapSize}
          title="Grid size"
          aria-label="Grid size"
          onChange={(e) => {
            const parsed = parseFloat(e.target.value)
            if (SNAP_SIZES.includes(parsed as SnapSize)) {
              dispatch({ type: "SET_SNAP", size: parsed as SnapSize })
            }
          }}
        >
          {SNAP_SIZES.map((s) => (
            <option key={s} value={s}>
              {s}
            </option>
          ))}
        </select>
      )}

      {/* Selection count indicator */}
      {selCount > 1 && (
        <span
          className="text-xs text-muted-foreground"
          aria-live="polite"
        >
          {selCount} selected
        </span>
      )}

      {/* Spacer */}
      <div className="flex-1" />

      {/* Save */}
      <Button
        size="sm"
        disabled={!state.dirty}
        title="Save scene"
        aria-label="Save scene"
        onClick={onSave}
      >
        <Save className="h-4 w-4 mr-1" />
        Save
      </Button>
    </div>
  )
}
