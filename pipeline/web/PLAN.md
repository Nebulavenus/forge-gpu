# Pipeline Web UI — Improvement Plan

Current state: The web UI has a functional asset browser, asset detail pages
with previewers, a scene list, and a three-panel scene editor with 3D viewport.
The stack is React 19 + TanStack Router + React Query + Three.js/r3f + Tailwind.

This plan covers improvements to three areas: the landing page, the asset
browser, and the scene editor.

---

## 1. Landing page (dashboard)

The app currently opens directly to the asset browser. There is no overview or
entry point that orients the user. The `/` route should become a dashboard that
surfaces pipeline health at a glance.

### 1a. Pipeline status summary

Display the data already returned by `GET /api/status` (total count, by_type,
by_status, source_dir, output_dir) as a compact dashboard header.

- **Stat cards**: Total assets, textures, meshes, animations, scenes — each
  card clickable to jump to the asset browser pre-filtered by that type.
- **Status breakdown bar**: A horizontal stacked bar showing the proportion of
  processed / new / changed / missing assets, color-coded to match existing
  badge colors. Gives an instant read on pipeline health.
- **Source / output paths**: Show the configured directories in a small footer
  line so the user knows what the pipeline is pointing at.

Frontend only — no backend changes. The `/api/status` endpoint already returns
everything needed.

### 1b. Recent activity feed

Show the most recently modified assets (by output file mtime) so the user can
see what was last processed.

- **Backend**: Add an optional `sort=recent` query parameter to
  `GET /api/assets` that sorts by output file modification time descending.
  Add a `limit` parameter to cap results (default: all, for this widget use
  `limit=8`).
- **Frontend**: A "Recent" section below the stats showing small asset cards
  with name, type, and relative timestamp ("2 min ago").

### 1c. Quick actions

- **Process All** button: Triggers a full pipeline run. Backend needs a new
  `POST /api/process` endpoint that runs all unprocessed/changed assets
  sequentially (or reports progress over the existing WebSocket).
- **Rescan** button: Triggers `GET /api/assets` with a cache-bust to force
  re-scanning the source directory. Alternatively, a dedicated
  `POST /api/rescan` endpoint that clears the in-memory asset cache.

### 1d. Navigation cards

Two large cards linking to the asset browser and the scene list, each with a
brief count ("42 assets", "3 scenes") so the user has a clear path forward.

---

## 2. Asset browser improvements

### 2a. Thumbnail previews on asset cards

The asset cards currently show only name, type badge, and file size. Adding
small thumbnail previews would make scanning the grid far more useful.

- **Backend**: Add `GET /api/assets/{id}/thumbnail` that returns a small
  (128×128) image. For textures, resize the source image on the fly (cache
  result on disk in `{output_dir}/.thumbnails/`). For meshes, return a
  placeholder icon by type (or pre-rendered thumbnail if available). Use
  Pillow for resizing — already a transitive dependency or trivially added.
- **Frontend**: Show the thumbnail as a 4:3 image area at the top of each
  asset card. Lazy-load with `loading="lazy"` to avoid hammering the server
  on page load. Fall back to a colored icon per type if thumbnail fails.

### 2b. Sort controls

Assets are currently unsorted (server order). Add a sort dropdown:

- **Name** (A-Z, Z-A)
- **Size** (largest first, smallest first)
- **Status** (unprocessed first — surfaces work to do)
- **Type** (group by type)

Frontend-only if sorting client-side, or add `sort` / `order` query params to
`GET /api/assets` for server-side sort.

### 2c. Grid / list view toggle

Add a toggle between the current grid view and a compact list/table view.
The table view would show columns: thumbnail, name, type, size, output size,
status, fingerprint (truncated). Useful when managing many assets.

Frontend only — same data, different layout.

### 2d. Batch processing

Allow selecting multiple assets and processing them in one action.

- **Frontend**: Add a checkbox to each card (visible in a "select mode"
  toggled by a toolbar button). Show a floating action bar at the bottom with
  "Process Selected (N)" when items are checked.
- **Backend**: Add `POST /api/process/batch` accepting `{ asset_ids: string[] }`.
  Process sequentially, streaming progress over WebSocket. Return summary
  (succeeded, failed, skipped).

### 2e. Status filter chips

Replace the current `TypeFilter` button group with a dual-filter system:
type chips AND status chips. Let the user filter to "textures that are
changed" or "meshes that are missing". Both filters compose (AND logic).

Frontend-only — the asset list is already small enough for client-side
filtering, or extend `GET /api/assets` with a `status` query parameter.

### 2f. Asset dependency graph

For mesh assets that reference textures (via glTF materials), show which
textures a mesh depends on and which scenes reference it.

- **Backend**: Add `GET /api/assets/{id}/dependencies` returning
  `{ depends_on: string[], depended_by: string[] }`. Parse glTF files to
  extract texture references. Scan scene files to find asset_id references.
- **Frontend**: Show a "Dependencies" section on the asset detail page with
  clickable links to related assets.

---

## 3. Scene editor improvements

### 3a. Drag-and-drop from asset browser

Allow dragging assets from a collapsible asset shelf into the 3D viewport.

