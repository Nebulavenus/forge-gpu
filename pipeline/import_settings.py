"""Per-asset import settings stored as TOML sidecar files.

.. note:: Uses :func:`copy.deepcopy` for schema defaults to prevent mutable
   objects (e.g. lists) from leaking across assets.

Every source asset can have an optional ``.import.toml`` sidecar that overrides
the global plugin settings from ``pipeline.toml``.  For example, a normal map
texture might set ``normal_map = true`` while the rest of the project uses the
default ``false``.

The sidecar filename is the source filename with ``.import.toml`` appended::

    assets/raw/textures/brick_albedo.png
    assets/raw/textures/brick_albedo.png.import.toml
    assets/raw/textures/brick_normal.png
    assets/raw/textures/brick_normal.png.import.toml

Sidecar contents are a flat TOML table whose keys match the plugin's settings.
Only the overridden keys need to be present — missing keys fall back to the
global ``[plugin-name]`` section in ``pipeline.toml``.

Example ``.import.toml`` for a texture::

    normal_map = true
    compression = "basisu"
    basisu_quality = 200

At processing time, ``merge_settings()`` combines the global defaults with any
per-asset overrides so the plugin receives a single unified dict.
"""

from __future__ import annotations

import logging
import sys
from copy import deepcopy
from pathlib import Path

if sys.version_info >= (3, 11):
    import tomllib
else:
    try:
        import tomllib  # type: ignore[import-not-found]
    except ModuleNotFoundError:
        import tomli as tomllib  # type: ignore[import-not-found,no-redef]

log = logging.getLogger(__name__)

# Suffix appended to the source filename to form the sidecar path.
SIDECAR_SUFFIX = ".import.toml"

# ---------------------------------------------------------------------------
# Schema — describes valid settings per plugin type
# ---------------------------------------------------------------------------

# Each entry maps a setting name to a dict with:
#   "type": one of "bool", "int", "float", "str", "list[float]"
#   "label": human-readable label for the web UI
#   "description": tooltip text
#   "default": default value (must match global pipeline.toml defaults)
#   "options": (optional) list of valid values for enum-style settings
#   "min"/"max": (optional) numeric range
#   "group": (optional) group name for UI organization

TEXTURE_SETTINGS_SCHEMA: dict[str, dict] = {
    "max_size": {
        "type": "int",
        "label": "Max size",
        "description": "Clamp width and height to this limit (pixels).",
        "default": 2048,
        "min": 1,
        "max": 8192,
    },
    "generate_mipmaps": {
        "type": "bool",
        "label": "Generate mipmaps",
        "description": "Create a mip chain alongside the base image.",
        "default": True,
    },
    "output_format": {
        "type": "str",
        "label": "Output format",
        "description": "Image format for the base output file.",
        "default": "png",
        "options": ["png", "jpg", "bmp"],
    },
    "jpg_quality": {
        "type": "int",
        "label": "JPEG quality",
        "description": "Quality level for JPEG output (1–100).",
        "default": 90,
        "min": 1,
        "max": 100,
        "group": "JPEG",
    },
    "compression": {
        "type": "str",
        "label": "GPU compression",
        "description": "GPU block-compression codec applied after base output.",
        "default": "none",
        "options": ["none", "basisu", "astc"],
    },
    "basisu_format": {
        "type": "str",
        "label": "Basis format",
        "description": "Basis Universal encoding mode.",
        "default": "uastc",
        "options": ["etc1s", "uastc"],
        "group": "Basis Universal",
    },
    "basisu_quality": {
        "type": "int",
        "label": "Basis quality",
        "description": "Basis Universal quality level (1–255, higher is better).",
        "default": 128,
        "min": 1,
        "max": 255,
        "group": "Basis Universal",
    },
    "astc_block_size": {
        "type": "str",
        "label": "ASTC block size",
        "description": "ASTC block footprint — smaller blocks give higher quality.",
        "default": "6x6",
        "options": ["4x4", "5x5", "6x6", "8x8"],
        "group": "ASTC",
    },
    "astc_quality": {
        "type": "str",
        "label": "ASTC quality",
        "description": "ASTC encoder quality preset.",
        "default": "medium",
        "options": ["fastest", "fast", "medium", "thorough", "exhaustive"],
        "group": "ASTC",
    },
    "normal_map": {
        "type": "bool",
        "label": "Normal map",
        "description": "Treat as a normal map (BC5 encoding, linear color space).",
        "default": False,
    },
}

ATLAS_SETTINGS_SCHEMA: dict[str, dict] = {
    "atlas_enabled": {
        "type": "bool",
        "label": "Enable atlas packing",
        "description": "Pack all material textures into a single atlas image.",
        "default": False,
    },
    "atlas_max_size": {
        "type": "int",
        "label": "Atlas max size",
        "description": "Maximum atlas dimension on either axis (pixels).",
        "default": 4096,
        "min": 256,
        "max": 8192,
    },
    "atlas_padding": {
        "type": "int",
        "label": "Atlas padding",
        "description": "Texel padding around each rectangle to prevent bleeding.",
        "default": 4,
        "min": 0,
        "max": 32,
    },
}

MESH_SETTINGS_SCHEMA: dict[str, dict] = {
    "deduplicate": {
        "type": "bool",
        "label": "Deduplicate vertices",
        "description": "Remove identical vertices before optimization.",
        "default": True,
    },
    "optimize": {
        "type": "bool",
        "label": "Optimize indices",
        "description": "Reorder indices for vertex and overdraw cache efficiency.",
        "default": True,
    },
    "generate_tangents": {
        "type": "bool",
        "label": "Generate tangents",
        "description": "Compute MikkTSpace tangent frames for normal mapping.",
        "default": True,
    },
    "lod_levels": {
        "type": "list[float]",
        "label": "LOD levels",
        "description": (
            "Target triangle ratios for LOD generation. "
            "1.0 = full detail, 0.5 = half, 0.25 = quarter."
        ),
        "default": [1.0],
    },
}

