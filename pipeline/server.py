"""FastAPI backend for the asset pipeline web UI.

Provides REST endpoints and a WebSocket for monitoring pipeline status,
browsing assets, and (eventually) triggering builds from the browser.
"""

from __future__ import annotations

import asyncio
import logging
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Literal

from fastapi import FastAPI, HTTPException, Query, WebSocket, WebSocketDisconnect
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from pipeline.config import PipelineConfig
from pipeline.scanner import FingerprintCache, fingerprint_file

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Asset type detection
# ---------------------------------------------------------------------------

TEXTURE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr", ".exr"}
MESH_EXTENSIONS = {".gltf", ".glb", ".obj"}
ANIMATION_EXTENSIONS = {".fanim", ".fanims"}
SCENE_EXTENSIONS = {".fscene"}


def _classify_extension(ext: str) -> str:
    """Return the asset type string for a file extension (with leading dot)."""
    ext = ext.lower()
    if ext in TEXTURE_EXTENSIONS:
        return "texture"
    if ext in MESH_EXTENSIONS:
        return "mesh"
    if ext in ANIMATION_EXTENSIONS:
        return "animation"
    if ext in SCENE_EXTENSIONS:
        return "scene"
    return "unknown"


def _make_asset_id(relative: Path) -> str:
    """Derive a URL-safe ID from a relative path.

    Replaces ``/`` with ``--`` and strips the extension, so
    ``textures/brick_albedo.png`` becomes ``textures--brick_albedo``.
    """
    return relative.with_suffix("").as_posix().replace("/", "--")


# ---------------------------------------------------------------------------
# Internal data model
# ---------------------------------------------------------------------------


@dataclass
class AssetInfo:
    """Internal representation of a single asset file."""

    id: str
    name: str
    relative_path: str
    asset_type: str
    source_path: str
    output_path: str | None
    fingerprint: str
    file_size: int
    output_size: int | None
    status: str


# ---------------------------------------------------------------------------
# Pydantic response models
# ---------------------------------------------------------------------------


class AssetResponse(BaseModel):
    """JSON shape for a single asset."""

    id: str
    name: str
    relative_path: str
    asset_type: str
    source_path: str
    output_path: str | None
    fingerprint: str
    file_size: int
    output_size: int | None
    status: str


class AssetListResponse(BaseModel):
    """JSON shape for the asset list endpoint."""

    assets: list[AssetResponse]
    total: int


class StatusResponse(BaseModel):
    """JSON shape for the pipeline status endpoint."""

    total: int
    by_type: dict[str, int]
    by_status: dict[str, int]
    source_dir: str
    output_dir: str


# ---------------------------------------------------------------------------
# Scanning
# ---------------------------------------------------------------------------


def scan_assets(config: PipelineConfig) -> list[AssetInfo]:
    """Walk the source directory and build an AssetInfo for every file.

    Uses the fingerprint cache to determine each file's processing status
    and checks for corresponding output files.
    """
    cache_path = config.cache_dir / "fingerprints.json"
    cache = FingerprintCache(cache_path)

    results: list[AssetInfo] = []

    if not config.source_dir.is_dir():
        log.warning("Source directory does not exist: %s", config.source_dir)
        return results

    for path in sorted(config.source_dir.rglob("*")):
        if not path.is_file():
            continue

        relative = path.relative_to(config.source_dir)
        ext = path.suffix.lower()
        asset_type = _classify_extension(ext)
        fp = fingerprint_file(path)

        # Determine output path — mirror the source tree under output_dir
        # with the same name (plugins may change the extension, but we check
        # for an exact mirror first, then fall back to stem-based glob).
        output_file: Path | None = None
        output_candidate = config.output_dir / relative
        if output_candidate.is_file():
            output_file = output_candidate
        else:
            # Check for any file with the same stem in the output subdirectory
            output_subdir = config.output_dir / relative.parent
            if output_subdir.is_dir():
                stem = relative.stem
                for candidate in output_subdir.iterdir():
                    if candidate.is_file() and candidate.stem == stem:
                        output_file = candidate
                        break

        # Determine status
        cached_fp = cache.get(relative)
        if cached_fp is None:
            status = "new"
        elif cached_fp != fp:
            status = "changed"
        elif output_file is not None:
            status = "processed"
        else:
            status = "missing"

        file_size = path.stat().st_size
        output_size = output_file.stat().st_size if output_file else None

        results.append(
            AssetInfo(
                id=_make_asset_id(relative),
                name=path.name,
                relative_path=relative.as_posix(),
                asset_type=asset_type,
                source_path=str(path),
                output_path=str(output_file) if output_file else None,
                fingerprint=fp,
                file_size=file_size,
                output_size=output_size,
                status=status,
            )
        )

    log.info("Scanned %d asset(s) from %s", len(results), config.source_dir)
    return results


