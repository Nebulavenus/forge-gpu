"""Mesh processing plugin — vertex deduplication, index optimization,
tangent generation, and LOD simplification.

Processes 3D model files (OBJ, glTF, GLB) through an external C tool:

1. **Deduplicate** vertices — remove identical positions/normals/UVs.
2. **Optimize** — reorder indices for vertex and overdraw cache efficiency
   (meshoptimizer).
3. **Generate tangents** — compute MikkTSpace tangent frames for normal mapping.
4. **LOD simplification** — produce simplified meshes at configurable ratios
   (e.g. 50 %, 25 % of the original triangle count).

The heavy lifting is done by ``forge-mesh-tool``, a compiled C binary that
wraps meshoptimizer and MikkTSpace.  If the tool is not installed the plugin
falls back to copying the source file unchanged, logging a warning — the
pipeline still completes so that users without the tool can work on other
parts of the asset pipeline.

Settings are read from the ``[mesh]`` section of ``pipeline.toml``::

    [mesh]
    deduplicate = true              # Vertex deduplication (on by default)
    optimize = true                 # Index/vertex cache optimization
    generate_tangents = true        # MikkTSpace tangent generation
    lod_levels = [1.0, 0.5, 0.25]  # LOD target ratios (1.0 = full detail)
    tool_path = ""                  # Override forge-mesh-tool location
"""

from __future__ import annotations

import json
import logging
import subprocess
from pathlib import Path

from pipeline.plugin import AssetPlugin, AssetResult
from pipeline.tool_finder import find_tool

log = logging.getLogger(__name__)

# Default LOD level list — full detail only.
DEFAULT_LOD_LEVELS: list[float] = [1.0]


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _find_mesh_tool(settings: dict) -> str | None:
    """Locate the ``forge-mesh-tool`` binary via the shared tool finder."""
    return find_tool("forge_mesh_tool", settings)


def _validate_lod_levels(levels: list[float]) -> None:
    """Raise ``ValueError`` if any LOD ratio is out of range."""
    for ratio in levels:
        if ratio <= 0.0 or ratio > 1.0:
            raise ValueError(f"LOD ratio must be > 0.0 and <= 1.0, got {ratio}")


def _parse_lod_levels(raw: object) -> list[float]:
    """Coerce the ``lod_levels`` setting into a validated list of floats."""
    if raw is None:
        return list(DEFAULT_LOD_LEVELS)

    if isinstance(raw, (list, tuple)):
        levels = [float(v) for v in raw]
    elif isinstance(raw, str):
        levels = [float(v) for v in raw.split(",") if v.strip()]
    else:
        levels = [float(raw)]  # type: ignore[arg-type]

    if not levels:
        return list(DEFAULT_LOD_LEVELS)

    _validate_lod_levels(levels)
    return levels


# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------


class MeshPlugin(AssetPlugin):
    """Process 3D model files: deduplicate, optimize, generate tangents, LOD."""

    name = "mesh"
    extensions = [".obj", ".gltf", ".glb"]

    def process(self, source: Path, output_dir: Path, settings: dict) -> AssetResult:
        """Run ``forge-mesh-tool`` on *source* and return the result.

        *settings* comes from ``[mesh]`` in ``pipeline.toml``.
        """
        # -- Read settings with defaults ------------------------------------
        deduplicate = bool(settings.get("deduplicate", True))
        optimize = bool(settings.get("optimize", True))
        generate_tangents = bool(settings.get("generate_tangents", True))
        lod_levels = _parse_lod_levels(settings.get("lod_levels"))

        # -- Locate the tool ------------------------------------------------
        tool = _find_mesh_tool(settings)
        output_dir.mkdir(parents=True, exist_ok=True)
        output_path = output_dir / f"{source.stem}.fmesh"

        if tool is None:
            log.warning(
                "forge-mesh-tool not installed — skipping %s. "
                "Install the tool for vertex deduplication, optimization, "
                "tangent generation, and LOD simplification.",
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

        # -- Build command line ---------------------------------------------
        args: list[str] = [tool, str(source), str(output_path)]

        if not deduplicate:
            args.append("--no-deduplicate")
        if not optimize:
            args.append("--no-optimize")
        if not generate_tangents:
            args.append("--no-tangents")
        if lod_levels and lod_levels != DEFAULT_LOD_LEVELS:
            args.extend(
                [
                    "--lod-levels",
                    ",".join(str(r) for r in lod_levels),
                ]
            )
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
                f"forge-mesh-tool timed out after 600 s processing {source.name}"
            ) from exc
        except OSError as exc:
            raise RuntimeError(
                f"forge-mesh-tool could not be launched for {source.name}: {exc}"
            ) from exc

        if result.returncode != 0:
            raise RuntimeError(
                f"forge-mesh-tool failed (exit {result.returncode}): "
                f"{result.stderr.strip()}"
            )

        if not output_path.exists():
            raise RuntimeError(
                f"forge-mesh-tool exited successfully but did not create {output_path}"
            )

        if result.stdout:
            log.debug("  stdout: %s", result.stdout.strip())

        # -- Read metadata sidecar ------------------------------------------
        meta_path = output_path.with_suffix(".meta.json")
        metadata: dict = {
            "format": ".fmesh",
            "processed": True,
            "deduplicate": deduplicate,
            "optimize": optimize,
            "generate_tangents": generate_tangents,
            "lod_levels": lod_levels,
        }

        if meta_path.exists():
            try:
                sidecar = json.loads(meta_path.read_text(encoding="utf-8"))
                metadata.update(sidecar)
                log.debug("  metadata loaded from %s", meta_path.name)
            except (json.JSONDecodeError, OSError) as exc:
                log.warning(
                    "Could not read mesh metadata sidecar %s: %s",
                    meta_path,
                    exc,
                )
        else:
            log.debug("  no metadata sidecar at %s — using defaults", meta_path.name)

        metadata["meta_file"] = meta_path.name

        # -- Read material sidecar (.fmat) ----------------------------------
        fmat_path = output_path.with_suffix(".fmat")
        if fmat_path.exists():
            try:
                fmat_data = json.loads(fmat_path.read_text(encoding="utf-8"))
                if not isinstance(fmat_data, dict):
                    raise ValueError(
                        f"top-level .fmat JSON must be an object, "
                        f"got {type(fmat_data).__name__}"
                    )
                materials = fmat_data.get("materials", [])
                if not isinstance(materials, list):
                    raise TypeError(
                        f"expected 'materials' to be a list, "
                        f"got {type(materials).__name__}"
                    )
                material_count = len(materials)
                metadata["material_file"] = fmat_path.name
                metadata["material_count"] = material_count
                log.debug(
                    "  material sidecar loaded: %s (%d materials)",
                    fmat_path.name,
                    material_count,
                )
            except (json.JSONDecodeError, OSError, TypeError, ValueError) as exc:
                log.warning(
                    "Could not read material sidecar %s: %s",
                    fmat_path,
                    exc,
                )

        # -- Log morph target metadata if present -----------------------------
        if metadata.get("has_morphs"):
            log.info(
                "  morph targets detected in %s",
                source.name,
            )

        return AssetResult(
            source=source,
            output=output_path,
            metadata=metadata,
        )
