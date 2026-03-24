import { describe, expect, it } from "vitest"
import type { SceneData, SceneObject, SceneState } from "../types"
import { initialState, sceneReducer } from "../use-scene-store"

// ── Helpers ─────────────────────────────────────────────────────────────

function makeScene(objects: SceneObject[] = []): SceneData {
  return {
    version: 1,
    name: "Test",
    created_at: "2026-01-01T00:00:00Z",
    modified_at: "2026-01-01T00:00:00Z",
    objects,
  }
}

function makeObject(id: string, overrides: Partial<SceneObject> = {}): SceneObject {
  return {
    id,
    name: `Object ${id}`,
    asset_id: null,
    position: [0, 0, 0],
    rotation: [0, 0, 0, 1],
    scale: [1, 1, 1],
    parent_id: null,
    visible: true,
    ...overrides,
  }
}

function loadedState(objects: SceneObject[] = []): SceneState {
  return sceneReducer(initialState, {
    type: "LOAD_SCENE",
    scene: makeScene(objects),
  })
}

// ── Tests ───────────────────────────────────────────────────────────────

describe("sceneReducer", () => {
  it("LOAD_SCENE sets scene and clears stacks", () => {
    const scene = makeScene([makeObject("a")])
    const state = sceneReducer(initialState, { type: "LOAD_SCENE", scene })

    expect(state.scene).toEqual(scene)
    expect(state.selectedId).toBeNull()
    expect(state.undoStack).toHaveLength(0)
    expect(state.redoStack).toHaveLength(0)
    expect(state.dirty).toBe(false)
  })

  it("ADD_OBJECT adds object and pushes to undo", () => {
    const state = loadedState()
    const obj = makeObject("new1")
    const next = sceneReducer(state, { type: "ADD_OBJECT", object: obj })

    expect(next.scene!.objects).toHaveLength(1)
    expect(next.scene!.objects[0].id).toBe("new1")
    expect(next.undoStack).toHaveLength(1)
    expect(next.redoStack).toHaveLength(0)
    expect(next.dirty).toBe(true)
  })

  it("REMOVE_OBJECT removes object and reparents children to null", () => {
    const parent = makeObject("parent")
    const child = makeObject("child", { parent_id: "parent" })
    const state = loadedState([parent, child])

    const next = sceneReducer(state, {
      type: "REMOVE_OBJECT",
      objectId: "parent",
    })

    expect(next.scene!.objects).toHaveLength(1)
    expect(next.scene!.objects[0].id).toBe("child")
    expect(next.scene!.objects[0].parent_id).toBeNull()
    expect(next.undoStack).toHaveLength(1)
    expect(next.dirty).toBe(true)
  })

  it("UPDATE_TRANSFORM updates the correct object", () => {
    const state = loadedState([makeObject("a"), makeObject("b")])
    const next = sceneReducer(state, {
      type: "UPDATE_TRANSFORM",
      objectId: "a",
      position: [1, 2, 3],
      rotation: [0, 0.707, 0, 0.707],
      scale: [2, 2, 2],
    })

    const obj = next.scene!.objects.find((o) => o.id === "a")!
    expect(obj.position).toEqual([1, 2, 3])
    expect(obj.rotation).toEqual([0, 0.707, 0, 0.707])
    expect(obj.scale).toEqual([2, 2, 2])

    // b unchanged
    const b = next.scene!.objects.find((o) => o.id === "b")!
    expect(b.position).toEqual([0, 0, 0])
  })

  it("RENAME_OBJECT updates name", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "RENAME_OBJECT",
      objectId: "a",
      name: "New Name",
    })

    expect(next.scene!.objects[0].name).toBe("New Name")
    expect(next.undoStack).toHaveLength(1)
  })

  it("REPARENT_OBJECT updates parent_id", () => {
    const state = loadedState([makeObject("a"), makeObject("b")])
    const next = sceneReducer(state, {
      type: "REPARENT_OBJECT",
      objectId: "b",
      newParentId: "a",
    })

    expect(next.scene!.objects.find((o) => o.id === "b")!.parent_id).toBe("a")
    expect(next.undoStack).toHaveLength(1)
  })

  it("REPARENT_OBJECT rejects circular reference", () => {
    const a = makeObject("a")
    const b = makeObject("b", { parent_id: "a" })
    const state = loadedState([a, b])

    // Try to make a a child of b — would create a→b→a cycle
    const next = sceneReducer(state, {
      type: "REPARENT_OBJECT",
      objectId: "a",
      newParentId: "b",
    })

    // State should be unchanged
    expect(next.scene!.objects.find((o) => o.id === "a")!.parent_id).toBeNull()
    expect(next.undoStack).toHaveLength(0)
  })

  it("SET_VISIBILITY updates visible flag", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "SET_VISIBILITY",
      objectId: "a",
      visible: false,
    })

    expect(next.scene!.objects[0].visible).toBe(false)
    expect(next.undoStack).toHaveLength(1)
  })

  it("UNDO restores previous scene from undo stack", () => {
    const state = loadedState([makeObject("a")])
    // Add an object to create an undo entry
    const withObj = sceneReducer(state, {
      type: "ADD_OBJECT",
      object: makeObject("b"),
    })
    expect(withObj.scene!.objects).toHaveLength(2)

    const undone = sceneReducer(withObj, { type: "UNDO" })
    expect(undone.scene!.objects).toHaveLength(1)
    expect(undone.undoStack).toHaveLength(0)
    expect(undone.redoStack).toHaveLength(1)
  })

  it("REDO restores from redo stack", () => {
    const state = loadedState([makeObject("a")])
    const withObj = sceneReducer(state, {
      type: "ADD_OBJECT",
      object: makeObject("b"),
    })
    const undone = sceneReducer(withObj, { type: "UNDO" })
    const redone = sceneReducer(undone, { type: "REDO" })

    expect(redone.scene!.objects).toHaveLength(2)
    expect(redone.undoStack).toHaveLength(1)
    expect(redone.redoStack).toHaveLength(0)
  })

  it("UNDO with empty stack is a no-op", () => {
    const state = loadedState()
    const next = sceneReducer(state, { type: "UNDO" })
    expect(next).toBe(state)
  })

  it("REDO with empty stack is a no-op", () => {
    const state = loadedState()
    const next = sceneReducer(state, { type: "REDO" })
    expect(next).toBe(state)
  })

  it("UNDO sets dirty=false when returning to loaded state", () => {
    const state = loadedState()
    const withObj = sceneReducer(state, {
      type: "ADD_OBJECT",
      object: makeObject("a"),
    })
    expect(withObj.dirty).toBe(true)
    expect(withObj.undoStack).toHaveLength(1)

    const undone = sceneReducer(withObj, { type: "UNDO" })
    expect(undone.dirty).toBe(false)
    expect(undone.undoStack).toHaveLength(0)
  })

  it("REDO always marks dirty (re-applies a change)", () => {
    const state = loadedState()
    const withObj = sceneReducer(state, {
      type: "ADD_OBJECT",
      object: makeObject("a"),
    })
    const undone = sceneReducer(withObj, { type: "UNDO" })
    const redone = sceneReducer(undone, { type: "REDO" })

    expect(redone.dirty).toBe(true)
    expect(redone.redoStack).toHaveLength(0)
  })

  it("dirty flag transitions through undo/add/redo sequence", () => {
    const state = loadedState()
    expect(state.dirty).toBe(false)

    const withObj = sceneReducer(state, {
      type: "ADD_OBJECT",
      object: makeObject("a"),
    })
    expect(withObj.dirty).toBe(true)

    const undone = sceneReducer(withObj, { type: "UNDO" })
    expect(undone.dirty).toBe(false)

    const redone = sceneReducer(undone, { type: "REDO" })
    expect(redone.dirty).toBe(true)
  })

  it("REPARENT_OBJECT rejects self-parenting", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "REPARENT_OBJECT",
      objectId: "a",
      newParentId: "a",
    })

    // State should be unchanged (same reference)
    expect(next).toBe(state)
  })

  it("DUPLICATE_OBJECT clones object with new ID and offset position", () => {
    const obj = makeObject("a", {
      name: "Cube",
      asset_id: "mesh_001",
      position: [2, 3, 4],
      rotation: [0, 0.707, 0, 0.707],
      scale: [2, 2, 2],
      parent_id: null,
    })
    const state = loadedState([obj])
    const next = sceneReducer(state, {
      type: "DUPLICATE_OBJECT",
      objectId: "a",
    })

    expect(next.scene!.objects).toHaveLength(2)
    const clone = next.scene!.objects[1]
    expect(clone.id).not.toBe("a")
    expect(clone.id).toHaveLength(12)
    expect(clone.name).toBe("Cube (copy)")
    expect(clone.asset_id).toBe("mesh_001")
    expect(clone.position).toEqual([3, 3, 4]) // X offset +1
    expect(clone.rotation).toEqual([0, 0.707, 0, 0.707])
    expect(clone.scale).toEqual([2, 2, 2])
    expect(clone.parent_id).toBeNull()
    expect(clone.visible).toBe(true)
    // Selection moves to the clone
    expect(next.selectedId).toBe(clone.id)
    expect(next.undoStack).toHaveLength(1)
    expect(next.dirty).toBe(true)
  })

  it("DUPLICATE_OBJECT preserves parent_id", () => {
    const parent = makeObject("p")
    const child = makeObject("c", { parent_id: "p", name: "Child" })
    const state = loadedState([parent, child])
    const next = sceneReducer(state, {
      type: "DUPLICATE_OBJECT",
      objectId: "c",
    })

    const clone = next.scene!.objects[2]
    expect(clone.parent_id).toBe("p")
    expect(clone.name).toBe("Child (copy)")
  })

  it("DUPLICATE_OBJECT with invalid objectId is a no-op", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "DUPLICATE_OBJECT",
      objectId: "nonexistent",
    })

    expect(next).toBe(state)
  })

  it("DUPLICATE_OBJECT without loaded scene is a no-op", () => {
    const next = sceneReducer(initialState, {
      type: "DUPLICATE_OBJECT",
      objectId: "a",
    })

    expect(next).toBe(initialState)
  })

  it("mutation after undo clears redo stack", () => {
    const state = loadedState([makeObject("a")])
    const s1 = sceneReducer(state, {
      type: "ADD_OBJECT",
      object: makeObject("b"),
    })
    const undone = sceneReducer(s1, { type: "UNDO" })
    expect(undone.redoStack).toHaveLength(1)

    // New mutation should clear redo
    const s2 = sceneReducer(undone, {
      type: "ADD_OBJECT",
      object: makeObject("c"),
    })
    expect(s2.redoStack).toHaveLength(0)
  })

  it("SELECT does NOT push to undo stack", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, { type: "SELECT", objectId: "a" })

    expect(next.selectedId).toBe("a")
    expect(next.undoStack).toHaveLength(0)
    expect(next.dirty).toBe(false)
  })

  it("SET_GIZMO_MODE does NOT push to undo stack", () => {
    const state = loadedState()
    const next = sceneReducer(state, {
      type: "SET_GIZMO_MODE",
      mode: "rotate",
    })

    expect(next.gizmoMode).toBe("rotate")
    expect(next.undoStack).toHaveLength(0)
    expect(next.dirty).toBe(false)
  })

  it("SET_SNAP toggles snap enabled", () => {
    const state = loadedState()
    expect(state.snapEnabled).toBe(false)

    const next = sceneReducer(state, { type: "SET_SNAP", enabled: true })
    expect(next.snapEnabled).toBe(true)
    expect(next.snapSize).toBe(1.0) // default unchanged
    expect(next.undoStack).toHaveLength(0)
    expect(next.dirty).toBe(false)
  })

  it("SET_SNAP toggles snap off", () => {
    const state = loadedState()
    const on = sceneReducer(state, { type: "SET_SNAP", enabled: true })
    const off = sceneReducer(on, { type: "SET_SNAP", enabled: false })

    expect(off.snapEnabled).toBe(false)
    expect(off.snapSize).toBe(1.0) // unchanged
    expect(off.undoStack).toHaveLength(0)
    expect(off.dirty).toBe(false)
  })

  it("SET_SNAP changes grid size", () => {
    const state = loadedState()
    const next = sceneReducer(state, { type: "SET_SNAP", size: 0.25 })
    expect(next.snapSize).toBe(0.25)
    expect(next.snapEnabled).toBe(false) // enabled unchanged
  })

  it("SET_SNAP can set both enabled and size at once", () => {
    const state = loadedState()
    const next = sceneReducer(state, {
      type: "SET_SNAP",
      enabled: true,
      size: 0.5,
    })
    expect(next.snapEnabled).toBe(true)
    expect(next.snapSize).toBe(0.5)
  })

  it("SET_SNAP ignores invalid snap sizes", () => {
    const state = loadedState()

    for (const badSize of [-1, 0, 3.0, NaN, Infinity] as any[]) {
      const next = sceneReducer(state, { type: "SET_SNAP", size: badSize })
      expect(next.snapSize).toBe(1.0) // default unchanged
      expect(next.snapEnabled).toBe(false)
      expect(next.undoStack).toHaveLength(0)
      expect(next.dirty).toBe(false)
    }
  })
})