# ---------------------------------------------------------------------------
# WebSocket manager
# ---------------------------------------------------------------------------


class ConnectionManager:
    """Track active WebSocket connections and broadcast messages."""

    def __init__(self) -> None:
        self._connections: set[WebSocket] = set()

    async def connect(self, ws: WebSocket) -> None:
        """Accept and register a WebSocket connection."""
        await ws.accept()
        self._connections.add(ws)
        log.debug("WebSocket connected (%d total)", len(self._connections))

    def disconnect(self, ws: WebSocket) -> None:
        """Remove a WebSocket connection."""
        self._connections.discard(ws)
        log.debug("WebSocket disconnected (%d remaining)", len(self._connections))

    async def broadcast(self, message: dict) -> None:
        """Send a JSON message to all connected clients."""
        stale: list[WebSocket] = []
        for ws in self._connections:
            try:
                await ws.send_json(message)
            except Exception:
                stale.append(ws)
        for ws in stale:
            self._connections.discard(ws)


# ---------------------------------------------------------------------------
# Application factory
# ---------------------------------------------------------------------------

HEARTBEAT_INTERVAL_SECONDS = 5


def create_app(config: PipelineConfig) -> FastAPI:
    """Build and return the FastAPI application.

    The *config* is captured by closure and used for all asset scanning.
    """
    app = FastAPI(title="Forge Asset Pipeline", docs_url="/api/docs")
    manager = ConnectionManager()

    # -- Cached asset index ------------------------------------------------
    # Avoid re-scanning the entire source tree on every single-asset lookup.
    # The cache is populated on the first call and refreshed by list/status
    # endpoints (which are the natural "show me everything" operations).

    _asset_cache: dict[str, AssetInfo] = {}

    def _refresh_cache() -> list[AssetInfo]:
        """Re-scan assets and update the id->AssetInfo cache."""
        nonlocal _asset_cache
        assets = scan_assets(config)
        _asset_cache = {a.id: a for a in assets}
        return assets

    def _get_cached_asset(asset_id: str) -> AssetInfo | None:
        """Look up a single asset by ID, scanning once if the cache is empty."""
        if not _asset_cache:
            _refresh_cache()
        return _asset_cache.get(asset_id)

    # -- REST endpoints ----------------------------------------------------

    @app.get("/api/assets", response_model=AssetListResponse)
    async def list_assets(
        type: str | None = Query(None, description="Filter by asset type"),
        search: str | None = Query(None, description="Search by filename"),
    ) -> AssetListResponse:
        """Return all assets, optionally filtered by type or name."""
        assets = list(_refresh_cache())

        if type is not None:
            assets = [a for a in assets if a.asset_type == type]
        if search is not None:
            term = search.lower()
            assets = [a for a in assets if term in a.name.lower()]

        responses = [AssetResponse(**a.__dict__) for a in assets]
        return AssetListResponse(assets=responses, total=len(responses))

    @app.get("/api/assets/{asset_id}", response_model=AssetResponse)
    async def get_asset(asset_id: str) -> AssetResponse:
        """Return a single asset by its ID."""
        asset = _get_cached_asset(asset_id)
        if asset is None:
            raise HTTPException(status_code=404, detail="Asset not found")
        return AssetResponse(**asset.__dict__)

    @app.get("/api/status", response_model=StatusResponse)
    async def get_status() -> StatusResponse:
        """Return a summary of the pipeline state."""
        assets = _refresh_cache()

        by_type: dict[str, int] = {}
        by_status: dict[str, int] = {}
        for a in assets:
            by_type[a.asset_type] = by_type.get(a.asset_type, 0) + 1
            by_status[a.status] = by_status.get(a.status, 0) + 1

        return StatusResponse(
            total=len(assets),
            by_type=by_type,
            by_status=by_status,
            source_dir=str(config.source_dir),
            output_dir=str(config.output_dir),
        )

    # -- File serving ------------------------------------------------------

    _MEDIA_TYPES: dict[str, str] = {
        ".png": "image/png",
        ".jpg": "image/jpeg",
        ".jpeg": "image/jpeg",
        ".bmp": "image/bmp",
        ".tga": "image/x-tga",
        ".hdr": "image/vnd.radiance",
        ".exr": "image/x-exr",
        ".gltf": "model/gltf+json",
        ".glb": "model/gltf-binary",
        ".obj": "text/plain",
        ".bin": "application/octet-stream",
    }

    def _media_type_for(path: Path) -> str:
        """Return the media type for a file based on its extension."""
        return _MEDIA_TYPES.get(path.suffix.lower(), "application/octet-stream")

    @app.get("/api/assets/{asset_id}/file")
    async def get_asset_file(
        asset_id: str,
        variant: Literal["source", "processed"] = Query(
            "source", description="'source' or 'processed'"
        ),
    ) -> FileResponse:
        """Serve the raw source or processed file for an asset.

        Used by the frontend to load textures for preview and glTF files
        for 3D rendering.
        """
        asset = _get_cached_asset(asset_id)
        if asset is None:
            raise HTTPException(status_code=404, detail="Asset not found")

        if variant == "processed":
            if asset.output_path is None:
                raise HTTPException(
                    status_code=404, detail="No processed output for this asset"
                )
            file_path = Path(asset.output_path).resolve()
            root_dir = config.output_dir.resolve()
        else:
            file_path = Path(asset.source_path).resolve()
            root_dir = config.source_dir.resolve()

        if not file_path.is_relative_to(root_dir):
            raise HTTPException(status_code=403, detail="Path traversal not allowed")

        if not file_path.is_file():
            raise HTTPException(status_code=404, detail="File not found on disk")

        return FileResponse(
            path=str(file_path),
            media_type=_media_type_for(file_path),
            filename=file_path.name,
        )

    @app.get("/api/assets/{asset_id}/companions")
    async def get_asset_companion(
        asset_id: str,
        path: str = Query(
            ..., description="Relative filename within the asset directory"
        ),
        variant: Literal["source", "processed"] = Query(
            "source", description="'source' or 'processed'"
        ),
    ) -> FileResponse:
        """Serve a companion file from the same directory as the asset.

        For glTF models the browser needs to load companion files (.bin,
        textures) relative to the .gltf file.  The *path* parameter is
        resolved relative to the asset's directory and must stay within
        the configured source (or output) directory tree.
        """
        asset = _get_cached_asset(asset_id)
        if asset is None:
            raise HTTPException(status_code=404, detail="Asset not found")

        if variant == "processed":
            if asset.output_path is None:
                raise HTTPException(
                    status_code=404, detail="No processed output for this asset"
                )
            base_dir = Path(asset.output_path).parent
            root_dir = config.output_dir.resolve()
        else:
            base_dir = Path(asset.source_path).parent
            root_dir = config.source_dir.resolve()

        resolved = (base_dir / path).resolve()

        # Security: ensure the resolved path stays within the root directory.
        # Use is_relative_to() instead of string prefix to prevent sibling
        # directory bypass (e.g. /source_evil matching /source).
        if not resolved.is_relative_to(root_dir):
            raise HTTPException(status_code=403, detail="Path traversal not allowed")

        if not resolved.is_file():
            raise HTTPException(status_code=404, detail="Companion file not found")

        return FileResponse(
            path=str(resolved),
            media_type=_media_type_for(resolved),
            filename=resolved.name,
        )

    # -- WebSocket ---------------------------------------------------------

    @app.websocket("/ws/status")
    async def ws_status(ws: WebSocket) -> None:
        """Stream pipeline status events to connected clients.

        Currently sends a heartbeat every few seconds. Real build events
        will be added when the pipeline gains async processing.
        """
        await manager.connect(ws)
        try:
            while True:
                await asyncio.sleep(HEARTBEAT_INTERVAL_SECONDS)
                ts = datetime.now(tz=timezone.utc).isoformat()
                await ws.send_json({"type": "heartbeat", "timestamp": ts})
        except WebSocketDisconnect:
            pass
        finally:
            manager.disconnect(ws)

    # -- Static files (production build) -----------------------------------

    dist_dir = Path(__file__).resolve().parent / "web" / "dist"
    if dist_dir.is_dir():
        app.mount("/", StaticFiles(directory=str(dist_dir), html=True), name="static")
        log.info("Serving frontend from %s", dist_dir)
    else:
        log.info("No frontend build at %s — API-only mode", dist_dir)

    return app
