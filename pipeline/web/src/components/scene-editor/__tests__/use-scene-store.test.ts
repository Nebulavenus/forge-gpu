import { describe, expect, it } from "vitest"
import type { SceneData, SceneObject, SceneState } from "../types"
import { initialState, sceneReducer } from "../use-scene-store"
import { computeWorldPosition, computeWorldRotation, computeWorldScale, worldToLocal } from "../scene-utils"

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
    expect(state.selectedIds.size).toBe(0)
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

  it("REMOVE_OBJECT removes object and reparents children preserving world position", () => {
    const parent = makeObject("parent", { position: [5, 0, 0] })
    const child = makeObject("child", { parent_id: "parent", position: [3, 0, 0] })
    const state = loadedState([parent, child])

    const next = sceneReducer(state, {
      type: "REMOVE_OBJECT",
      objectId: "parent",
    })

    expect(next.scene!.objects).toHaveLength(1)
    expect(next.scene!.objects[0]!.id).toBe("child")
    expect(next.scene!.objects[0]!.parent_id).toBeNull()
    // Child was at local [3,0,0] under parent at [5,0,0] → world [8,0,0]
    expect(next.scene!.objects[0]!.position).toEqual([8, 0, 0])
    expect(next.undoStack).toHaveLength(1)
    expect(next.dirty).toBe(true)
  })

  it("REMOVE_OBJECT reparents to grandparent with correct local position", () => {
    const grandparent = makeObject("gp", { position: [10, 0, 0] })
    const parent = makeObject("parent", { parent_id: "gp", position: [5, 0, 0] })
    const child = makeObject("child", { parent_id: "parent", position: [3, 0, 0] })
    const state = loadedState([grandparent, parent, child])

    const next = sceneReducer(state, {
      type: "REMOVE_OBJECT",
      objectId: "parent",
    })

    expect(next.scene!.objects).toHaveLength(2)
    const childObj = next.scene!.objects.find((o) => o.id === "child")!
    // Child world = 10 + 5 + 3 = 18; grandparent world = 10
    // New local relative to grandparent = 18 - 10 = 8
    expect(childObj.parent_id).toBe("gp")
    expect(childObj.position).toEqual([8, 0, 0])
  })

  it("REMOVE_OBJECT clears removed object from selectedIds", () => {
    const state = loadedState([makeObject("a"), makeObject("b")])
    const selected = sceneReducer(state, { type: "SELECT", objectId: "a" })
    expect(selected.selectedIds.has("a")).toBe(true)

    const next = sceneReducer(selected, {
      type: "REMOVE_OBJECT",
      objectId: "a",
    })
    expect(next.selectedIds.has("a")).toBe(false)
  })

  it("REMOVE_OBJECTS removes multiple objects at once", () => {
    const state = loadedState([makeObject("a"), makeObject("b"), makeObject("c")])
    const next = sceneReducer(state, {
      type: "REMOVE_OBJECTS",
      objectIds: ["a", "c"],
    })

    expect(next.scene!.objects).toHaveLength(1)
    expect(next.scene!.objects[0].id).toBe("b")
    expect(next.undoStack).toHaveLength(1)
    expect(next.dirty).toBe(true)
  })

  it("REMOVE_OBJECTS clears selection for removed objects", () => {
    const state = loadedState([makeObject("a"), makeObject("b"), makeObject("c")])
    // Select a and c
    let s = sceneReducer(state, { type: "SELECT", objectId: "a" })
    s = sceneReducer(s, { type: "SELECT", objectId: "c", mode: "add" })
    expect(s.selectedIds.size).toBe(2)

    const next = sceneReducer(s, {
      type: "REMOVE_OBJECTS",
      objectIds: ["a"],
    })
    expect(next.selectedIds.has("a")).toBe(false)
    expect(next.selectedIds.has("c")).toBe(true)
  })

  it("REMOVE_OBJECTS preserves world position of orphaned children", () => {
    const parent = makeObject("parent", { position: [5, 3, 0] })
    const child = makeObject("child", { parent_id: "parent", position: [2, 1, 0] })
    const unrelated = makeObject("other", { position: [0, 0, 0] })
    const state = loadedState([parent, child, unrelated])

    const next = sceneReducer(state, {
      type: "REMOVE_OBJECTS",
      objectIds: ["parent"],
    })

    expect(next.scene!.objects).toHaveLength(2)
    const childObj = next.scene!.objects.find((o) => o.id === "child")!
    // Child world = [5+2, 3+1, 0] = [7, 4, 0]; promoted to root
    expect(childObj.parent_id).toBeNull()
    expect(childObj.position).toEqual([7, 4, 0])
  })

  it("REMOVE_OBJECTS with empty array is a no-op", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "REMOVE_OBJECTS",
      objectIds: [],
    })
    expect(next).toBe(state)
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

  it("UNDO prunes ghost selected IDs that no longer exist", () => {
    const state = loadedState([makeObject("a")])
    // Add an object, then select it
    const withObj = sceneReducer(state, {
      type: "ADD_OBJECT",
      object: makeObject("b"),
    })
    const selected = sceneReducer(withObj, { type: "SELECT", objectId: "b" })
    expect(selected.selectedIds.has("b")).toBe(true)

    // Undo removes "b" — selectedIds should no longer contain "b"
    const undone = sceneReducer(selected, { type: "UNDO" })
    expect(undone.scene!.objects).toHaveLength(1)
    expect(undone.selectedIds.has("b")).toBe(false)
    expect(undone.selectedIds.size).toBe(0)
  })

  it("REDO prunes ghost selected IDs that no longer exist", () => {
    const state = loadedState([makeObject("a"), makeObject("b")])
    // Remove "b", then select "a", then redo removal of "b" shouldn't matter
    // But test: select "b", remove it (select clears "b"), undo (restores "b"),
    // select "b" again, redo removal — "b" should be pruned from selection
    let s = sceneReducer(state, { type: "SELECT", objectId: "b" })
    s = sceneReducer(s, { type: "REMOVE_OBJECT", objectId: "b" })
    s = sceneReducer(s, { type: "UNDO" }) // restore "b"
    s = sceneReducer(s, { type: "SELECT", objectId: "b" }) // select "b"
    expect(s.selectedIds.has("b")).toBe(true)

    s = sceneReducer(s, { type: "REDO" }) // re-remove "b"
    expect(s.scene!.objects.find((o) => o.id === "b")).toBeUndefined()
    expect(s.selectedIds.has("b")).toBe(false)
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
    expect(next.selectedIds.has(clone.id)).toBe(true)
    expect(next.selectedIds.size).toBe(1)
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

    expect(next.selectedIds.has("a")).toBe(true)
    expect(next.selectedIds.size).toBe(1)
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

  it("REORDER_OBJECT reparents and places before target sibling", () => {
    const a = makeObject("a")
    const b = makeObject("b")
    const c = makeObject("c")
    const state = loadedState([a, b, c])

    // Move c before a (both at root level)
    const next = sceneReducer(state, {
      type: "REORDER_OBJECT",
      objectId: "c",
      newParentId: null,
      beforeId: "a",
    })

    const ids = next.scene!.objects.map((o) => o.id)
    expect(ids).toEqual(["c", "a", "b"])
    expect(next.undoStack).toHaveLength(1)
    expect(next.dirty).toBe(true)
  })

  it("REORDER_OBJECT appends when beforeId is null", () => {
    const a = makeObject("a")
    const b = makeObject("b")
    const state = loadedState([a, b])

    const next = sceneReducer(state, {
      type: "REORDER_OBJECT",
      objectId: "a",
      newParentId: null,
      beforeId: null,
    })

    const ids = next.scene!.objects.map((o) => o.id)
    expect(ids).toEqual(["b", "a"])
  })

  it("REORDER_OBJECT changes parent_id when reparenting", () => {
    const parent = makeObject("p")
    const child = makeObject("c")
    const state = loadedState([parent, child])

    const next = sceneReducer(state, {
      type: "REORDER_OBJECT",
      objectId: "c",
      newParentId: "p",
      beforeId: null,
    })

    expect(next.scene!.objects.find((o) => o.id === "c")!.parent_id).toBe("p")
  })

  it("REORDER_OBJECT rejects self-parenting", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "REORDER_OBJECT",
      objectId: "a",
      newParentId: "a",
      beforeId: null,
    })

    expect(next).toBe(state)
  })

  it("REORDER_OBJECT rejects circular reference", () => {
    const a = makeObject("a")
    const b = makeObject("b", { parent_id: "a" })
    const state = loadedState([a, b])

    const next = sceneReducer(state, {
      type: "REORDER_OBJECT",
      objectId: "a",
      newParentId: "b",
      beforeId: null,
    })

    expect(next.scene!.objects.find((o) => o.id === "a")!.parent_id).toBeNull()
    expect(next.undoStack).toHaveLength(0)
  })

  it("REORDER_OBJECT returns same state for non-existent objectId", () => {
    const state = loadedState([makeObject("a"), makeObject("b")])
    const next = sceneReducer(state, {
      type: "REORDER_OBJECT",
      objectId: "does-not-exist",
      newParentId: null,
      beforeId: null,
    })

    expect(next).toBe(state)
  })

  it("REORDER_OBJECT is a no-op when position is unchanged", () => {
    const a = makeObject("a")
    const b = makeObject("b")
    const c = makeObject("c")
    const state = loadedState([a, b, c])

    // "b" is already between "a" and "c" — reordering before "c" is a no-op
    const next = sceneReducer(state, {
      type: "REORDER_OBJECT",
      objectId: "b",
      newParentId: null,
      beforeId: "c",
    })

    expect(next).toBe(state)
    expect(next.undoStack).toHaveLength(0)
    expect(next.dirty).toBe(false)
  })

  it("REORDER_OBJECT appends when beforeId is not found", () => {
    const a = makeObject("a")
    const b = makeObject("b")
    const c = makeObject("c")
    const state = loadedState([a, b, c])

    const next = sceneReducer(state, {
      type: "REORDER_OBJECT",
      objectId: "a",
      newParentId: null,
      beforeId: "does-not-exist",
    })

    const ids = next.scene!.objects.map((o) => o.id)
    expect(ids).toEqual(["b", "c", "a"])
  })
})

