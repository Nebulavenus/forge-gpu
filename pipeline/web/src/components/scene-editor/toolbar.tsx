import { type Dispatch, useMemo, useState } from "react"
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
  Bookmark,
  Camera,
  Home,
  X,
  Pencil,
  Check,
  Group,
  Ungroup,
} from "lucide-react"
import { Button } from "@/components/ui/button"
import { Separator } from "@/components/ui/separator"
import type { CameraBookmark, SceneAction, SceneState, SnapSize } from "./types"
import { SNAP_SIZES } from "./types"
import { DEFAULT_CAMERA_POSITION, DEFAULT_CAMERA_TARGET } from "./viewport"

interface ToolbarProps {
  state: SceneState
  dispatch: Dispatch<SceneAction>
  onSave: () => void
  onAdd: () => void
  /** Read current camera position/target for saving bookmarks. */
  onGetCameraState?: () => { position: [number, number, number]; target: [number, number, number] } | null
  /** Restore camera to a saved bookmark. */
  onRestoreBookmark?: (bookmark: CameraBookmark) => void
}

const GIZMO_MODES = [
  { mode: "translate" as const, icon: Move, label: "Move" },
  { mode: "rotate" as const, icon: RotateCcw, label: "Rotate" },
  { mode: "scale" as const, icon: Maximize2, label: "Scale" },
] as const

