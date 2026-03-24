"""FastAPI backend for the asset pipeline web UI.

Provides REST endpoints and a WebSocket for monitoring pipeline status,
browsing assets, and (eventually) triggering builds from the browser.
"""

from __future__ import annotations

import asyncio
import json
import logging
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Literal

from fastapi import (
    FastAPI,
    HTTPException,
    Query,
    Request,
    WebSocket,
    WebSocketDisconnect,
)
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

from pipeline.config import PipelineConfig
from pipeline.import_settings import (
    delete_sidecar,
    get_effective_settings,
    get_schema,
    load_sidecar,
    save_sidecar,
)
from pipeline.plugin import PluginRegistry
from pipeline.scanner import FingerprintCache, fingerprint_file
from pipeline.scenes import (
    create_scene,
    delete_scene,
    list_scenes,
    load_scene,
    save_scene,
)

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Asset type detection
# ---------------------------------------------------------------------------

TEXTURE_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr", ".exr"}
MESH_EXTENSIONS = {".gltf", ".glb", ".obj"}
ANIMATION_EXTENSIONS = {".fanim", ".fanims"}
SCENE_EXTENSIONS = {".fscene"}

# Primary pipeline output extensions (consumers load these, not sidecars)
PRIMARY_OUTPUT_EXTENSIONS = {".fmesh", ".ftex", ".fscene", ".fanim", ".fskin"}

# Map source asset types to their expected primary output extension(s)
EXPECTED_OUTPUT_EXTENSIONS: dict[str, set[str]] = {
    "texture": {".ftex"},
    "mesh": {".fmesh"},
    "scene": {".fscene"},
    "animation": {".fanim", ".fskin"},
}


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
    output_mtime: str | None


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
    output_mtime: str | None


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


class ImportSettingsResponse(BaseModel):
    """JSON shape for import settings endpoints."""

    effective: dict[str, Any]
    per_asset: dict[str, Any]
    global_settings: dict[str, Any]
    schema_fields: dict[str, dict[str, Any]]
    has_overrides: bool


class ProcessResponse(BaseModel):
    """JSON shape for the process endpoint."""

    message: str


# -- Scene editor models ----------------------------------------------------


class SceneObjectModel(BaseModel):
    """A single placed object in an authored scene."""

    id: str
    name: str
    asset_id: str | None
    position: list[float]
    rotation: list[float]
    scale: list[float]
    parent_id: str | None
    visible: bool


class SceneResponse(BaseModel):
    """Full scene data returned by get/create/save endpoints."""

    id: str
    version: int
    name: str
    created_at: str
    modified_at: str
    objects: list[SceneObjectModel]


class SceneListItem(BaseModel):
    """Summary of a scene for the list endpoint."""

    id: str
    name: str
    modified_at: str
    object_count: int


class SceneListResponse(BaseModel):
    """Response from GET /api/scenes."""

    scenes: list[SceneListItem]
    total: int