// ── Multi-select tests ──────────────────────────────────────────────────

describe("multi-select", () => {
  it("SELECT with mode=replace selects a single object", () => {
    const state = loadedState([makeObject("a"), makeObject("b")])
    const next = sceneReducer(state, { type: "SELECT", objectId: "a" })

    expect(next.selectedIds.size).toBe(1)
    expect(next.selectedIds.has("a")).toBe(true)
  })

  it("SELECT with mode=replace replaces previous selection", () => {
    const state = loadedState([makeObject("a"), makeObject("b")])
    const s1 = sceneReducer(state, { type: "SELECT", objectId: "a" })
    const s2 = sceneReducer(s1, { type: "SELECT", objectId: "b" })

    expect(s2.selectedIds.size).toBe(1)
    expect(s2.selectedIds.has("b")).toBe(true)
    expect(s2.selectedIds.has("a")).toBe(false)
  })

  it("SELECT with objectId=null clears selection", () => {
    const state = loadedState([makeObject("a")])
    const selected = sceneReducer(state, { type: "SELECT", objectId: "a" })
    const cleared = sceneReducer(selected, { type: "SELECT", objectId: null })

    expect(cleared.selectedIds.size).toBe(0)
  })

  it("SELECT with mode=add adds to selection", () => {
    const state = loadedState([makeObject("a"), makeObject("b"), makeObject("c")])
    let s = sceneReducer(state, { type: "SELECT", objectId: "a" })
    s = sceneReducer(s, { type: "SELECT", objectId: "b", mode: "add" })

    expect(s.selectedIds.size).toBe(2)
    expect(s.selectedIds.has("a")).toBe(true)
    expect(s.selectedIds.has("b")).toBe(true)
  })

  it("SELECT with mode=add and null objectId is a no-op", () => {
    const state = loadedState([makeObject("a")])
    const selected = sceneReducer(state, { type: "SELECT", objectId: "a" })
    const next = sceneReducer(selected, { type: "SELECT", objectId: null, mode: "add" })

    expect(next).toBe(selected)
  })

  it("SELECT with mode=add for already-selected object is a no-op", () => {
    const state = loadedState([makeObject("a")])
    const selected = sceneReducer(state, { type: "SELECT", objectId: "a" })
    const next = sceneReducer(selected, { type: "SELECT", objectId: "a", mode: "add" })

    expect(next).toBe(selected)
  })

  it("SELECT with mode=toggle adds unselected object", () => {
    const state = loadedState([makeObject("a"), makeObject("b")])
    const s1 = sceneReducer(state, { type: "SELECT", objectId: "a" })
    const s2 = sceneReducer(s1, { type: "SELECT", objectId: "b", mode: "toggle" })

    expect(s2.selectedIds.size).toBe(2)
    expect(s2.selectedIds.has("a")).toBe(true)
    expect(s2.selectedIds.has("b")).toBe(true)
  })

  it("SELECT with mode=toggle removes selected object", () => {
    const state = loadedState([makeObject("a"), makeObject("b")])
    let s = sceneReducer(state, { type: "SELECT", objectId: "a" })
    s = sceneReducer(s, { type: "SELECT", objectId: "b", mode: "add" })
    s = sceneReducer(s, { type: "SELECT", objectId: "a", mode: "toggle" })

    expect(s.selectedIds.size).toBe(1)
    expect(s.selectedIds.has("b")).toBe(true)
    expect(s.selectedIds.has("a")).toBe(false)
  })

  it("SELECT_ALL selects all objects", () => {
    const state = loadedState([makeObject("a"), makeObject("b"), makeObject("c")])
    const next = sceneReducer(state, { type: "SELECT_ALL" })

    expect(next.selectedIds.size).toBe(3)
    expect(next.selectedIds.has("a")).toBe(true)
    expect(next.selectedIds.has("b")).toBe(true)
    expect(next.selectedIds.has("c")).toBe(true)
  })

  it("SELECT_ALL with no objects is a no-op", () => {
    const state = loadedState()
    const next = sceneReducer(state, { type: "SELECT_ALL" })
    expect(next).toBe(state)
  })
})

