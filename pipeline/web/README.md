# Pipeline Web Frontend

A React + TypeScript single-page application for browsing and previewing
pipeline assets. Built with Vite, styled with Tailwind CSS, and uses
react-three-fiber for 3D mesh previews.

## Development

```bash
npm install   # first time only
npm run dev   # dev server at http://localhost:5173 (proxies API to backend)
npm run build # production build (served by FastAPI in production)
```

The backend must be running for API calls to work:

```bash
# From the repository root
uv run python -m pipeline serve
```

## Stack

- **Vite** — build tool and dev server
- **React 19** + **TypeScript** — UI framework
- **Tailwind CSS 4** — utility-first styling
- **TanStack Router** — file-based routing
- **TanStack React Query** — data fetching and caching
- **react-three-fiber** + **drei** — 3D asset previews
- **Vitest** + **Testing Library** — unit tests

## Files

| Path | Purpose |
|------|---------|
| `src/main.tsx` | Application entry point |
| `src/globals.css` | Global Tailwind styles |
| `index.html` | HTML shell |
| `vite.config.ts` | Vite configuration (React plugin, API proxy) |
| `package.json` | Dependencies and scripts |
