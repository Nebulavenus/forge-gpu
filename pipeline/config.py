"""Pipeline configuration loaded from TOML files.

The pipeline reads project-level settings from a ``pipeline.toml`` file.
This module parses the TOML, validates required fields, and produces a typed
``PipelineConfig`` dataclass that the rest of the pipeline consumes.

Why TOML?
  TOML maps naturally to the nested key-value structure of pipeline settings
  (source dirs, per-format options, output paths).  It is human-readable,
  diff-friendly, and has first-class support in Python 3.11+ via ``tomllib``.
"""

from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path

# tomllib is stdlib in 3.11+; fall back to the compatible third-party package
if sys.version_info >= (3, 11):
    import tomllib
else:
    try:
        import tomllib  # type: ignore[import-not-found]
    except ModuleNotFoundError:
        import tomli as tomllib  # type: ignore[import-not-found,no-redef]


# ---------------------------------------------------------------------------
# Default values
# ---------------------------------------------------------------------------

DEFAULT_SOURCE_DIR = "assets/raw"
DEFAULT_OUTPUT_DIR = "assets/processed"
DEFAULT_CACHE_DIR = ".forge-cache"


# ---------------------------------------------------------------------------
# Configuration dataclass
# ---------------------------------------------------------------------------


@dataclass
class PipelineConfig:
    """Typed representation of ``pipeline.toml``.

    Fields mirror the TOML sections and keys so that every consumer can rely
    on typed access rather than raw dictionary lookups.
    """

    source_dir: Path
    output_dir: Path
    cache_dir: Path

    # Per-plugin settings stored as plain dicts — each plugin interprets its
    # own section.  This keeps the core config loader decoupled from plugin
    # internals.
    plugin_settings: dict[str, dict] = field(default_factory=dict)

    # The raw parsed TOML for forward-compatibility — plugins can read keys
    # the core doesn't know about yet.
    raw: dict = field(default_factory=dict, repr=False)


# ---------------------------------------------------------------------------
# Loading
# ---------------------------------------------------------------------------


class ConfigError(Exception):
    """Raised when the configuration file is missing or malformed."""


def load_config(path: Path) -> PipelineConfig:
    """Parse *path* as TOML and return a ``PipelineConfig``.

    Raises ``ConfigError`` if the file does not exist or contains invalid
    TOML.
    """
    if not path.is_file():
        raise ConfigError(f"Configuration file not found: {path}")

    try:
        with path.open("rb") as f:
            raw = tomllib.load(f)
    except OSError as exc:
        raise ConfigError(f"Could not read configuration file {path}: {exc}") from exc
    except tomllib.TOMLDecodeError as exc:
        raise ConfigError(f"Invalid TOML in {path}: {exc}") from exc

    pipeline_section = raw.get("pipeline", {})
    if not isinstance(pipeline_section, dict):
        raise ConfigError("[pipeline] must be a table")

    def _resolve(key: str, default: str) -> Path:
        value = pipeline_section.get(key, default)
        candidate = Path(value)
        return candidate if candidate.is_absolute() else path.parent / candidate

    source_dir = _resolve("source_dir", DEFAULT_SOURCE_DIR)
    output_dir = _resolve("output_dir", DEFAULT_OUTPUT_DIR)
    cache_dir = _resolve("cache_dir", DEFAULT_CACHE_DIR)

    # Collect per-plugin sections — everything that isn't [pipeline] is
    # treated as a plugin configuration block.
    plugin_settings: dict[str, dict] = {}
    for key, value in raw.items():
        if key != "pipeline" and isinstance(value, dict):
            plugin_settings[key] = value

    return PipelineConfig(
        source_dir=source_dir,
        output_dir=output_dir,
        cache_dir=cache_dir,
        plugin_settings=plugin_settings,
        raw=raw,
    )


def default_config() -> PipelineConfig:
    """Return a ``PipelineConfig`` with all default values.

    Used when no ``pipeline.toml`` is present and the user has not specified
    ``--config``.
    """
    return PipelineConfig(
        source_dir=Path(DEFAULT_SOURCE_DIR),
        output_dir=Path(DEFAULT_OUTPUT_DIR),
        cache_dir=Path(DEFAULT_CACHE_DIR),
    )
