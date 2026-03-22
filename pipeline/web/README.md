# Pipeline Web UI

Browser frontend for the forge asset pipeline. Displays asset status,
previews textures and 3D models, edits per-asset import settings, and
provides a visual scene editor for composing 3D scenes.

## Stack

- React 19, TypeScript
- Vite 6 (build + dev server)
- Tailwind CSS 4
- TanStack Router (file-based routing)
- TanStack React Query (data fetching)
- Three.js via react-three-fiber and drei (mesh preview)
- Vitest (tests)

## Development

Start the FastAPI backend and the Vite dev server separately:

```bash
# Terminal 1 — backend (from repo root)
python -m pipeline serve

# Terminal 2 — frontend (from pipeline/web/)
npm install
npm run dev
```

Vite proxies `/api` and `/ws` requests to `http://127.0.0.1:8000` so
the frontend can call the backend without CORS issues during development.
The proxy is configured in `vite.config.ts` — both HTTP (`/api`) and
WebSocket (`/ws`) paths are forwarded. In production the FastAPI server
serves the built frontend directly, so no proxy is needed.

## Production build

```bash
npm run build
```

Output goes to `dist/`. The FastAPI server mounts this directory via
`StaticFiles` automatically when it exists — no separate frontend
process needed. Run `python -m pipeline serve` and open the browser.

## Routes

| Path | Description |
|------|-------------|
| `/` | Asset browser — grid of cards with search, type filter, atlas preview |
| `/assets/$assetId` | Asset detail — metadata table, preview, import settings editor |
| `/scenes` | Scene list — cards for each authored scene, create and delete |
| `/scenes/$sceneId` | Scene editor — 3D viewport, hierarchy panel, inspector, undo/redo |

TanStack Router generates `routeTree.gen.ts` from files in `src/routes/`.
Do not edit the generated file — add or rename route files and the plugin
regenerates it on build or dev server start.

## API endpoints

The frontend consumes these REST endpoints from the FastAPI backend
(`pipeline/server.py`):

| Method | Path | Purpose |
|--------|------|---------|
| `GET` | `/api/assets` | List all assets, with optional `?type=` and `?search=` filters |
| `GET` | `/api/assets/{asset_id}` | Single asset metadata |
| `GET` | `/api/assets/{asset_id}/file` | Serve source file (`?variant=processed` for output) |
| `GET` | `/api/assets/{asset_id}/companions` | Serve companion files (`.bin`, textures) relative to an asset |
| `GET` | `/api/assets/{asset_id}/settings` | Import settings (schema, global, per-asset, effective) |
| `PUT` | `/api/assets/{asset_id}/settings` | Save per-asset overrides |
| `DELETE` | `/api/assets/{asset_id}/settings` | Delete per-asset overrides, revert to global defaults |
| `POST` | `/api/assets/{asset_id}/process` | Re-process a single asset with its effective settings |
| `GET` | `/api/status` | Pipeline summary (counts by type and status) |
| `GET` | `/api/atlas` | Atlas metadata JSON (rect positions, UV coordinates) |
| `GET` | `/api/atlas/image` | Atlas PNG image |
| `GET` | `/api/scenes` | List all authored scenes |
| `POST` | `/api/scenes` | Create a new scene (`{name}`) |
| `GET` | `/api/scenes/{scene_id}` | Get scene data (objects, hierarchy, transforms) |
| `PUT` | `/api/scenes/{scene_id}` | Save scene data (full replacement) |
| `DELETE` | `/api/scenes/{scene_id}` | Delete a scene |

Asset fetch logic is in `src/lib/api.ts`, scene fetch logic in
`src/lib/scene-api.ts`. Both use typed request/response interfaces.
Errors throw `ApiError` with the HTTP status code.

## WebSocket

The root layout connects to `ws://host/ws/status`. The backend sends a
JSON heartbeat every 5 seconds:

```json
{ "type": "heartbeat", "timestamp": "2026-03-21T12:00:00+00:00" }
```

The `useWebSocket` hook (`src/lib/ws.ts`) reconnects automatically with
exponential backoff (1 s base, 30 s cap). The `StatusBar` component
shows a green/red dot for connection state and the last heartbeat time.

