import type { SceneObject } from "./types"

/**
 * Compute world-space position by walking the parent chain and summing
 * translations. (Rotation/scale are identity for group nodes created by
 * GROUP_OBJECTS, so additive translation is correct.)
 *
 * The visited set guards against cycles in malformed hierarchies.
 */
export function computeWorldPosition(
  obj: SceneObject,
  objectMap: Map<string, SceneObject>,
): [number, number, number] {
  let wx = obj.position[0],
    wy = obj.position[1],
    wz = obj.position[2]
  let parentId = obj.parent_id
  const visited = new Set<string>()
  while (parentId !== null && !visited.has(parentId)) {
    visited.add(parentId)
    const parent = objectMap.get(parentId)
    if (!parent) break
    wx += parent.position[0]
    wy += parent.position[1]
    wz += parent.position[2]
    parentId = parent.parent_id
  }
  return [wx, wy, wz]
}