- **Frontend**: Add a collapsible left-side "Asset Shelf" panel (below or
  tabbed with the hierarchy panel) listing mesh assets with thumbnails.
  Implement HTML5 drag from the shelf; on drop over the Canvas, raycast
  against the ground plane to determine placement position. Dispatch
  `ADD_OBJECT` with the drop coordinates as the initial position.
- No backend changes — uses existing asset list endpoint.

### 3b. Multi-select and group operations

Currently only one object can be selected. Add multi-select:

- **Store**: Change `selectedId: string | null` to
  `selectedIds: Set<string>`. Update all actions that reference selection.
- **Viewport**: Shift+click to add to selection. Ctrl+click to toggle.
  Drag-select (box select) with a rubber-band rectangle.
- **Toolbar**: "Group" button that creates an empty parent and reparents all
  selected objects under it. "Ungroup" to flatten.
- **Inspector**: When multiple objects are selected, show shared properties.
  Mixed values show "—". Changing a value applies to all selected objects.

### 3c. Duplicate object

Add Ctrl+D / toolbar button to clone the selected object (deep copy with
offset position). Straightforward — dispatch a new `DUPLICATE_OBJECT` action
in the reducer that copies the object, generates a new ID, and offsets
position by +1 on X.

### 3d. Snap and grid alignment

- **Toolbar toggle**: "Snap to grid" checkbox with configurable grid size
  (0.25, 0.5, 1.0, 2.0). When enabled, TransformControls uses
  `translationSnap` / `rotationSnap` / `scaleSnap` props.
- **Inspector**: Snap position values to nearest grid on commit.

Frontend only — drei's TransformControls already supports snap props.

### 3e. Camera bookmarks

Save and restore camera positions for quick navigation in large scenes.

- **Store**: Add `cameras: { name: string, position: vec3, target: vec3 }[]`
  to scene data. Persist with the scene JSON.
- **Backend**: Extend the scene schema to accept an optional `cameras` array.
  No validation changes needed if the field is optional and pass-through.
- **Toolbar**: "Save View" button stores current OrbitControls position/target.
  Dropdown to restore saved views.

### 3f. Scene statistics overlay

Show a small overlay in the viewport corner with: object count, triangle
count (from loaded meshes), texture memory estimate. Helps the user
understand scene complexity.

- **Frontend**: Accumulate geometry stats from loaded Three.js objects. Display
  as a semi-transparent overlay div positioned over the Canvas.

### 3g. Improved hierarchy panel

- **Drag-to-reparent**: Drag tree nodes to reparent objects. Drop on another
  node to make it a child; drop between nodes to reorder. Dispatch
  `REPARENT_OBJECT` on drop.
- **Right-click context menu**: Rename, Duplicate, Delete, Add Child, Toggle
  Visibility. Uses a simple absolutely-positioned menu.
- **Search/filter**: Small search input at the top of the hierarchy panel to
  filter objects by name in large scenes.

### 3h. Material / appearance overrides

Allow per-object material property overrides in the inspector (color tint,
opacity, wireframe toggle) without changing the source asset.

- **Store**: Add optional `material_overrides: { color?: string, opacity?:
  number, wireframe?: boolean }` to `SceneObject`.
- **Backend**: Extend scene schema to accept the new field (pass-through).
- **Inspector**: Add a "Material" section with color picker, opacity slider,
  wireframe checkbox. Apply overrides to the Three.js material at render time.

### 3i. Scene export

Export the authored scene to a format consumable by the C runtime.

- **Backend**: Add `POST /api/scenes/{id}/export` that converts the scene
  JSON into the binary `.fscene` format used by `common/pipeline/`. Write the
  file to the output directory.
- **Toolbar**: "Export" button triggers the endpoint and shows a success toast.

---

## Priority order

Roughly ordered by impact and implementation cost:

| Priority | Item | Effort | Impact |
|----------|------|--------|--------|
| 1 | 1a. Dashboard status summary | S | High — first thing users see |
| 2 | 2a. Thumbnail previews | M | High — makes browsing visual |
| 3 | 3c. Duplicate object | S | High — basic editor workflow |
| 4 | 3d. Snap to grid | S | Medium — precision editing |
| 5 | 2b. Sort controls | S | Medium — findability |
| 6 | 1b. Recent activity | S | Medium — shows pipeline pulse |
| 7 | 2c. Grid/list toggle | S | Medium — dense asset view |
| 8 | 3a. Drag-and-drop assets | M | High — natural scene building |
| 9 | 3g. Hierarchy drag-reparent | M | Medium — hierarchy editing |
| 10 | 2d. Batch processing | M | Medium — bulk operations |
| 11 | 3b. Multi-select | L | Medium — needed for complex scenes |
| 12 | 1c. Quick actions | S | Medium — pipeline control |
| 13 | 2e. Status filter chips | S | Low — refinement |
| 14 | 3f. Scene stats overlay | S | Low — informational |
| 15 | 3e. Camera bookmarks | M | Low — convenience |
| 16 | 3h. Material overrides | M | Low — polish |
| 17 | 2f. Dependency graph | L | Low — advanced |
| 18 | 3i. Scene export | M | Medium — bridges editor to runtime |
| 19 | 1d. Navigation cards | S | Low — wayfinding |

S = small (< 1 day), M = medium (1-2 days), L = large (3+ days)
