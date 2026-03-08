"""Tests for the texture processing plugin."""

from __future__ import annotations

import json
from pathlib import Path
from unittest.mock import MagicMock, patch

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


# -- Compression tests (mock-based) ----------------------------------------


def test_compression_none_default(plugin, source_png, tmp_path):
    """No compression by default."""
    output_dir = tmp_path / "out"
    result = plugin.process(source_png, output_dir, {"generate_mipmaps": False})
    assert result.metadata["compression"] == "none"

    meta = json.loads((output_dir / "hero.meta.json").read_text(encoding="utf-8"))
    assert "compression" not in meta  # No compression block in metadata


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/basisu")
def test_basisu_etc1s_invocation(mock_tool, mock_run, plugin, source_png, tmp_path):
    """Verify correct basisu CLI args for ETC1S mode."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"
    # Create a fake .ktx2 output so stat() works
    output_dir.mkdir(parents=True, exist_ok=True)
    ktx2 = output_dir / "hero.ktx2"
    ktx2.write_bytes(b"\x00" * 100)

    plugin.process(
        source_png,
        output_dir,
        {
            "compression": "basisu",
            "basisu_format": "etc1s",
            "basisu_quality": 200,
            "generate_mipmaps": False,
        },
    )

    mock_run.assert_called_once()
    args = mock_run.call_args[0][0]
    assert args[0] == "/usr/bin/basisu"
    assert "-ktx2" in args
    assert "-q" in args
    assert "200" in args
    assert "-uastc" not in args  # ETC1S mode — no -uastc flag
    assert "-mipmap" not in args  # mipmaps disabled


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/basisu")
def test_basisu_uastc_invocation(mock_tool, mock_run, plugin, source_png, tmp_path):
    """Verify correct basisu CLI args for UASTC mode."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "hero.ktx2").write_bytes(b"\x00" * 100)

    plugin.process(
        source_png,
        output_dir,
        {
            "compression": "basisu",
            "basisu_format": "uastc",
            "generate_mipmaps": False,
        },
    )

    args = mock_run.call_args[0][0]
    assert "-uastc" in args


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/basisu")
def test_basisu_normal_map_flags(mock_tool, mock_run, plugin, source_png, tmp_path):
    """Verify -normal_map and -separate_rg_to_color_alpha flags for normal maps."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "hero.ktx2").write_bytes(b"\x00" * 100)

    plugin.process(
        source_png,
        output_dir,
        {
            "compression": "basisu",
            "normal_map": True,
            "generate_mipmaps": False,
        },
    )

    args = mock_run.call_args[0][0]
    assert "-normal_map" in args
    assert "-separate_rg_to_color_alpha" in args


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/basisu")
def test_basisu_quality_setting(mock_tool, mock_run, plugin, source_png, tmp_path):
    """Quality parameter is passed through to basisu."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "hero.ktx2").write_bytes(b"\x00" * 100)

    plugin.process(
        source_png,
        output_dir,
        {
            "compression": "basisu",
            "basisu_quality": 42,
            "generate_mipmaps": False,
        },
    )

    args = mock_run.call_args[0][0]
    q_idx = args.index("-q")
    assert args[q_idx + 1] == "42"


