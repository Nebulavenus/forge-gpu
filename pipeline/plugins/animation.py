"""Animation extraction plugin — reads glTF/GLB animation data and writes
compact binary ``.fanim`` files.

Processes 3D model files that contain animation clips through an external
C tool:

1. **Parse** the glTF ``animations[]`` array — channels, samplers, keyframes.
2. **Write** per-clip ``.fanim`` binaries (split mode) or a single combined
   ``.fanim`` (legacy mode).
3. **Generate** a ``.fanims`` stub manifest listing the exported clips.

The heavy lifting is done by ``forge-anim-tool``, a compiled C binary that
uses the shared ``forge_gltf.h`` parser.  If the tool is not installed the
plugin falls back to a no-op, logging a warning — the pipeline still
completes so that users without the tool can work on other parts of the
asset pipeline.

Settings are read from the ``[animation]`` section of ``pipeline.toml``::

    [animation]
    tool_path = ""    # Override forge-anim-tool location
    split = true      # Per-clip export (default: true)
"""

from __future__ import annotations

import json
import logging
import subprocess
from pathlib import Path

from pipeline.plugin import AssetPlugin, AssetResult
from pipeline.tool_finder import find_tool

log = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _find_anim_tool(settings: dict) -> str | None:
    """Locate the ``forge-anim-tool`` binary via the shared tool finder."""
    return find_tool("forge_anim_tool", settings)


# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------


class AnimationPlugin(AssetPlugin):
    """Extract animation clips from glTF/GLB files into .fanim binaries."""

    name = "animation"
    extensions = [".gltf", ".glb"]

    def process(self, source: Path, output_dir: Path, settings: dict) -> AssetResult:
        """Run ``forge-anim-tool`` on *source* and return the result.

        *settings* comes from ``[animation]`` in ``pipeline.toml``.

        When ``split`` is true (the default), the tool writes one ``.fanim``
        file per animation clip plus a ``.fanims`` stub manifest.  When
        false, all clips are packed into a single ``.fanim`` file (legacy
        behaviour from Lesson 08).
        """
        # -- Locate the tool ------------------------------------------------
        tool = _find_anim_tool(settings)
        output_dir.mkdir(parents=True, exist_ok=True)

        if tool is None:
            log.warning(
                "forge-anim-tool not installed — skipping %s. "
                "Install the tool for animation extraction.",
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

        # -- Determine mode -------------------------------------------------
        split = settings.get("split", True)

        # -- Build command line ---------------------------------------------
        if split:
            args: list[str] = [
                tool,
                str(source),
                "--split",
                "--output-dir",
                str(output_dir),
                "--verbose",
            ]
        else:
            output_path = output_dir / f"{source.stem}.fanim"
            args = [tool, str(source), str(output_path), "--verbose"]

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
                f"forge-anim-tool timed out after 600 s processing {source.name}"
            ) from exc
        except OSError as exc:
            raise RuntimeError(
                f"forge-anim-tool could not be launched for {source.name}: {exc}"
            ) from exc

        if result.returncode != 0:
            raise RuntimeError(
                f"forge-anim-tool failed (exit {result.returncode}): "
                f"{result.stderr.strip()}"
            )

        if result.stdout:
            log.debug("  stdout: %s", result.stdout.strip())

        # -- Collect outputs ------------------------------------------------
        metadata: dict = {
            "format": ".fanim",
            "processed": True,
            "split": split,
        }

        if split:
            # Use the deterministic manifest as the source of truth for
            # which clip files were produced (avoids picking up stale
            # .fanim files from a previous run).
            manifest_path = output_dir / f"{source.stem}.fanims"
            clip_files: list[str] = []
            manifest_loaded = False

            if manifest_path.exists():
                metadata["manifest"] = manifest_path.name
                try:
                    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
                    if not isinstance(manifest, dict):
                        raise RuntimeError(
                            f"Invalid manifest: root is not an object "
                            f"in {manifest_path}"
                        )
                    metadata["manifest_data"] = manifest
                    clips = manifest.get("clips", {})
                    if not isinstance(clips, dict):
                        raise RuntimeError(
                            f"Invalid manifest: 'clips' is not an object "
                            f"in {manifest_path}"
                        )
                    manifest_loaded = True
                    clip_files = []
                    for clip_name, clip in clips.items():
                        if not isinstance(clip, dict):
                            raise RuntimeError(
                                f"Invalid manifest: clip '{clip_name}' is not "
                                f"an object in {manifest_path}"
                            )
                        file_val = clip.get("file")
                        if not isinstance(file_val, str):
                            raise RuntimeError(
                                f"Invalid manifest: clip '{clip_name}' missing "
                                f"string 'file' field in {manifest_path}"
                            )
                        clip_files.append(file_val)

                    # Validate that all manifest-referenced clip files exist
                    missing = [
                        name for name in clip_files if not (output_dir / name).is_file()
                    ]
                    if missing:
                        raise RuntimeError(
                            f"Invalid manifest: missing clip file(s) "
                            f"{missing} referenced by {manifest_path}"
                        )
                except (json.JSONDecodeError, OSError, RuntimeError) as exc:
                    raise RuntimeError(
                        f"Could not read animation manifest {manifest_path}: {exc}"
                    ) from exc

            # Only fall back to directory scan when no manifest exists.
            # A manifest with zero clips is valid (source has no animations).
            if not manifest_loaded:
                clip_files = [f.name for f in sorted(output_dir.glob("*.fanim"))]
                if not clip_files:
                    raise RuntimeError(
                        f"forge-anim-tool exited successfully but did not "
                        f"create split outputs for {source.name}"
                    )

            metadata["clip_files"] = clip_files
            metadata["clip_count"] = len(clip_files)

            # Use the first clip file as the canonical output, or the
            # manifest if there are no clip binaries (zero animations).
            canonical = output_dir / clip_files[0] if clip_files else manifest_path
            return AssetResult(
                source=source,
                output=canonical,
                metadata=metadata,
            )

        # Legacy single-file mode
        output_path = output_dir / f"{source.stem}.fanim"
        if not output_path.exists():
            raise RuntimeError(
                f"forge-anim-tool exited successfully but did not create {output_path}"
            )

        meta_path = output_path.with_suffix(".meta.json")
        if meta_path.exists():
            try:
                sidecar = json.loads(meta_path.read_text(encoding="utf-8"))
                metadata.update(sidecar)
                log.debug("  metadata loaded from %s", meta_path.name)
            except (json.JSONDecodeError, OSError) as exc:
                log.warning(
                    "Could not read animation metadata sidecar %s: %s",
                    meta_path,
                    exc,
                )
        metadata["meta_file"] = meta_path.name

        return AssetResult(
            source=source,
            output=output_path,
            metadata=metadata,
        )