// ── Group/Ungroup tests ─────────────────────────────────────────────────

describe("group and ungroup", () => {
  it("GROUP_OBJECTS creates a group at centroid and offsets children", () => {
    const a = makeObject("a", { position: [2, 0, 0] })
    const b = makeObject("b", { position: [4, 0, 6] })
    const c = makeObject("c")
    const state = loadedState([a, b, c])
    const next = sceneReducer(state, {
      type: "GROUP_OBJECTS",
      objectIds: ["a", "b"],
    })

    expect(next.scene!.objects).toHaveLength(4) // 3 original + 1 group
    const group = next.scene!.objects[0] // group is prepended
    expect(group.name).toBe("Group")
    expect(group.asset_id).toBeNull()
    // Group at centroid of a and b
    expect(group.position).toEqual([3, 0, 3])

    // a and b are children with offset positions preserving world-space location
    const aObj = next.scene!.objects.find((o) => o.id === "a")!
    const bObj = next.scene!.objects.find((o) => o.id === "b")!
    expect(aObj.parent_id).toBe(group.id)
    expect(bObj.parent_id).toBe(group.id)
    expect(aObj.position).toEqual([-1, 0, -3]) // [2,0,0] - [3,0,3]
    expect(bObj.position).toEqual([1, 0, 3])   // [4,0,6] - [3,0,3]

    // c is unchanged
    const cObj = next.scene!.objects.find((o) => o.id === "c")!
    expect(cObj.parent_id).toBeNull()

    // Group is now selected
    expect(next.selectedIds.size).toBe(1)
    expect(next.selectedIds.has(group.id)).toBe(true)

    expect(next.undoStack).toHaveLength(1)
    expect(next.dirty).toBe(true)
  })

  it("GROUP_OBJECTS with fewer than 2 objects is a no-op", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "GROUP_OBJECTS",
      objectIds: ["a"],
    })
    expect(next).toBe(state)
  })

  it("UNGROUP_OBJECT restores world-space positions and removes group", () => {
    const a = makeObject("a", { position: [2, 0, 0] })
    const b = makeObject("b", { position: [4, 0, 6] })
    const state = loadedState([a, b])
    // First group them
    const grouped = sceneReducer(state, {
      type: "GROUP_OBJECTS",
      objectIds: ["a", "b"],
    })
    const groupId = [...grouped.selectedIds][0]

    // Now ungroup
    const ungrouped = sceneReducer(grouped, {
      type: "UNGROUP_OBJECT",
      groupId,
    })

    // Group object should be removed
    expect(ungrouped.scene!.objects).toHaveLength(2)
    expect(ungrouped.scene!.objects.every((o) => o.id !== groupId)).toBe(true)

    // Children are back at root
    expect(ungrouped.scene!.objects.every((o) => o.parent_id === null)).toBe(true)

    // World-space positions restored (group pos + child offset = original)
    const aObj = ungrouped.scene!.objects.find((o) => o.id === "a")!
    const bObj = ungrouped.scene!.objects.find((o) => o.id === "b")!
    expect(aObj.position).toEqual([2, 0, 0])
    expect(bObj.position).toEqual([4, 0, 6])

    // Children are now selected
    expect(ungrouped.selectedIds.size).toBe(2)
    expect(ungrouped.selectedIds.has("a")).toBe(true)
    expect(ungrouped.selectedIds.has("b")).toBe(true)

    expect(ungrouped.undoStack).toHaveLength(2) // group + ungroup
    expect(ungrouped.dirty).toBe(true)
  })

  it("UNGROUP_OBJECT preserves world positions when group has been rotated", () => {
    const s = Math.SQRT1_2
    const a = makeObject("a", { position: [2, 0, 0] })
    const b = makeObject("b", { position: [4, 0, 0] })
    const state = loadedState([a, b])

    // Group them — centroid is [3, 0, 0]
    const grouped = sceneReducer(state, {
      type: "GROUP_OBJECTS",
      objectIds: ["a", "b"],
    })
    const groupId = [...grouped.selectedIds][0]!

    // Rotate the group 90° around Y
    const rotated = sceneReducer(grouped, {
      type: "UPDATE_TRANSFORM",
      objectId: groupId,
      position: [3, 0, 0],
      rotation: [0, s, 0, s], // 90° Y
      scale: [1, 1, 1],
    })

    // Ungroup — children should keep their rotated world positions
    const ungrouped = sceneReducer(rotated, {
      type: "UNGROUP_OBJECT",
      groupId,
    })

    expect(ungrouped.scene!.objects).toHaveLength(2)
    const aObj = ungrouped.scene!.objects.find((o) => o.id === "a")!
    const bObj = ungrouped.scene!.objects.find((o) => o.id === "b")!

    // Original: a at [2,0,0], b at [4,0,0], centroid [3,0,0]
    // After grouping: a local [-1,0,0], b local [1,0,0]
    // 90° Y rotation of [-1,0,0] → [0,0,1]; world = [3,0,0] + [0,0,1] = [3,0,1]
    // 90° Y rotation of [1,0,0] → [0,0,-1]; world = [3,0,0] + [0,0,-1] = [3,0,-1]
    expect(aObj.parent_id).toBeNull()
    expect(aObj.position[0]).toBeCloseTo(3, 5)
    expect(aObj.position[1]).toBeCloseTo(0, 5)
    expect(aObj.position[2]).toBeCloseTo(1, 5)
    expect(bObj.parent_id).toBeNull()
    expect(bObj.position[0]).toBeCloseTo(3, 5)
    expect(bObj.position[1]).toBeCloseTo(0, 5)
    expect(bObj.position[2]).toBeCloseTo(-1, 5)
  })

  it("UNGROUP_OBJECT preserves world positions when group has been scaled", () => {
    const a = makeObject("a", { position: [1, 0, 0] })
    const b = makeObject("b", { position: [3, 0, 0] })
    const state = loadedState([a, b])

    // Group — centroid [2, 0, 0]
    const grouped = sceneReducer(state, {
      type: "GROUP_OBJECTS",
      objectIds: ["a", "b"],
    })
    const groupId = [...grouped.selectedIds][0]!

    // Scale the group 2x
    const scaled = sceneReducer(grouped, {
      type: "UPDATE_TRANSFORM",
      objectId: groupId,
      position: [2, 0, 0],
      rotation: [0, 0, 0, 1],
      scale: [2, 2, 2],
    })

    const ungrouped = sceneReducer(scaled, {
      type: "UNGROUP_OBJECT",
      groupId,
    })

    expect(ungrouped.scene!.objects).toHaveLength(2)
    const aObj = ungrouped.scene!.objects.find((o) => o.id === "a")!
    const bObj = ungrouped.scene!.objects.find((o) => o.id === "b")!

    // a local [-1,0,0] scaled 2x → [-2,0,0]; world = [2,0,0] + [-2,0,0] = [0,0,0]
    // b local [1,0,0] scaled 2x → [2,0,0]; world = [2,0,0] + [2,0,0] = [4,0,0]
    expect(aObj.position[0]).toBeCloseTo(0, 5)
    expect(aObj.position[1]).toBeCloseTo(0, 5)
    expect(aObj.position[2]).toBeCloseTo(0, 5)
    expect(bObj.position[0]).toBeCloseTo(4, 5)
    expect(bObj.position[1]).toBeCloseTo(0, 5)
    expect(bObj.position[2]).toBeCloseTo(0, 5)
  })

  it("UNGROUP_OBJECT with non-existent groupId is a no-op", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "UNGROUP_OBJECT",
      groupId: "nonexistent",
    })
    expect(next).toBe(state)
  })

  it("UNGROUP_OBJECT with no children is a no-op", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "UNGROUP_OBJECT",
      groupId: "a",
    })
    expect(next).toBe(state)
  })

  it("UNGROUP_OBJECT rejects asset-backed objects", () => {
    const state = loadedState([
      makeObject("a", { asset_id: "mesh_001" }),
      makeObject("b", { parent_id: "a" }),
    ])
    const next = sceneReducer(state, {
      type: "UNGROUP_OBJECT",
      groupId: "a",
    })
    expect(next).toBe(state)
  })
})

