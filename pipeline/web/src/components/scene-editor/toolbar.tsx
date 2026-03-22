import type { Dispatch } from "react"
import {
  Move,
  RotateCcw,
  Maximize2,
  Plus,
  Trash2,
  Undo2,
  Redo2,
  Save,
} from "lucide-react"
import { Button } from "@/components/ui/button"
import { Separator } from "@/components/ui/separator"
import type { SceneAction, SceneState } from "./types"

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
  return (
    <div className="flex items-center gap-2 border-b border-border bg-card px-4 py-2">
      {/* Gizmo mode toggle */}
      <div className="flex gap-1">
        {GIZMO_MODES.map(({ mode, icon: Icon, label }) => (
          <Button
            key={mode}
            size="sm"
            variant={state.gizmoMode === mode ? "default" : "ghost"}
            title={label}
            onClick={() => dispatch({ type: "SET_GIZMO_MODE", mode })}
          >
            <Icon className="h-4 w-4" />
          </Button>
        ))}
      </div>

      <Separator orientation="vertical" className="h-6" />

      {/* Add / Delete */}
      <Button size="sm" variant="ghost" onClick={onAdd} title="Add object">
        <Plus className="h-4 w-4 mr-1" />
        Add
      </Button>
      <Button
        size="sm"
        variant="ghost"
        disabled={!state.selectedId}
        title="Delete selected"
        onClick={() => {
          if (state.selectedId) {
            dispatch({ type: "REMOVE_OBJECT", objectId: state.selectedId })
          }
        }}
      >
        <Trash2 className="h-4 w-4" />
      </Button>

      <Separator orientation="vertical" className="h-6" />

      {/* Undo / Redo */}
      <Button
        size="sm"
        variant="ghost"
        disabled={state.undoStack.length === 0}
        title="Undo (Ctrl+Z)"
        onClick={() => dispatch({ type: "UNDO" })}
      >
        <Undo2 className="h-4 w-4" />
      </Button>
      <Button
        size="sm"
        variant="ghost"
        disabled={state.redoStack.length === 0}
        title="Redo (Ctrl+Shift+Z)"
        onClick={() => dispatch({ type: "REDO" })}
      >
        <Redo2 className="h-4 w-4" />
      </Button>

      {/* Spacer */}
      <div className="flex-1" />

      {/* Save */}
      <Button
        size="sm"
        disabled={!state.dirty}
        title="Save scene"
        onClick={onSave}
      >
        <Save className="h-4 w-4 mr-1" />
        Save
      </Button>
    </div>
  )
}
