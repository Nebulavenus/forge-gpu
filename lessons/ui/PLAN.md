# UI Track — Upcoming Lessons

## Lesson 11: Widget ID System

**Problem:** The current ID system requires callers to manually assign unique
integer IDs to every widget.  Windows internally reserve `id+1` (scrollbar) and
`id+2` (collapse toggle), creating invisible collision zones that cause
hard-to-debug input bugs (see issue #151 — checkbox toggling failed because its
ID collided with the window's collapse toggle).

**Goals:**

- Replace manual integer IDs with automatic ID generation
- Eliminate the hidden `id+1`, `id+2` reservation pattern
- Support hierarchical scoping so widgets inside different windows/panels
  cannot collide regardless of user-chosen IDs
- Maintain IMGUI simplicity — no registration, no cleanup

**Approach — hashed string IDs with scope stacking:**

1. Introduce `forge_ui_push_id(ctx, name)` / `forge_ui_pop_id(ctx)` that push
   a scope onto an ID stack.  Windows and panels call push/pop automatically.
2. Widget IDs are computed as `hash(scope_stack + local_name)` using FNV-1a,
   producing a uint32 that is virtually collision-free.
3. Callers pass string labels instead of integers:
   `forge_ui_ctx_button(ctx, "OK", rect)` instead of
   `forge_ui_ctx_button(ctx, 42, rect)`.
4. For widgets that share a label (e.g. two "Delete" buttons), callers
   can append `##suffix`: `"Delete##item_1"`, `"Delete##item_2"`.
5. Window internals use `push_id("__scrollbar")` / `push_id("__toggle")`
   inside their own scope — no leaked ID arithmetic.

**Lesson structure:**

- Explain the collision problem with the old integer system
- Introduce FNV-1a hashing and the `##` separator convention
- Implement the ID stack in `forge_ui_ctx.h`
- Migrate all existing widgets to the new system
- Show before/after: old code with manual IDs vs new code with labels

## Lesson 12: Theming and Color System

**Problem:** Every widget call requires explicit RGBA color parameters, making
it tedious to build a consistent UI and impossible to swap themes at runtime.
The current default colors are hardcoded as `#define` constants scattered across
`forge_ui_ctx.h` and `forge_ui_window.h`.

**Goals:**

- Centralize all UI colors into a `ForgeUiTheme` struct
- Support runtime theme switching (dark, light, custom)
- Ship a built-in dark theme matching the project's diagram palette
  (bg #1a1a2e, cyan #4fc3f7, orange #ff7043, green #66bb6a)
- Reduce widget API surface — colors come from the theme unless overridden

**Approach — theme struct with per-widget color slots:**

1. Define `ForgeUiTheme` with named color slots:

   ```c
   typedef struct ForgeUiTheme {
       /* Global */
       ForgeUiColor background;
       ForgeUiColor text;
       ForgeUiColor text_dim;
       ForgeUiColor accent;        /* primary interactive color  */
       ForgeUiColor accent_hover;
       ForgeUiColor accent_active;
       /* Buttons */
       ForgeUiColor button_normal;
       ForgeUiColor button_hot;
       ForgeUiColor button_active;
       ForgeUiColor button_text;
       /* Checkbox, slider, text input, panel, window, scrollbar ... */
   } ForgeUiTheme;
   ```

2. Store a `ForgeUiTheme *theme` pointer on `ForgeUiContext`.  Widget
   functions read from the theme by default.
3. Provide `forge_ui_theme_dark()` and `forge_ui_theme_light()` as built-in
   presets.  The dark preset matches the diagram palette.
4. Allow per-call color overrides for one-off styling without changing the
   theme.
5. Replace the scattered `#define` color constants with theme lookups.

**Lesson structure:**

- Show the pain of per-call colors and hardcoded defines
- Design the ForgeUiTheme struct with clear slot naming
- Implement dark and light presets
- Add theme pointer to ForgeUiContext
- Update all widget functions to read theme colors as defaults
- Demo: runtime theme switching with a toggle button