# Map plugin names to their schemas.
SETTINGS_SCHEMAS: dict[str, dict[str, dict]] = {
    "texture": TEXTURE_SETTINGS_SCHEMA,
    "mesh": MESH_SETTINGS_SCHEMA,
    "atlas": ATLAS_SETTINGS_SCHEMA,
}


# ---------------------------------------------------------------------------
# Sidecar path
# ---------------------------------------------------------------------------


def sidecar_path(source: Path) -> Path:
    """Return the ``.import.toml`` sidecar path for a source file.

    The sidecar sits alongside the source file with the full filename
    (including extension) plus ``.import.toml``::

        source: assets/raw/textures/brick.png
        sidecar: assets/raw/textures/brick.png.import.toml
    """
    return source.parent / (source.name + SIDECAR_SUFFIX)


# ---------------------------------------------------------------------------
# Load / save / delete
# ---------------------------------------------------------------------------


def load_sidecar(source: Path) -> dict:
    """Load per-asset import settings from the ``.import.toml`` sidecar.

    Returns an empty dict if no sidecar exists.  Raises ``ValueError`` on
    malformed TOML.
    """
    path = sidecar_path(source)
    if not path.is_file():
        return {}

    try:
        with path.open("rb") as f:
            data = tomllib.load(f)
    except tomllib.TOMLDecodeError as exc:
        raise ValueError(f"Malformed import sidecar {path}: {exc}") from exc

    log.debug("Loaded import settings from %s: %s", path.name, data)
    return data


def save_sidecar(source: Path, settings: dict) -> Path:
    """Write per-asset import settings to the ``.import.toml`` sidecar.

    Only non-empty dicts are written — if *settings* is empty the sidecar
    is deleted instead (revert to global defaults).

    Returns the sidecar path.
    """
    path = sidecar_path(source)

    if not settings:
        delete_sidecar(source)
        return path

    lines: list[str] = []
    for key, value in settings.items():
        lines.append(_format_toml_value(key, value))

    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    log.debug("Saved import settings to %s", path.name)
    return path


def delete_sidecar(source: Path) -> bool:
    """Remove the ``.import.toml`` sidecar if it exists.

    Returns ``True`` if a file was deleted, ``False`` otherwise.
    """
    path = sidecar_path(source)
    if path.is_file():
        path.unlink()
        log.debug("Deleted import sidecar %s", path.name)
        return True
    return False


# ---------------------------------------------------------------------------
# Merge
# ---------------------------------------------------------------------------


def merge_settings(global_settings: dict, per_asset: dict) -> dict:
    """Merge global plugin settings with per-asset overrides.

    Per-asset values take precedence.  The result is a new dict — neither
    input is modified.
    """
    merged = dict(global_settings)
    merged.update(per_asset)
    return merged


# ---------------------------------------------------------------------------
# Schema helpers
# ---------------------------------------------------------------------------


def get_schema(plugin_name: str) -> dict[str, dict] | None:
    """Return the settings schema for *plugin_name*, or ``None``."""
    return SETTINGS_SCHEMAS.get(plugin_name)


def get_effective_settings(
    plugin_name: str,
    global_settings: dict,
    per_asset: dict,
) -> dict:
    """Return the fully resolved settings for an asset.

    Starts with schema defaults, overlays global settings, then per-asset
    overrides.  This ensures every key has a value even if the global config
    or sidecar omits it.
    """
    schema = SETTINGS_SCHEMAS.get(plugin_name, {})
    result: dict = {}

    # Layer 1: schema defaults (deepcopy to prevent mutable leaks)
    for key, spec in schema.items():
        result[key] = deepcopy(spec["default"])

    # Layer 2: global pipeline.toml settings
    result.update(global_settings)

    # Layer 3: per-asset sidecar overrides
    result.update(per_asset)

    return result


# ---------------------------------------------------------------------------
# TOML formatting
# ---------------------------------------------------------------------------


def _format_toml_value(key: str, value: object) -> str:
    """Format a single key = value line in TOML syntax."""
    if isinstance(value, bool):
        return f"{key} = {'true' if value else 'false'}"
    if isinstance(value, int):
        return f"{key} = {value}"
    if isinstance(value, float):
        # TOML requires a decimal point for floats
        s = f"{value}"
        if "." not in s and "e" not in s.lower():
            s += ".0"
        return f"{key} = {s}"
    if isinstance(value, str):
        # Escape backslashes and double quotes for TOML basic strings
        escaped = value.replace("\\", "\\\\").replace('"', '\\"')
        return f'{key} = "{escaped}"'
    if isinstance(value, (list, tuple)):
        items = ", ".join(_format_toml_inline(v) for v in value)
        return f"{key} = [{items}]"
    raise TypeError(
        f"Unsupported type for import setting '{key}': {type(value).__name__}"
    )


def _format_toml_inline(value: object) -> str:
    """Format a value for use inside a TOML inline array."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        s = f"{value}"
        if "." not in s and "e" not in s.lower():
            s += ".0"
        return s
    if isinstance(value, str):
        escaped = value.replace("\\", "\\\\").replace('"', '\\"')
        return f'"{escaped}"'
    raise TypeError(f"Unsupported type in TOML array value: {type(value).__name__}")