// ── Batch action tests ─────────────────────────────────────────────────

describe("batch actions", () => {
  it("UPDATE_TRANSFORMS_BATCH updates multiple objects in one undo step", () => {
    const a = makeObject("a", { position: [0, 0, 0] })
    const b = makeObject("b", { position: [1, 1, 1] })
    const c = makeObject("c", { position: [2, 2, 2] })
    const state = loadedState([a, b, c])
    const next = sceneReducer(state, {
      type: "UPDATE_TRANSFORMS_BATCH",
      updates: [
        { objectId: "a", position: [10, 0, 0], rotation: [0, 0, 0, 1], scale: [1, 1, 1] },
        { objectId: "b", position: [11, 1, 1], rotation: [0, 0, 0, 1], scale: [2, 2, 2] },
      ],
    })

    const aObj = next.scene!.objects.find((o) => o.id === "a")!
    const bObj = next.scene!.objects.find((o) => o.id === "b")!
    const cObj = next.scene!.objects.find((o) => o.id === "c")!
    expect(aObj.position).toEqual([10, 0, 0])
    expect(bObj.position).toEqual([11, 1, 1])
    expect(bObj.scale).toEqual([2, 2, 2])
    expect(cObj.position).toEqual([2, 2, 2]) // unchanged
    expect(next.undoStack).toHaveLength(1) // single undo step
    expect(next.dirty).toBe(true)
  })

  it("UPDATE_TRANSFORMS_BATCH with empty updates is a no-op", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "UPDATE_TRANSFORMS_BATCH",
      updates: [],
    })
    expect(next).toBe(state)
  })

  it("SET_VISIBILITY_BATCH updates multiple objects in one undo step", () => {
    const state = loadedState([
      makeObject("a", { visible: true }),
      makeObject("b", { visible: true }),
      makeObject("c", { visible: true }),
    ])
    const next = sceneReducer(state, {
      type: "SET_VISIBILITY_BATCH",
      objectIds: ["a", "c"],
      visible: false,
    })

    expect(next.scene!.objects.find((o) => o.id === "a")!.visible).toBe(false)
    expect(next.scene!.objects.find((o) => o.id === "b")!.visible).toBe(true)
    expect(next.scene!.objects.find((o) => o.id === "c")!.visible).toBe(false)
    expect(next.undoStack).toHaveLength(1) // single undo step
    expect(next.dirty).toBe(true)
  })

  it("SET_VISIBILITY_BATCH with empty objectIds is a no-op", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "SET_VISIBILITY_BATCH",
      objectIds: [],
      visible: false,
    })
    expect(next).toBe(state)
  })

  it("DUPLICATE_OBJECTS clones multiple objects in one undo step", () => {
    const state = loadedState([
      makeObject("a", { name: "Cube", position: [1, 0, 0] }),
      makeObject("b", { name: "Sphere", position: [3, 0, 0] }),
    ])
    const next = sceneReducer(state, {
      type: "DUPLICATE_OBJECTS",
      objectIds: ["a", "b"],
    })

    expect(next.scene!.objects).toHaveLength(4) // 2 original + 2 clones
    const clones = next.scene!.objects.filter((o) => o.id !== "a" && o.id !== "b")
    expect(clones).toHaveLength(2)
    expect(clones[0]!.name).toBe("Cube (copy)")
    expect(clones[1]!.name).toBe("Sphere (copy)")
    expect(clones[0]!.position).toEqual([2, 0, 0]) // offset +1 on X
    expect(clones[1]!.position).toEqual([4, 0, 0])
    // Selection moves to clones
    expect(next.selectedIds.size).toBe(2)
    expect(next.selectedIds.has(clones[0]!.id)).toBe(true)
    expect(next.selectedIds.has(clones[1]!.id)).toBe(true)
    expect(next.undoStack).toHaveLength(1) // single undo step
    expect(next.dirty).toBe(true)
  })

  it("DUPLICATE_OBJECTS remaps parent_id for cloned hierarchies", () => {
    const parent = makeObject("p", { name: "Parent", position: [0, 0, 0] })
    const child = makeObject("c", { name: "Child", parent_id: "p", position: [1, 0, 0] })
    const state = loadedState([parent, child])
    const next = sceneReducer(state, {
      type: "DUPLICATE_OBJECTS",
      objectIds: ["p", "c"],
    })

    expect(next.scene!.objects).toHaveLength(4)
    const clones = next.scene!.objects.filter((o) => o.id !== "p" && o.id !== "c")
    expect(clones).toHaveLength(2)
    const clonedParent = clones.find((o) => o.name === "Parent (copy)")!
    const clonedChild = clones.find((o) => o.name === "Child (copy)")!
    // Cloned child should point to cloned parent, not original "p"
    expect(clonedChild.parent_id).toBe(clonedParent.id)
    expect(clonedChild.parent_id).not.toBe("p")
    // Cloned parent (root) gets +1 x offset; child keeps original local position
    // to avoid double-offset in world space
    expect(clonedParent.position).toEqual([1, 0, 0])
    expect(clonedChild.position).toEqual([1, 0, 0])
  })

  it("DUPLICATE_OBJECTS with empty array is a no-op", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "DUPLICATE_OBJECTS",
      objectIds: [],
    })
    expect(next).toBe(state)
  })

  it("SELECT_SET replaces selection in a single dispatch", () => {
    const state = loadedState([makeObject("a"), makeObject("b"), makeObject("c")])
    const s1 = sceneReducer(state, { type: "SELECT", objectId: "a" })
    const s2 = sceneReducer(s1, {
      type: "SELECT_SET",
      objectIds: ["b", "c"],
    })

    expect(s2.selectedIds.size).toBe(2)
    expect(s2.selectedIds.has("b")).toBe(true)
    expect(s2.selectedIds.has("c")).toBe(true)
    expect(s2.selectedIds.has("a")).toBe(false)
  })

  it("SELECT_SET with empty array clears selection", () => {
    const state = loadedState([makeObject("a")])
    const s1 = sceneReducer(state, { type: "SELECT", objectId: "a" })
    const s2 = sceneReducer(s1, { type: "SELECT_SET", objectIds: [] })
    expect(s2.selectedIds.size).toBe(0)
  })

  // ── Non-identity parent rotation/scale (#437) ─────────────────────

  it("REMOVE_OBJECT preserves world position when parent has rotation", () => {
    // Parent rotated 90° around Y axis: quaternion [0, sin(45°), 0, cos(45°)]
    const s = Math.SQRT1_2
    const parent = makeObject("parent", {
      position: [10, 0, 0],
      rotation: [0, s, 0, s], // 90° around Y
    })
    // Child at local [0, 0, -5] under rotated parent
    // 90° Y rotation maps [0,0,-5] → [-5,0,0]
    // World = [10,0,0] + [-5,0,0] = [5, 0, 0]
    const child = makeObject("child", {
      parent_id: "parent",
      position: [0, 0, -5],
    })
    const state = loadedState([parent, child])

    const next = sceneReducer(state, {
      type: "REMOVE_OBJECT",
      objectId: "parent",
    })

    const childObj = next.scene!.objects.find((o) => o.id === "child")!
    expect(childObj.parent_id).toBeNull()
    expect(childObj.position[0]).toBeCloseTo(5, 5)
    expect(childObj.position[1]).toBeCloseTo(0, 5)
    expect(childObj.position[2]).toBeCloseTo(0, 5)
  })

  it("REMOVE_OBJECT preserves world position when parent has scale", () => {
    const parent = makeObject("parent", {
      position: [0, 0, 0],
      scale: [2, 2, 2],
    })
    // Child at local [3, 4, 0] under 2x-scaled parent → world = [6, 8, 0]
    const child = makeObject("child", {
      parent_id: "parent",
      position: [3, 4, 0],
    })
    const state = loadedState([parent, child])

    const next = sceneReducer(state, {
      type: "REMOVE_OBJECT",
      objectId: "parent",
    })

    const childObj = next.scene!.objects.find((o) => o.id === "child")!
    expect(childObj.parent_id).toBeNull()
    expect(childObj.position[0]).toBeCloseTo(6, 5)
    expect(childObj.position[1]).toBeCloseTo(8, 5)
    expect(childObj.position[2]).toBeCloseTo(0, 5)
  })

  it("REMOVE_OBJECT reparents to rotated grandparent with correct local position", () => {
    const s = Math.SQRT1_2
    const grandparent = makeObject("gp", {
      position: [0, 0, 0],
      rotation: [0, s, 0, s], // 90° around Y
    })
    const parent = makeObject("parent", {
      parent_id: "gp",
      position: [5, 0, 0],
    })
    // Child local [1, 0, 0] under parent (which is under rotated gp)
    const child = makeObject("child", {
      parent_id: "parent",
      position: [1, 0, 0],
    })
    const state = loadedState([grandparent, parent, child])

    const next = sceneReducer(state, {
      type: "REMOVE_OBJECT",
      objectId: "parent",
    })

    const childObj = next.scene!.objects.find((o) => o.id === "child")!
    expect(childObj.parent_id).toBe("gp")
    // Child's new local position under gp = parent's local [5,0,0] + child's local [1,0,0] = [6,0,0]
    // (Both were subject to gp's rotation equally, so the rotation cancels out in the local conversion)
    expect(childObj.position[0]).toBeCloseTo(6, 5)
    expect(childObj.position[1]).toBeCloseTo(0, 5)
    expect(childObj.position[2]).toBeCloseTo(0, 5)
  })

  it("REMOVE_OBJECTS preserves world position with rotated parent", () => {
    const s = Math.SQRT1_2
    const parent = makeObject("parent", {
      position: [0, 5, 0],
      rotation: [0, s, 0, s], // 90° around Y
    })
    const child = makeObject("child", {
      parent_id: "parent",
      position: [3, 0, 0],
    })
    const state = loadedState([parent, child])

    const next = sceneReducer(state, {
      type: "REMOVE_OBJECTS",
      objectIds: ["parent"],
    })

    const childObj = next.scene!.objects.find((o) => o.id === "child")!
    expect(childObj.parent_id).toBeNull()
    // 90° Y rotation maps [3,0,0] → [0,0,-3]
    // World = [0,5,0] + [0,0,-3] = [0,5,-3]
    expect(childObj.position[0]).toBeCloseTo(0, 5)
    expect(childObj.position[1]).toBeCloseTo(5, 5)
    expect(childObj.position[2]).toBeCloseTo(-3, 5)
  })

  it("REMOVE_OBJECTS preserves world position with scaled parent reparented to surviving ancestor", () => {
    const grandparent = makeObject("gp", { position: [10, 0, 0] })
    const parent = makeObject("parent", {
      parent_id: "gp",
      position: [5, 0, 0],
      scale: [3, 3, 3],
    })
    const child = makeObject("child", {
      parent_id: "parent",
      position: [2, 1, 0],
    })
    const state = loadedState([grandparent, parent, child])

    const next = sceneReducer(state, {
      type: "REMOVE_OBJECTS",
      objectIds: ["parent"],
    })

    const childObj = next.scene!.objects.find((o) => o.id === "child")!
    expect(childObj.parent_id).toBe("gp")
    // Child world: gp[10,0,0] + parent[5,0,0] + scale(3)*child[2,1,0] = [10+5+6, 0+0+3, 0] = [21, 3, 0]
    // Local under gp (identity rot/scale): worldPos - gpWorldPos = [21-10, 3-0, 0] = [11, 3, 0]
    expect(childObj.position[0]).toBeCloseTo(11, 5)
    expect(childObj.position[1]).toBeCloseTo(3, 5)
    expect(childObj.position[2]).toBeCloseTo(0, 5)
  })
})