export function Toolbar({ state, dispatch, onSave, onAdd, onGetCameraState, onRestoreBookmark }: ToolbarProps) {
  const [bookmarkMenuOpen, setBookmarkMenuOpen] = useState(false)
  const [renamingId, setRenamingId] = useState<string | null>(null)
  const [renameValue, setRenameValue] = useState("")

  const bookmarks = state.scene?.cameras ?? []
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

  const handleSaveView = () => {
    const camState = onGetCameraState?.()
    if (!camState) return
    const bookmark: CameraBookmark = {
      id: crypto.randomUUID().slice(0, 12),
      name: `View ${bookmarks.length + 1}`,
      position: camState.position,
      target: camState.target,
    }
    dispatch({ type: "SAVE_CAMERA_BOOKMARK", bookmark })
  }

  const handleRestore = (bookmark: CameraBookmark) => {
    onRestoreBookmark?.(bookmark)
    setBookmarkMenuOpen(false)
  }

  const handleDelete = (bookmarkId: string) => {
    dispatch({ type: "DELETE_CAMERA_BOOKMARK", bookmarkId })
  }

  const handleStartRename = (bookmark: CameraBookmark) => {
    setRenamingId(bookmark.id)
    setRenameValue(bookmark.name)
  }

  const handleCommitRename = () => {
    if (renamingId && renameValue.trim()) {
      dispatch({ type: "RENAME_CAMERA_BOOKMARK", bookmarkId: renamingId, name: renameValue.trim() })
    }
    setRenamingId(null)
    setRenameValue("")
  }

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

      <Separator orientation="vertical" className="h-6" />

      {/* Reset camera */}
      <Button
        size="sm"
        variant="ghost"
        title="Reset camera to default position"
        aria-label="Reset camera to default position"
        onClick={() => onRestoreBookmark?.({
          id: "_reset",
          name: "Default",
          position: DEFAULT_CAMERA_POSITION,
          target: DEFAULT_CAMERA_TARGET,
        })}
      >
        <Home className="h-4 w-4" />
      </Button>

      {/* Camera bookmarks */}
      <div className="relative">
        <Button
          size="sm"
          variant="ghost"
          title="Save current camera view"
          aria-label="Save current camera view as bookmark"
          onClick={handleSaveView}
        >
          <Camera className="h-4 w-4 mr-1" />
          Save View
        </Button>
        <Button
          size="sm"
          variant={bookmarkMenuOpen ? "default" : "ghost"}
          title="Camera bookmarks"
          aria-label="Open camera bookmarks menu"
          aria-expanded={bookmarkMenuOpen}
          aria-haspopup="true"
          onClick={() => setBookmarkMenuOpen(!bookmarkMenuOpen)}
        >
          <Bookmark className="h-4 w-4 mr-1" />
          Views
          {bookmarks.length > 0 && (
            <span className="ml-1 text-xs text-muted-foreground">
              ({bookmarks.length})
            </span>
          )}
        </Button>

        {bookmarkMenuOpen && (
          <>
            {/* Backdrop to close menu on outside click */}
            {/* eslint-disable-next-line jsx-a11y/click-events-have-key-events, jsx-a11y/no-static-element-interactions */}
            <div
              className="fixed inset-0 z-40"
              onClick={() => {
                setBookmarkMenuOpen(false)
                setRenamingId(null)
              }}
            />
            <div
              className="absolute left-0 top-full z-50 mt-1 min-w-56 rounded-md border border-border bg-popover p-1 shadow-md"
              aria-label="Camera bookmarks"
            >
              {bookmarks.length === 0 ? (
                <p
                  className="px-3 py-2 text-xs text-muted-foreground"
                  role="status"
                >
                  No saved views. Use &quot;Save View&quot; to bookmark the
                  current camera position.
                </p>
              ) : (
                <ul role="list" aria-label="Saved camera views">
                  {bookmarks.map((bm) => (
                    <li
                      key={bm.id}
                      className="flex items-center gap-1 rounded px-1 hover:bg-accent"
                    >
                      {renamingId === bm.id ? (
                        /* Inline rename */
                        <form
                          className="flex flex-1 items-center gap-1"
                          onSubmit={(e) => {
                            e.preventDefault()
                            handleCommitRename()
                          }}
                        >
                          <label className="sr-only" htmlFor={`rename-${bm.id}`}>
                            Rename bookmark
                          </label>
                          <input
                            id={`rename-${bm.id}`}
                            className="flex-1 rounded border border-input bg-background px-1 py-0.5 text-xs"
                            value={renameValue}
                            autoFocus
                            onChange={(e) => setRenameValue(e.target.value)}
                            onBlur={() => {
                              // Guard: skip if renamingId was already cleared
                              // by form submit (Enter key) in the same event cycle.
                              if (renamingId) handleCommitRename()
                            }}
                            onKeyDown={(e) => {
                              if (e.key === "Escape") {
                                setRenamingId(null)
                              }
                            }}
                          />
                          <Button
                            size="sm"
                            variant="ghost"
                            type="submit"
                            className="h-6 w-6 p-0"
                            title="Confirm rename"
                            aria-label="Confirm bookmark rename"
                          >
                            <Check className="h-3 w-3" />
                          </Button>
                        </form>
                      ) : (
                        <>
                          <button
                            type="button"
                            className="flex-1 truncate px-2 py-1.5 text-left text-xs"
                            title={`Restore view: ${bm.name}`}
                            aria-label={`Restore camera to ${bm.name}`}
                            onClick={() => handleRestore(bm)}
                          >
                            {bm.name}
                          </button>
                          <Button
                            size="sm"
                            variant="ghost"
                            className="h-6 w-6 shrink-0 p-0"
                            title={`Rename ${bm.name}`}
                            aria-label={`Rename bookmark ${bm.name}`}
                            onClick={(e) => {
                              e.stopPropagation()
                              handleStartRename(bm)
                            }}
                          >
                            <Pencil className="h-3 w-3" />
                          </Button>
                          <Button
                            size="sm"
                            variant="ghost"
                            className="h-6 w-6 shrink-0 p-0"
                            title={`Delete ${bm.name}`}
                            aria-label={`Delete bookmark ${bm.name}`}
                            onClick={(e) => {
                              e.stopPropagation()
                              handleDelete(bm.id)
                            }}
                          >
                            <X className="h-3 w-3" />
                          </Button>
                        </>
                      )}
                    </li>
                  ))}
                </ul>
              )}
            </div>
          </>
        )}
      </div>

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
