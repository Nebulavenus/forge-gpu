"""Scene file management for authored scene compositions.

Scenes are JSON files stored in ``{output_dir}/scenes/``.  Each scene
describes a set of placed objects referencing pipeline assets, with
transforms and a parent-child hierarchy.

This is the **authored** format — users compose scenes visually in the web
editor.  It is distinct from the binary ``.fscene`` format (Asset Lesson 09)
used for processed glTF hierarchies.
"""

from __future__ import annotations

import json
import logging
import math
import re
import uuid
from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path

from pipeline.config import PipelineConfig

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

SCENE_VERSION = 1

# ---------------------------------------------------------------------------
# Data model
# ---------------------------------------------------------------------------


@dataclass
class SceneObject:
    """A single placed object in an authored scene."""

    id: str
    name: str
    asset_id: str | None
    position: list[float]  # [x, y, z]
    rotation: list[float]  # [x, y, z, w] quaternion
    scale: list[float]  # [x, y, z]
    parent_id: str | None
    visible: bool


@dataclass
class CameraBookmark:
    """A saved camera position/target pair for quick navigation."""

    id: str
    name: str
    position: list[float]  # [x, y, z]
    target: list[float]  # [x, y, z]


@dataclass
class SceneData:
    """Top-level scene document."""

    version: int
    name: str
    created_at: str  # ISO 8601
    modified_at: str  # ISO 8601
    objects: list[SceneObject] = field(default_factory=list)
    cameras: list[CameraBookmark] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def scenes_dir(config: PipelineConfig) -> Path:
    """Return the scenes directory, creating it if it does not exist."""
    d = config.output_dir / "scenes"
    d.mkdir(parents=True, exist_ok=True)
    return d


_SCENE_ID_PATTERN = re.compile(r"^[a-zA-Z0-9_-]+$")


def _validate_scene_id(scene_id: str) -> None:
    """Raise ``ValueError`` if *scene_id* contains unsafe characters.

    IDs must be non-empty and consist only of alphanumerics, hyphens, and
    underscores.  This prevents path traversal and filesystem issues with
    null bytes or special characters.
    """
    if not scene_id:
        msg = "Scene ID must not be empty"
        raise ValueError(msg)
    if not _SCENE_ID_PATTERN.match(scene_id):
        msg = f"Invalid scene ID: {scene_id!r}"
        raise ValueError(msg)


def _now_iso() -> str:
    return datetime.now(tz=timezone.utc).isoformat()


# ---------------------------------------------------------------------------
# Validation
# ---------------------------------------------------------------------------


def _is_finite_number(v: object) -> bool:
    """Return True for finite int/float values, rejecting bools."""
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)