// ── computeWorldPosition / worldToLocal unit tests ───────────────────

describe("computeWorldPosition", () => {
  it("returns object position when no parent", () => {
    const obj = makeObject("a", { position: [3, 4, 5] })
    const map = new Map([["a", obj]])
    expect(computeWorldPosition(obj, map)).toEqual([3, 4, 5])
  })

  it("sums positions for identity-transform parents", () => {
    const parent = makeObject("p", { position: [10, 20, 30] })
    const child = makeObject("c", { parent_id: "p", position: [1, 2, 3] })
    const map = new Map<string, SceneObject>([["p", parent], ["c", child]])
    expect(computeWorldPosition(child, map)).toEqual([11, 22, 33])
  })

  it("applies parent rotation to child position", () => {
    const s = Math.SQRT1_2
    // 90° around Y: [0, sin45, 0, cos45]
    const parent = makeObject("p", {
      position: [0, 0, 0],
      rotation: [0, s, 0, s],
    })
    const child = makeObject("c", {
      parent_id: "p",
      position: [1, 0, 0],
    })
    const map = new Map<string, SceneObject>([["p", parent], ["c", child]])
    const wp = computeWorldPosition(child, map)
    // 90° Y rotation maps [1,0,0] → [0,0,-1]
    expect(wp[0]).toBeCloseTo(0, 5)
    expect(wp[1]).toBeCloseTo(0, 5)
    expect(wp[2]).toBeCloseTo(-1, 5)
  })

  it("applies parent scale to child position", () => {
    const parent = makeObject("p", {
      position: [0, 0, 0],
      scale: [2, 3, 4],
    })
    const child = makeObject("c", {
      parent_id: "p",
      position: [1, 1, 1],
    })
    const map = new Map<string, SceneObject>([["p", parent], ["c", child]])
    const wp = computeWorldPosition(child, map)
    expect(wp).toEqual([2, 3, 4])
  })

  it("composes rotation + scale + translation through ancestor chain", () => {
    const s = Math.SQRT1_2
    const root = makeObject("root", {
      position: [10, 0, 0],
      rotation: [0, s, 0, s], // 90° Y
      scale: [2, 2, 2],
    })
    const mid = makeObject("mid", {
      parent_id: "root",
      position: [5, 0, 0],
    })
    const leaf = makeObject("leaf", {
      parent_id: "mid",
      position: [1, 0, 0],
    })
    const map = new Map<string, SceneObject>([
      ["root", root],
      ["mid", mid],
      ["leaf", leaf],
    ])
    const wp = computeWorldPosition(leaf, map)
    // root transform: pos=[10,0,0], rot=90°Y, scale=[2,2,2]
    // mid local [5,0,0] → scaled [10,0,0] → rotated [0,0,-10] → world [10,0,-10]
    // At mid level: accumulated rot=90°Y, scale=[2,2,2]
    // leaf local [1,0,0] → scaled [2,0,0] → rotated [0,0,-2] → world [10,0,-12]
    expect(wp[0]).toBeCloseTo(10, 5)
    expect(wp[1]).toBeCloseTo(0, 5)
    expect(wp[2]).toBeCloseTo(-12, 5)
  })

  it("handles cycle in parent chain without infinite loop", () => {
    const a = makeObject("a", { parent_id: "b", position: [1, 0, 0] })
    const b = makeObject("b", { parent_id: "a", position: [2, 0, 0] })
    const map = new Map<string, SceneObject>([["a", a], ["b", b]])
    // Should not hang — just returns a finite result
    const wp = computeWorldPosition(a, map)
    expect(Array.isArray(wp)).toBe(true)
    expect(wp).toHaveLength(3)
  })
})