## Previews

### Texture preview

`TexturePreview` renders images on a `<canvas>` element with:

- **Channel isolation** — toggle buttons show RGB, R, G, B, or A
  channels. Non-RGB modes zero out other channels and force alpha to
  255 via `getImageData`/`putImageData`.
- **Zoom** — mouse wheel scales the canvas. Above 2x zoom the canvas
  switches to `image-rendering: pixelated` for crisp texel inspection.
- **Pan** — click-drag repositions the canvas within its container.
- **Imperative wheel handler** — React's `onWheel` registers a passive
  listener where `preventDefault()` is silently ignored. The component
  attaches the handler with `addEventListener("wheel", fn, { passive: false })`
  to suppress page scroll during zoom.

When a processed output exists, the detail page shows source and
processed previews side by side.

### Mesh preview

`MeshPreview` uses react-three-fiber with `useGLTF` and drei's
`OrbitControls`, `Bounds`, and `Center`:

- **Auto-framing** — `useBounds` fits the camera to the model's
  bounding box on load.
- **Wireframe toggle** — traverses the scene graph and sets
  `material.wireframe` on every mesh.
- **Companion file loading** — Three.js resolves relative URLs (`.bin`
  files, textures) against the loaded file's URL, which produces
  invalid paths like `/api/assets/models--hero/hero.bin`. The
  `useCompanionManager` hook creates a `THREE.LoadingManager` that
  intercepts these and rewrites them to
  `/api/assets/{asset_id}/companions?path=hero.bin`. The backend resolves the
  path relative to the asset's directory and validates it stays within
  the configured source tree.

### Material preview

`MaterialPreview` loads the same glTF scene and extracts textures from
`MeshStandardMaterial` instances. It checks five slots (base color,
normal, metallic-roughness, emissive, occlusion) and renders each as a
128x128 canvas tile. Textures loaded asynchronously by Three.js are
polled at 100 ms intervals until their `.image` property is populated.

### Atlas preview

`AtlasPreview` fetches `atlas.json` metadata and the `atlas.png` image,
then draws labeled colored rectangles for each material entry. Hover
detection maps mouse coordinates through the current zoom/offset
transform back to atlas pixel space and hit-tests against entry rects.
Hovered entries show pixel position, dimensions, and UV offset/scale.

## Import settings editor

`ImportSettings` implements a schema-driven form for per-asset
processing overrides:

1. **Schema** — the backend returns a `schema_fields` dict describing
   each setting's type, label, description, default, and constraints
   (min/max, options, group). The form generates controls from this
   schema: `Switch` for bools, `Select` for enums, `Input` for
   numbers and strings, comma-separated text input for `list[float]`.

2. **Three-layer merge** — settings resolve as: schema default →
   global `pipeline.toml` value → per-asset `.import.toml` override.
   The editor shows the effective value, marks overridden fields, and
   displays the global value as reference text.

3. **Workflow** — edit fields locally (tracked in React state), then
   Save (PUT) to write the `.import.toml` sidecar, Reset (DELETE) to
   remove it, or Process (POST) to re-run the plugin with current
   settings. All three operations use React Query mutations and
   invalidate relevant query caches on success.

4. **Grouping** — fields with a `group` property are visually grouped
   under a header with separators (e.g., "Basis Universal", "ASTC",
   "JPEG").

## Scene editor

The scene editor (`/scenes/$sceneId`) provides visual scene composition
with four panels:

- **Toolbar** — gizmo mode (translate/rotate/scale), add/delete object,
  undo/redo, save
- **Hierarchy panel** — tree view built from a flat object list with
  `parent_id` references. Click to select, chevron to expand/collapse.
- **Viewport** — react-three-fiber canvas with drei `TransformControls`
  on the selected object, `OrbitControls` for camera, and an infinite
  grid. Objects without an `asset_id` render as wireframe boxes.
- **Inspector panel** — name, position, rotation (Euler degrees converted
  to/from quaternions), scale, parent dropdown, visibility toggle.

### Undo/redo

