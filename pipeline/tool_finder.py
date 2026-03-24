"""Locate C tool executables built by CMake.

Plugins call ``find_tool("forge_mesh_tool")`` (or similar) to discover the
compiled binaries.  The search order is:

1. Explicit ``tool_path`` in *settings* (if provided).
2. Common CMake build directories relative to the project root:
   ``build/tools/*/``, ``build/_deps/*/bin/``.
3. ``shutil.which()`` — the system PATH.

This avoids hardcoding platform-specific paths (e.g. ``.exe``) in
``pipeline.toml`` while still finding tools that were built locally.
"""

from __future__ import annotations

import logging
import platform
import shutil
from pathlib import Path

log = logging.getLogger(__name__)

# Project root — pipeline/ is one level below the repo root.
_PROJECT_ROOT = Path(__file__).resolve().parent.parent

# Directories under the build tree where tools live.
_BUILD_TOOL_DIRS = [
    "build/tools/mesh",
    "build/tools/anim",
    "build/tools/scene",
    "build/tools/texture",
]

# basisu is built via FetchContent into _deps.
_BUILD_BASISU_DIRS = [
    "build/_deps/basis_universal_full-src/bin",
]

_EXE_SUFFIX = ".exe" if platform.system() == "Windows" else ""


def find_tool(
    name: str,
    settings: dict | None = None,
    *,
    settings_key: str = "tool_path",
) -> str | None:
    """Locate a tool binary by *name*.

    Parameters
    ----------
    name:
        Base name of the executable (e.g. ``"forge_mesh_tool"``, ``"basisu"``).
    settings:
        Plugin settings dict — if it contains *settings_key*, that value is
        returned directly.
    settings_key:
        Key to look up in *settings* for an explicit path override.  Defaults
        to ``"tool_path"``.  The texture plugin uses ``"basisu_path"`` for
        basisu.
    """
    # 1. Explicit override from settings — trust the caller's path.
    # An empty string means the tool is intentionally disabled (return None).
    # A non-empty string is a path override (return it directly).
    # Key absent means no override (fall through to auto-discovery).
    if settings and settings_key in settings:
        explicit = settings[settings_key]
        if explicit != "":
            return str(explicit)
        return None

    # 2. Search CMake build directories.
    search_dirs = _BUILD_TOOL_DIRS
    if name == "basisu":
        search_dirs = _BUILD_BASISU_DIRS + _BUILD_TOOL_DIRS

    for d in search_dirs:
        candidate = _PROJECT_ROOT / d / f"{name}{_EXE_SUFFIX}"
        if candidate.is_file():
            log.debug("Found %s at %s", name, candidate)
            return str(candidate)

    # Also check build root for tools that end up directly in build/.
    candidate = _PROJECT_ROOT / "build" / f"{name}{_EXE_SUFFIX}"
    if candidate.is_file():
        return str(candidate)

    # 3. System PATH.
    path = shutil.which(name)
    if path is not None:
        return path

    # Also try with underscores replaced by hyphens and vice versa.
    alt_name = name.replace("_", "-") if "_" in name else name.replace("-", "_")
    if alt_name != name:
        path = shutil.which(alt_name)
        if path is not None:
            return path

    return None
