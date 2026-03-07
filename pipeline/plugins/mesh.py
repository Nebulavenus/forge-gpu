"""Example mesh plugin — a no-op placeholder for Lesson 01.

This plugin registers itself for common 3D model formats.  Real mesh
processing (meshoptimizer, MikkTSpace) is added in Lesson 03 as a compiled
C tool invoked as a subprocess.
"""

from pathlib import Path

from pipeline.plugin import AssetPlugin, AssetResult


class MeshPlugin(AssetPlugin):
    """Handle 3D model files (OBJ, glTF)."""

    name = "mesh"
    extensions = [".obj", ".gltf", ".glb"]

    def process(self, source: Path, output_dir: Path, settings: dict) -> AssetResult:
        # Lesson 01 — no processing, just report.
        output = output_dir / source.name
        return AssetResult(
            source=source, output=output, metadata={"format": source.suffix}
        )