describe("worldToLocal", () => {
  it("inverts identity-transform parent", () => {
    const parent = makeObject("p", { position: [10, 20, 30] })
    const map = new Map<string, SceneObject>([["p", parent]])
    const local = worldToLocal([15, 25, 35], parent, map)
    expect(local).toEqual([5, 5, 5])
  })

  it("inverts rotated parent", () => {
    const s = Math.SQRT1_2
    const parent = makeObject("p", {
      position: [0, 0, 0],
      rotation: [0, s, 0, s], // 90° Y
    })
    const map = new Map<string, SceneObject>([["p", parent]])
    // World [0,0,-1] under 90°Y parent should yield local [1,0,0]
    const local = worldToLocal([0, 0, -1], parent, map)
    expect(local[0]).toBeCloseTo(1, 5)
    expect(local[1]).toBeCloseTo(0, 5)
    expect(local[2]).toBeCloseTo(0, 5)
  })

  it("inverts scaled parent", () => {
    const parent = makeObject("p", {
      position: [0, 0, 0],
      scale: [2, 3, 4],
    })
    const map = new Map<string, SceneObject>([["p", parent]])
    const local = worldToLocal([6, 9, 12], parent, map)
    expect(local).toEqual([3, 3, 3])
  })

  it("round-trips with computeWorldPosition", () => {
    const s = Math.SQRT1_2
    const parent = makeObject("p", {
      position: [5, 10, 15],
      rotation: [0, s, 0, s],
      scale: [2, 2, 2],
    })
    const child = makeObject("c", {
      parent_id: "p",
      position: [3, 4, 5],
    })
    const map = new Map<string, SceneObject>([["p", parent], ["c", child]])
    const wp = computeWorldPosition(child, map)
    const local = worldToLocal(wp, parent, map)
    expect(local[0]).toBeCloseTo(3, 5)
    expect(local[1]).toBeCloseTo(4, 5)
    expect(local[2]).toBeCloseTo(5, 5)
  })
})

// ── Material overrides ──────────────────────────────────────────────────