def validate_scene(data: object) -> list[str]:
    """Validate a scene data dict.  Returns a list of error messages."""
    if not isinstance(data, dict):
        return ["Scene document must be a JSON object"]

    errors: list[str] = []

    # Top-level fields
    version = data.get("version")
    if version != SCENE_VERSION:
        errors.append(f"Expected version {SCENE_VERSION}, got {version!r}")

    name = data.get("name")
    if not isinstance(name, str) or not name.strip():
        errors.append("'name' must be a non-empty string")

    created_at = data.get("created_at")
    if not isinstance(created_at, str) or not created_at.strip():
        errors.append("'created_at' must be a non-empty string")

    modified_at = data.get("modified_at")
    if not isinstance(modified_at, str) or not modified_at.strip():
        errors.append("'modified_at' must be a non-empty string")

    objects = data.get("objects")
    if not isinstance(objects, list):
        errors.append("'objects' must be a list")
        return errors  # can't validate further

    # Per-object validation
    seen_ids: set[str] = set()
    for i, obj in enumerate(objects):
        prefix = f"objects[{i}]"

        if not isinstance(obj, dict):
            errors.append(f"{prefix}: must be a dict")
            continue

        # Required string fields
        obj_id = obj.get("id")
        if not isinstance(obj_id, str) or not obj_id:
            errors.append(f"{prefix}: 'id' must be a non-empty string")
        elif obj_id in seen_ids:
            errors.append(f"{prefix}: duplicate id {obj_id!r}")
        else:
            seen_ids.add(obj_id)

        if not isinstance(obj.get("name"), str):
            errors.append(f"{prefix}: 'name' must be a string")

        # asset_id is nullable
        asset_id = obj.get("asset_id")
        if asset_id is not None and not isinstance(asset_id, str):
            errors.append(f"{prefix}: 'asset_id' must be a string or null")

        # Numeric arrays — reject NaN and Infinity
        pos = obj.get("position")
        if (
            not isinstance(pos, list)
            or len(pos) != 3
            or not all(_is_finite_number(v) for v in pos)
        ):
            errors.append(f"{prefix}: 'position' must be [x, y, z] (finite numbers)")

        rot = obj.get("rotation")
        if (
            not isinstance(rot, list)
            or len(rot) != 4
            or not all(_is_finite_number(v) for v in rot)
        ):
            errors.append(f"{prefix}: 'rotation' must be [x, y, z, w] (finite numbers)")

        scl = obj.get("scale")
        if (
            not isinstance(scl, list)
            or len(scl) != 3
            or not all(_is_finite_number(v) for v in scl)
        ):
            errors.append(f"{prefix}: 'scale' must be [x, y, z] (finite numbers)")

        # parent_id is nullable — must reference an existing id or be null
        parent_id = obj.get("parent_id")
        if parent_id is not None and not isinstance(parent_id, str):
            errors.append(f"{prefix}: 'parent_id' must be a string or null")

        if "visible" not in obj or not isinstance(obj.get("visible"), bool):
            errors.append(f"{prefix}: 'visible' must be a boolean")

    # ── Optional cameras array ────────────────────────────────────────
    cameras = data.get("cameras")
    if cameras is not None:
        if not isinstance(cameras, list):
            errors.append("'cameras' must be a list")
        else:
            cam_ids: set[str] = set()
            for ci, cam in enumerate(cameras):
                cpfx = f"cameras[{ci}]"
                if not isinstance(cam, dict):
                    errors.append(f"{cpfx}: must be a dict")
                    continue

                cam_id = cam.get("id")
                if not isinstance(cam_id, str) or not cam_id:
                    errors.append(f"{cpfx}: 'id' must be a non-empty string")
                elif cam_id in cam_ids:
                    errors.append(f"{cpfx}: duplicate id {cam_id!r}")
                else:
                    cam_ids.add(cam_id)

                cam_name = cam.get("name")
                if not isinstance(cam_name, str) or not cam_name.strip():
                    errors.append(f"{cpfx}: 'name' must be a non-empty string")

                cam_pos = cam.get("position")
                if (
                    not isinstance(cam_pos, list)
                    or len(cam_pos) != 3
                    or not all(_is_finite_number(v) for v in cam_pos)
                ):
                    errors.append(
                        f"{cpfx}: 'position' must be [x, y, z] (finite numbers)"
                    )

                cam_tgt = cam.get("target")
                if (
                    not isinstance(cam_tgt, list)
                    or len(cam_tgt) != 3
                    or not all(_is_finite_number(v) for v in cam_tgt)
                ):
                    errors.append(
                        f"{cpfx}: 'target' must be [x, y, z] (finite numbers)"
                    )

    # Cross-referential checks (only if individual objects are valid enough)
    if not errors:
        _check_parent_refs(objects, seen_ids, errors)
        _check_cycles(objects, errors)

    return errors


def _check_parent_refs(
    objects: list[dict], valid_ids: set[str], errors: list[str]
) -> None:
    """Verify every parent_id references an existing object."""
    for obj in objects:
        parent_id = obj.get("parent_id")
        if parent_id is not None and parent_id not in valid_ids:
            errors.append(
                f"Object {obj['id']!r}: parent_id {parent_id!r} "
                "does not reference an existing object"
            )


def _check_cycles(objects: list[dict], errors: list[str]) -> None:
    """Detect circular parent chains."""
    parent_map = {obj["id"]: obj.get("parent_id") for obj in objects}
    for obj_id in parent_map:
        visited: set[str] = set()
        current: str | None = obj_id
        while current is not None:
            if current in visited:
                errors.append(f"Circular parent chain detected involving {obj_id!r}")
                break
            visited.add(current)
            current = parent_map.get(current)


# ---------------------------------------------------------------------------
# CRUD operations
# ---------------------------------------------------------------------------