class CreateSceneRequest(BaseModel):
    """Request body for POST /api/scenes."""

    name: str


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
            # Check for any file with the same stem in the output subdirectory.
            # Prefer the expected output extension for this asset type so we
            # don't pick the wrong same-stem file (e.g. Duck.fmat instead of
            # Duck.fmesh for a mesh asset).
            output_subdir = config.output_dir / relative.parent
            if output_subdir.is_dir():
                stem = relative.stem
                type_exts = EXPECTED_OUTPUT_EXTENSIONS.get(asset_type, set())
                fallback: Path | None = None
                fallback_is_primary = False
                for candidate in output_subdir.iterdir():
                    if candidate.is_file() and candidate.stem == stem:
                        # Best match: extension expected for this asset type
                        if candidate.suffix in type_exts:
                            output_file = candidate
                            break
                        # Second best: any primary output extension
                        # (overwrites a non-primary fallback)
                        if (
                            candidate.suffix in PRIMARY_OUTPUT_EXTENSIONS
                            and not fallback_is_primary
                        ):
                            fallback = candidate
                            fallback_is_primary = True
                        # Last resort: any same-stem file (skip sidecars)
                        elif fallback is None and candidate.suffix not in {
                            ".fmat",
                            ".json",
                        }:
                            fallback = candidate
                if output_file is None and fallback is not None:
                    output_file = fallback

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
        output_stat = output_file.stat() if output_file else None
        output_size = output_stat.st_size if output_stat else None
        output_mtime = (
            datetime.fromtimestamp(output_stat.st_mtime, tz=timezone.utc).isoformat()
            if output_stat
            else None
        )

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
                output_mtime=output_mtime,
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

    # -- Plugin registry (created once, reused across requests) ------------
    _registry = PluginRegistry()
    plugins_dir = Path(__file__).resolve().parent / "plugins"
    _registry.discover(plugins_dir)

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

    # Status values ordered so unprocessed assets surface first (most
    # actionable).  Lower index = higher priority in the sort.
    _STATUS_ORDER = {"new": 0, "changed": 1, "missing": 2, "processed": 3}

    def _sort_assets(
        assets: list[AssetInfo],
        sort: str,
        order: str,
    ) -> list[AssetInfo]:
        """Sort *assets* in place by the requested field and direction."""
        reverse = order == "desc"
        if sort == "name":
            assets.sort(key=lambda a: a.name.lower(), reverse=reverse)
        elif sort == "size":
            assets.sort(key=lambda a: a.file_size, reverse=reverse)
        elif sort == "status":
            assets.sort(
                key=lambda a: _STATUS_ORDER.get(a.status, 99),
                reverse=reverse,
            )
        elif sort == "type":
            assets.sort(key=lambda a: a.asset_type, reverse=reverse)
        elif sort == "recent":
            # Sort by output file modification time.  Assets without an
            # output file (no mtime) sort last regardless of direction.
            # Partition into two groups so nulls always end up at the tail.
            with_mtime = [a for a in assets if a.output_mtime is not None]
            without_mtime = [a for a in assets if a.output_mtime is None]
            with_mtime.sort(key=lambda a: a.output_mtime, reverse=reverse)  # type: ignore[arg-type]
            assets[:] = with_mtime + without_mtime
        return assets

    @app.get("/api/assets", response_model=AssetListResponse)
    async def list_assets(
        type: str | None = Query(None, description="Filter by asset type"),
        status: str | None = Query(None, description="Filter by asset status"),
        search: str | None = Query(None, description="Search by filename"),
        sort: str | None = Query(
            None,
            description="Sort field: name, size, status, type, recent",
        ),
        order: str | None = Query(
            None,
            description="Sort direction: asc or desc",
        ),
        limit: int | None = Query(
            None,
            description="Maximum number of results to return",
            ge=1,
        ),
    ) -> AssetListResponse:
        """Return all assets, optionally filtered and sorted."""
        assets = list(_refresh_cache())

        if type is not None:
            assets = [a for a in assets if a.asset_type == type]
        if status is not None:
            assets = [a for a in assets if a.status == status]
        if search is not None:
            term = search.lower()
            assets = [a for a in assets if term in a.name.lower()]

        _VALID_SORT_FIELDS = {"name", "size", "status", "type", "recent"}
        _VALID_SORT_ORDERS = {"asc", "desc"}
        if order is not None and order not in _VALID_SORT_ORDERS:
            raise HTTPException(
                status_code=400,
                detail=f"Invalid sort order: '{order}'. Must be one of: asc, desc",
            )
        if sort is not None:
            if sort not in _VALID_SORT_FIELDS:
                raise HTTPException(
                    status_code=400,
                    detail=f"Invalid sort field: '{sort}'. Must be one of: {', '.join(sorted(_VALID_SORT_FIELDS))}",
                )
            _default_order = "desc" if sort in {"size", "recent"} else "asc"
            _sort_assets(assets, sort, order or _default_order)

        # Apply limit after filtering and sorting.  The total reflects the
        # number of assets that matched the filters, not the capped count.
        total = len(assets)
        if limit is not None:
            assets = assets[:limit]

        responses = [AssetResponse(**a.__dict__) for a in assets]
        return AssetListResponse(assets=responses, total=total)

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
        ".json": "application/json",
        ".fmesh": "application/octet-stream",
        ".fmat": "application/json",
        ".ftex": "application/octet-stream",
        ".fscene": "application/octet-stream",
        ".fanim": "application/octet-stream",
        ".fskin": "application/octet-stream",
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

    # -- Thumbnail generation -----------------------------------------------

    # Extensions we can generate thumbnails for via Pillow
    _THUMBNAIL_EXTENSIONS = {".png", ".jpg", ".jpeg", ".bmp", ".tga"}

    THUMBNAIL_SIZE = (128, 128)

    def _thumbnail_cache_dir() -> Path:
        """Return the .thumbnails/ directory under the output dir."""
        d = config.output_dir / ".thumbnails"
        d.mkdir(parents=True, exist_ok=True)
        return d

    def _generate_thumbnail(source: Path, asset_id: str) -> Path | None:
        """Resize *source* to a 128x128 thumbnail and cache on disk.

        Returns the cached thumbnail path, or None if the source cannot
        be opened by Pillow.
        """
        import os

        from PIL import Image as PILImage  # noqa: N811

        cache_path = _thumbnail_cache_dir() / f"{asset_id}.png"

        try:
            # Open the source and read its mtime from the fd to avoid a
            # TOCTOU race between stat() and the later open().
            fd = os.open(str(source), os.O_RDONLY)
            try:
                source_mtime = os.fstat(fd).st_mtime
            finally:
                os.close(fd)

            # Serve cached version if the thumbnail is newer than the source
            if cache_path.is_file() and cache_path.stat().st_mtime >= source_mtime:
                return cache_path

            with PILImage.open(source) as img:
                img = img.convert("RGBA")
                img.thumbnail(THUMBNAIL_SIZE, PILImage.Resampling.LANCZOS)
                img.save(cache_path, format="PNG")
            return cache_path
        except Exception:
            log.warning("Could not generate thumbnail for %s", source, exc_info=True)
            return None

    @app.get("/api/assets/{asset_id}/thumbnail")
    async def get_asset_thumbnail(asset_id: str) -> FileResponse:
        """Return a small 128x128 thumbnail for an asset.

        For textures with a supported extension, the source image is
        resized on the fly and cached under ``{output_dir}/.thumbnails/``.
        For other asset types (meshes, animations, scenes), returns 404
        — the frontend falls back to a colored icon.
        """
        asset = _get_cached_asset(asset_id)
        if asset is None:
            raise HTTPException(status_code=404, detail="Asset not found")

        source = Path(asset.source_path)

        # Only generate thumbnails for image formats Pillow can handle
        if source.suffix.lower() not in _THUMBNAIL_EXTENSIONS:
            raise HTTPException(
                status_code=404,
                detail=f"No thumbnail available for {asset.asset_type} assets",
            )

        thumb = _generate_thumbnail(source, asset_id)
        if thumb is None:
            raise HTTPException(status_code=500, detail="Failed to generate thumbnail")

        return FileResponse(
            path=str(thumb),
            media_type="image/png",
            filename=f"{asset_id}_thumb.png",
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

    # -- Import settings ---------------------------------------------------

    def _build_settings_response(
        asset: AssetInfo, schema_fields: dict[str, dict], per_asset: dict
    ) -> ImportSettingsResponse:
        """Build the standard import-settings response shape."""
        global_settings = config.plugin_settings.get(asset.asset_type, {})
        effective = get_effective_settings(asset.asset_type, global_settings, per_asset)
        return ImportSettingsResponse(
            schema_fields=schema_fields,
            global_settings=global_settings,
            per_asset=per_asset,
            effective=effective,
            has_overrides=bool(per_asset),
        )

    def _validate_source_path(source: Path) -> None:
        """Raise 403 if *source* is outside the configured source directory."""
        if not source.resolve().is_relative_to(config.source_dir.resolve()):
            raise HTTPException(status_code=403, detail="Path traversal not allowed")

    def _load_sidecar_or_400(source: Path) -> dict[str, Any]:
        """Load the sidecar, returning 400 on malformed TOML instead of 500."""
        try:
            return load_sidecar(source)
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc

    @app.get(
        "/api/assets/{asset_id}/settings",
        response_model=ImportSettingsResponse,
    )
    async def get_asset_settings(asset_id: str) -> ImportSettingsResponse:
        """Return import settings (schema, global, per-asset, effective)."""
        asset = _get_cached_asset(asset_id)
        if asset is None:
            raise HTTPException(status_code=404, detail="Asset not found")

        source = Path(asset.source_path)
        _validate_source_path(source)

        schema_fields = get_schema(asset.asset_type)
        if schema_fields is None:
            raise HTTPException(
                status_code=400,
                detail=f"No settings schema for asset type '{asset.asset_type}'",
            )

        per_asset = _load_sidecar_or_400(source)
        return _build_settings_response(asset, schema_fields, per_asset)

    @app.put(
        "/api/assets/{asset_id}/settings",
        response_model=ImportSettingsResponse,
    )
    async def put_asset_settings(
        asset_id: str, request: Request
    ) -> ImportSettingsResponse:
        """Save per-asset import setting overrides.

        The frontend sends the overrides dict directly as the JSON body.
        Only keys present in the plugin's settings schema are accepted;
        unknown keys are rejected with 400.
        """
        overrides = await request.json()

        if not isinstance(overrides, dict):
            raise HTTPException(
                status_code=400, detail="Request body must be a JSON object"
            )

        asset = _get_cached_asset(asset_id)
        if asset is None:
            raise HTTPException(status_code=404, detail="Asset not found")

        source = Path(asset.source_path)
        _validate_source_path(source)

        schema_fields = get_schema(asset.asset_type)
        if schema_fields is None:
            raise HTTPException(
                status_code=400,
                detail=f"No settings schema for asset type '{asset.asset_type}'",
            )

        # Reject keys not in the schema
        unknown = set(overrides.keys()) - set(schema_fields.keys())
        if unknown:
            raise HTTPException(
                status_code=400,
                detail=f"Unknown setting(s): {', '.join(sorted(unknown))}",
            )

        try:
            save_sidecar(source, overrides)
        except TypeError as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc

        per_asset = _load_sidecar_or_400(source)
        return _build_settings_response(asset, schema_fields, per_asset)

    @app.delete(
        "/api/assets/{asset_id}/settings",
        response_model=ImportSettingsResponse,
    )
    async def delete_asset_settings(asset_id: str) -> ImportSettingsResponse:
        """Delete per-asset overrides, reverting to global defaults."""
        asset = _get_cached_asset(asset_id)
        if asset is None:
            raise HTTPException(status_code=404, detail="Asset not found")

        source = Path(asset.source_path)
        _validate_source_path(source)

        # Validate schema support before any side effects
        schema_fields = get_schema(asset.asset_type)
        if schema_fields is None:
            raise HTTPException(
                status_code=400,
                detail=f"No settings schema for asset type '{asset.asset_type}'",
            )

        delete_sidecar(source)

        return _build_settings_response(asset, schema_fields, {})

    @app.post(
        "/api/assets/{asset_id}/process",
        response_model=ProcessResponse,
    )
    async def process_asset(asset_id: str) -> ProcessResponse:
        """Process a single asset with its effective import settings."""
        asset = _get_cached_asset(asset_id)
        if asset is None:
            raise HTTPException(status_code=404, detail="Asset not found")

        source = Path(asset.source_path)
        _validate_source_path(source)

        # Compute effective settings
        global_settings = config.plugin_settings.get(asset.asset_type, {})
        per_asset = _load_sidecar_or_400(source)
        effective = get_effective_settings(asset.asset_type, global_settings, per_asset)

        # Find the plugin that matches both the file extension and the
        # asset type. Extensions like .gltf can have multiple plugins
        # (mesh, animation, scene), so we match by plugin name.
        ext = source.suffix.lower()
        candidates = _registry.get_by_extension(ext)
        plugin = None
        for p in candidates:
            if p.name == asset.asset_type:
                plugin = p
                break
        if plugin is None and candidates:
            log.debug(
                "No %r plugin for '%s'; falling back to %r",
                asset.asset_type,
                ext,
                candidates[0].name,
            )
            plugin = candidates[0]
        if plugin is None:
            raise HTTPException(
                status_code=400,
                detail=f"No plugin found for extension '{ext}'",
            )

        # Build the output directory, mirroring the source tree structure
        relative = Path(asset.relative_path)
        output_subdir = config.output_dir / relative.parent
        output_subdir.mkdir(parents=True, exist_ok=True)

        # Run plugin off the event loop to avoid blocking async handlers
        try:
            result = await asyncio.to_thread(
                plugin.process, source, output_subdir, effective
            )
        except Exception as exc:
            log.exception("Plugin %r failed processing %s", plugin.name, source.name)
            raise HTTPException(
                status_code=500,
                detail=f"Processing failed: {exc}",
            ) from exc

        # If the plugin reports the asset was not actually processed (e.g.
        # the required tool binary is not installed), do not update the
        # fingerprint cache — the asset is not in a processed state.
        if result.metadata.get("processed") is False:
            reason = result.metadata.get("reason", "unknown")
            return ProcessResponse(message=f"Skipped {asset.name}: {reason}")

        # Update fingerprint cache and refresh the in-memory asset index.
        # Run off the event loop — fingerprinting hashes file contents and
        # _refresh_cache re-scans the entire source tree.
        def _update_cache() -> None:
            cache_path = config.cache_dir / "fingerprints.json"
            fp_cache = FingerprintCache(cache_path)
            fp_cache.set(relative, fingerprint_file(source))
            fp_cache.save()
            _refresh_cache()

        await asyncio.to_thread(_update_cache)

        return ProcessResponse(message=f"Processed {asset.name}")

    # -- Atlas metadata ----------------------------------------------------

    @app.get("/api/atlas")
    async def get_atlas_metadata() -> dict:
        """Return atlas metadata if an atlas has been built."""
        atlas_meta_path = config.output_dir / "atlas.json"
        if not atlas_meta_path.is_file():
            raise HTTPException(status_code=404, detail="No atlas found")

        try:
            data = json.loads(atlas_meta_path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError) as exc:
            log.exception("Failed to read atlas metadata from %s", atlas_meta_path)
            raise HTTPException(
                status_code=500, detail="Failed to read atlas metadata"
            ) from exc

        return data

    @app.get("/api/atlas/image")
    async def get_atlas_image() -> FileResponse:
        """Serve the atlas PNG image."""
        atlas_path = config.output_dir / "atlas.png"
        if not atlas_path.is_file():
            raise HTTPException(status_code=404, detail="No atlas image found")

        return FileResponse(
            path=str(atlas_path),
            media_type="image/png",
            filename="atlas.png",
        )

    # -- Scene editor ------------------------------------------------------

    @app.get("/api/scenes")
    async def get_scenes() -> SceneListResponse:
        """List all authored scenes."""
        items = list_scenes(config)
        return SceneListResponse(
            scenes=[SceneListItem(**s) for s in items],
            total=len(items),
        )

    @app.post("/api/scenes", status_code=201)
    async def post_scene(body: CreateSceneRequest) -> SceneResponse:
        """Create a new empty scene."""
        data = create_scene(config, body.name)
        return SceneResponse(**data)

    @app.get("/api/scenes/{scene_id}")
    async def get_scene(scene_id: str) -> SceneResponse:
        """Get a single scene by ID."""
        try:
            data = load_scene(config, scene_id)
        except FileNotFoundError as exc:
            raise HTTPException(status_code=404, detail="Scene not found") from exc
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return SceneResponse(**data)

    @app.put("/api/scenes/{scene_id}")
    async def put_scene(scene_id: str, request: Request) -> SceneResponse:
        """Save (overwrite) scene data."""
        try:
            body = await request.json()
        except json.JSONDecodeError as exc:
            raise HTTPException(
                status_code=400, detail="Request body must be valid JSON"
            ) from exc
        if not isinstance(body, dict):
            raise HTTPException(
                status_code=400, detail="Request body must be a JSON object"
            )
        try:
            data = save_scene(config, scene_id, body)
        except FileNotFoundError as exc:
            raise HTTPException(status_code=404, detail="Scene not found") from exc
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return SceneResponse(**data)

    @app.delete("/api/scenes/{scene_id}")
    async def delete_scene_endpoint(scene_id: str) -> dict:
        """Delete a scene."""
        try:
            delete_scene(config, scene_id)
        except FileNotFoundError as exc:
            raise HTTPException(status_code=404, detail="Scene not found") from exc
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc
        return {"message": "Deleted"}

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
