"""Tests for the texture processing plugin."""

from __future__ import annotations

import json
from pathlib import Path

import pytest
from PIL import Image

from pipeline.plugins.texture import (
    TexturePlugin,
    _clamp_dimensions,
    _mip_count,
)

# -- Helpers ----------------------------------------------------------------


def _make_image(path: Path, width: int, height: int, mode: str = "RGB") -> Path:
    """Create a test image file at *path*."""
    img = Image.new(mode, (width, height), color=(255, 0, 0))
    path.parent.mkdir(parents=True, exist_ok=True)
    img.save(path)
    return path


# -- Unit tests: dimension clamping -----------------------------------------


def test_clamp_within_limits():
    assert _clamp_dimensions(512, 512, 2048) == (512, 512)


def test_clamp_square_over_limit():
    w, h = _clamp_dimensions(4096, 4096, 2048)
    assert w == 2048
    assert h == 2048


def test_clamp_landscape():
    w, h = _clamp_dimensions(4000, 2000, 1024)
    assert w == 1024
    assert h == 512


def test_clamp_portrait():
    w, h = _clamp_dimensions(1000, 4000, 2048)
    assert w == 512
    assert h == 2048


def test_clamp_tiny_image():
    assert _clamp_dimensions(1, 1, 2048) == (1, 1)


def test_clamp_extreme_aspect_ratio():
    w, h = _clamp_dimensions(10000, 1, 100)
    assert w == 100
    assert h == 1


# -- Unit tests: mip count --------------------------------------------------


def test_mip_count_power_of_two():
    assert _mip_count(256, 256) == 9  # 256 -> 1 = 8 halvings + base


def test_mip_count_non_power_of_two():
    assert _mip_count(300, 200) == 9  # log2(300) = 8.2 -> 9 levels


def test_mip_count_1x1():
    assert _mip_count(1, 1) == 1


def test_mip_count_rectangle():
    assert _mip_count(512, 64) == 10  # driven by the larger axis


# -- Integration tests: full processing ------------------------------------


@pytest.fixture
def plugin():
    return TexturePlugin()


@pytest.fixture
def source_png(tmp_path: Path) -> Path:
    return _make_image(tmp_path / "src" / "hero.png", 256, 256)


def test_process_creates_output(plugin, source_png, tmp_path):
    output_dir = tmp_path / "out"
    result = plugin.process(source_png, output_dir, {})

    assert result.output.exists()
    assert result.output.suffix == ".png"


def test_process_creates_metadata(plugin, source_png, tmp_path):
    output_dir = tmp_path / "out"
    plugin.process(source_png, output_dir, {})

    meta_path = output_dir / "hero.meta.json"
    assert meta_path.exists()

    meta = json.loads(meta_path.read_text(encoding="utf-8"))
    assert meta["source"] == "hero.png"
    assert meta["original_width"] == 256
    assert meta["original_height"] == 256


def test_process_generates_mipmaps(plugin, source_png, tmp_path):
    output_dir = tmp_path / "out"
    result = plugin.process(source_png, output_dir, {"generate_mipmaps": True})

    # 256x256 -> 9 mip levels (256, 128, 64, 32, 16, 8, 4, 2, 1)
    assert result.metadata["mip_levels"] == 9

    # Check that mip files exist (mip1 through mip8)
    for level in range(1, 9):
        mip_path = output_dir / f"hero_mip{level}.png"
        assert mip_path.exists(), f"Missing mip level {level}"


def test_process_no_mipmaps(plugin, source_png, tmp_path):
    output_dir = tmp_path / "out"
    result = plugin.process(source_png, output_dir, {"generate_mipmaps": False})

    assert result.metadata["mip_levels"] == 1

    # No mip files should exist
    mip_files = list(output_dir.glob("*_mip*.png"))
    assert len(mip_files) == 0


def test_process_resize(plugin, tmp_path):
    big = _make_image(tmp_path / "src" / "big.png", 4096, 2048)
    output_dir = tmp_path / "out"
    result = plugin.process(big, output_dir, {"max_size": 1024})

    assert result.metadata["output_size"] == [1024, 512]

    # Verify the actual output image dimensions.
    out_img = Image.open(result.output)
    assert out_img.size == (1024, 512)


def test_process_no_resize_when_within_limits(plugin, source_png, tmp_path):
    output_dir = tmp_path / "out"
    result = plugin.process(source_png, output_dir, {"max_size": 2048})

    assert result.metadata["output_size"] == [256, 256]


