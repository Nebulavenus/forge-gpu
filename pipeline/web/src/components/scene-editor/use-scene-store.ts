/**
 * Scene state manager with snapshot-based undo/redo.
 *
 * Uses useReducer with the command pattern: every undoable action pushes
 * a full scene snapshot to the undo stack before applying the mutation.
 * This is simple and correct — scene objects are small JSON, so 50
 * snapshots of 100 objects is negligible memory (~500 KB).
 */

import { useEffect, useReducer } from "react"
import type {
  GizmoMode,
  SceneAction,
  SceneData,
  SceneObject,
  SceneState,
  SnapSize,
} from "./types"
import { SNAP_SIZES } from "./types"

// ── Helpers ─────────────────────────────────────────────────────────────

/** Check if objectId is a descendant of ancestorId in the object tree. */
export function isDescendantOf(
  objects: SceneObject[],
  objectId: string,
  ancestorId: string,
): boolean {
  const parentMap = new Map(objects.map((o) => [o.id, o.parent_id]))
  let current: string | null | undefined = objectId
  while (current != null) {
    current = parentMap.get(current)
    if (current === ancestorId) return true
  }
  return false
}

/** Deep-clone a SceneData for the undo stack. */
function cloneScene(scene: SceneData): SceneData {
  return JSON.parse(JSON.stringify(scene)) as SceneData
}

// ── Reducer ─────────────────────────────────────────────────────────────

export const initialState: SceneState = {
  scene: null,
  selectedId: null,
  gizmoMode: "translate" as GizmoMode,
  snapEnabled: false,
  snapSize: 1.0 as SnapSize,
  undoStack: [],
  redoStack: [],
  dirty: false,
}

/** Maximum number of undo snapshots to retain. */
const MAX_UNDO = 50

/** Push current scene to undo stack and clear redo stack. */
function pushUndo(state: SceneState): {
  undoStack: SceneData[]
  redoStack: SceneData[]
} {
  if (!state.scene) return { undoStack: state.undoStack, redoStack: [] }
  return {
    undoStack: [...state.undoStack, cloneScene(state.scene)].slice(-MAX_UNDO),
    redoStack: [],
  }
}