State is managed by `useReducer` in `use-scene-store.ts` with
snapshot-based undo. Every undoable action (add, remove, transform,
rename, reparent, visibility) deep-clones the current scene and pushes
it to the undo stack. SELECT and SET_GIZMO_MODE do not create undo
entries. Keyboard shortcuts: Ctrl+Z (undo), Ctrl+Shift+Z (redo).

### TransformControls integration

Transforms are committed on mouse release, not every frame. During a
drag, drei's `TransformControls` manipulates the Three.js object
directly for smooth feedback. On release, `onMouseUp` reads the final
position/quaternion/scale and dispatches `UPDATE_TRANSFORM` — one undo
snapshot per drag operation. OrbitControls is disabled during gizmo
drags via the `dragging-changed` event.

## UI primitives

The `src/components/ui/` directory contains low-level components
following the [shadcn/ui](https://ui.shadcn.com/) pattern:

- Built on native HTML elements (no Radix or headless-ui dependency)
- Styled with Tailwind classes and
  [class-variance-authority](https://cva.style/) for variant props
- Composed with `cn()` (`clsx` + `tailwind-merge`) for class merging
- Each component is a single file with a `forwardRef` wrapper

Available primitives: Badge, Button, Card (with Header/Title/Content/
Footer), Input, Label, Select, Separator, Switch, Table (with Header/
Body/Row/Head/Cell).

To add a new primitive, create a file in `src/components/ui/`, export
the component, and use `cn()` for className merging.

## Project structure

```text
src/
├── main.tsx                        Entry point (React Query + Router providers)
├── globals.css                     Tailwind theme (dark, neutral palette)
├── routeTree.gen.ts                Auto-generated route tree (TanStack Router)
├── test-setup.ts                   Vitest setup (jest-dom matchers)
├── routes/
│   ├── __root.tsx                  Root layout (header, nav, status bar, WebSocket)
│   ├── index.tsx                   Asset browser page
│   ├── assets/$assetId.tsx         Asset detail page
│   └── scenes/
│       ├── index.tsx               Scene list page
│       └── $sceneId.tsx            Scene editor page
├── components/
│   ├── preview-panel.tsx           Routes asset type to the correct preview
│   ├── texture-preview.tsx         Canvas viewer — RGBA channels, zoom, pan
│   ├── mesh-preview.tsx            Three.js orbit viewer — wireframe toggle
│   ├── material-preview.tsx        Texture tile grid from glTF materials
│   ├── atlas-preview.tsx           Atlas canvas — labeled rects, hover info
│   ├── import-settings.tsx         Settings editor — save, reset, process
│   ├── status-bar.tsx              WebSocket connection indicator
│   ├── type-filter.tsx             Asset type button group
│   ├── preview-panel.test.tsx      Preview routing tests
│   ├── scene-editor/
│   │   ├── types.ts                Scene object and action type definitions
│   │   ├── use-scene-store.ts      useReducer state manager with undo/redo
│   │   ├── toolbar.tsx             Gizmo mode, add/delete, undo/redo, save
│   │   ├── hierarchy-panel.tsx     Tree view with selection and expand/collapse
│   │   ├── inspector-panel.tsx     Transform fields, parent dropdown, visibility
│   │   ├── viewport.tsx            R3F canvas with TransformControls and grid
│   │   └── __tests__/
│   │       └── use-scene-store.test.ts  Reducer unit tests (15 cases)
│   └── ui/                         Primitives (badge, button, card, input,
│       ...                           label, select, separator, switch, table)
└── lib/
    ├── api.ts                      Typed fetch wrappers for asset REST endpoints
    ├── scene-api.ts                Typed fetch wrappers for scene REST endpoints
    ├── ws.ts                       WebSocket hook with reconnect backoff
    ├── utils.ts                    cn() helper, formatBytes()
    └── companion-manager.ts        Three.js LoadingManager for glTF companions
```

## Tests

```bash
npm test
```

Runs Vitest with jsdom. Preview component tests mock the heavy renderers
(Three.js, canvas) and verify routing logic only — which preview
component renders for which asset type, side-by-side vs single layout,
and fallback messages for unsupported formats.