describe("material overrides", () => {
  it("UPDATE_MATERIAL_OVERRIDES sets overrides on a single object", () => {
    const state = loadedState([makeObject("a"), makeObject("b")])
    const next = sceneReducer(state, {
      type: "UPDATE_MATERIAL_OVERRIDES",
      objectId: "a",
      overrides: { color: "#ff0000", opacity: 0.5, wireframe: true },
    })

    const objA = next.scene!.objects.find((o) => o.id === "a")!
    expect(objA.material_overrides).toEqual({
      color: "#ff0000",
      opacity: 0.5,
      wireframe: true,
    })
    // Other object unchanged
    const objB = next.scene!.objects.find((o) => o.id === "b")!
    expect(objB.material_overrides).toBeUndefined()
    expect(next.dirty).toBe(true)
    expect(next.undoStack).toHaveLength(1)
  })

  it("UPDATE_MATERIAL_OVERRIDES_BATCH sets overrides on multiple objects", () => {
    const state = loadedState([makeObject("a"), makeObject("b"), makeObject("c")])
    const next = sceneReducer(state, {
      type: "UPDATE_MATERIAL_OVERRIDES_BATCH",
      objectIds: ["a", "c"],
      overrides: { wireframe: true },
    })

    const objA = next.scene!.objects.find((o) => o.id === "a")!
    const objB = next.scene!.objects.find((o) => o.id === "b")!
    const objC = next.scene!.objects.find((o) => o.id === "c")!
    expect(objA.material_overrides).toEqual({ wireframe: true })
    expect(objB.material_overrides).toBeUndefined()
    expect(objC.material_overrides).toEqual({ wireframe: true })
    expect(next.dirty).toBe(true)
  })

  it("UPDATE_MATERIAL_OVERRIDES_BATCH no-ops with empty objectIds", () => {
    const state = loadedState([makeObject("a")])
    const next = sceneReducer(state, {
      type: "UPDATE_MATERIAL_OVERRIDES_BATCH",
      objectIds: [],
      overrides: { color: "#00ff00" },
    })
    expect(next).toBe(state)
  })

  it("material_overrides survives undo/redo", () => {
    const state = loadedState([makeObject("a")])
    const withOverride = sceneReducer(state, {
      type: "UPDATE_MATERIAL_OVERRIDES",
      objectId: "a",
      overrides: { color: "#0000ff" },
    })

    // Undo restores original (no overrides)
    const undone = sceneReducer(withOverride, { type: "UNDO" })
    expect(undone.scene!.objects[0].material_overrides).toBeUndefined()

    // Redo restores the override
    const redone = sceneReducer(undone, { type: "REDO" })
    expect(redone.scene!.objects[0].material_overrides).toEqual({ color: "#0000ff" })
  })

  it("DUPLICATE_OBJECT preserves material_overrides", () => {
    const obj = makeObject("a", {
      material_overrides: { color: "#ff0000", opacity: 0.8 },
    })
    const state = loadedState([obj])
    const selected = sceneReducer(state, { type: "SELECT", objectId: "a" })
    const next = sceneReducer(selected, { type: "DUPLICATE_OBJECT", objectId: "a" })

    expect(next.scene!.objects).toHaveLength(2)
    const clone = next.scene!.objects[1]
    expect(clone.material_overrides).toEqual({ color: "#ff0000", opacity: 0.8 })
  })

  it("UPDATE_MATERIAL_OVERRIDES merges into existing overrides", () => {
    const obj = makeObject("a", {
      material_overrides: { color: "#ff0000", opacity: 0.5 },
    })
    const state = loadedState([obj])

    // Toggling wireframe should preserve existing color and opacity
    const next = sceneReducer(state, {
      type: "UPDATE_MATERIAL_OVERRIDES",
      objectId: "a",
      overrides: { wireframe: true },
    })

    expect(next.scene!.objects[0].material_overrides).toEqual({
      color: "#ff0000",
      opacity: 0.5,
      wireframe: true,
    })
  })

  it("UPDATE_MATERIAL_OVERRIDES_BATCH merges per-object overrides", () => {
    const objA = makeObject("a", {
      material_overrides: { color: "#ff0000" },
    })
    const objB = makeObject("b", {
      material_overrides: { opacity: 0.5 },
    })
    const state = loadedState([objA, objB])

    // Setting wireframe on both should preserve each object's existing overrides
    const next = sceneReducer(state, {
      type: "UPDATE_MATERIAL_OVERRIDES_BATCH",
      objectIds: ["a", "b"],
      overrides: { wireframe: true },
    })

    expect(next.scene!.objects.find((o) => o.id === "a")!.material_overrides).toEqual({
      color: "#ff0000",
      wireframe: true,
    })
    expect(next.scene!.objects.find((o) => o.id === "b")!.material_overrides).toEqual({
      opacity: 0.5,
      wireframe: true,
    })
  })

  it("UPDATE_MATERIAL_OVERRIDES with null resets individual fields", () => {
    const obj = makeObject("a", {
      material_overrides: { color: "#ff0000", opacity: 0.5, wireframe: true },
    })
    const state = loadedState([obj])

    // Reset color to null (unset)
    const next = sceneReducer(state, {
      type: "UPDATE_MATERIAL_OVERRIDES",
      objectId: "a",
      overrides: { color: null },
    })

    expect(next.scene!.objects[0].material_overrides).toEqual({
      color: null,
      opacity: 0.5,
      wireframe: true,
    })
  })

  it("UPDATE_MATERIAL_OVERRIDES_BATCH with null resets fields across objects", () => {
    const objA = makeObject("a", {
      material_overrides: { color: "#ff0000", opacity: 0.5 },
    })
    const objB = makeObject("b", {
      material_overrides: { color: "#00ff00", wireframe: true },
    })
    const state = loadedState([objA, objB])

    // Reset all overrides to null via batch
    const next = sceneReducer(state, {
      type: "UPDATE_MATERIAL_OVERRIDES_BATCH",
      objectIds: ["a", "b"],
      overrides: { color: null, opacity: null, wireframe: null },
    })

    // Shallow merge: all three null keys are spread into existing overrides
    expect(next.scene!.objects.find((o) => o.id === "a")!.material_overrides).toEqual({
      color: null,
      opacity: null,
      wireframe: null,
    })
    expect(next.scene!.objects.find((o) => o.id === "b")!.material_overrides).toEqual({
      color: null,
      opacity: null,
      wireframe: null,
    })
  })
})

// ── Group translation tests ──────────────────────────────────────────

