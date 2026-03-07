"""Example texture plugin — a no-op placeholder for Lesson 01.

This plugin registers itself for common image formats.  The ``process``
method is a stub that simply copies the source path to the result — real
texture processing (resize, compress, mipmaps) is added in Lesson 02.
"""

from pathlib import Path

from pipeline.plugin import AssetPlugin, AssetResult


class TexturePlugin(AssetPlugin):
    """Handle image files (PNG, JPG, TGA, BMP)."""

    name = "texture"
    extensions = [".png", ".jpg", ".jpeg", ".tga", ".bmp"]

    def process(self, source: Path, output_dir: Path, settings: dict) -> AssetResult:
        # Lesson 01 — no processing, just report.
        output = output_dir / source.name
        return AssetResult(
            source=source, output=output, metadata={"format": source.suffix}
        )
