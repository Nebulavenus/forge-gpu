# Asset Lesson 14 — Web UI Scaffold — Implementation Plan

## Overview

Add a web-based asset browser to the forge pipeline. FastAPI backend serves
REST endpoints and WebSocket for build status. Vite + React + TypeScript
frontend with shadcn/ui and Tailwind for the UI. TanStack Router for
navigation, TanStack Query for data fetching.

This is a **Type A (Python + web)** lesson. Code lives in `pipeline/` (backend)
and `pipeline/web/` (frontend). The lesson directory contains only the README
and diagrams.

## What the user gets

```bash
python -m pipeline serve
```

Opens a browser at `http://localhost:8000` showing:

- Asset grid/table with thumbnails, type badges, file sizes, status
- Filter by asset type (texture, mesh, animation, scene)
- Search by name
- Status bar showing real-time pipeline activity via WebSocket

## New files

### Backend (Python — in `pipeline/`)

```text
pipeline/
  server.py          # FastAPI app, REST endpoints, WebSocket, static serving
```

**Endpoints:**

- `GET /api/assets` — list processed assets with metadata (type, size,
  format, source path, output path, fingerprint, thumbnail URL)
- `GET /api/assets/{id}` — single asset detail
- `GET /api/status` — pipeline status (last run, asset counts by type)
- `WS  /ws/status` — push build progress events (scan start, processing,
  complete, error)
- `GET /` — serve the built React frontend (static files)

**Data source:** Reads from the existing fingerprint cache
(`.forge-cache/fingerprints.json`) and scans `assets/processed/` for output
files. No database — the pipeline's existing cache is the source of truth.

### Frontend (React + TypeScript — in `pipeline/web/`)

```text
pipeline/web/
  package.json
  vite.config.ts
  tsconfig.json
  tsconfig.app.json
  tsconfig.node.json
  tailwind.config.ts
  postcss.config.js
  components.json           # shadcn/ui config
  index.html
  src/
    main.tsx                 # React entry point
    app.tsx                  # TanStack Router root
    routeTree.gen.ts         # auto-generated route tree
    globals.css              # Tailwind base styles
    lib/
      utils.ts               # shadcn/ui cn() helper
      api.ts                 # fetch wrappers for /api/* endpoints
      ws.ts                  # WebSocket hook for /ws/status
    routes/
      __root.tsx             # root layout (nav bar, status bar)
      index.tsx              # asset browser (grid view)
      assets/
        $assetId.tsx         # asset detail page
    components/
      ui/                    # shadcn/ui components (table, card, badge, input)
      asset-card.tsx         # asset thumbnail card
      asset-table.tsx        # asset list table
      status-bar.tsx         # WebSocket build status indicator
      type-filter.tsx        # asset type filter buttons
```

### Tests (in `tests/pipeline/`)

```text
tests/pipeline/
  test_server.py     # FastAPI endpoint tests (httpx TestClient)
```

### Lesson directory

```text
lessons/assets/14-web-ui-scaffold/
  PLAN.md            # this file
  README.md          # lesson walkthrough
```

## Dependencies

### Python (add to pyproject.toml)

```toml
dependencies = [
    # ... existing ...
    "fastapi>=0.100.0",
    "uvicorn[standard]>=0.20.0",
    "websockets>=11.0",
]
```

### Node (pipeline/web/package.json)

```json
{
  "dependencies": {
    "react": "^19",
    "react-dom": "^19",
    "@tanstack/react-router": "^1",
    "@tanstack/react-query": "^5"
  },
  "devDependencies": {
    "@vitejs/plugin-react": "^4",
    "vite": "^6",
    "typescript": "^5",
    "tailwindcss": "^4",
    "@tailwindcss/vite": "^4",
    "@tanstack/react-router-devtools": "^1",
    "@tanstack/router-plugin": "^1"
  }
}
```

shadcn/ui components are copy-pasted (not an npm dependency) — added via
`npx shadcn@latest add table card badge input`.

## Implementation phases

### Phase 1 — Backend (`pipeline/server.py`)

Write the FastAPI application:

1. Asset listing endpoint — scan processed directory, read cache, return JSON
2. Asset detail endpoint — single asset with full metadata
3. Status endpoint — pipeline summary stats
4. WebSocket endpoint — broadcast build events
5. Static file serving — serve `pipeline/web/dist/` at root
6. `serve` subcommand in `pipeline/__main__.py`

### Phase 2 — Frontend scaffold (`pipeline/web/`)

Create the Vite + React + TypeScript project structure:

1. Config files (vite, tsconfig, tailwind, postcss, shadcn)
2. Entry point and root layout
3. TanStack Router setup with file-based routing

### Phase 3 — Frontend components

Build the asset browser UI:

1. API fetch layer (`lib/api.ts`) and WebSocket hook (`lib/ws.ts`)
2. Asset grid with cards showing thumbnails, type, size
3. Type filter and search
4. Status bar showing WebSocket build events
5. Asset detail page

### Phase 4 — Tests

1. Backend tests with httpx TestClient
2. Test asset listing, detail, status endpoints
3. Test WebSocket connection and message format

### Phase 5 — README

Write the lesson walkthrough covering:

1. Why a web UI for asset pipelines
2. FastAPI backend architecture
3. Vite + React + TypeScript setup
4. TanStack Router and Query patterns
5. WebSocket for real-time updates
6. OpenAPI type generation

### Phase 6 — Project file updates

1. `pyproject.toml` — add FastAPI/uvicorn dependencies
2. `lessons/assets/README.md` — add lesson 14 row
3. `PLAN.md` — check off lesson 14
4. Root `README.md` — add lesson row if needed

## Cross-references

- **Asset Lesson 01** (Pipeline Scaffold) — CLI architecture this extends
- **Asset Lesson 02** (Texture Processing) — textures shown in the browser
- **Asset Lesson 03** (Mesh Processing) — meshes shown in the browser
- **Asset Lesson 15** (Asset Preview) — next lesson, adds 3D preview

## README structure

1. What you'll learn
2. Result — screenshot of the asset browser
3. Architecture diagram (FastAPI ↔ React ↔ WebSocket)
4. Backend walkthrough (FastAPI endpoints, data model)
5. Frontend walkthrough (Vite setup, TanStack, shadcn/ui)
6. WebSocket integration
7. Exercises
8. Further reading
