"""Tests for pipeline.scenes — authored scene file management."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from pipeline.config import PipelineConfig
from pipeline.scenes import (
    create_scene,
    delete_scene,
    list_scenes,
    load_scene,
    save_scene,
    scenes_dir,
    validate_scene,
)

# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------


@pytest.fixture()
def config(tmp_path: Path) -> PipelineConfig:
    """A PipelineConfig with temp directories."""
    source = tmp_path / "source"
    output = tmp_path / "output"
    cache = tmp_path / "cache"
    source.mkdir()
    output.mkdir()
    cache.mkdir()
    return PipelineConfig(
        source_dir=source,
        output_dir=output,
        cache_dir=cache,
    )


# ---------------------------------------------------------------------------
# create_scene
# ---------------------------------------------------------------------------


def test_create_scene(config: PipelineConfig) -> None:
    result = create_scene(config, "Test Scene")

    assert result["name"] == "Test Scene"
    assert result["version"] == 1
    assert result["objects"] == []
    assert "id" in result
    assert "created_at" in result
    assert "modified_at" in result

    # Verify file on disk
    path = scenes_dir(config) / f"{result['id']}.json"
    assert path.is_file()
    on_disk = json.loads(path.read_text(encoding="utf-8"))
    assert on_disk["name"] == "Test Scene"


# ---------------------------------------------------------------------------
# list_scenes
# ---------------------------------------------------------------------------


def test_list_scenes_empty(config: PipelineConfig) -> None:
    result = list_scenes(config)
    assert result == []


def test_list_scenes(config: PipelineConfig) -> None:
    s1 = create_scene(config, "Alpha")
    s2 = create_scene(config, "Beta")
    s3 = create_scene(config, "Gamma")

    result = list_scenes(config)
    assert len(result) == 3

    ids = {s["id"] for s in result}
    assert ids == {s1["id"], s2["id"], s3["id"]}

    # Should be sorted by modified_at descending — most recent first
    assert result[0]["id"] == s3["id"]

    # Metadata should be present
    for item in result:
        assert "name" in item
        assert "modified_at" in item
        assert "object_count" in item
        assert item["object_count"] == 0


# ---------------------------------------------------------------------------
# load_scene
# ---------------------------------------------------------------------------


def test_load_scene(config: PipelineConfig) -> None:
    created = create_scene(config, "Round Trip")
    loaded = load_scene(config, created["id"])

    assert loaded["id"] == created["id"]
    assert loaded["name"] == "Round Trip"
    assert loaded["version"] == 1
    assert loaded["objects"] == []


def test_load_scene_not_found(config: PipelineConfig) -> None:
    with pytest.raises(FileNotFoundError):
        load_scene(config, "nonexistent")


# ---------------------------------------------------------------------------
# save_scene
# ---------------------------------------------------------------------------


def test_save_scene(config: PipelineConfig) -> None:
    created = create_scene(config, "Editable")
    original_modified = created["modified_at"]

    # Add an object
    obj = {
        "id": "obj1",
        "name": "Cube",
        "asset_id": None,
        "position": [1.0, 2.0, 3.0],
        "rotation": [0.0, 0.0, 0.0, 1.0],
        "scale": [1.0, 1.0, 1.0],
        "parent_id": None,
        "visible": True,
    }
    data = {
        "version": 1,
        "name": "Editable",
        "created_at": created["created_at"],
        "modified_at": created["modified_at"],
        "objects": [obj],
    }

    saved = save_scene(config, created["id"], data)
    assert len(saved["objects"]) == 1
    assert saved["objects"][0]["name"] == "Cube"
    assert saved["modified_at"] >= original_modified

    # Verify round-trip
    loaded = load_scene(config, created["id"])
    assert len(loaded["objects"]) == 1
    assert loaded["objects"][0]["position"] == [1.0, 2.0, 3.0]


def test_save_scene_preserves_created_at(config: PipelineConfig) -> None:
    """save_scene must keep the original created_at even if the client sends a different one."""
    created = create_scene(config, "Timestamped")
    original_created_at = created["created_at"]

    # Attempt to overwrite created_at with a tampered value
    data = {
        "version": 1,
        "name": "Timestamped",
        "created_at": "2000-01-01T00:00:00Z",
        "modified_at": created["modified_at"],
        "objects": [],
    }

    saved = save_scene(config, created["id"], data)
    assert saved["created_at"] == original_created_at

    # Verify on disk as well
    path = scenes_dir(config) / f"{created['id']}.json"
    on_disk = json.loads(path.read_text(encoding="utf-8"))
    assert on_disk["created_at"] == original_created_at


def test_save_scene_no_mutation(config: PipelineConfig) -> None:
    """save_scene must not mutate the caller's data dict."""
    created = create_scene(config, "Immutable")
    data = {
        "version": 1,
        "name": "Immutable",
        "created_at": created["created_at"],
        "modified_at": created["modified_at"],
        "objects": [],
    }
    original_modified = data["modified_at"]
    save_scene(config, created["id"], data)
    assert data["modified_at"] == original_modified
    assert "id" not in data


