# UI Track — Upcoming Lessons

## ~~Lesson 11: Widget ID System~~ (Done)

Implemented in [lessons/ui/11-widget-id-system/](11-widget-id-system/).
Replaced manual integer IDs with FNV-1a hashed string labels and hierarchical
scope stacking.  All existing lessons and tests migrated to the new API.

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