export function sceneReducer(
  state: SceneState,
  action: SceneAction,
): SceneState {
  switch (action.type) {
    case "LOAD_SCENE":
      return {
        ...state,
        scene: action.scene,
        selectedId: null,
        undoStack: [],
        redoStack: [],
        dirty: false,
      }

    case "ADD_OBJECT": {
      if (!state.scene) return state
      const stacks = pushUndo(state)
      return {
        ...state,
        ...stacks,
        scene: {
          ...state.scene,
          objects: [...state.scene.objects, action.object],
        },
        dirty: true,
      }
    }

    case "REMOVE_OBJECT": {
      if (!state.scene) return state
      const stacks = pushUndo(state)
      // Reparent children of removed object to null (root)
      const objects = state.scene.objects
        .filter((o) => o.id !== action.objectId)
        .map((o) =>
          o.parent_id === action.objectId ? { ...o, parent_id: null } : o,
        )
      return {
        ...state,
        ...stacks,
        scene: { ...state.scene, objects },
        selectedId:
          state.selectedId === action.objectId ? null : state.selectedId,
        dirty: true,
      }
    }

    case "UPDATE_TRANSFORM": {
      if (!state.scene) return state
      const stacks = pushUndo(state)
      const objects = state.scene.objects.map((o) =>
        o.id === action.objectId
          ? {
              ...o,
              position: action.position,
              rotation: action.rotation,
              scale: action.scale,
            }
          : o,
      )
      return {
        ...state,
        ...stacks,
        scene: { ...state.scene, objects },
        dirty: true,
      }
    }

    case "RENAME_OBJECT": {
      if (!state.scene) return state
      const stacks = pushUndo(state)
      const objects = state.scene.objects.map((o) =>
        o.id === action.objectId ? { ...o, name: action.name } : o,
      )
      return {
        ...state,
        ...stacks,
        scene: { ...state.scene, objects },
        dirty: true,
      }
    }

    case "REPARENT_OBJECT": {
      if (!state.scene) return state
      // Reject self-parenting
      if (action.newParentId === action.objectId) {
        return state
      }
      // Prevent circular references
      if (
        action.newParentId !== null &&
        isDescendantOf(state.scene.objects, action.newParentId, action.objectId)
      ) {
        return state // reject — would create a cycle
      }
      // Skip if object doesn't exist or parent is already correct
      const reparentObj = state.scene.objects.find((o) => o.id === action.objectId)
      if (!reparentObj) return state
      if (reparentObj.parent_id === action.newParentId) return state
      const stacks = pushUndo(state)
      const objects = state.scene.objects.map((o) =>
        o.id === action.objectId
          ? { ...o, parent_id: action.newParentId }
          : o,
      )
      return {
        ...state,
        ...stacks,
        scene: { ...state.scene, objects },
        dirty: true,
      }
    }

    case "REORDER_OBJECT": {
      if (!state.scene) return state
      // Reject self-parenting
      if (action.newParentId === action.objectId) return state
      // Prevent circular references
      if (
        action.newParentId !== null &&
        isDescendantOf(state.scene.objects, action.newParentId, action.objectId)
      ) {
        return state
      }
      // Update parent_id and reorder within the objects array so the object
      // appears before `beforeId` among its new siblings.
      const obj = state.scene.objects.find((o) => o.id === action.objectId)
      if (!obj) return state
      const updated = { ...obj, parent_id: action.newParentId }
      // Remove the object from its current position
      const without = state.scene.objects.filter(
        (o) => o.id !== action.objectId,
      )
      // Insert before `beforeId` if specified, otherwise append
      if (action.beforeId !== null) {
        const idx = without.findIndex((o) => o.id === action.beforeId)
        if (idx >= 0) {
          without.splice(idx, 0, updated)
        } else {
          without.push(updated)
        }
      } else {
        without.push(updated)
      }
      // Skip undo/dirty when the move is a no-op (same parent and same
      // sibling order). Compare only the sibling sequence for the relevant
      // parent — global array interleaving may differ without affecting order.
      if (obj.parent_id === action.newParentId) {
        const oldSiblings = state.scene.objects
          .filter((o) => o.parent_id === obj.parent_id)
          .map((o) => o.id)
        const newSiblings = without
          .filter((o) => o.parent_id === action.newParentId)
          .map((o) => o.id)
        if (
          oldSiblings.length === newSiblings.length &&
          oldSiblings.every((id, i) => id === newSiblings[i])
        ) {
          return state
        }
      }
      const stacks = pushUndo(state)
      return {
        ...state,
        ...stacks,
        scene: { ...state.scene, objects: without },
        dirty: true,
      }
    }

    case "DUPLICATE_OBJECT": {
      if (!state.scene) return state
      const source = state.scene.objects.find(
        (o) => o.id === action.objectId,
      )
      if (!source) return state
      const stacks = pushUndo(state)
      const clone: SceneObject = {
        ...source,
        id: crypto.randomUUID().slice(0, 12),
        name: `${source.name} (copy)`,
        position: [source.position[0] + 1, source.position[1], source.position[2]],
      }
      return {
        ...state,
        ...stacks,
        scene: {
          ...state.scene,
          objects: [...state.scene.objects, clone],
        },
        selectedId: clone.id,
        dirty: true,
      }
    }

    case "SET_VISIBILITY": {
      if (!state.scene) return state
      const stacks = pushUndo(state)
      const objects = state.scene.objects.map((o) =>
        o.id === action.objectId ? { ...o, visible: action.visible } : o,
      )
      return {
        ...state,
        ...stacks,
        scene: { ...state.scene, objects },
        dirty: true,
      }
    }

    // Non-undoable UI state changes
    case "SELECT":
      return { ...state, selectedId: action.objectId }

    case "SET_GIZMO_MODE":
      return { ...state, gizmoMode: action.mode }

    case "SET_SNAP": {
      const newEnabled = action.enabled ?? state.snapEnabled
      const newSize =
        action.size != null && SNAP_SIZES.includes(action.size)
          ? action.size
          : state.snapSize
      if (newEnabled === state.snapEnabled && newSize === state.snapSize) {
        return state
      }
      return {
        ...state,
        snapEnabled: newEnabled,
        snapSize: newSize,
      }
    }

    case "UNDO": {
      if (state.undoStack.length === 0 || !state.scene) return state
      const previous = state.undoStack[state.undoStack.length - 1] ?? null
      return {
        ...state,
        scene: previous,
        undoStack: state.undoStack.slice(0, -1),
        redoStack: [...state.redoStack, cloneScene(state.scene)],
        dirty: state.undoStack.length > 1,
      }
    }

    case "REDO": {
      if (state.redoStack.length === 0 || !state.scene) return state
      const next = state.redoStack[state.redoStack.length - 1] ?? null
      return {
        ...state,
        scene: next,
        redoStack: state.redoStack.slice(0, -1),
        undoStack: [...state.undoStack, cloneScene(state.scene)].slice(
          -MAX_UNDO,
        ),
        dirty: true,
      }
    }

    default:
      return state
  }
}

// ── Hook ────────────────────────────────────────────────────────────────

export function useSceneStore() {
  const [state, dispatch] = useReducer(sceneReducer, initialState)

  // Keyboard shortcuts: Ctrl+Z = undo, Ctrl+Shift+Z = redo
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      // Don't intercept shortcuts when editing text
      const target = e.target as HTMLElement
      if (
        target.tagName === "INPUT" ||
        target.tagName === "TEXTAREA" ||
        target.isContentEditable
      ) {
        return
      }
      const key = e.key.toLowerCase()
      if ((e.ctrlKey || e.metaKey) && key === "z") {
        e.preventDefault()
        dispatch({ type: e.shiftKey ? "REDO" : "UNDO" })
      }
      if ((e.ctrlKey || e.metaKey) && key === "d") {
        e.preventDefault()
        if (state.selectedId) {
          dispatch({ type: "DUPLICATE_OBJECT", objectId: state.selectedId })
        }
      }
    }
    window.addEventListener("keydown", handler)
    return () => window.removeEventListener("keydown", handler)
  }, [state.selectedId])

  const selectedObject =
    state.scene?.objects.find((o) => o.id === state.selectedId) ?? null

  const rootObjects =
    state.scene?.objects.filter((o) => o.parent_id === null) ?? []

  return { state, dispatch, selectedObject, rootObjects }
}
