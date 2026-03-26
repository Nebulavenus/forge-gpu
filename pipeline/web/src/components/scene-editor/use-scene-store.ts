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
import { computeWorldPosition, computeWorldRotation, computeWorldScale, quatMultiply, worldToLocal } from "./scene-utils"

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

const EMPTY_SELECTION: Set<string> = new Set()

export const initialState: SceneState = {
  scene: null,
  selectedIds: EMPTY_SELECTION,
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
        selectedIds: EMPTY_SELECTION,
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
      // Reparent children of removed object to root, preserving world position
      const objectMap = new Map(state.scene.objects.map((o) => [o.id, o]))
      const removed = objectMap.get(action.objectId)
      const objects = state.scene.objects
        .filter((o) => o.id !== action.objectId)
        .map((o) => {
          if (o.parent_id !== action.objectId) return o
          // Child was local to removed parent — reparent to grandparent
          const wp = computeWorldPosition(o, objectMap)
          const newParentId = removed?.parent_id ?? null
          if (newParentId !== null) {
            const grandparent = objectMap.get(newParentId)
            if (grandparent) {
              return {
                ...o,
                parent_id: newParentId,
                position: worldToLocal(wp, grandparent, objectMap),
              }
            }
          }
          return { ...o, parent_id: null, position: wp }
        })
      const newIds = new Set(state.selectedIds)
      newIds.delete(action.objectId)
      return {
        ...state,
        ...stacks,
        scene: { ...state.scene, objects },
        selectedIds: newIds.size === state.selectedIds.size ? state.selectedIds : newIds,
        dirty: true,
      }
    }

    case "REMOVE_OBJECTS": {
      if (!state.scene) return state
      if (action.objectIds.length === 0) return state
      const removeSet = new Set(action.objectIds)
      const stacks = pushUndo(state)
      // Build map before filtering so we can compute world positions
      const objectMap = new Map(state.scene.objects.map((o) => [o.id, o]))
      const objects = state.scene.objects
        .filter((o) => !removeSet.has(o.id))
        .map((o) => {
          if (o.parent_id === null || !removeSet.has(o.parent_id)) return o
          // Orphaned child — promote to world space, preserving visual position.
          // Walk up to find the nearest surviving ancestor.
          const wp = computeWorldPosition(o, objectMap)
          let newParent: string | null = null
          const removedParent = objectMap.get(o.parent_id)
          if (removedParent) {
            // Walk up from removed parent to find nearest non-removed ancestor
            let ancestor = removedParent.parent_id
            while (ancestor !== null && removeSet.has(ancestor)) {
              const a = objectMap.get(ancestor)
              ancestor = a ? a.parent_id : null
            }
            newParent = ancestor
          }
          if (newParent !== null) {
            // Convert world position to local relative to surviving ancestor
            const ancestorObj = objectMap.get(newParent)!
            return {
              ...o,
              parent_id: newParent,
              position: worldToLocal(wp, ancestorObj, objectMap),
            }
          }
          return { ...o, parent_id: null, position: wp }
        })
      const newIds = new Set(state.selectedIds)
      for (const id of action.objectIds) newIds.delete(id)
      return {
        ...state,
        ...stacks,
        scene: { ...state.scene, objects },
        selectedIds: newIds.size > 0 ? newIds : EMPTY_SELECTION,
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

    case "UPDATE_TRANSFORMS_BATCH": {
      if (!state.scene) return state
      if (action.updates.length === 0) return state
      const stacks = pushUndo(state)
      const updateMap = new Map(
        action.updates.map((u) => [u.objectId, u]),
      )
      const objects = state.scene.objects.map((o) => {
        const u = updateMap.get(o.id)
        return u
          ? { ...o, position: u.position, rotation: u.rotation, scale: u.scale }
          : o
      })
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
        selectedIds: new Set([clone.id]),
        dirty: true,
      }
    }

    case "DUPLICATE_OBJECTS": {
      if (!state.scene) return state
      if (action.objectIds.length === 0) return state
      const sources = action.objectIds
        .map((id) => state.scene!.objects.find((o) => o.id === id))
        .filter((o): o is SceneObject => o !== undefined)
      if (sources.length === 0) return state
      const stacks = pushUndo(state)
      // Build old→new ID map first so cloned parent/child relationships
      // point to the cloned parent, not the original.
      const idMap = new Map(
        sources.map((source) => [source.id, crypto.randomUUID().slice(0, 12)]),
      )
      const clones = sources.map((source) => {
        const parentWasDuplicated =
          source.parent_id !== null && idMap.has(source.parent_id)

        return {
          ...source,
          id: idMap.get(source.id)!,
          name: `${source.name} (copy)`,
          // Only offset top-level roots; children keep their local position
          // to avoid double-offset when the parent clone is also shifted.
          position: parentWasDuplicated
            ? (source.position as [number, number, number])
            : ([source.position[0] + 1, source.position[1], source.position[2]] as [number, number, number]),
          parent_id: parentWasDuplicated
            ? idMap.get(source.parent_id!)!
            : source.parent_id,
        }
      })
      return {
        ...state,
        ...stacks,
        scene: {
          ...state.scene,
          objects: [...state.scene.objects, ...clones],
        },
        selectedIds: new Set(clones.map((c) => c.id)),
        dirty: true,
      }
    }

    case "UPDATE_MATERIAL_OVERRIDES": {
      if (!state.scene) return state
      const stacks = pushUndo(state)
      const objects = state.scene.objects.map((o) =>
        o.id === action.objectId
          ? {
              ...o,
              material_overrides: {
                ...(o.material_overrides ?? {}),
                ...action.overrides,
              },
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

    case "UPDATE_MATERIAL_OVERRIDES_BATCH": {
      if (!state.scene) return state
      if (action.objectIds.length === 0) return state
      const stacks = pushUndo(state)
      const idSet = new Set(action.objectIds)
      const objects = state.scene.objects.map((o) =>
        idSet.has(o.id)
          ? {
              ...o,
              material_overrides: {
                ...(o.material_overrides ?? {}),
                ...action.overrides,
              },
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

    case "SET_VISIBILITY_BATCH": {
      if (!state.scene) return state
      if (action.objectIds.length === 0) return state
      const stacks = pushUndo(state)
      const idSet = new Set(action.objectIds)
      const objects = state.scene.objects.map((o) =>
        idSet.has(o.id) ? { ...o, visible: action.visible } : o,
      )
      return {
        ...state,
        ...stacks,
        scene: { ...state.scene, objects },
        dirty: true,
      }
    }

    case "GROUP_OBJECTS": {
      if (!state.scene) return state
      if (action.objectIds.length < 2) return state
      const stacks = pushUndo(state)
      const groupId = crypto.randomUUID().slice(0, 12)
      const groupedSet = new Set(action.objectIds)
      const objectMap = new Map(state.scene.objects.map((o) => [o.id, o]))
      // Compute centroid using world-space positions so grouping works
      // correctly for objects that already have parents.
      const grouped = state.scene.objects.filter((o) => groupedSet.has(o.id))
      const worldPositions = grouped.map((o) => computeWorldPosition(o, objectMap))
      const cx = worldPositions.reduce((s, p) => s + p[0], 0) / grouped.length
      const cy = worldPositions.reduce((s, p) => s + p[1], 0) / grouped.length
      const cz = worldPositions.reduce((s, p) => s + p[2], 0) / grouped.length
      const group: SceneObject = {
        id: groupId,
        name: "Group",
        asset_id: null,
        position: [cx, cy, cz],
        rotation: [0, 0, 0, 1],
        scale: [1, 1, 1],
        parent_id: null,
        visible: true,
      }
      // Convert each child's world transform to local space relative to the
      // new group container so they stay at the same visual location,
      // orientation, and size. The group has identity rotation and scale,
      // so the child's local rotation/scale equals its world rotation/scale.
      const worldTransformMap = new Map(
        grouped.map((o, i) => [o.id, {
          pos: worldPositions[i]!,
          rot: computeWorldRotation(o, objectMap),
          scl: computeWorldScale(o, objectMap),
        }]),
      )
      const objects = [
        group,
        ...state.scene.objects.map((o) => {
          if (!groupedSet.has(o.id)) return o
          const wt = worldTransformMap.get(o.id)!
          return {
            ...o,
            parent_id: groupId,
            position: [
              wt.pos[0] - cx,
              wt.pos[1] - cy,
              wt.pos[2] - cz,
            ] as [number, number, number],
            rotation: wt.rot,
            scale: wt.scl,
          }
        }),
      ]
      return {
        ...state,
        ...stacks,
        scene: { ...state.scene, objects },
        selectedIds: new Set([groupId]),
        dirty: true,
      }
    }

    case "UNGROUP_OBJECT": {
      if (!state.scene) return state
      const groupObj = state.scene.objects.find((o) => o.id === action.groupId)
      if (!groupObj) return state
      // Only ungroup empty-asset groups — reject ungrouping asset-backed objects
      if (groupObj.asset_id !== null) return state
      const children = state.scene.objects.filter(
        (o) => o.parent_id === action.groupId,
      )
      if (children.length === 0) return state
      const stacks = pushUndo(state)
      // Compute each child's full world transform (position, rotation, scale),
      // then convert to local space relative to the group's parent (or root).
      // This handles groups that have been rotated or scaled after creation.
      const objectMap = new Map(state.scene.objects.map((o) => [o.id, o]))
      const childIds = new Set(children.map((c) => c.id))
      const grandparent = groupObj.parent_id !== null
        ? objectMap.get(groupObj.parent_id) ?? null
        : null
      const objects = state.scene.objects
        .filter((o) => o.id !== action.groupId)
        .map((o) => {
          if (!childIds.has(o.id)) return o
          const wp = computeWorldPosition(o, objectMap)
          const wr = computeWorldRotation(o, objectMap)
          const ws = computeWorldScale(o, objectMap)
          const localPos = grandparent
            ? worldToLocal(wp, grandparent, objectMap)
            : wp
          // When the grandparent has identity rotation/scale (common case),
          // world rotation/scale IS the local rotation/scale. For non-identity
          // grandparents, factor out the grandparent's accumulated transform.
          let localRot = wr
          let localScale = ws
          if (grandparent) {
            const gpRot = computeWorldRotation(grandparent, objectMap)
            const gpScale = computeWorldScale(grandparent, objectMap)
            // localRot = inverse(gpRot) * worldRot
            const gpRotInv: [number, number, number, number] = [
              -gpRot[0], -gpRot[1], -gpRot[2], gpRot[3],
            ]
            localRot = quatMultiply(gpRotInv, wr)
            // localScale = worldScale / gpScale (component-wise)
            localScale = [
              gpScale[0] !== 0 ? ws[0] / gpScale[0] : ws[0],
              gpScale[1] !== 0 ? ws[1] / gpScale[1] : ws[1],
              gpScale[2] !== 0 ? ws[2] / gpScale[2] : ws[2],
            ]
          }
          return {
            ...o,
            parent_id: groupObj.parent_id,
            position: localPos,
            rotation: localRot,
            scale: localScale,
          }
        })
      return {
        ...state,
        ...stacks,
        scene: { ...state.scene, objects },
        selectedIds: childIds,
        dirty: true,
      }
    }

    // Non-undoable UI state changes
    case "SELECT": {
      const mode = action.mode ?? "replace"
      if (mode === "replace") {
        if (action.objectId === null) {
          return state.selectedIds.size === 0
            ? state
            : { ...state, selectedIds: EMPTY_SELECTION }
        }
        // Single-select replace: if already the only selection, no-op
        if (state.selectedIds.size === 1 && state.selectedIds.has(action.objectId)) {
          return state
        }
        return { ...state, selectedIds: new Set([action.objectId]) }
      }
      if (mode === "add") {
        if (action.objectId === null) return state
        if (state.selectedIds.has(action.objectId)) return state
        const ids = new Set(state.selectedIds)
        ids.add(action.objectId)
        return { ...state, selectedIds: ids }
      }
      // toggle
      if (action.objectId === null) return state
      const ids = new Set(state.selectedIds)
      if (ids.has(action.objectId)) {
        ids.delete(action.objectId)
      } else {
        ids.add(action.objectId)
      }
      return { ...state, selectedIds: ids.size > 0 ? ids : EMPTY_SELECTION }
    }

    case "SELECT_SET": {
      if (action.objectIds.length === 0) {
        return state.selectedIds.size === 0
          ? state
          : { ...state, selectedIds: EMPTY_SELECTION }
      }
      return { ...state, selectedIds: new Set(action.objectIds) }
    }

    case "SELECT_ALL": {
      if (!state.scene || state.scene.objects.length === 0) return state
      const allIds = new Set(state.scene.objects.map((o) => o.id))
      return { ...state, selectedIds: allIds }
    }

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

    case "SAVE_CAMERA_BOOKMARK": {
      if (!state.scene) return state
      const stacks = pushUndo(state)
      const existing = state.scene.cameras ?? []
      return {
        ...state,
        ...stacks,
        scene: {
          ...state.scene,
          cameras: [...existing, action.bookmark],
        },
        dirty: true,
      }
    }

    case "DELETE_CAMERA_BOOKMARK": {
      if (!state.scene) return state
      const cameras = state.scene.cameras ?? []
      if (!cameras.some((c) => c.id === action.bookmarkId)) return state
      const stacks = pushUndo(state)
      return {
        ...state,
        ...stacks,
        scene: {
          ...state.scene,
          cameras: cameras.filter((c) => c.id !== action.bookmarkId),
        },
        dirty: true,
      }
    }

    case "RENAME_CAMERA_BOOKMARK": {
      if (!state.scene) return state
      const cameras = state.scene.cameras ?? []
      const nextName = action.name.trim()
      const current = cameras.find((c) => c.id === action.bookmarkId)
      if (!current || !nextName || current.name === nextName) return state
      const stacks = pushUndo(state)
      return {
        ...state,
        ...stacks,
        scene: {
          ...state.scene,
          cameras: cameras.map((c) =>
            c.id === action.bookmarkId ? { ...c, name: nextName } : c,
          ),
        },
        dirty: true,
      }
    }

    case "UNDO": {
      if (state.undoStack.length === 0 || !state.scene) return state
      const previous = state.undoStack[state.undoStack.length - 1]
      if (!previous) return state
      // Prune selected IDs that no longer exist in the restored scene
      const undoValidIds = new Set(previous.objects.map((o) => o.id))
      const undoSelected = new Set(
        [...state.selectedIds].filter((id) => undoValidIds.has(id)),
      )
      return {
        ...state,
        scene: previous,
        selectedIds: undoSelected.size > 0 ? undoSelected : EMPTY_SELECTION,
        undoStack: state.undoStack.slice(0, -1),
        redoStack: [...state.redoStack, cloneScene(state.scene)],
        dirty: state.undoStack.length > 1,
      }
    }

    case "REDO": {
      if (state.redoStack.length === 0 || !state.scene) return state
      const next = state.redoStack[state.redoStack.length - 1]
      if (!next) return state
      // Prune selected IDs that no longer exist in the restored scene
      const redoValidIds = new Set(next.objects.map((o) => o.id))
      const redoSelected = new Set(
        [...state.selectedIds].filter((id) => redoValidIds.has(id)),
      )
      return {
        ...state,
        scene: next,
        selectedIds: redoSelected.size > 0 ? redoSelected : EMPTY_SELECTION,
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

  // Keyboard shortcuts: Ctrl+Z = undo, Ctrl+Shift+Z = redo, Ctrl+D = duplicate,
  // Ctrl+A = select all, Delete/Backspace = remove selected
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
        if (state.selectedIds.size === 1) {
          dispatch({ type: "DUPLICATE_OBJECT", objectId: [...state.selectedIds][0]! })
        } else if (state.selectedIds.size > 1) {
          dispatch({ type: "DUPLICATE_OBJECTS", objectIds: [...state.selectedIds] })
        }
      }
      if ((e.ctrlKey || e.metaKey) && key === "a") {
        e.preventDefault()
        dispatch({ type: "SELECT_ALL" })
      }
      if (key === "delete" || key === "backspace") {
        if (state.selectedIds.size > 0) {
          e.preventDefault()
          dispatch({ type: "REMOVE_OBJECTS", objectIds: [...state.selectedIds] })
        }
      }
    }
    window.addEventListener("keydown", handler)
    return () => window.removeEventListener("keydown", handler)
  }, [state.selectedIds])

  // Derive the "primary" selected object (last in set iteration order) for
  // the inspector panel when a single object is selected. For multi-select,
  // consumers use selectedIds directly.
  const selectedObjects =
    state.scene?.objects.filter((o) => state.selectedIds.has(o.id)) ?? []

  const selectedObject = selectedObjects.length === 1 ? (selectedObjects[0] ?? null) : null

  const rootObjects =
    state.scene?.objects.filter((o) => o.parent_id === null) ?? []

  return { state, dispatch, selectedObject, selectedObjects, rootObjects }
}
