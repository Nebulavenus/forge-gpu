"""Tests for the atlas bin packer and atlas plugin."""

from __future__ import annotations

import json
from pathlib import Path

import pytest
from PIL import Image

from pipeline.atlas import _next_power_of_two, pack_rects
from pipeline.plugins.atlas import (
    AtlasPlugin,
    _find_material_textures,
    build_atlas,
)

# ── Helpers ───────────────────────────────────────────────────────────


def _make_texture(
    path: Path, width: int, height: int, color: tuple = (255, 0, 0)
) -> Path:
    """Create a test PNG image and its .meta.json sidecar."""
    path.parent.mkdir(parents=True, exist_ok=True)
    img = Image.new("RGB", (width, height), color=color)
    img.save(path)

    meta = {
        "source": path.name,
        "output": path.name,
        "output_width": width,
        "output_height": height,
        "mip_levels": [{"level": 0, "width": width, "height": height}],
        "settings": {},
    }
    meta_path = path.parent / f"{path.stem}.meta.json"
    meta_path.write_text(json.dumps(meta), encoding="utf-8")
    return path


# ── pack_rects — unit tests ──────────────────────────────────────────


class TestPackRects:
    def test_empty_input(self):
        result = pack_rects([])
        assert result.width == 0
        assert result.height == 0
        assert result.rects == []

    def test_single_rect(self):
        result = pack_rects([("a", 64, 64)], max_size=256, padding=0)
        assert len(result.rects) == 1
        assert result.rects[0].name == "a"
        assert result.rects[0].width == 64
        assert result.rects[0].height == 64
        assert result.width == 64
        assert result.height == 64

    def test_two_rects_fit(self):
        result = pack_rects(
            [("a", 64, 64), ("b", 64, 64)],
            max_size=256,
            padding=0,
        )
        assert len(result.rects) == 2
        # Both should fit within 128x128 or smaller.
        assert result.width <= 256
        assert result.height <= 256

    def test_many_small_rects(self):
        rects = [(f"r{i}", 32, 32) for i in range(16)]
        result = pack_rects(rects, max_size=512, padding=0)
        assert len(result.rects) == 16
        # All 16 rects must fit within the atlas.
        assert result.width <= 512
        assert result.height <= 512

    def test_padding_increases_atlas_size(self):
        no_pad = pack_rects([("a", 64, 64)], max_size=256, padding=0)
        with_pad = pack_rects([("a", 64, 64)], max_size=256, padding=4)
        # With padding, the atlas must be at least 72x72 -> 128.
        assert with_pad.width >= no_pad.width

    def test_overflow_raises(self):
        with pytest.raises(ValueError, match="exceeds max_size"):
            pack_rects([("big", 512, 512)], max_size=256, padding=0)

    def test_utilization_range(self):
        result = pack_rects(
            [("a", 100, 100), ("b", 50, 50), ("c", 80, 40)],
            max_size=512,
            padding=0,
        )
        assert 0.0 < result.utilization <= 1.0

    def test_power_of_two_dimensions(self):
        result = pack_rects(
            [("a", 100, 100), ("b", 50, 200)],
            max_size=512,
            padding=0,
        )
        # Output dimensions should be powers of two.
        assert result.width & (result.width - 1) == 0
        assert result.height & (result.height - 1) == 0

    def test_rects_within_bounds(self):
        rects = [("a", 128, 64), ("b", 64, 128), ("c", 100, 100)]
        result = pack_rects(rects, max_size=512, padding=4)
        for r in result.rects:
            # Rect + padding must fit within atlas.
            assert r.x >= 0
            assert r.y >= 0
            assert r.x + r.width <= result.width
            assert r.y + r.height <= result.height

    def test_no_overlap(self):
        rects = [(f"r{i}", 32 + i * 8, 32 + i * 4) for i in range(8)]
        result = pack_rects(rects, max_size=1024, padding=0)
        # Check that no two rects overlap.
        for i, a in enumerate(result.rects):
            for b in result.rects[i + 1 :]:
                overlap_x = a.x < b.x + b.width and b.x < a.x + a.width
                overlap_y = a.y < b.y + b.height and b.y < a.y + a.height
                assert not (overlap_x and overlap_y), f"{a.name} and {b.name} overlap"

    def test_no_overlap_with_padding(self):
        rects = [(f"r{i}", 32 + i * 8, 32 + i * 4) for i in range(8)]
        padding = 4
        result = pack_rects(rects, max_size=1024, padding=padding)
        # Check that padded bounding boxes do not overlap.
        for i, a in enumerate(result.rects):
            for b in result.rects[i + 1 :]:
                ax0, ax1 = a.x - padding, a.x + a.width + padding
                ay0, ay1 = a.y - padding, a.y + a.height + padding
                bx0, bx1 = b.x - padding, b.x + b.width + padding
                by0, by1 = b.y - padding, b.y + b.height + padding
                overlap_x = ax0 < bx1 and bx0 < ax1
                overlap_y = ay0 < by1 and by0 < ay1
                assert not (overlap_x and overlap_y), (
                    f"{a.name} and {b.name} padded rects overlap"
                )


# ── _next_power_of_two ───────────────────────────────────────────────


