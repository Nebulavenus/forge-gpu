import type { SceneObject } from "./types"

// ── Quaternion / vector helpers (minimal, self-contained) ────────────

type Vec3 = [number, number, number]
type Quat = [number, number, number, number] // xyzw

/** Rotate a vector by a unit quaternion. */
function quatRotateVec3(q: Quat, v: Vec3): Vec3 {
  const [qx, qy, qz, qw] = q
  const [vx, vy, vz] = v
  // t = 2 * cross(q.xyz, v)
  const tx = 2 * (qy * vz - qz * vy)
  const ty = 2 * (qz * vx - qx * vz)
  const tz = 2 * (qx * vy - qy * vx)
  // result = v + qw * t + cross(q.xyz, t)
  return [
    vx + qw * tx + (qy * tz - qz * ty),
    vy + qw * ty + (qz * tx - qx * tz),
    vz + qw * tz + (qx * ty - qy * tx),
  ]
}

/** Conjugate (inverse for unit quaternions). */
function quatConjugate(q: Quat): Quat {
  return [-q[0], -q[1], -q[2], q[3]]
}

/** Multiply two quaternions: a * b (apply b first, then a). */
function quatMultiply(a: Quat, b: Quat): Quat {
  const [ax, ay, az, aw] = a
  const [bx, by, bz, bw] = b
  return [
    aw * bx + ax * bw + ay * bz - az * by,
    aw * by - ax * bz + ay * bw + az * bx,
    aw * bz + ax * by - ay * bx + az * bw,
    aw * bw - ax * bx - ay * by - az * bz,
  ]
}

/** Component-wise multiply two Vec3. */
function vec3Mul(a: Vec3, b: Vec3): Vec3 {
  return [a[0] * b[0], a[1] * b[1], a[2] * b[2]]
}

/** Component-wise divide two Vec3 (safe: treats zero divisor as 1). */
function vec3Div(a: Vec3, b: Vec3): Vec3 {
  // Zero scale is degenerate; fall back to identity to avoid NaN/Infinity.
  return [
    b[0] !== 0 ? a[0] / b[0] : a[0],
    b[1] !== 0 ? a[1] / b[1] : a[1],
    b[2] !== 0 ? a[2] / b[2] : a[2],
  ]
}

// ── Public API ───────────────────────────────────────────────────────

/**
 * Collect the ancestor chain from `obj` up to the root (bottom-up).
 * Returns an array of ancestors in parent→grandparent→… order.
 * Guards against cycles with a visited set.
 */
function getAncestorChain(
  obj: SceneObject,
  objectMap: Map<string, SceneObject>,
): SceneObject[] {
  const ancestors: SceneObject[] = []
  let parentId = obj.parent_id
  const visited = new Set<string>()
  while (parentId !== null && !visited.has(parentId)) {
    visited.add(parentId)
    const parent = objectMap.get(parentId)
    if (!parent) break
    ancestors.push(parent)
    parentId = parent.parent_id
  }
  return ancestors
}

/**
 * Compute world-space position by composing each ancestor's TRS transform
 * (translation, rotation, scale) from root down to the object.
 *
 * For each ancestor (root-to-parent order), the local position of its child
 * is transformed by: worldPos = parentPos + parentRot * (parentScale * localPos)
 *
 * The visited set guards against cycles in malformed hierarchies.
 */
export function computeWorldPosition(
  obj: SceneObject,
  objectMap: Map<string, SceneObject>,
): Vec3 {
  // Collect ancestors bottom-up, then reverse to process root-first
  const ancestors = getAncestorChain(obj, objectMap)

  // Build accumulated world rotation and scale from root down
  let worldPos: Vec3 = [0, 0, 0]
  let worldRot: Quat = [0, 0, 0, 1]
  let worldScale: Vec3 = [1, 1, 1]

  // Process from root (last in ancestors) down to immediate parent (first)
  for (let i = ancestors.length - 1; i >= 0; i--) {
    const a = ancestors[i]!
    // Apply current accumulated transform to this ancestor's local position
    const scaled = vec3Mul(a.position, worldScale)
    const rotated = quatRotateVec3(worldRot, scaled)
    worldPos = [
      worldPos[0] + rotated[0],
      worldPos[1] + rotated[1],
      worldPos[2] + rotated[2],
    ]
    // Accumulate this ancestor's rotation and scale
    worldRot = quatMultiply(worldRot, a.rotation)
    worldScale = vec3Mul(worldScale, a.scale)
  }

  // Finally apply accumulated transform to the object's own local position
  const scaled = vec3Mul(obj.position, worldScale)
  const rotated = quatRotateVec3(worldRot, scaled)
  return [
    worldPos[0] + rotated[0],
    worldPos[1] + rotated[1],
    worldPos[2] + rotated[2],
  ]
}

