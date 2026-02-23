# Lesson 07 — Camera & Input

## What you'll learn

- **First-person camera** — fly through a 3D scene with quaternion orientation
- **Keyboard input** — poll key state for smooth, continuous movement (WASD)
- **Mouse look** — relative mouse mode for FPS-style camera rotation
- **Delta time** — decouple movement speed from frame rate
- **Multiple objects** — draw several cubes with different model transforms
- **Pitch clamping** — prevent the camera from flipping upside down

## Result

![Lesson 07 preview](assets/screenshot.png)

A small scene of colored cubes at different positions, sizes, and rotation
speeds. Move through the scene with WASD, look around with the mouse, and
fly up/down with Space and Shift.

### Controls

| Input | Action |
|-------|--------|
| W / Up Arrow | Move forward |
| S / Down Arrow | Move backward |
| A / Left Arrow | Strafe left |
| D / Right Arrow | Strafe right |
| Space | Fly up |
| Left Shift | Fly down |
| Mouse | Look around |
| Escape | Release mouse / Quit |
| Click | Recapture mouse |

## Key concepts

### First-person camera with quaternions

In Lesson 06, the camera was fixed — `mat4_look_at(eye, target, up)` never
changed. Now the camera is controlled by the player. We store the camera as:

```c
vec3  cam_position;   /* where the camera is */
float cam_yaw;        /* rotation around Y (left/right) */
float cam_pitch;      /* rotation around X (up/down) */
```

Each frame, we convert yaw and pitch to a quaternion, then build the view
matrix:

```c
quat orientation = quat_from_euler(yaw, pitch, 0.0f);
mat4 view = mat4_view_from_quat(position, orientation);
```

This is the **exact pattern** from
[Math Lesson 09 — View Matrix](../../math/09-view-matrix/), Section 7.

**Why store euler angles instead of the quaternion directly?**

1. Mouse deltas naturally map to yaw/pitch increments
2. We need to clamp pitch to avoid flipping (gimbal lock at the poles)
3. For an FPS camera, yaw + pitch is sufficient (no roll needed)

See [Math Lesson 08 — Orientation](../../math/08-orientation/) for the full
discussion of when to use euler angles vs quaternions.

### Keyboard state polling

Previous lessons only handled discrete events (`SDL_EVENT_KEY_DOWN`). For
smooth movement, we need to know which keys are *currently held*:

```c
const bool *keys = SDL_GetKeyboardState(NULL);

if (keys[SDL_SCANCODE_W]) {
    position = vec3_add(position,
        vec3_scale(forward, MOVE_SPEED * dt));
}
```

`SDL_GetKeyboardState` returns a pointer to an array indexed by scancode.
Unlike key events (which fire once per press), this gives a continuous
true/false for every key — essential for smooth camera movement.

### Mouse look with relative mode

`SDL_SetWindowRelativeMouseMode(window, true)` hides the cursor and reports
movement as delta X/Y instead of absolute position:

```c
/* In SDL_AppEvent: */
if (event->type == SDL_EVENT_MOUSE_MOTION && mouse_captured) {
    cam_yaw   -= event->motion.xrel * MOUSE_SENSITIVITY;
    cam_pitch -= event->motion.yrel * MOUSE_SENSITIVITY;
}
```

Horizontal mouse movement becomes yaw (turn left/right). Vertical becomes
pitch (look up/down). The sensitivity constant controls how much rotation
per pixel of mouse movement.

### Delta time

Without delta time, movement speed depends on frame rate — the game would
run twice as fast at 120 FPS compared to 60 FPS. We fix this by multiplying
all movement by the time since the last frame:

```c
float dt = (float)(now_ms - last_ticks) / 1000.0f;
position += forward * speed * dt;  /* units per second, not per frame */
```

At 60 FPS: `dt ≈ 0.0167s`, movement per frame = `3.0 * 0.0167 ≈ 0.05 units`
At 30 FPS: `dt ≈ 0.0333s`, movement per frame = `3.0 * 0.0333 ≈ 0.10 units`

