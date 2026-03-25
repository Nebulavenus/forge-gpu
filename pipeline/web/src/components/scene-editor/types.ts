/** Types for the scene editor — authored scene composition. */

export interface SceneObject {
  id: string
  name: string
  asset_id: string | null
  position: [number, number, number]
  rotation: [number, number, number, number] // quaternion xyzw
  scale: [number, number, number]
  parent_id: string | null
  visible: boolean
}

export interface SceneData {
  version: number
  name: string
  created_at: string
  modified_at: string
  objects: SceneObject[]
}

export interface SceneListItem {
  id: string
  name: string
  modified_at: string
  object_count: number
}

export type GizmoMode = "translate" | "rotate" | "scale"

/** Available snap grid sizes (world units for translation). */
export const SNAP_SIZES = [0.25, 0.5, 1.0, 2.0] as const
export type SnapSize = (typeof SNAP_SIZES)[number]

/** Selection mode for the SELECT action. */
export type SelectMode = "replace" | "add" | "toggle"

export type SceneAction =
  | { type: "LOAD_SCENE"; scene: SceneData }
  | { type: "ADD_OBJECT"; object: SceneObject }
  | { type: "REMOVE_OBJECT"; objectId: string }
  | { type: "REMOVE_OBJECTS"; objectIds: string[] }
  | {
      type: "UPDATE_TRANSFORM"
      objectId: string
      position: [number, number, number]
      rotation: [number, number, number, number]
      scale: [number, number, number]
    }
  | {
      type: "UPDATE_TRANSFORMS_BATCH"
      updates: {
        objectId: string
        position: [number, number, number]
        rotation: [number, number, number, number]
        scale: [number, number, number]
      }[]
    }
  | { type: "RENAME_OBJECT"; objectId: string; name: string }
  | { type: "REPARENT_OBJECT"; objectId: string; newParentId: string | null }
  | {
      type: "REORDER_OBJECT"
      objectId: string
      newParentId: string | null
      beforeId: string | null
    }
  | { type: "SET_VISIBILITY"; objectId: string; visible: boolean }
  | { type: "SET_VISIBILITY_BATCH"; objectIds: string[]; visible: boolean }
  | { type: "DUPLICATE_OBJECT"; objectId: string }
  | { type: "DUPLICATE_OBJECTS"; objectIds: string[] }
  | { type: "SELECT"; objectId: string | null; mode?: SelectMode }
  | { type: "SELECT_SET"; objectIds: string[] }
  | { type: "SELECT_ALL" }
  | { type: "SET_GIZMO_MODE"; mode: GizmoMode }
  | { type: "SET_SNAP"; enabled?: boolean; size?: SnapSize }
  | { type: "GROUP_OBJECTS"; objectIds: string[] }
  | { type: "UNGROUP_OBJECT"; groupId: string }
  | { type: "UNDO" }
  | { type: "REDO" }

export interface SceneState {
  scene: SceneData | null
  /** Set of currently selected object IDs. */
  selectedIds: Set<string>
  gizmoMode: GizmoMode
  snapEnabled: boolean
  snapSize: SnapSize
  undoStack: SceneData[]
  redoStack: SceneData[]
  dirty: boolean
}