describe("group translation", () => {
  it("translating a group moves all children world positions by the same delta", () => {
    const a = makeObject("a", { position: [2, 0, 0] })
    const b = makeObject("b", { position: [4, 0, 6] })
    const state = loadedState([a, b])

    // Group them — centroid [3, 0, 3]
    const grouped = sceneReducer(state, {
      type: "GROUP_OBJECTS",
      objectIds: ["a", "b"],
    })
    const groupId = [...grouped.selectedIds][0]!
    const objectMap = new Map(grouped.scene!.objects.map((o) => [o.id, o]))

    // Verify initial world positions are preserved
    const aWorld0 = computeWorldPosition(objectMap.get("a")!, objectMap)
    const bWorld0 = computeWorldPosition(objectMap.get("b")!, objectMap)
    expect(aWorld0[0]).toBeCloseTo(2, 5)
    expect(aWorld0[2]).toBeCloseTo(0, 5)
    expect(bWorld0[0]).toBeCloseTo(4, 5)
    expect(bWorld0[2]).toBeCloseTo(6, 5)

    // Move the group by [2, 0, 0]
    const moved = sceneReducer(grouped, {
      type: "UPDATE_TRANSFORM",
      objectId: groupId,
      position: [5, 0, 3],
      rotation: [0, 0, 0, 1],
      scale: [1, 1, 1],
    })
    const movedMap = new Map(moved.scene!.objects.map((o) => [o.id, o]))

    // Children world positions should shift by [2, 0, 0]
    const aWorld1 = computeWorldPosition(movedMap.get("a")!, movedMap)
    const bWorld1 = computeWorldPosition(movedMap.get("b")!, movedMap)
    expect(aWorld1[0]).toBeCloseTo(4, 5) // 2 + 2
    expect(aWorld1[1]).toBeCloseTo(0, 5)
    expect(aWorld1[2]).toBeCloseTo(0, 5)
    expect(bWorld1[0]).toBeCloseTo(6, 5) // 4 + 2
    expect(bWorld1[1]).toBeCloseTo(0, 5)
    expect(bWorld1[2]).toBeCloseTo(6, 5)

    // Relative distance between children is preserved
    expect(bWorld1[0] - aWorld1[0]).toBeCloseTo(bWorld0[0] - aWorld0[0], 5)
    expect(bWorld1[2] - aWorld1[2]).toBeCloseTo(bWorld0[2] - aWorld0[2], 5)
  })

  it("GROUP_OBJECTS with parented objects uses world positions for centroid", () => {
    // Parent at [10, 0, 0], children at local [2, 0, 0] and [4, 0, 0]
    // World positions: a = [12, 0, 0], b = [14, 0, 0]
    const parent = makeObject("parent", { position: [10, 0, 0] })
    const a = makeObject("a", { parent_id: "parent", position: [2, 0, 0] })
    const b = makeObject("b", { parent_id: "parent", position: [4, 0, 0] })
    const state = loadedState([parent, a, b])

    const grouped = sceneReducer(state, {
      type: "GROUP_OBJECTS",
      objectIds: ["a", "b"],
    })
    const groupId = [...grouped.selectedIds][0]!
    const group = grouped.scene!.objects.find((o) => o.id === groupId)!

    // Group centroid should be at world [13, 0, 0] (avg of 12 and 14)
    expect(group.position[0]).toBeCloseTo(13, 5)
    expect(group.position[1]).toBeCloseTo(0, 5)
    expect(group.position[2]).toBeCloseTo(0, 5)

    // Children world positions should be preserved
    const objectMap = new Map(grouped.scene!.objects.map((o) => [o.id, o]))
    const aWorld = computeWorldPosition(objectMap.get("a")!, objectMap)
    const bWorld = computeWorldPosition(objectMap.get("b")!, objectMap)
    expect(aWorld[0]).toBeCloseTo(12, 5)
    expect(aWorld[1]).toBeCloseTo(0, 5)
    expect(aWorld[2]).toBeCloseTo(0, 5)
    expect(bWorld[0]).toBeCloseTo(14, 5)
    expect(bWorld[1]).toBeCloseTo(0, 5)
    expect(bWorld[2]).toBeCloseTo(0, 5)
  })

  it("GROUP_OBJECTS does not create a visible artifact (group has asset_id null)", () => {
    const a = makeObject("a", { asset_id: "mesh_a", position: [0, 0, 0] })
    const b = makeObject("b", { asset_id: "mesh_b", position: [4, 0, 0] })
    const state = loadedState([a, b])

    const grouped = sceneReducer(state, {
      type: "GROUP_OBJECTS",
      objectIds: ["a", "b"],
    })
    const groupId = [...grouped.selectedIds][0]!

    // 3 objects total: group container + 2 children
    expect(grouped.scene!.objects).toHaveLength(3)

    // The group container must have asset_id = null (no visual representation)
    const group = grouped.scene!.objects.find((o) => o.id === groupId)!
    expect(group.asset_id).toBeNull()

    // The children retain their asset IDs
    expect(grouped.scene!.objects.find((o) => o.id === "a")!.asset_id).toBe("mesh_a")
    expect(grouped.scene!.objects.find((o) => o.id === "b")!.asset_id).toBe("mesh_b")
  })

  it("GROUP_OBJECTS with rotated parent preserves full world transform", () => {
    const s = Math.SQRT1_2
    // Parent rotated 90° around Y. Child at local [5, 0, 0] → world [0, 0, -5]
    const parent = makeObject("parent", {
      position: [0, 0, 0],
      rotation: [0, s, 0, s],
      scale: [2, 2, 2],
    })
    const a = makeObject("a", { parent_id: "parent", position: [5, 0, 0] })
    const b = makeObject("b", { parent_id: "parent", position: [0, 0, 3] })
    const state = loadedState([parent, a, b])

    // Before grouping, capture full world transforms
    const preMap = new Map(state.scene!.objects.map((o) => [o.id, o]))
    const aWorldPre = computeWorldPosition(preMap.get("a")!, preMap)
    const bWorldPre = computeWorldPosition(preMap.get("b")!, preMap)
    const aRotPre = computeWorldRotation(preMap.get("a")!, preMap)
    const bRotPre = computeWorldRotation(preMap.get("b")!, preMap)
    const aSclPre = computeWorldScale(preMap.get("a")!, preMap)
    const bSclPre = computeWorldScale(preMap.get("b")!, preMap)

    const grouped = sceneReducer(state, {
      type: "GROUP_OBJECTS",
      objectIds: ["a", "b"],
    })
    const groupId = [...grouped.selectedIds][0]!
    const objectMap = new Map(grouped.scene!.objects.map((o) => [o.id, o]))

    // Children world positions must be preserved after grouping
    const aWorld = computeWorldPosition(objectMap.get("a")!, objectMap)
    const bWorld = computeWorldPosition(objectMap.get("b")!, objectMap)
    expect(aWorld[0]).toBeCloseTo(aWorldPre[0], 5)
    expect(aWorld[1]).toBeCloseTo(aWorldPre[1], 5)
    expect(aWorld[2]).toBeCloseTo(aWorldPre[2], 5)
    expect(bWorld[0]).toBeCloseTo(bWorldPre[0], 5)
    expect(bWorld[1]).toBeCloseTo(bWorldPre[1], 5)
    expect(bWorld[2]).toBeCloseTo(bWorldPre[2], 5)

    // Children world rotation must be preserved
    const aRot = computeWorldRotation(objectMap.get("a")!, objectMap)
    const bRot = computeWorldRotation(objectMap.get("b")!, objectMap)
    for (let i = 0; i < 4; i++) {
      expect(aRot[i]).toBeCloseTo(aRotPre[i], 5)
      expect(bRot[i]).toBeCloseTo(bRotPre[i], 5)
    }

    // Children world scale must be preserved
    const aScl = computeWorldScale(objectMap.get("a")!, objectMap)
    const bScl = computeWorldScale(objectMap.get("b")!, objectMap)
    for (let i = 0; i < 3; i++) {
      expect(aScl[i]).toBeCloseTo(aSclPre[i], 5)
      expect(bScl[i]).toBeCloseTo(bSclPre[i], 5)
    }
  })
})