# ---------------------------------------------------------------------------
# delete_scene
# ---------------------------------------------------------------------------


def test_delete_scene(config: PipelineConfig) -> None:
    created = create_scene(config, "Deletable")
    scene_id = created["id"]
    path = scenes_dir(config) / f"{scene_id}.json"
    assert path.is_file()

    delete_scene(config, scene_id)
    assert not path.is_file()


def test_delete_scene_not_found(config: PipelineConfig) -> None:
    with pytest.raises(FileNotFoundError):
        delete_scene(config, "nonexistent")


# ---------------------------------------------------------------------------
# validate_scene
# ---------------------------------------------------------------------------


def test_validate_rejects_nan_and_infinity() -> None:
    base_obj = {
        "id": "a",
        "name": "Obj",
        "asset_id": None,
        "rotation": [0, 0, 0, 1],
        "scale": [1, 1, 1],
        "parent_id": None,
        "visible": True,
    }
    for bad_value in [float("nan"), float("inf"), float("-inf")]:
        data = {
            "version": 1,
            "name": "Test",
            "created_at": "2026-01-01T00:00:00Z",
            "modified_at": "2026-01-01T00:00:00Z",
            "objects": [{**base_obj, "position": [bad_value, 0, 0]}],
        }
        errors = validate_scene(data)
        assert any("position" in e for e in errors), (
            f"Expected rejection for {bad_value}"
        )


def test_validate_missing_fields() -> None:
    errors = validate_scene({})
    assert any("version" in e.lower() or "Expected version" in e for e in errors)
    assert any("name" in e for e in errors)
    assert any("objects" in e for e in errors)


def test_validate_invalid_types() -> None:
    data = {
        "version": 1,
        "name": "Test",
        "created_at": "2026-01-01T00:00:00Z",
        "modified_at": "2026-01-01T00:00:00Z",
        "objects": [
            {
                "id": "a",
                "name": "Obj",
                "asset_id": None,
                "position": "not a list",
                "rotation": [0, 0, 0, 1],
                "scale": [1, 1, 1],
                "parent_id": None,
                "visible": True,
            }
        ],
    }
    errors = validate_scene(data)
    assert any("position" in e for e in errors)


def test_validate_bad_parent_ref() -> None:
    data = {
        "version": 1,
        "name": "Test",
        "created_at": "2026-01-01T00:00:00Z",
        "modified_at": "2026-01-01T00:00:00Z",
        "objects": [
            {
                "id": "a",
                "name": "Obj",
                "asset_id": None,
                "position": [0, 0, 0],
                "rotation": [0, 0, 0, 1],
                "scale": [1, 1, 1],
                "parent_id": "nonexistent",
                "visible": True,
            }
        ],
    }
    errors = validate_scene(data)
    assert any("nonexistent" in e for e in errors)


def test_validate_circular_parent() -> None:
    data = {
        "version": 1,
        "name": "Test",
        "created_at": "2026-01-01T00:00:00Z",
        "modified_at": "2026-01-01T00:00:00Z",
        "objects": [
            {
                "id": "a",
                "name": "A",
                "asset_id": None,
                "position": [0, 0, 0],
                "rotation": [0, 0, 0, 1],
                "scale": [1, 1, 1],
                "parent_id": "b",
                "visible": True,
            },
            {
                "id": "b",
                "name": "B",
                "asset_id": None,
                "position": [0, 0, 0],
                "rotation": [0, 0, 0, 1],
                "scale": [1, 1, 1],
                "parent_id": "a",
                "visible": True,
            },
        ],
    }
    errors = validate_scene(data)
    assert any("circular" in e.lower() for e in errors)


# ---------------------------------------------------------------------------
# Path traversal protection
# ---------------------------------------------------------------------------


def test_path_traversal_rejected(config: PipelineConfig) -> None:
    with pytest.raises(ValueError, match="Invalid scene ID"):
        load_scene(config, "../etc/passwd")

    with pytest.raises(ValueError, match="Invalid scene ID"):
        delete_scene(config, "../../foo")

    with pytest.raises(ValueError, match="Invalid scene ID"):
        save_scene(config, "foo/bar", {})

    with pytest.raises(ValueError, match="must not be empty"):
        load_scene(config, "")