/**
 * Compute the accumulated world rotation of an object by composing
 * all ancestor rotations from root down, then applying the object's own.
 */
export function computeWorldRotation(
  obj: SceneObject,
  objectMap: Map<string, SceneObject>,
): Quat {
  const ancestors = getAncestorChain(obj, objectMap)
  let worldRot: Quat = [0, 0, 0, 1]
  for (let i = ancestors.length - 1; i >= 0; i--) {
    worldRot = quatMultiply(worldRot, ancestors[i]!.rotation)
  }
  return quatMultiply(worldRot, obj.rotation)
}

/**
 * Compute the accumulated world scale of an object by composing
 * all ancestor scales from root down, then applying the object's own.
 */
export function computeWorldScale(
  obj: SceneObject,
  objectMap: Map<string, SceneObject>,
): Vec3 {
  const ancestors = getAncestorChain(obj, objectMap)
  let worldScale: Vec3 = [1, 1, 1]
  for (let i = ancestors.length - 1; i >= 0; i--) {
    worldScale = vec3Mul(worldScale, ancestors[i]!.scale)
  }
  return vec3Mul(worldScale, obj.scale)
}

/**
 * Convert a world-space position to local-space relative to a given parent.
 * Inverts the parent's accumulated world TRS: localPos = invScale * (invRot * (worldPos - parentWorldPos))
 */
export function worldToLocal(
  worldPos: Vec3,
  parent: SceneObject,
  objectMap: Map<string, SceneObject>,
): Vec3 {
  // Compute the parent's accumulated world transform
  const ancestors = getAncestorChain(parent, objectMap)

  let parentWorldPos: Vec3 = [0, 0, 0]
  let parentWorldRot: Quat = [0, 0, 0, 1]
  let parentWorldScale: Vec3 = [1, 1, 1]

  // Accumulate from root down to parent (exclusive — parent's own TRS is included)
  for (let i = ancestors.length - 1; i >= 0; i--) {
    const a = ancestors[i]!
    const scaled = vec3Mul(a.position, parentWorldScale)
    const rotated = quatRotateVec3(parentWorldRot, scaled)
    parentWorldPos = [
      parentWorldPos[0] + rotated[0],
      parentWorldPos[1] + rotated[1],
      parentWorldPos[2] + rotated[2],
    ]
    parentWorldRot = quatMultiply(parentWorldRot, a.rotation)
    parentWorldScale = vec3Mul(parentWorldScale, a.scale)
  }

  // Include the parent's own transform
  const scaled = vec3Mul(parent.position, parentWorldScale)
  const rotated = quatRotateVec3(parentWorldRot, scaled)
  parentWorldPos = [
    parentWorldPos[0] + rotated[0],
    parentWorldPos[1] + rotated[1],
    parentWorldPos[2] + rotated[2],
  ]
  parentWorldRot = quatMultiply(parentWorldRot, parent.rotation)
  parentWorldScale = vec3Mul(parentWorldScale, parent.scale)

  // Invert: localPos = invScale * (invRot * (worldPos - parentWorldPos))
  const delta: Vec3 = [
    worldPos[0] - parentWorldPos[0],
    worldPos[1] - parentWorldPos[1],
    worldPos[2] - parentWorldPos[2],
  ]
  const invRot = quatConjugate(parentWorldRot)
  const unrotated = quatRotateVec3(invRot, delta)
  return vec3Div(unrotated, parentWorldScale)
}
