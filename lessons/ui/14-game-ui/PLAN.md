# UI Lesson 14 — Game UI

## Scope

Demonstrate common game UI patterns composed from the existing immediate-mode
controls: health/mana/stamina bars, inventory grids, HUD anchoring, action
bars, and pause menus. Introduces `forge_ui_ctx_progress_bar` for non-interactive
stat display and shows proportional layout for different screen sizes.

## Key topics

- Progress bars for health, mana, stamina, and XP
- Inventory grid using nested horizontal layouts inside a vertical panel
- HUD anchoring — positioning elements at screen edges with margins
- Action bar — horizontal row of ability buttons
- Pause menu — centered overlay panel with navigation buttons
- Proportional layout — computing positions from screen dimensions

## Output images

1. **status_bars.bmp** — Player stat bars (health, mana, stamina, XP)
2. **inventory.bmp** — 4×5 inventory grid with item slots and counts
3. **hud.bmp** — Complete HUD layout at 1280×720
4. **pause_menu.bmp** — Centered overlay menu on dimmed background

## main.c Decomposition

### Chunk A (~250 lines): Header, helpers, status bars, inventory

- License, includes, defines (FB dimensions, colors, grid sizes)
- `init_codepoints()` helper
- `render_status_bars()` — progress bars with labels
- `render_inventory()` — nested layout grid with item slot buttons

### Chunk B (~250 lines): HUD, pause menu, main

- `render_hud()` — full-screen HUD composition
- `render_pause_menu()` — centered overlay
- `main()` — init SDL, font, atlas, context; call each render function; cleanup

## New library additions

- `forge_ui_ctx_progress_bar()` — non-interactive filled bar
- `forge_ui_ctx_progress_bar_layout()` — layout-aware variant