def create_scene(config: PipelineConfig, name: str) -> dict:
    """Create a new empty scene and write it to disk.

    Returns the scene dict with an ``id`` field.
    """
    d = scenes_dir(config)
    max_attempts = 5
    for _ in range(max_attempts):
        scene_id = uuid.uuid4().hex[:12]
        path = d / f"{scene_id}.json"
        if not path.exists():
            break
    else:
        msg = "Failed to generate unique scene ID after multiple attempts"
        raise RuntimeError(msg)

    now = _now_iso()

    scene = SceneData(
        version=SCENE_VERSION,
        name=name,
        created_at=now,
        modified_at=now,
        objects=[],
    )

    data = asdict(scene)

    errors = validate_scene(data)
    if errors:
        msg = "Scene validation failed: " + "; ".join(errors)
        raise ValueError(msg)

    path.write_text(json.dumps(data, indent=2), encoding="utf-8")

    data["id"] = scene_id
    log.info("Created scene %r (%s)", name, scene_id)
    return data


def list_scenes(config: PipelineConfig) -> list[dict]:
    """List all scenes, sorted by modified_at descending."""
    result: list[dict] = []
    d = scenes_dir(config)
    for p in d.glob("*.json"):
        try:
            raw = json.loads(p.read_text(encoding="utf-8"))
            errors = validate_scene(raw)
            if errors:
                log.warning("Skipping invalid scene file %s: %s", p, "; ".join(errors))
                continue
        except (json.JSONDecodeError, OSError, ValueError) as exc:
            log.warning("Skipping unreadable scene file: %s (%s)", p, exc)
            continue
        result.append(
            {
                "id": p.stem,
                "name": raw.get("name", ""),
                "modified_at": raw.get("modified_at", ""),
                "object_count": len(raw.get("objects", [])),
            }
        )
    result.sort(key=lambda s: s["modified_at"], reverse=True)
    return result


def load_scene(config: PipelineConfig, scene_id: str) -> dict:
    """Load a scene by ID.  Raises ``FileNotFoundError`` if missing."""
    _validate_scene_id(scene_id)
    path = scenes_dir(config) / f"{scene_id}.json"
    if not path.is_file():
        msg = f"Scene not found: {scene_id}"
        raise FileNotFoundError(msg)
    data = json.loads(path.read_text(encoding="utf-8"))

    errors = validate_scene(data)
    if errors:
        msg = f"Scene {scene_id} failed validation: " + "; ".join(errors)
        raise ValueError(msg)

    data["id"] = scene_id
    return data


def save_scene(config: PipelineConfig, scene_id: str, data: dict) -> dict:
    """Validate and save scene data.

    Returns a new dict with the saved content and ``id``.  Does not mutate
    the input *data* dict.
    """
    _validate_scene_id(scene_id)
    path = scenes_dir(config) / f"{scene_id}.json"
    if not path.is_file():
        msg = f"Scene not found: {scene_id}"
        raise FileNotFoundError(msg)

    errors = validate_scene(data)
    if errors:
        msg = "Scene validation failed: " + "; ".join(errors)
        raise ValueError(msg)

    # Build the on-disk representation without mutating the caller's dict
    to_write = {k: v for k, v in data.items() if k != "id"}

    # Preserve the on-disk created_at — clients must not overwrite history
    existing = json.loads(path.read_text(encoding="utf-8"))
    if "created_at" in existing:
        to_write["created_at"] = existing["created_at"]

    to_write["modified_at"] = _now_iso()

    # Re-validate the final document (catches corrupted on-disk data)
    final_errors = validate_scene(to_write)
    if final_errors:
        msg = "Final scene validation failed: " + "; ".join(final_errors)
        raise ValueError(msg)

    path.write_text(json.dumps(to_write, indent=2), encoding="utf-8")

    result = {**to_write, "id": scene_id}
    log.info("Saved scene %s", scene_id)
    return result


def delete_scene(config: PipelineConfig, scene_id: str) -> None:
    """Delete a scene file.  Raises ``FileNotFoundError`` if missing."""
    _validate_scene_id(scene_id)
    path = scenes_dir(config) / f"{scene_id}.json"
    if not path.is_file():
        msg = f"Scene not found: {scene_id}"
        raise FileNotFoundError(msg)
    path.unlink()
    log.info("Deleted scene %s", scene_id)