Different per-frame distances, but the same distance per second.

We also clamp dt to a maximum (0.1s) to prevent huge teleportation if the
app stalls or the user alt-tabs away.

### Camera movement directions

The camera moves along its own local axes, not the world axes. We extract
these from the quaternion orientation using optimized functions from the
math library:

```c
vec3 forward = quat_forward(orientation);  /* where camera looks */
vec3 right   = quat_right(orientation);    /* camera's right side */
```

Forward/back movement uses `forward`, strafing uses `right`. Flying up/down
uses the world Y axis (not `quat_up`) so "up" always means up, even when
looking at the ground — like a noclip camera in games.

### Drawing multiple objects

Each cube in the scene gets its own model matrix (position + rotation + scale),
but they all share the same vertex/index buffers and pipeline:

```c
for (each cube) {
    mat4 model = translate * rotate * scale;
    mat4 mvp   = view_proj * model;
    SDL_PushGPUVertexUniformData(cmd, 0, &mvp, sizeof(mvp));
    SDL_DrawGPUIndexedPrimitives(pass, INDEX_COUNT, 1, 0, 0, 0);
}
```

This is a simple multi-object rendering pattern. For many objects, instanced
rendering (Lesson 14) is more efficient, but separate draw calls are easier
to understand and fine for a small scene.

## Math

This lesson uses:

- **Orientation** — [Math Lesson 08](../../math/08-orientation/) for
  quaternions, euler angles, and gimbal lock
- **View Matrix** — [Math Lesson 09](../../math/09-view-matrix/) for building
  view matrices from position + quaternion, extracting basis vectors
- **Matrices** — [Math Lesson 05](../../math/05-matrices/) for model transforms
  (translate, rotate, scale)
- **Projections** — [Math Lesson 06](../../math/06-projections/) for
  perspective projection

Key math library functions:

| Function | Purpose |
|----------|---------|
| `quat_from_euler(yaw, pitch, roll)` | Convert euler angles to quaternion |
| `quat_forward(q)` | Extract camera's look direction |
| `quat_right(q)` | Extract camera's right direction |
| `mat4_view_from_quat(pos, q)` | Build view matrix from position + quaternion |
| `mat4_perspective(fov, aspect, near, far)` | Perspective projection matrix |
| `mat4_translate(v)` | Translation matrix |
| `mat4_rotate_y(angle)` | Y-axis rotation matrix |
| `mat4_scale(v)` | Scale matrix |

## Shaders

| File | Purpose |
|------|---------|
| `scene.vert.hlsl` | Transforms vertices via the MVP matrix with a view matrix rebuilt each frame from camera position and quaternion |
| `scene.frag.hlsl` | Outputs interpolated vertex color without texturing or lighting |

## Building

```bash
cmake -B build
cmake --build build --config Debug
python scripts/run.py 07
```

## AI skill

This lesson has a matching Claude Code skill at
[`.claude/skills/camera-and-input/SKILL.md`](../../../.claude/skills/camera-and-input/SKILL.md).

Invoke it with `/camera-and-input` in Claude Code, or copy it into your own
project's `.claude/skills/` directory.

## Exercises

1. **Walk mode** — Instead of flying (moving along the camera's forward
   direction including vertical), keep the Y component of movement at zero.
   This makes WASD behave like walking on a flat surface. Hint: zero out
   the Y of `forward` and `right` before using them, then re-normalize.

2. **Sprint** — Hold Left Ctrl to move at 2x speed. This is a one-line
   change: multiply `MOVE_SPEED` by a factor when the key is held.

3. **Mouse sensitivity slider** — Add Q/E keys to increase/decrease mouse
   sensitivity at runtime. Display the current value with `SDL_Log`.

4. **Orbit camera** — Instead of first-person, implement an orbit camera
   that always looks at the origin. Use `mat4_look_at` (from Lesson 06) with
   the eye position calculated from spherical coordinates (radius + yaw +
   pitch). Compare how this feels vs the FPS camera.