@patch("pipeline.plugins.texture._find_tool", return_value=None)
def test_basisu_not_installed_warns(mock_tool, plugin, source_png, tmp_path, caplog):
    """Graceful fallback when basisu is not installed."""
    output_dir = tmp_path / "out"

    with caplog.at_level("WARNING"):
        result = plugin.process(
            source_png,
            output_dir,
            {"compression": "basisu", "generate_mipmaps": False},
        )

    assert "basisu not installed" in caplog.text
    assert result.output.exists()  # Base image still produced


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/astcenc")
def test_astcenc_invocation(mock_tool, mock_run, plugin, source_png, tmp_path):
    """Verify correct astcenc CLI args."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "hero.astc").write_bytes(b"\x00" * 100)

    plugin.process(
        source_png,
        output_dir,
        {"compression": "astc", "generate_mipmaps": False},
    )

    mock_run.assert_called_once()
    args = mock_run.call_args[0][0]
    assert args[0] == "/usr/bin/astcenc"
    assert "-cl" in args


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/astcenc")
def test_astcenc_block_size(mock_tool, mock_run, plugin, source_png, tmp_path):
    """Block size parameter is passed through to astcenc."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "hero.astc").write_bytes(b"\x00" * 100)

    plugin.process(
        source_png,
        output_dir,
        {
            "compression": "astc",
            "astc_block_size": "4x4",
            "generate_mipmaps": False,
        },
    )

    args = mock_run.call_args[0][0]
    assert "4x4" in args


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/astcenc")
def test_astcenc_quality_levels(mock_tool, mock_run, plugin, source_png, tmp_path):
    """Each quality level maps correctly to astcenc flag."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "hero.astc").write_bytes(b"\x00" * 100)

    plugin.process(
        source_png,
        output_dir,
        {
            "compression": "astc",
            "astc_quality": "thorough",
            "generate_mipmaps": False,
        },
    )

    args = mock_run.call_args[0][0]
    assert "-thorough" in args


@patch("pipeline.plugins.texture._find_tool", return_value=None)
def test_astcenc_not_installed_warns(mock_tool, plugin, source_png, tmp_path, caplog):
    """Graceful fallback when astcenc is not installed."""
    output_dir = tmp_path / "out"

    with caplog.at_level("WARNING"):
        result = plugin.process(
            source_png,
            output_dir,
            {"compression": "astc", "generate_mipmaps": False},
        )

    assert "astcenc not installed" in caplog.text
    assert result.output.exists()


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/basisu")
def test_compression_metadata_recorded(
    mock_tool, mock_run, plugin, source_png, tmp_path
):
    """meta.json includes compression info when compression is active."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "hero.ktx2").write_bytes(b"\x00" * 50)

    plugin.process(
        source_png,
        output_dir,
        {"compression": "basisu", "generate_mipmaps": False},
    )

    meta = json.loads((output_dir / "hero.meta.json").read_text(encoding="utf-8"))
    assert "compression" in meta
    assert meta["compression"]["container"] == "ktx2"
    assert meta["compression"]["compressed_file"] == "hero.ktx2"
    assert meta["settings"]["compression"] == "basisu"


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/basisu")
def test_compression_with_resize(mock_tool, mock_run, plugin, tmp_path):
    """Compression runs after resize."""
    big = _make_image(tmp_path / "src" / "big.png", 4096, 2048)
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "big.ktx2").write_bytes(b"\x00" * 50)

    result = plugin.process(
        big,
        output_dir,
        {
            "compression": "basisu",
            "max_size": 1024,
            "generate_mipmaps": False,
        },
    )

    assert result.metadata["output_size"] == [1024, 512]
    mock_run.assert_called_once()  # Compression still ran


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/basisu")
def test_compression_with_mipmaps(mock_tool, mock_run, plugin, source_png, tmp_path):
    """basisu -mipmap flag is set when mipmaps are enabled."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "hero.ktx2").write_bytes(b"\x00" * 50)

    plugin.process(
        source_png,
        output_dir,
        {"compression": "basisu", "generate_mipmaps": True},
    )

    args = mock_run.call_args[0][0]
    assert "-mipmap" in args


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/basisu")
def test_basisu_failure_raises(mock_tool, mock_run, plugin, source_png, tmp_path):
    """Subprocess error from basisu is reported."""
    mock_run.return_value = MagicMock(returncode=1, stderr="encode failed", stdout="")
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError, match="basisu failed"):
        plugin.process(
            source_png,
            output_dir,
            {"compression": "basisu", "generate_mipmaps": False},
        )


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/astcenc")
def test_astcenc_failure_raises(mock_tool, mock_run, plugin, source_png, tmp_path):
    """Subprocess error from astcenc is reported."""
    mock_run.return_value = MagicMock(returncode=1, stderr="compress failed", stdout="")
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError, match="astcenc failed"):
        plugin.process(
            source_png,
            output_dir,
            {"compression": "astc", "generate_mipmaps": False},
        )


def test_compression_invalid_format_raises(plugin, source_png, tmp_path):
    """Bad compression value is rejected."""
    output_dir = tmp_path / "out"
    with pytest.raises(ValueError, match="Unsupported compression"):
        plugin.process(
            source_png, output_dir, {"compression": "zstd", "generate_mipmaps": False}
        )


@patch("pipeline.plugins.texture.subprocess.run")
@patch("pipeline.plugins.texture._find_tool", return_value="/usr/bin/basisu")
def test_basisu_output_extension(mock_tool, mock_run, plugin, source_png, tmp_path):
    """Output file has .ktx2 extension."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "hero.ktx2").write_bytes(b"\x00" * 50)

    plugin.process(
        source_png,
        output_dir,
        {"compression": "basisu", "generate_mipmaps": False},
    )

    meta = json.loads((output_dir / "hero.meta.json").read_text(encoding="utf-8"))
    assert meta["compression"]["compressed_file"].endswith(".ktx2")


def test_basisu_invalid_format_raises(plugin, source_png, tmp_path):
    """Invalid basisu_format is rejected."""
    output_dir = tmp_path / "out"
    with pytest.raises(ValueError, match="Unsupported basisu_format"):
        plugin.process(
            source_png,
            output_dir,
            {
                "compression": "basisu",
                "basisu_format": "bad",
                "generate_mipmaps": False,
            },
        )


def test_basisu_invalid_quality_raises(plugin, source_png, tmp_path):
    """basisu_quality outside 1-255 is rejected."""
    output_dir = tmp_path / "out"
    with pytest.raises(ValueError, match="basisu_quality must be between"):
        plugin.process(
            source_png,
            output_dir,
            {
                "compression": "basisu",
                "basisu_quality": 0,
                "generate_mipmaps": False,
            },
        )


def test_astc_settings_not_validated_for_basisu(plugin, source_png, tmp_path):
    """ASTC settings are ignored when compression is basisu."""
    output_dir = tmp_path / "out"
    # Invalid ASTC settings should NOT raise when compression is basisu.
    with patch("pipeline.plugins.texture._find_tool", return_value=None):
        result = plugin.process(
            source_png,
            output_dir,
            {
                "compression": "basisu",
                "astc_block_size": "99x99",
                "astc_quality": "invalid",
                "generate_mipmaps": False,
            },
        )
    assert result.output.exists()


@patch("pipeline.plugins.texture._find_tool", return_value=None)
def test_compression_metadata_none_when_tool_missing(
    mock_tool, plugin, source_png, tmp_path
):
    """metadata['compression'] is 'none' when the tool is not installed."""
    output_dir = tmp_path / "out"
    result = plugin.process(
        source_png,
        output_dir,
        {"compression": "basisu", "generate_mipmaps": False},
    )
    assert result.metadata["compression"] == "none"