class TestNextPowerOfTwo:
    def test_zero(self):
        assert _next_power_of_two(0) == 1

    def test_one(self):
        assert _next_power_of_two(1) == 1

    def test_exact(self):
        assert _next_power_of_two(256) == 256

    def test_non_power(self):
        assert _next_power_of_two(129) == 256

    def test_large(self):
        assert _next_power_of_two(3000) == 4096


# ── _find_material_textures ──────────────────────────────────────────


class TestFindMaterialTextures:
    def test_groups_by_material(self, tmp_path: Path):
        _make_texture(tmp_path / "brick_albedo.png", 128, 128)
        _make_texture(tmp_path / "brick_normal.png", 128, 128)
        _make_texture(tmp_path / "metal_albedo.png", 64, 64)

        materials = _find_material_textures(tmp_path)

        assert "brick" in materials
        assert "metal" in materials
        assert len(materials["brick"]) == 2
        assert len(materials["metal"]) == 1

    def test_no_meta_json(self, tmp_path: Path):
        # Create an image without a sidecar.
        img = Image.new("RGB", (64, 64))
        img.save(tmp_path / "orphan.png")

        materials = _find_material_textures(tmp_path)
        assert len(materials) == 0

    def test_unknown_suffix(self, tmp_path: Path):
        _make_texture(tmp_path / "custom_texture.png", 64, 64)
        materials = _find_material_textures(tmp_path)
        # Without a known suffix, the whole stem becomes the material name.
        assert "custom_texture" in materials


# ── build_atlas — integration ────────────────────────────────────────


class TestBuildAtlas:
    def test_builds_atlas_image(self, tmp_path: Path):
        _make_texture(tmp_path / "mat_a_albedo.png", 64, 64, (255, 0, 0))
        _make_texture(tmp_path / "mat_b_albedo.png", 64, 64, (0, 255, 0))

        result = build_atlas(tmp_path, max_size=512, padding=0)

        assert result is not None
        atlas_path = tmp_path / "atlas.png"
        assert atlas_path.is_file()

        with Image.open(atlas_path) as img:
            assert img.width > 0
            assert img.height > 0

    def test_writes_atlas_metadata(self, tmp_path: Path):
        _make_texture(tmp_path / "mat_a_albedo.png", 64, 64)
        _make_texture(tmp_path / "mat_b_albedo.png", 32, 32)

        build_atlas(tmp_path, max_size=512, padding=4)

        meta_path = tmp_path / "atlas.json"
        assert meta_path.is_file()
        meta = json.loads(meta_path.read_text(encoding="utf-8"))

        assert meta["version"] == 1
        assert meta["width"] > 0
        assert meta["height"] > 0
        assert meta["padding"] == 4
        assert "mat_a" in meta["entries"]
        assert "mat_b" in meta["entries"]

        # Each entry should have UV coordinates.
        entry = meta["entries"]["mat_a"]
        assert "u_offset" in entry
        assert "v_offset" in entry
        assert "u_scale" in entry
        assert "v_scale" in entry

    def test_returns_none_for_single_material(self, tmp_path: Path):
        _make_texture(tmp_path / "only_albedo.png", 64, 64)
        result = build_atlas(tmp_path, max_size=512, padding=0)
        assert result is None

    def test_utilization_in_metadata(self, tmp_path: Path):
        _make_texture(tmp_path / "a_albedo.png", 128, 128)
        _make_texture(tmp_path / "b_albedo.png", 64, 64)

        build_atlas(tmp_path, max_size=512, padding=0)

        meta = json.loads((tmp_path / "atlas.json").read_text(encoding="utf-8"))
        assert 0.0 < meta["utilization"] <= 1.0

    def test_padding_applied(self, tmp_path: Path):
        _make_texture(tmp_path / "a_albedo.png", 60, 60)
        _make_texture(tmp_path / "b_albedo.png", 60, 60)

        build_atlas(tmp_path, max_size=512, padding=8)

        meta = json.loads((tmp_path / "atlas.json").read_text(encoding="utf-8"))
        assert meta["padding"] == 8

        # Rects should have positions >= padding.
        for entry in meta["entries"].values():
            assert entry["x"] >= 8
            assert entry["y"] >= 8


# ── AtlasPlugin ──────────────────────────────────────────────────────


class TestAtlasPlugin:
    def test_disabled_by_default(self, tmp_path: Path):
        plugin = AtlasPlugin()
        result = plugin.process(tmp_path, tmp_path, {})
        assert result.metadata.get("processed") is False

    def test_enabled_builds_atlas(self, tmp_path: Path):
        _make_texture(tmp_path / "a_albedo.png", 64, 64)
        _make_texture(tmp_path / "b_albedo.png", 64, 64)

        plugin = AtlasPlugin()
        result = plugin.process(
            tmp_path,
            tmp_path,
            {"atlas_enabled": True, "atlas_max_size": 512, "atlas_padding": 0},
        )

        assert "utilization" in result.metadata
        assert (tmp_path / "atlas.png").is_file()
        assert (tmp_path / "atlas.json").is_file()

    def test_skips_when_few_materials(self, tmp_path: Path):
        _make_texture(tmp_path / "only_albedo.png", 64, 64)

        plugin = AtlasPlugin()
        result = plugin.process(
            tmp_path,
            tmp_path,
            {"atlas_enabled": True},
        )
        assert result.metadata.get("processed") is False
