"""Tests for pipeline.import_settings — TOML sidecar CRUD and merging."""

from __future__ import annotations

from pathlib import Path

import pytest

from pipeline.import_settings import (
    SIDECAR_SUFFIX,
    delete_sidecar,
    get_effective_settings,
    get_schema,
    load_sidecar,
    merge_settings,
    save_sidecar,
    sidecar_path,
)

# ---------------------------------------------------------------------------
# sidecar_path
# ---------------------------------------------------------------------------


def test_sidecar_path_simple() -> None:
    src = Path("/project/assets/brick.png")
    assert sidecar_path(src) == Path("/project/assets/brick.png.import.toml")


def test_sidecar_path_nested() -> None:
    src = Path("/project/assets/textures/wall/brick.png")
    expected = Path("/project/assets/textures/wall/brick.png.import.toml")
    assert sidecar_path(src) == expected


def test_sidecar_suffix() -> None:
    assert SIDECAR_SUFFIX == ".import.toml"


# ---------------------------------------------------------------------------
# load_sidecar
# ---------------------------------------------------------------------------


def test_load_sidecar_missing(tmp_path: Path) -> None:
    """load_sidecar returns empty dict when no sidecar exists."""
    src = tmp_path / "texture.png"
    src.write_bytes(b"fake png")
    assert load_sidecar(src) == {}


def test_load_sidecar_valid(tmp_path: Path) -> None:
    """load_sidecar parses a valid .import.toml."""
    src = tmp_path / "brick.png"
    src.write_bytes(b"fake png")
    sidecar = tmp_path / "brick.png.import.toml"
    sidecar.write_text(
        'normal_map = true\ncompression = "basisu"\nbasisu_quality = 200\n',
        encoding="utf-8",
    )
    result = load_sidecar(src)
    assert result == {
        "normal_map": True,
        "compression": "basisu",
        "basisu_quality": 200,
    }


def test_load_sidecar_malformed(tmp_path: Path) -> None:
    """load_sidecar raises ValueError on malformed TOML."""
    src = tmp_path / "bad.png"
    src.write_bytes(b"fake")
    sidecar = tmp_path / "bad.png.import.toml"
    sidecar.write_text("this is not valid toml = = =", encoding="utf-8")
    with pytest.raises(ValueError, match="Malformed"):
        load_sidecar(src)


# ---------------------------------------------------------------------------
# save_sidecar
# ---------------------------------------------------------------------------


def test_save_sidecar_creates_file(tmp_path: Path) -> None:
    """save_sidecar writes a .import.toml with correct content."""
    src = tmp_path / "wall.png"
    src.write_bytes(b"fake")
    save_sidecar(src, {"max_size": 1024, "normal_map": True})
    path = sidecar_path(src)
    assert path.is_file()
    content = path.read_text(encoding="utf-8")
    assert "max_size = 1024" in content
    assert "normal_map = true" in content


def test_save_sidecar_roundtrip(tmp_path: Path) -> None:
    """save then load produces the same data."""
    src = tmp_path / "metal.png"
    src.write_bytes(b"fake")
    original = {
        "compression": "basisu",
        "basisu_quality": 128,
        "generate_mipmaps": False,
    }
    save_sidecar(src, original)
    loaded = load_sidecar(src)
    assert loaded == original


def test_save_sidecar_empty_deletes(tmp_path: Path) -> None:
    """save_sidecar with empty dict deletes the sidecar."""
    src = tmp_path / "sky.png"
    src.write_bytes(b"fake")
    save_sidecar(src, {"max_size": 512})
    assert sidecar_path(src).is_file()
    save_sidecar(src, {})
    assert not sidecar_path(src).is_file()


def test_save_sidecar_string_value(tmp_path: Path) -> None:
    """String values are properly quoted in TOML output."""
    src = tmp_path / "tex.png"
    src.write_bytes(b"fake")
    save_sidecar(src, {"output_format": "jpg"})
    content = sidecar_path(src).read_text(encoding="utf-8")
    assert 'output_format = "jpg"' in content


def test_save_sidecar_float_list(tmp_path: Path) -> None:
    """Float lists are written as TOML inline arrays."""
    src = tmp_path / "mesh.gltf"
    src.write_bytes(b"fake")
    save_sidecar(src, {"lod_levels": [1.0, 0.5, 0.25]})
    loaded = load_sidecar(src)
    assert loaded["lod_levels"] == [1.0, 0.5, 0.25]


# ---------------------------------------------------------------------------
# delete_sidecar
# ---------------------------------------------------------------------------


def test_delete_sidecar_exists(tmp_path: Path) -> None:
    src = tmp_path / "del.png"
    src.write_bytes(b"fake")
    save_sidecar(src, {"max_size": 256})
    assert delete_sidecar(src) is True
    assert not sidecar_path(src).is_file()


def test_delete_sidecar_missing(tmp_path: Path) -> None:
    src = tmp_path / "nope.png"
    src.write_bytes(b"fake")
    assert delete_sidecar(src) is False


# ---------------------------------------------------------------------------
# merge_settings
# ---------------------------------------------------------------------------


def test_merge_empty() -> None:
    assert merge_settings({}, {}) == {}


def test_merge_global_only() -> None:
    result = merge_settings({"max_size": 2048}, {})
    assert result == {"max_size": 2048}


def test_merge_override() -> None:
    result = merge_settings(
        {"max_size": 2048, "compression": "none"},
        {"max_size": 512},
    )
    assert result == {"max_size": 512, "compression": "none"}


def test_merge_adds_new_keys() -> None:
    result = merge_settings(
        {"max_size": 2048},
        {"normal_map": True},
    )
    assert result == {"max_size": 2048, "normal_map": True}


def test_merge_does_not_mutate_inputs() -> None:
    global_s = {"max_size": 2048}
    per_asset = {"max_size": 512}
    merge_settings(global_s, per_asset)
    assert global_s == {"max_size": 2048}
    assert per_asset == {"max_size": 512}


# ---------------------------------------------------------------------------
# get_effective_settings
# ---------------------------------------------------------------------------


def test_effective_schema_defaults() -> None:
    """With no global or per-asset settings, schema defaults are used."""
    result = get_effective_settings("texture", {}, {})
    assert result["max_size"] == 2048
    assert result["generate_mipmaps"] is True
    assert result["compression"] == "none"


def test_effective_global_overrides_schema() -> None:
    result = get_effective_settings("texture", {"max_size": 1024}, {})
    assert result["max_size"] == 1024


def test_effective_per_asset_overrides_global() -> None:
    result = get_effective_settings(
        "texture",
        {"max_size": 1024},
        {"max_size": 256},
    )
    assert result["max_size"] == 256


def test_effective_all_three_layers() -> None:
    result = get_effective_settings(
        "texture",
        {"max_size": 1024, "compression": "basisu"},
        {"normal_map": True},
    )
    # Schema default for generate_mipmaps
    assert result["generate_mipmaps"] is True
    # Global override
    assert result["max_size"] == 1024
    assert result["compression"] == "basisu"
    # Per-asset override
    assert result["normal_map"] is True


# ---------------------------------------------------------------------------
# get_schema
# ---------------------------------------------------------------------------


def test_get_schema_texture() -> None:
    schema = get_schema("texture")
    assert schema is not None
    assert "max_size" in schema
    assert "normal_map" in schema


def test_get_schema_mesh() -> None:
    schema = get_schema("mesh")
    assert schema is not None
    assert "deduplicate" in schema
    assert "lod_levels" in schema


def test_get_schema_unknown() -> None:
    assert get_schema("audio") is None