def test_process_jpg_output(plugin, source_png, tmp_path):
    output_dir = tmp_path / "out"
    result = plugin.process(
        source_png, output_dir, {"output_format": "jpg", "generate_mipmaps": False}
    )

    assert result.output.suffix == ".jpg"
    assert result.output.exists()
    assert result.metadata["format"] == "jpg"


def test_process_rgba_image(plugin, tmp_path):
    rgba = _make_image(tmp_path / "src" / "alpha.png", 64, 64, mode="RGBA")
    output_dir = tmp_path / "out"
    result = plugin.process(rgba, output_dir, {"generate_mipmaps": False})

    assert result.output.exists()
    out_img = Image.open(result.output)
    assert out_img.mode == "RGBA"  # PNG output preserves alpha


def test_process_tga_input(plugin, tmp_path):
    tga = _make_image(tmp_path / "src" / "detail.tga", 128, 128)
    output_dir = tmp_path / "out"
    result = plugin.process(tga, output_dir, {})

    # Default output format is PNG regardless of input format.
    assert result.output.suffix == ".png"
    assert result.output.exists()


def test_process_unsupported_format_raises(plugin, source_png, tmp_path):
    output_dir = tmp_path / "out"
    with pytest.raises(ValueError, match="Unsupported output format"):
        plugin.process(source_png, output_dir, {"output_format": "webp"})


def test_process_metadata_settings_recorded(plugin, source_png, tmp_path):
    output_dir = tmp_path / "out"
    plugin.process(source_png, output_dir, {"max_size": 512, "generate_mipmaps": True})

    meta = json.loads((output_dir / "hero.meta.json").read_text(encoding="utf-8"))
    assert meta["settings"]["max_size"] == 512
    assert meta["settings"]["generate_mipmaps"] is True
    assert meta["settings"]["output_format"] == "png"


def test_process_mip_dimensions(plugin, source_png, tmp_path):
    """Verify mip chain dimensions are correct halvings."""
    output_dir = tmp_path / "out"
    plugin.process(source_png, output_dir, {})

    meta = json.loads((output_dir / "hero.meta.json").read_text(encoding="utf-8"))
    mips = meta["mip_levels"]

    # Check the first few levels.
    assert mips[0] == {"level": 0, "width": 256, "height": 256}
    assert mips[1] == {"level": 1, "width": 128, "height": 128}
    assert mips[2] == {"level": 2, "width": 64, "height": 64}

    # Last level should be 1x1.
    assert mips[-1]["width"] == 1
    assert mips[-1]["height"] == 1


def test_process_creates_output_dir(plugin, source_png, tmp_path):
    """Output directory is created automatically if it doesn't exist."""
    output_dir = tmp_path / "deep" / "nested" / "out"
    result = plugin.process(source_png, output_dir, {"generate_mipmaps": False})

    assert result.output.exists()
    assert output_dir.is_dir()


def test_process_jpg_quality_in_metadata(plugin, source_png, tmp_path):
    """Verify jpg_quality is recorded in metadata for JPEG output."""
    output_dir = tmp_path / "out"
    plugin.process(
        source_png,
        output_dir,
        {"output_format": "jpg", "jpg_quality": 85, "generate_mipmaps": False},
    )

    meta = json.loads((output_dir / "hero.meta.json").read_text(encoding="utf-8"))
    assert meta["settings"]["jpg_quality"] == 85


def test_process_bmp_output(plugin, source_png, tmp_path):
    """Verify BMP output format works correctly."""
    output_dir = tmp_path / "out"
    result = plugin.process(
        source_png, output_dir, {"output_format": "bmp", "generate_mipmaps": False}
    )

    assert result.output.suffix == ".bmp"
    assert result.output.exists()
    assert result.metadata["format"] == "bmp"


def test_process_invalid_max_size_raises(plugin, source_png, tmp_path):
    """Non-positive max_size is rejected immediately."""
    output_dir = tmp_path / "out"
    with pytest.raises(ValueError, match="max_size must be >= 1"):
        plugin.process(source_png, output_dir, {"max_size": 0})


def test_process_palette_transparency_preserved(plugin, tmp_path):
    """Indexed-color (palette) images with tRNS transparency are preserved."""
    # Create a P-mode image with a transparency index.
    img = Image.new("P", (32, 32))
    img.putpalette([i % 256 for i in range(768)])
    img.info["transparency"] = 0
    src = tmp_path / "src" / "indexed.png"
    src.parent.mkdir(parents=True, exist_ok=True)
    img.save(src, transparency=0)

    output_dir = tmp_path / "out"
    result = plugin.process(src, output_dir, {"generate_mipmaps": False})

    out_img = Image.open(result.output)
    assert out_img.mode == "RGBA"  # Transparency preserved as alpha
