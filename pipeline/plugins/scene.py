"""Scene hierarchy extraction plugin — reads glTF/GLB node trees and writes
a compact binary ``.fscene`` file.

Processes 3D model files that contain node hierarchies through an external
C tool:

1. **Parse** the glTF node tree — parent-child relationships, transforms,
   mesh references.
2. **Write** a ``.fscene`` binary with nodes, mesh table, and children array.
3. **Generate** a ``.meta.json`` sidecar with node names, mesh mappings, and
   hierarchy statistics.

The heavy lifting is done by ``forge-scene-tool``, a compiled C binary that
uses the shared ``forge_gltf.h`` parser.  If the tool is not installed the
plugin falls back to a no-op, logging a warning — the pipeline still
completes so that users without the tool can work on other parts of the
asset pipeline.

Settings are read from the ``[scene]`` section of ``pipeline.toml``::

    [scene]
    tool_path = ""    # Override forge-scene-tool location
"""

from __future__ import annotations

import json
import logging
import shutil
import subprocess
from pathlib import Path

from pipeline.plugin import AssetPlugin, AssetResult

log = logging.getLogger(__name__)

# Names to search for via ``shutil.which``.
_TOOL_NAMES = ["forge_scene_tool", "forge-scene-tool"]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _find_scene_tool(settings: dict) -> str | None:
    """Locate the ``forge-scene-tool`` binary.

    If ``tool_path`` is set in *settings*, return that directly (the caller
    is responsible for ensuring it exists).  Otherwise search ``$PATH`` for
    each name in ``_TOOL_NAMES`` and return the first hit, or ``None``.
    """
    explicit = settings.get("tool_path", "")
    if explicit:
        return str(explicit)

    for name in _TOOL_NAMES:
        path = shutil.which(name)
        if path is not None:
            return path

    return None


# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------


class ScenePlugin(AssetPlugin):
    """Extract scene node hierarchy from glTF/GLB files into .fscene binaries."""

    name = "scene"
    extensions = [".gltf", ".glb"]

    def process(self, source: Path, output_dir: Path, settings: dict) -> AssetResult:
        """Run ``forge-scene-tool`` on *source* and return the result.

        *settings* comes from ``[scene]`` in ``pipeline.toml``.
        """
        # -- Locate the tool ------------------------------------------------
        tool = _find_scene_tool(settings)
        output_dir.mkdir(parents=True, exist_ok=True)
        output_path = output_dir / f"{source.stem}.fscene"

        if tool is None:
            log.warning(
                "forge-scene-tool not installed — skipping %s. "
                "Install the tool for scene hierarchy extraction.",
                source.name,
            )
            return AssetResult(
                source=source,
                output=source,
                metadata={
                    "format": source.suffix,
                    "processed": False,
                    "reason": "tool_not_found",
                },
            )

        # -- Remove stale outputs so exists() is trustworthy ----------------
        meta_path = output_path.with_suffix(".meta.json")
        for stale in (output_path, meta_path):
            if stale.exists():
                stale.unlink()

        # -- Build command line ---------------------------------------------
        args: list[str] = [tool, str(source), str(output_path)]
        args.append("--verbose")

        log.debug("Running: %s", " ".join(args))

        # -- Execute --------------------------------------------------------
        try:
            result = subprocess.run(
                args,
                capture_output=True,
                text=True,
                check=False,
                timeout=600,
            )
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError(
                f"forge-scene-tool timed out after 600 s processing {source.name}"
            ) from exc
        except OSError as exc:
            raise RuntimeError(
                f"forge-scene-tool could not be launched for {source.name}: {exc}"
            ) from exc

        if result.returncode != 0:
            raise RuntimeError(
                f"forge-scene-tool failed (exit {result.returncode}): "
                f"{result.stderr.strip()}"
            )

        if not output_path.exists():
            raise RuntimeError(
                f"forge-scene-tool exited successfully but did not create {output_path}"
            )

        if result.stdout:
            log.debug("  stdout: %s", result.stdout.strip())

        # -- Read metadata sidecar ------------------------------------------
        metadata: dict = {
            "format": ".fscene",
            "processed": True,
        }

        if meta_path.exists():
            try:
                sidecar = json.loads(meta_path.read_text(encoding="utf-8"))
                metadata.update(sidecar)
                log.debug("  metadata loaded from %s", meta_path.name)
            except (json.JSONDecodeError, OSError) as exc:
                log.warning(
                    "Could not read scene metadata sidecar %s: %s",
                    meta_path,
                    exc,
                )
        else:
            log.debug("  no metadata sidecar at %s — using defaults", meta_path.name)

        metadata["meta_file"] = meta_path.name

        return AssetResult(
            source=source,
            output=output_path,
            metadata=metadata,
        )
