"""Tests for pipeline.server."""

from __future__ import annotations

import json
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest
from fastapi.testclient import TestClient

from pipeline.config import PipelineConfig
from pipeline.import_settings import MESH_SETTINGS_SCHEMA, TEXTURE_SETTINGS_SCHEMA
from pipeline.plugin import AssetResult
from pipeline.scanner import fingerprint_file
from pipeline.server import create_app

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _setup(
    tmp_path: Path,
    *,
    source_files: dict[str, bytes] | None = None,
    output_files: dict[str, bytes] | None = None,
    cache_entries: dict[str, str] | None = None,
) -> tuple[TestClient, PipelineConfig]:
    """Create temp directories, populate them, and return a test client.

    *source_files* maps relative paths to file contents for the source dir.
    *output_files* maps relative paths to file contents for the output dir.
    *cache_entries* maps POSIX relative paths to fingerprint strings for the
    fingerprint cache JSON.
    """
    source_dir = tmp_path / "source"
    output_dir = tmp_path / "output"
    cache_dir = tmp_path / "cache"

    source_dir.mkdir()
    output_dir.mkdir()
    cache_dir.mkdir()

    if source_files:
        for rel, data in source_files.items():
            p = source_dir / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)

    if output_files:
        for rel, data in output_files.items():
            p = output_dir / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)

    if cache_entries is not None:
        fp_path = cache_dir / "fingerprints.json"
        fp_path.write_text(json.dumps(cache_entries), encoding="utf-8")

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=output_dir,
        cache_dir=cache_dir,
    )
    client = TestClient(create_app(config))
    return client, config


# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------

SAMPLE_SOURCES: dict[str, bytes] = {
    "b_textures/brick.png": b"PNG fake",
    "a_models/hero.gltf": b'{"asset":{}}',
    "c_anims/walk.fanim": b"FANIM data",
}


@pytest.fixture()
def three_assets(tmp_path: Path) -> tuple[TestClient, PipelineConfig]:
    """Three source files of different types, no cache or outputs."""
    return _setup(tmp_path, source_files=SAMPLE_SOURCES)


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


def test_assets_list(three_assets: tuple[TestClient, PipelineConfig]) -> None:
    client, _ = three_assets
    resp = client.get("/api/assets")
    assert resp.status_code == 200

    body = resp.json()
    assert body["total"] == 3
    assert len(body["assets"]) == 3

    types = {a["asset_type"] for a in body["assets"]}
    assert types == {"texture", "mesh", "animation"}

    # Every asset has all required fields
    required = {
        "id",
        "name",
        "relative_path",
        "asset_type",
        "source_path",
        "output_path",
        "fingerprint",
        "file_size",
        "output_size",
        "status",
    }
    for asset in body["assets"]:
        assert required <= set(asset.keys())


def test_assets_filter_by_type(tmp_path: Path) -> None:
    client, _ = _setup(
        tmp_path,
        source_files={
            "a.png": b"tex1",
            "b.jpg": b"tex2",
            "c.gltf": b"mesh",
        },
    )
    resp = client.get("/api/assets", params={"type": "texture"})
    assert resp.status_code == 200
    body = resp.json()
    assert body["total"] == 2
    assert all(a["asset_type"] == "texture" for a in body["assets"])


def test_assets_search(tmp_path: Path) -> None:
    client, _ = _setup(
        tmp_path,
        source_files={
            "brick_albedo.png": b"a",
            "brick_normal.png": b"b",
            "metal_rough.png": b"c",
        },
    )
    resp = client.get("/api/assets", params={"search": "brick"})
    assert resp.status_code == 200
    body = resp.json()
    assert body["total"] == 2
    names = {a["name"] for a in body["assets"]}
    assert names == {"brick_albedo.png", "brick_normal.png"}


def test_asset_detail(tmp_path: Path) -> None:
    client, config = _setup(tmp_path, source_files={"hero.png": b"PNG data"})
    resp = client.get("/api/assets/hero")
    assert resp.status_code == 200

    asset = resp.json()
    assert asset["id"] == "hero"
    assert asset["name"] == "hero.png"
    assert asset["relative_path"] == "hero.png"
    assert asset["asset_type"] == "texture"
    assert asset["file_size"] == len(b"PNG data")
    assert asset["status"] == "new"


def test_asset_not_found(three_assets: tuple[TestClient, PipelineConfig]) -> None:
    client, _ = three_assets
    resp = client.get("/api/assets/does-not-exist")
    assert resp.status_code == 404


def test_status(three_assets: tuple[TestClient, PipelineConfig]) -> None:
    client, config = three_assets
    resp = client.get("/api/status")
    assert resp.status_code == 200

    body = resp.json()
    assert body["total"] == 3
    assert body["by_type"]["texture"] == 1
    assert body["by_type"]["mesh"] == 1
    assert body["by_type"]["animation"] == 1
    # All are new (no cache)
    assert body["by_status"]["new"] == 3
    assert body["source_dir"] == str(config.source_dir)
    assert body["output_dir"] == str(config.output_dir)
    # Verify actionable_count field is present (value depends on discovered plugins)
    assert "actionable_count" in body


def test_asset_status_processed(tmp_path: Path) -> None:
    source_data = b"PNG processed data"
    source_files = {"tex/wall.png": source_data}
    output_files = {"tex/wall.png": b"output data"}

    # Write the source file first to compute its fingerprint
    src = tmp_path / "pre" / "tex"
    src.mkdir(parents=True)
    (src / "wall.png").write_bytes(source_data)
    fp = fingerprint_file(src / "wall.png")

    cache_entries = {"tex/wall.png": fp}
    client, _ = _setup(
        tmp_path,
        source_files=source_files,
        output_files=output_files,
        cache_entries=cache_entries,
    )
    resp = client.get("/api/assets/tex--wall")
    assert resp.status_code == 200
    assert resp.json()["status"] == "processed"


def test_asset_status_new(tmp_path: Path) -> None:
    client, _ = _setup(tmp_path, source_files={"new.png": b"brand new"})
    resp = client.get("/api/assets/new")
    assert resp.status_code == 200
    assert resp.json()["status"] == "new"


def test_asset_status_changed(tmp_path: Path) -> None:
    client, _ = _setup(
        tmp_path,
        source_files={"old.png": b"version 2"},
        cache_entries={"old.png": "stale_fingerprint_that_wont_match"},
    )
    resp = client.get("/api/assets/old")
    assert resp.status_code == 200
    assert resp.json()["status"] == "changed"


def test_empty_source_dir(tmp_path: Path) -> None:
    client, _ = _setup(tmp_path)
    resp = client.get("/api/assets")
    assert resp.status_code == 200
    body = resp.json()
    assert body["total"] == 0
    assert body["assets"] == []


def test_websocket_connect(tmp_path: Path) -> None:
    client, _ = _setup(tmp_path)
    with client.websocket_connect("/ws/status") as ws:
        data = ws.receive_json()
        assert data["type"] == "heartbeat"
        assert "timestamp" in data


# ---------------------------------------------------------------------------
# File-serving endpoint tests
# ---------------------------------------------------------------------------


def test_file_source(tmp_path: Path) -> None:
    """GET /api/assets/{id}/file returns the source file by default."""
    png_data = b"\x89PNG\r\n\x1a\n fake png content"
    client, _ = _setup(tmp_path, source_files={"brick.png": png_data})

    resp = client.get("/api/assets/brick/file")
    assert resp.status_code == 200
    assert resp.content == png_data
    assert resp.headers["content-type"] == "image/png"


def test_file_processed(tmp_path: Path) -> None:
    """GET /api/assets/{id}/file?variant=processed returns the output file."""
    source_data = b"source png"
    processed_data = b"processed png"
    client, _ = _setup(
        tmp_path,
        source_files={"wall.png": source_data},
        output_files={"wall.png": processed_data},
    )

    resp = client.get("/api/assets/wall/file", params={"variant": "processed"})
    assert resp.status_code == 200
    assert resp.content == processed_data


def test_file_not_found(tmp_path: Path) -> None:
    """GET /api/assets/{id}/file returns 404 for a nonexistent asset."""
    client, _ = _setup(tmp_path)

    resp = client.get("/api/assets/nonexistent/file")
    assert resp.status_code == 404


def test_file_no_processed(tmp_path: Path) -> None:
    """GET /api/assets/{id}/file?variant=processed returns 404 when no output exists."""
    client, _ = _setup(tmp_path, source_files={"sky.png": b"sky data"})

    resp = client.get("/api/assets/sky/file", params={"variant": "processed"})
    assert resp.status_code == 404


def test_file_gltf_content_type(tmp_path: Path) -> None:
    """GET /api/assets/{id}/file for a .gltf file returns model/gltf+json."""
    gltf_data = b'{"asset":{"version":"2.0"}}'
    client, _ = _setup(tmp_path, source_files={"scene.gltf": gltf_data})

    resp = client.get("/api/assets/scene/file")
    assert resp.status_code == 200
    assert resp.content == gltf_data
    assert resp.headers["content-type"] == "model/gltf+json"


def test_companions_bin(tmp_path: Path) -> None:
    """GET /api/assets/{id}/companions?path=model.bin serves the companion file."""
    gltf_data = b'{"asset":{"version":"2.0"}}'
    bin_data = b"\x00\x01\x02\x03 binary mesh data"
    client, _ = _setup(
        tmp_path,
        source_files={
            "models/scene.gltf": gltf_data,
            "models/model.bin": bin_data,
        },
    )

    resp = client.get(
        "/api/assets/models--scene/companions",
        params={"path": "model.bin"},
    )
    assert resp.status_code == 200
    assert resp.content == bin_data


def test_companions_traversal_blocked(tmp_path: Path) -> None:
    """GET /api/assets/{id}/companions rejects path traversal attempts."""
    client, _ = _setup(
        tmp_path,
        source_files={"models/scene.gltf": b'{"asset":{}}'},
    )

    resp = client.get(
        "/api/assets/models--scene/companions",
        params={"path": "../../etc/passwd"},
    )
    assert resp.status_code == 403


def test_companions_sibling_dir_blocked(tmp_path: Path) -> None:
    """Sibling directory whose name starts with source_dir must be rejected."""
    source_dir = tmp_path / "source"
    sibling_dir = tmp_path / "source_evil"
    source_dir.mkdir()
    sibling_dir.mkdir()
    (source_dir / "scene.gltf").write_bytes(b'{"asset":{}}')
    (sibling_dir / "secret.txt").write_bytes(b"sensitive data")

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=tmp_path / "output",
        cache_dir=tmp_path / "cache",
    )
    (tmp_path / "output").mkdir()
    (tmp_path / "cache").mkdir()
    client = TestClient(create_app(config))

    resp = client.get(
        "/api/assets/scene/companions",
        params={"path": "../source_evil/secret.txt"},
    )
    assert resp.status_code == 403


def test_companions_not_found(tmp_path: Path) -> None:
    """GET /api/assets/{id}/companions returns 404 for a missing companion."""
    client, _ = _setup(
        tmp_path,
        source_files={"models/scene.gltf": b'{"asset":{}}'},
    )

    resp = client.get(
        "/api/assets/models--scene/companions",
        params={"path": "missing.bin"},
    )
    assert resp.status_code == 404


# ---------------------------------------------------------------------------
# Import settings endpoint tests
# ---------------------------------------------------------------------------


def test_get_settings_texture(tmp_path: Path) -> None:
    """GET /api/assets/{id}/settings returns schema, global, and effective settings."""
    client, _ = _setup(tmp_path, source_files={"brick.png": b"PNG data"})
    resp = client.get("/api/assets/brick/settings")
    assert resp.status_code == 200

    body = resp.json()
    assert "schema_fields" in body
    assert "effective" in body
    assert "per_asset" in body
    assert "global_settings" in body
    assert body["has_overrides"] is False
    assert body["per_asset"] == {}
    # Schema should match the texture schema keys
    assert set(body["schema_fields"].keys()) == set(TEXTURE_SETTINGS_SCHEMA.keys())
    # Effective should have all schema defaults
    assert body["effective"]["max_size"] == 2048
    assert body["effective"]["generate_mipmaps"] is True


def test_get_settings_mesh(tmp_path: Path) -> None:
    """GET /api/assets/{id}/settings works for mesh assets."""
    client, _ = _setup(tmp_path, source_files={"hero.gltf": b'{"asset":{}}'})
    resp = client.get("/api/assets/hero/settings")
    assert resp.status_code == 200

    body = resp.json()
    assert set(body["schema_fields"].keys()) == set(MESH_SETTINGS_SCHEMA.keys())
    assert body["effective"]["deduplicate"] is True
    assert body["effective"]["optimize"] is True


def test_get_settings_not_found(tmp_path: Path) -> None:
    """GET /api/assets/{id}/settings returns 404 for a nonexistent asset."""
    client, _ = _setup(tmp_path)
    resp = client.get("/api/assets/nonexistent/settings")
    assert resp.status_code == 404


def test_get_settings_no_schema(tmp_path: Path) -> None:
    """GET /api/assets/{id}/settings returns 400 for types without a schema."""
    client, _ = _setup(tmp_path, source_files={"walk.fanim": b"FANIM data"})
    resp = client.get("/api/assets/walk/settings")
    assert resp.status_code == 400
    assert "No settings schema" in resp.json()["detail"]


def test_get_settings_with_existing_sidecar(tmp_path: Path) -> None:
    """GET /api/assets/{id}/settings reads an existing .import.toml sidecar."""
    source_dir = tmp_path / "source"
    source_dir.mkdir()
    (source_dir / "wall.png").write_bytes(b"PNG data")
    sidecar = source_dir / "wall.png.import.toml"
    sidecar.write_text("normal_map = true\nmax_size = 512\n", encoding="utf-8")

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=tmp_path / "output",
        cache_dir=tmp_path / "cache",
    )
    (tmp_path / "output").mkdir()
    (tmp_path / "cache").mkdir()
    client = TestClient(create_app(config))

    resp = client.get("/api/assets/wall/settings")
    assert resp.status_code == 200

    body = resp.json()
    assert body["has_overrides"] is True
    assert body["per_asset"]["normal_map"] is True
    assert body["per_asset"]["max_size"] == 512
    # Effective should merge: per-asset overrides schema defaults
    assert body["effective"]["normal_map"] is True
    assert body["effective"]["max_size"] == 512
    # Other defaults still present
    assert body["effective"]["generate_mipmaps"] is True


def test_get_settings_malformed_sidecar(tmp_path: Path) -> None:
    """GET /api/assets/{id}/settings returns 400 for a malformed .import.toml."""
    source_dir = tmp_path / "source"
    source_dir.mkdir()
    (source_dir / "wall.png").write_bytes(b"PNG data")
    sidecar = source_dir / "wall.png.import.toml"
    sidecar.write_text("this is not valid toml {{{{", encoding="utf-8")

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=tmp_path / "output",
        cache_dir=tmp_path / "cache",
    )
    (tmp_path / "output").mkdir()
    (tmp_path / "cache").mkdir()
    client = TestClient(create_app(config))

    resp = client.get("/api/assets/wall/settings")
    assert resp.status_code == 400
    assert "Malformed" in resp.json()["detail"]


def test_get_settings_with_global_config(tmp_path: Path) -> None:
    """GET /api/assets/{id}/settings reflects global plugin_settings."""
    source_dir = tmp_path / "source"
    source_dir.mkdir()
    (source_dir / "sky.png").write_bytes(b"PNG data")

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=tmp_path / "output",
        cache_dir=tmp_path / "cache",
        plugin_settings={"texture": {"max_size": 1024, "output_format": "jpg"}},
    )
    (tmp_path / "output").mkdir()
    (tmp_path / "cache").mkdir()
    client = TestClient(create_app(config))

    resp = client.get("/api/assets/sky/settings")
    assert resp.status_code == 200

    body = resp.json()
    assert body["global_settings"] == {"max_size": 1024, "output_format": "jpg"}
    # Effective: schema defaults overridden by global
    assert body["effective"]["max_size"] == 1024
    assert body["effective"]["output_format"] == "jpg"
    # Schema defaults still fill in the rest
    assert body["effective"]["generate_mipmaps"] is True


def test_put_settings(tmp_path: Path) -> None:
    """PUT /api/assets/{id}/settings creates a sidecar and returns updated settings."""
    source_dir = tmp_path / "source"
    source_dir.mkdir()
    (source_dir / "brick.png").write_bytes(b"PNG data")

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=tmp_path / "output",
        cache_dir=tmp_path / "cache",
    )
    (tmp_path / "output").mkdir()
    (tmp_path / "cache").mkdir()
    client = TestClient(create_app(config))

    overrides = {"normal_map": True, "max_size": 256}
    resp = client.put("/api/assets/brick/settings", json=overrides)
    assert resp.status_code == 200

    body = resp.json()
    assert body["has_overrides"] is True
    assert body["per_asset"]["normal_map"] is True
    assert body["per_asset"]["max_size"] == 256
    assert body["effective"]["normal_map"] is True
    assert body["effective"]["max_size"] == 256

    # Sidecar file should exist on disk
    sidecar = source_dir / "brick.png.import.toml"
    assert sidecar.is_file()


def test_put_settings_not_found(tmp_path: Path) -> None:
    """PUT /api/assets/{id}/settings returns 404 for a nonexistent asset."""
    client, _ = _setup(tmp_path)
    resp = client.put("/api/assets/nonexistent/settings", json={"max_size": 512})
    assert resp.status_code == 404


def test_put_settings_no_schema(tmp_path: Path) -> None:
    """PUT /api/assets/{id}/settings returns 400 for types without a schema."""
    client, _ = _setup(tmp_path, source_files={"walk.fanim": b"FANIM data"})
    resp = client.put("/api/assets/walk/settings", json={"key": "value"})
    assert resp.status_code == 400


def test_put_settings_non_dict_body(tmp_path: Path) -> None:
    """PUT /api/assets/{id}/settings rejects a non-object JSON body."""
    client, _ = _setup(tmp_path, source_files={"brick.png": b"PNG data"})
    resp = client.put("/api/assets/brick/settings", json=["not", "a", "dict"])
    assert resp.status_code == 400
    assert "JSON object" in resp.json()["detail"]


def test_put_settings_unknown_keys(tmp_path: Path) -> None:
    """PUT /api/assets/{id}/settings rejects keys not in the schema."""
    client, _ = _setup(tmp_path, source_files={"brick.png": b"PNG data"})
    resp = client.put(
        "/api/assets/brick/settings",
        json={"max_size": 512, "totally_fake_key": True},
    )
    assert resp.status_code == 400
    assert "totally_fake_key" in resp.json()["detail"]


def test_delete_settings(tmp_path: Path) -> None:
    """DELETE /api/assets/{id}/settings removes the sidecar."""
    source_dir = tmp_path / "source"
    source_dir.mkdir()
    (source_dir / "brick.png").write_bytes(b"PNG data")
    sidecar = source_dir / "brick.png.import.toml"
    sidecar.write_text("normal_map = true\n", encoding="utf-8")

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=tmp_path / "output",
        cache_dir=tmp_path / "cache",
    )
    (tmp_path / "output").mkdir()
    (tmp_path / "cache").mkdir()
    client = TestClient(create_app(config))

    # Confirm sidecar exists first
    assert sidecar.is_file()

    resp = client.delete("/api/assets/brick/settings")
    assert resp.status_code == 200

    body = resp.json()
    assert body["has_overrides"] is False
    assert body["per_asset"] == {}
    # Effective reverts to schema defaults
    assert body["effective"]["normal_map"] is False

    # Sidecar should be gone
    assert not sidecar.is_file()


def test_delete_settings_not_found(tmp_path: Path) -> None:
    """DELETE /api/assets/{id}/settings returns 404 for a nonexistent asset."""
    client, _ = _setup(tmp_path)
    resp = client.delete("/api/assets/nonexistent/settings")
    assert resp.status_code == 404


def test_delete_settings_no_schema(tmp_path: Path) -> None:
    """DELETE /api/assets/{id}/settings returns 400 for types without a schema."""
    client, _ = _setup(tmp_path, source_files={"walk.fanim": b"FANIM data"})
    resp = client.delete("/api/assets/walk/settings")
    assert resp.status_code == 400
    assert "No settings schema" in resp.json()["detail"]


def test_delete_settings_no_sidecar(tmp_path: Path) -> None:
    """DELETE /api/assets/{id}/settings succeeds even when no sidecar exists."""
    client, _ = _setup(tmp_path, source_files={"brick.png": b"PNG data"})
    resp = client.delete("/api/assets/brick/settings")
    assert resp.status_code == 200
    assert resp.json()["has_overrides"] is False


def test_settings_roundtrip(tmp_path: Path) -> None:
    """PUT then GET returns the same per-asset overrides."""
    source_dir = tmp_path / "source"
    source_dir.mkdir()
    (source_dir / "wall.png").write_bytes(b"PNG data")

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=tmp_path / "output",
        cache_dir=tmp_path / "cache",
    )
    (tmp_path / "output").mkdir()
    (tmp_path / "cache").mkdir()
    client = TestClient(create_app(config))

    overrides = {"normal_map": True, "max_size": 512, "output_format": "jpg"}
    client.put("/api/assets/wall/settings", json=overrides)

    resp = client.get("/api/assets/wall/settings")
    assert resp.status_code == 200
    body = resp.json()
    assert body["per_asset"]["normal_map"] is True
    assert body["per_asset"]["max_size"] == 512
    assert body["per_asset"]["output_format"] == "jpg"


def test_put_then_delete_settings(tmp_path: Path) -> None:
    """PUT overrides, then DELETE reverts to defaults."""
    source_dir = tmp_path / "source"
    source_dir.mkdir()
    (source_dir / "wall.png").write_bytes(b"PNG data")

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=tmp_path / "output",
        cache_dir=tmp_path / "cache",
    )
    (tmp_path / "output").mkdir()
    (tmp_path / "cache").mkdir()
    client = TestClient(create_app(config))

    # Set overrides
    client.put("/api/assets/wall/settings", json={"normal_map": True})
    # Delete them
    client.delete("/api/assets/wall/settings")

    resp = client.get("/api/assets/wall/settings")
    assert resp.status_code == 200
    body = resp.json()
    assert body["has_overrides"] is False
    assert body["effective"]["normal_map"] is False


# ---------------------------------------------------------------------------
# Process endpoint tests
# ---------------------------------------------------------------------------


def _setup_process(
    tmp_path: Path,
    *,
    source_files: dict[str, bytes] | None = None,
    plugin_settings: dict[str, dict] | None = None,
    mock_plugin: MagicMock | None = None,
) -> tuple[TestClient, PipelineConfig, MagicMock]:
    """Create a test client with a mocked plugin registry for process tests.

    The registry is patched *before* ``create_app`` so the closure captures
    the mock instance rather than a real registry.  After ``create_app``
    returns, the patch context can exit safely because the server module's
    ``_registry`` variable holds a reference to the mock instance, which
    persists and responds to ``get_by_extension()`` calls during test requests.
    """
    source_dir = tmp_path / "source"
    output_dir = tmp_path / "output"
    cache_dir = tmp_path / "cache"
    source_dir.mkdir()
    output_dir.mkdir()
    cache_dir.mkdir()

    if source_files:
        for rel, data in source_files.items():
            p = source_dir / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_bytes(data)

    if mock_plugin is None:
        mock_plugin = MagicMock()
        mock_plugin.name = "texture"
        mock_plugin.extensions = [".png"]
        # Default: return a successful AssetResult
        mock_plugin.process.return_value = AssetResult(
            source=Path("dummy"), output=Path("dummy"), metadata={}
        )

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=output_dir,
        cache_dir=cache_dir,
        plugin_settings=plugin_settings or {},
    )

    # Patch PluginRegistry before create_app so the closure captures the mock
    with patch("pipeline.server.PluginRegistry") as MockRegistry:
        instance = MockRegistry.return_value
        instance.discover.return_value = 1
        instance.get_by_extension.return_value = [mock_plugin]
        client = TestClient(create_app(config))

    return client, config, mock_plugin


def test_process_asset(tmp_path: Path) -> None:
    """POST /api/assets/{id}/process invokes the plugin and updates the cache."""
    client, config, mock_plugin = _setup_process(
        tmp_path, source_files={"brick.png": b"PNG data"}
    )

    resp = client.post("/api/assets/brick/process")

    assert resp.status_code == 200
    assert resp.json()["message"] == "Processed brick.png"
    mock_plugin.process.assert_called_once()

    # The call args: (source_path, output_subdir, effective_settings)
    call_args = mock_plugin.process.call_args
    assert call_args[0][0] == config.source_dir / "brick.png"
    assert call_args[0][1] == config.output_dir

    # Fingerprint cache should be updated
    fp_path = config.cache_dir / "fingerprints.json"
    assert fp_path.is_file()
    cache_data = json.loads(fp_path.read_text(encoding="utf-8"))
    assert "brick.png" in cache_data


def test_process_asset_with_settings(tmp_path: Path) -> None:
    """POST /api/assets/{id}/process passes effective settings to the plugin."""
    source_files = {"wall.png": b"PNG data"}
    client, config, mock_plugin = _setup_process(
        tmp_path,
        source_files=source_files,
        plugin_settings={"texture": {"output_format": "jpg"}},
    )

    # Write a sidecar with overrides
    sidecar = config.source_dir / "wall.png.import.toml"
    sidecar.write_text("normal_map = true\nmax_size = 256\n", encoding="utf-8")

    resp = client.post("/api/assets/wall/process")

    assert resp.status_code == 200

    # Check effective settings passed to plugin: schema + global + per-asset
    effective = mock_plugin.process.call_args[0][2]
    assert effective["normal_map"] is True  # per-asset override
    assert effective["max_size"] == 256  # per-asset override
    assert effective["output_format"] == "jpg"  # global override
    assert effective["generate_mipmaps"] is True  # schema default


def test_process_asset_not_found(tmp_path: Path) -> None:
    """POST /api/assets/{id}/process returns 404 for a nonexistent asset."""
    client, _, _ = _setup_process(tmp_path)
    resp = client.post("/api/assets/nonexistent/process")
    assert resp.status_code == 404


def test_process_asset_no_plugin(tmp_path: Path) -> None:
    """POST /api/assets/{id}/process returns 400 when no plugin handles the extension."""
    mock_plugin = MagicMock()
    mock_plugin.name = "texture"
    mock_plugin.extensions = [".png"]

    source_dir = tmp_path / "source"
    output_dir = tmp_path / "output"
    cache_dir = tmp_path / "cache"
    source_dir.mkdir()
    output_dir.mkdir()
    cache_dir.mkdir()
    (source_dir / "data.xyz").write_bytes(b"unknown format")

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=output_dir,
        cache_dir=cache_dir,
    )

    # Registry returns no plugins for .xyz
    with patch("pipeline.server.PluginRegistry") as MockRegistry:
        instance = MockRegistry.return_value
        instance.discover.return_value = 0
        instance.get_by_extension.return_value = []
        client = TestClient(create_app(config))

    resp = client.post("/api/assets/data/process")

    assert resp.status_code == 400
    assert "No plugin found" in resp.json()["detail"]


def test_process_creates_output_subdir(tmp_path: Path) -> None:
    """POST /api/assets/{id}/process creates output subdirectories as needed."""
    client, config, mock_plugin = _setup_process(
        tmp_path, source_files={"textures/brick.png": b"PNG data"}
    )

    resp = client.post("/api/assets/textures--brick/process")

    assert resp.status_code == 200
    # Output subdirectory should have been created
    assert (config.output_dir / "textures").is_dir()
    # Plugin should receive the subdirectory as output_dir
    call_args = mock_plugin.process.call_args
    assert call_args[0][1] == config.output_dir / "textures"


# ---------------------------------------------------------------------------
# Scene editor endpoints
# ---------------------------------------------------------------------------


def test_scene_create(tmp_path: Path) -> None:
    """POST /api/scenes creates a new scene."""
    client, _ = _setup(tmp_path)
    resp = client.post("/api/scenes", json={"name": "My Scene"})
    assert resp.status_code == 201
    data = resp.json()
    assert data["name"] == "My Scene"
    assert data["version"] == 1
    assert data["objects"] == []
    assert "id" in data


def test_scene_list(tmp_path: Path) -> None:
    """GET /api/scenes returns all scenes."""
    client, _ = _setup(tmp_path)
    client.post("/api/scenes", json={"name": "A"})
    client.post("/api/scenes", json={"name": "B"})

    resp = client.get("/api/scenes")
    assert resp.status_code == 200
    data = resp.json()
    assert data["total"] == 2
    assert len(data["scenes"]) == 2


def test_scene_get(tmp_path: Path) -> None:
    """GET /api/scenes/{id} returns a single scene."""
    client, _ = _setup(tmp_path)
    created = client.post("/api/scenes", json={"name": "Test"}).json()

    resp = client.get(f"/api/scenes/{created['id']}")
    assert resp.status_code == 200
    data = resp.json()
    assert data["id"] == created["id"]
    assert data["name"] == "Test"


def test_scene_save(tmp_path: Path) -> None:
    """PUT /api/scenes/{id} updates scene objects."""
    client, _ = _setup(tmp_path)
    created = client.post("/api/scenes", json={"name": "Editable"}).json()

    scene_data = {
        "version": 1,
        "name": "Editable",
        "created_at": created["created_at"],
        "modified_at": created["modified_at"],
        "objects": [
            {
                "id": "obj1",
                "name": "Cube",
                "asset_id": None,
                "position": [1.0, 2.0, 3.0],
                "rotation": [0.0, 0.0, 0.0, 1.0],
                "scale": [1.0, 1.0, 1.0],
                "parent_id": None,
                "visible": True,
            }
        ],
    }
    resp = client.put(f"/api/scenes/{created['id']}", json=scene_data)
    assert resp.status_code == 200
    data = resp.json()
    assert data["created_at"] == created["created_at"]
    assert data["modified_at"] >= created["modified_at"]
    assert len(data["objects"]) == 1
    assert data["objects"][0]["name"] == "Cube"


def test_scene_delete(tmp_path: Path) -> None:
    """DELETE /api/scenes/{id} removes a scene."""
    client, _ = _setup(tmp_path)
    created = client.post("/api/scenes", json={"name": "Doomed"}).json()

    resp = client.delete(f"/api/scenes/{created['id']}")
    assert resp.status_code == 200
    assert resp.json()["message"] == "Deleted"

    # Verify it's gone
    resp = client.get(f"/api/scenes/{created['id']}")
    assert resp.status_code == 404


def test_scene_not_found(tmp_path: Path) -> None:
    """GET /api/scenes/{id} returns 404 for nonexistent scene."""
    client, _ = _setup(tmp_path)
    resp = client.get("/api/scenes/nonexistent")
    assert resp.status_code == 404


def test_scene_create_missing_name(tmp_path: Path) -> None:
    """POST /api/scenes without name returns 422."""
    client, _ = _setup(tmp_path)
    resp = client.post("/api/scenes", json={})
    assert resp.status_code == 422


def test_process_asset_plugin_error(tmp_path: Path) -> None:
    """POST /api/assets/{id}/process returns 500 when the plugin raises."""
    mock_plugin = MagicMock()
    mock_plugin.name = "texture"
    mock_plugin.extensions = [".png"]
    mock_plugin.process.side_effect = RuntimeError("encoder crashed")

    client, config, _ = _setup_process(
        tmp_path,
        source_files={"brick.png": b"PNG data"},
        mock_plugin=mock_plugin,
    )

    resp = client.post("/api/assets/brick/process")

    assert resp.status_code == 500
    assert "encoder crashed" in resp.json()["detail"]

    # Fingerprint cache should NOT be updated on failure
    fp_path = config.cache_dir / "fingerprints.json"
    assert not fp_path.is_file()


def test_process_asset_skipped(tmp_path: Path) -> None:
    """POST /api/assets/{id}/process does not update cache when plugin skips."""
    mock_plugin = MagicMock()
    mock_plugin.name = "texture"
    mock_plugin.extensions = [".png"]
    mock_plugin.process.return_value = AssetResult(
        source=Path("brick.png"),
        output=Path("brick.png"),
        metadata={"processed": False, "reason": "tool_not_found"},
    )

    client, config, _ = _setup_process(
        tmp_path,
        source_files={"brick.png": b"PNG data"},
        mock_plugin=mock_plugin,
    )

    resp = client.post("/api/assets/brick/process")

    assert resp.status_code == 200
    assert "Skipped" in resp.json()["message"]
    assert "tool_not_found" in resp.json()["message"]

    # Fingerprint cache should NOT be updated on skip
    fp_path = config.cache_dir / "fingerprints.json"
    assert not fp_path.is_file()


# ---------------------------------------------------------------------------
# Thumbnail endpoint
# ---------------------------------------------------------------------------


def _make_png_bytes(width: int = 4, height: int = 4) -> bytes:
    """Create a minimal valid PNG image using Pillow."""
    import io

    from PIL import Image as PILImage

    img = PILImage.new("RGBA", (width, height), (255, 0, 0, 255))
    buf = io.BytesIO()
    img.save(buf, format="PNG")
    return buf.getvalue()


def test_thumbnail_texture(tmp_path: Path) -> None:
    """Thumbnail endpoint returns a resized PNG for texture assets."""
    png_data = _make_png_bytes(256, 256)
    client, config = _setup(tmp_path, source_files={"textures/brick.png": png_data})

    resp = client.get("/api/assets/textures--brick/thumbnail")
    assert resp.status_code == 200
    assert resp.headers["content-type"] == "image/png"

    # Verify thumbnail was cached on disk
    thumb_path = config.output_dir / ".thumbnails" / "textures--brick.png"
    assert thumb_path.is_file()


def test_thumbnail_mesh_returns_404(tmp_path: Path) -> None:
    """Non-texture assets return 404 — frontend shows a fallback icon."""
    client, _ = _setup(tmp_path, source_files={"models/hero.gltf": b'{"asset":{}}'})

    resp = client.get("/api/assets/models--hero/thumbnail")
    assert resp.status_code == 404


def test_thumbnail_not_found(tmp_path: Path) -> None:
    """Unknown asset IDs return 404."""
    client, _ = _setup(tmp_path, source_files={"textures/a.png": _make_png_bytes()})

    resp = client.get("/api/assets/nonexistent/thumbnail")
    assert resp.status_code == 404


def test_thumbnail_cache_hit(tmp_path: Path) -> None:
    """Second request for the same thumbnail serves the cached file."""
    import os
    import time

    png_data = _make_png_bytes(64, 64)
    client, config = _setup(tmp_path, source_files={"textures/tile.png": png_data})

    # First request generates
    resp1 = client.get("/api/assets/textures--tile/thumbnail")
    assert resp1.status_code == 200

    thumb_path = config.output_dir / ".thumbnails" / "textures--tile.png"
    mtime_first = thumb_path.stat().st_mtime

    # Ensure filesystem mtime granularity can distinguish a regeneration.
    # Sleep 1 s so any rewrite would produce a visibly different mtime.
    time.sleep(1)

    # Touch the thumbnail so its mtime advances — if the server regenerates
    # it will overwrite with a new mtime >= now; if it serves from cache the
    # mtime stays at mtime_first.
    os.utime(thumb_path, (mtime_first, mtime_first))

    # Second request serves from cache (same mtime)
    resp2 = client.get("/api/assets/textures--tile/thumbnail")
    assert resp2.status_code == 200
    assert thumb_path.stat().st_mtime == mtime_first


def test_thumbnail_corrupt_image(tmp_path: Path) -> None:
    """Corrupt image data returns 500 with a clear error message."""
    client, _ = _setup(tmp_path, source_files={"textures/bad.png": b"not-a-real-png"})

    resp = client.get("/api/assets/textures--bad/thumbnail")
    assert resp.status_code == 500
    assert "Failed to generate thumbnail" in resp.json()["detail"]


# ---------------------------------------------------------------------------
# Sort tests
# ---------------------------------------------------------------------------


def test_sort_by_name_asc(three_assets: tuple[TestClient, PipelineConfig]) -> None:
    """sort=name&order=asc returns assets in alphabetical order."""
    client, _ = three_assets
    resp = client.get("/api/assets?sort=name&order=asc")
    assert resp.status_code == 200
    names = [a["name"] for a in resp.json()["assets"]]
    assert names == sorted(names, key=str.lower)


def test_sort_by_name_desc(three_assets: tuple[TestClient, PipelineConfig]) -> None:
    """sort=name&order=desc returns assets in reverse alphabetical order."""
    client, _ = three_assets
    resp = client.get("/api/assets?sort=name&order=desc")
    assert resp.status_code == 200
    names = [a["name"] for a in resp.json()["assets"]]
    assert names == sorted(names, key=str.lower, reverse=True)


def test_sort_by_size(tmp_path: Path) -> None:
    """sort=size default order is desc (largest first)."""
    # Path order: a_medium (100B), b_small (1B), c_large (1000B)
    # — deliberately not in size order so the sort is exercised.
    client, _ = _setup(
        tmp_path,
        source_files={
            "a_medium.gltf": b"x" * 100,
            "b_small.png": b"x",
            "c_large.obj": b"x" * 1000,
        },
    )
    resp = client.get("/api/assets?sort=size")
    assert resp.status_code == 200
    sizes = [a["file_size"] for a in resp.json()["assets"]]
    assert sizes == sorted(sizes, reverse=True)


def test_sort_by_size_asc(tmp_path: Path) -> None:
    """sort=size&order=asc returns smallest first."""
    client, _ = _setup(
        tmp_path,
        source_files={
            "a_medium.gltf": b"x" * 100,
            "b_small.png": b"x",
            "c_large.obj": b"x" * 1000,
        },
    )
    resp = client.get("/api/assets?sort=size&order=asc")
    assert resp.status_code == 200
    sizes = [a["file_size"] for a in resp.json()["assets"]]
    assert sizes == sorted(sizes)


def test_sort_by_type(three_assets: tuple[TestClient, PipelineConfig]) -> None:
    """sort=type groups assets by type alphabetically."""
    client, _ = three_assets
    resp = client.get("/api/assets?sort=type&order=asc")
    assert resp.status_code == 200
    types = [a["asset_type"] for a in resp.json()["assets"]]
    assert types == sorted(types)


def test_sort_by_status(tmp_path: Path) -> None:
    """sort=status orders all four statuses: new → changed → missing → processed."""
    import hashlib

    # "processed": matching cache entry + output file
    fp_done = hashlib.sha256(b"PNG done").hexdigest()
    # "changed": cache entry exists but fingerprint differs from source
    fp_stale = hashlib.sha256(b"old content").hexdigest()

    client, _ = _setup(
        tmp_path,
        source_files={
            # Will be "new" — no cache entry at all
            "models/new.gltf": b'{"asset":{}}',
            # Will be "changed" — cache fingerprint mismatches source content
            "textures/stale.png": b"PNG updated",
            # Will be "missing" — cache matches but no output file
            "anims/gone.fanim": b"FANIM data",
            # Will be "processed" — cache matches + output exists
            "textures/done.png": b"PNG done",
        },
        output_files={
            "textures/done.ftex": b"processed",
        },
        cache_entries={
            "textures/done.png": fp_done,
            "textures/stale.png": fp_stale,
            "anims/gone.fanim": hashlib.sha256(b"FANIM data").hexdigest(),
        },
    )
    resp = client.get("/api/assets?sort=status&order=asc")
    assert resp.status_code == 200
    statuses = [a["status"] for a in resp.json()["assets"]]
    assert statuses == ["new", "changed", "missing", "processed"]


def test_sort_combined_with_filter(tmp_path: Path) -> None:
    """Sorting and filtering compose correctly with multiple matches."""
    client, _ = _setup(
        tmp_path,
        source_files={
            "textures/brick.png": b"PNG brick",
            "textures/alpha.png": b"PNG alpha",
            "models/hero.gltf": b'{"asset":{}}',
        },
    )
    resp = client.get("/api/assets?type=texture&sort=name&order=asc")
    assert resp.status_code == 200
    body = resp.json()
    assert body["total"] == 2
    names = [a["name"] for a in body["assets"]]
    assert names == sorted(names, key=str.lower)
    assert all(a["asset_type"] == "texture" for a in body["assets"])


def test_sort_without_order_uses_default(
    three_assets: tuple[TestClient, PipelineConfig],
) -> None:
    """sort=name without order defaults to asc."""
    client, _ = three_assets
    resp = client.get("/api/assets?sort=name")
    assert resp.status_code == 200
    names = [a["name"] for a in resp.json()["assets"]]
    assert names == sorted(names, key=str.lower)


def test_sort_invalid_field(
    three_assets: tuple[TestClient, PipelineConfig],
) -> None:
    """An unknown sort field returns 400 with a descriptive message."""
    client, _ = three_assets
    resp = client.get("/api/assets?sort=banana")
    assert resp.status_code == 400
    assert "Invalid sort field" in resp.json()["detail"]


def test_sort_invalid_order(
    three_assets: tuple[TestClient, PipelineConfig],
) -> None:
    """An unknown sort order returns 400 with a descriptive message."""
    client, _ = three_assets
    resp = client.get("/api/assets?sort=size&order=descending")
    assert resp.status_code == 400
    assert "Invalid sort order" in resp.json()["detail"]


def test_invalid_order_without_sort(
    three_assets: tuple[TestClient, PipelineConfig],
) -> None:
    """An invalid order is rejected even when sort is not provided."""
    client, _ = three_assets
    resp = client.get("/api/assets?order=descending")
    assert resp.status_code == 400
    assert "Invalid sort order" in resp.json()["detail"]


# ---------------------------------------------------------------------------
# Limit
# ---------------------------------------------------------------------------


def test_limit_caps_results(tmp_path: Path) -> None:
    """The limit parameter caps the number of returned assets."""
    client, _ = _setup(
        tmp_path,
        source_files={
            "a.png": b"a",
            "b.png": b"b",
            "c.png": b"c",
        },
    )
    resp = client.get("/api/assets?limit=2")
    assert resp.status_code == 200
    body = resp.json()
    assert len(body["assets"]) == 2
    # total reflects all matching assets, not the capped count
    assert body["total"] == 3


def test_limit_greater_than_total(tmp_path: Path) -> None:
    """When limit exceeds the asset count, all assets are returned."""
    client, _ = _setup(
        tmp_path,
        source_files={"a.png": b"a"},
    )
    resp = client.get("/api/assets?limit=100")
    assert resp.status_code == 200
    body = resp.json()
    assert len(body["assets"]) == 1
    assert body["total"] == 1


def test_limit_zero_rejected(tmp_path: Path) -> None:
    """limit=0 is rejected (minimum is 1)."""
    client, _ = _setup(tmp_path, source_files={"a.png": b"a"})
    resp = client.get("/api/assets?limit=0")
    assert resp.status_code == 422


# ---------------------------------------------------------------------------
# sort=recent and output_mtime
# ---------------------------------------------------------------------------


def test_output_mtime_present_for_processed(tmp_path: Path) -> None:
    """Processed assets include a non-null output_mtime ISO timestamp."""
    import hashlib

    fp = hashlib.sha256(b"PNG data").hexdigest()
    client, _ = _setup(
        tmp_path,
        source_files={"tex.png": b"PNG data"},
        output_files={"tex.ftex": b"FTEX data"},
        cache_entries={"tex.png": fp},
    )
    resp = client.get("/api/assets")
    assert resp.status_code == 200
    asset = resp.json()["assets"][0]
    assert asset["status"] == "processed"
    assert asset["output_mtime"] is not None


def test_output_mtime_null_for_new(tmp_path: Path) -> None:
    """New assets have null output_mtime."""
    client, _ = _setup(
        tmp_path,
        source_files={"tex.png": b"PNG data"},
    )
    resp = client.get("/api/assets")
    assert resp.status_code == 200
    asset = resp.json()["assets"][0]
    assert asset["status"] == "new"
    assert asset["output_mtime"] is None


def test_sort_recent(tmp_path: Path) -> None:
    """sort=recent orders by output_mtime descending by default."""
    import hashlib
    import os

    source_dir = tmp_path / "source"
    output_dir = tmp_path / "output"
    cache_dir = tmp_path / "cache"

    for d in (source_dir, output_dir, cache_dir):
        d.mkdir()

    # Create two source files
    (source_dir / "old.png").write_bytes(b"OLD")
    (source_dir / "new.png").write_bytes(b"NEW")

    # Create output files with different mtimes
    old_out = output_dir / "old.ftex"
    new_out = output_dir / "new.ftex"
    old_out.write_bytes(b"OLD OUT")
    new_out.write_bytes(b"NEW OUT")

    # Ensure distinct mtime (force older timestamp on old_out)
    old_stat = old_out.stat()
    os.utime(old_out, (old_stat.st_atime, old_stat.st_mtime - 10))

    fp_old = hashlib.sha256(b"OLD").hexdigest()
    fp_new = hashlib.sha256(b"NEW").hexdigest()
    fp_path = cache_dir / "fingerprints.json"
    fp_path.write_text(
        json.dumps({"old.png": fp_old, "new.png": fp_new}),
        encoding="utf-8",
    )

    config = PipelineConfig(
        source_dir=source_dir, output_dir=output_dir, cache_dir=cache_dir
    )
    client = TestClient(create_app(config))

    resp = client.get("/api/assets?sort=recent")
    assert resp.status_code == 200
    names = [a["name"] for a in resp.json()["assets"]]
    # Default order for recent is desc — newest first
    assert names[0] == "new.png"
    assert names[1] == "old.png"


def test_sort_recent_desc_nulls_last(tmp_path: Path) -> None:
    """sort=recent (desc, default) puts null-mtime assets last, not first."""
    import hashlib
    import os

    source_dir = tmp_path / "source"
    output_dir = tmp_path / "output"
    cache_dir = tmp_path / "cache"

    for d in (source_dir, output_dir, cache_dir):
        d.mkdir()

    # "processed.png" — has output
    (source_dir / "processed.png").write_bytes(b"PROC")
    proc_out = output_dir / "processed.ftex"
    proc_out.write_bytes(b"PROC OUT")

    # "unprocessed.png" — no output, no cache → null mtime
    (source_dir / "unprocessed.png").write_bytes(b"NONE")

    # Force a known mtime on the output
    st = proc_out.stat()
    os.utime(proc_out, (st.st_atime, st.st_mtime))

    fp_proc = hashlib.sha256(b"PROC").hexdigest()
    fp_path = cache_dir / "fingerprints.json"
    fp_path.write_text(
        json.dumps({"processed.png": fp_proc}),
        encoding="utf-8",
    )

    config = PipelineConfig(
        source_dir=source_dir, output_dir=output_dir, cache_dir=cache_dir
    )
    client = TestClient(create_app(config))

    # Default order for recent is desc
    resp = client.get("/api/assets?sort=recent")
    assert resp.status_code == 200
    names = [a["name"] for a in resp.json()["assets"]]
    # Processed asset first, null-mtime asset last
    assert names[0] == "processed.png"
    assert names[-1] == "unprocessed.png"


def test_sort_recent_asc_nulls_last(tmp_path: Path) -> None:
    """sort=recent&order=asc puts null-mtime (unprocessed) assets last."""
    import hashlib
    import os

    source_dir = tmp_path / "source"
    output_dir = tmp_path / "output"
    cache_dir = tmp_path / "cache"

    for d in (source_dir, output_dir, cache_dir):
        d.mkdir()

    # "old.png" — processed, older mtime
    (source_dir / "old.png").write_bytes(b"OLD")
    old_out = output_dir / "old.ftex"
    old_out.write_bytes(b"OLD OUT")

    # "new.png" — processed, newer mtime
    (source_dir / "new.png").write_bytes(b"NEW")
    new_out = output_dir / "new.ftex"
    new_out.write_bytes(b"NEW OUT")

    # Force distinct mtimes
    st = old_out.stat()
    os.utime(old_out, (st.st_atime, st.st_mtime - 10))

    # "unprocessed.png" — no output, no cache entry → status=new, null mtime
    (source_dir / "unprocessed.png").write_bytes(b"NONE")

    fp_old = hashlib.sha256(b"OLD").hexdigest()
    fp_new = hashlib.sha256(b"NEW").hexdigest()
    fp_path = cache_dir / "fingerprints.json"
    fp_path.write_text(
        json.dumps({"old.png": fp_old, "new.png": fp_new}),
        encoding="utf-8",
    )

    config = PipelineConfig(
        source_dir=source_dir, output_dir=output_dir, cache_dir=cache_dir
    )
    client = TestClient(create_app(config))

    resp = client.get("/api/assets?sort=recent&order=asc")
    assert resp.status_code == 200
    names = [a["name"] for a in resp.json()["assets"]]
    # Ascending: oldest processed first, then newer, null-mtime last
    assert names[0] == "old.png"
    assert names[1] == "new.png"
    assert names[2] == "unprocessed.png"


def test_sort_recent_with_limit(tmp_path: Path) -> None:
    """sort=recent combined with limit returns the N most recent assets."""
    import hashlib
    import os

    source_dir = tmp_path / "source"
    output_dir = tmp_path / "output"
    cache_dir = tmp_path / "cache"

    for d in (source_dir, output_dir, cache_dir):
        d.mkdir()

    cache_entries = {}
    for i, name in enumerate(["a.png", "b.png", "c.png"]):
        (source_dir / name).write_bytes(f"data{i}".encode())
        out = output_dir / name.replace(".png", ".ftex")
        out.write_bytes(f"out{i}".encode())
        # Set distinct mtimes
        st = out.stat()
        os.utime(out, (st.st_atime, st.st_mtime + i * 10))
        fp = hashlib.sha256(f"data{i}".encode()).hexdigest()
        cache_entries[name] = fp

    fp_path = cache_dir / "fingerprints.json"
    fp_path.write_text(json.dumps(cache_entries), encoding="utf-8")

    config = PipelineConfig(
        source_dir=source_dir, output_dir=output_dir, cache_dir=cache_dir
    )
    client = TestClient(create_app(config))

    resp = client.get("/api/assets?sort=recent&limit=2")
    assert resp.status_code == 200
    body = resp.json()
    assert len(body["assets"]) == 2
    assert body["total"] == 3
    # Newest first (c has highest mtime)
    assert body["assets"][0]["name"] == "c.png"


# ---------------------------------------------------------------------------
# Batch processing
# ---------------------------------------------------------------------------


def test_batch_process(tmp_path: Path) -> None:
    """POST /api/process/batch processes multiple assets and returns a summary."""
    client, config, mock_plugin = _setup_process(
        tmp_path,
        source_files={"a.png": b"PNG A", "b.png": b"PNG B"},
    )

    # First, get the asset IDs
    resp = client.get("/api/assets")
    assert resp.status_code == 200
    asset_ids = [a["id"] for a in resp.json()["assets"]]
    assert len(asset_ids) == 2

    resp = client.post("/api/process/batch", json={"asset_ids": asset_ids})
    assert resp.status_code == 200

    body = resp.json()
    assert "batch_id" in body
    assert isinstance(body["batch_id"], str)
    assert len(body["batch_id"]) > 0
    assert body["succeeded"] == 2
    assert body["failed"] == 0
    assert body["skipped"] == 0
    assert len(body["results"]) == 2
    assert all(r["status"] == "succeeded" for r in body["results"])
    assert mock_plugin.process.call_count == 2


def test_batch_process_empty_ids(tmp_path: Path) -> None:
    """POST /api/process/batch with empty asset_ids returns 400."""
    client, _, _ = _setup_process(tmp_path)

    resp = client.post("/api/process/batch", json={"asset_ids": []})
    assert resp.status_code == 400
    assert "empty" in resp.json()["detail"].lower()


def test_batch_process_unknown_id(tmp_path: Path) -> None:
    """POST /api/process/batch with unknown asset ID returns 404."""
    client, _, _ = _setup_process(tmp_path, source_files={"a.png": b"PNG A"})

    resp = client.post("/api/process/batch", json={"asset_ids": ["nonexistent"]})
    assert resp.status_code == 404
    assert "nonexistent" in resp.json()["detail"]


def test_batch_process_partial_failure(tmp_path: Path) -> None:
    """Batch processing continues after one asset fails."""
    mock_plugin = MagicMock()
    mock_plugin.name = "texture"
    mock_plugin.extensions = [".png"]

    # First call succeeds, second call raises
    mock_plugin.process.side_effect = [
        AssetResult(source=Path("a"), output=Path("a"), metadata={}),
        RuntimeError("disk full"),
    ]

    client, _, _ = _setup_process(
        tmp_path,
        source_files={"a.png": b"PNG A", "b.png": b"PNG B"},
        mock_plugin=mock_plugin,
    )

    # Get asset IDs — sorted by name so we know the order
    resp = client.get("/api/assets?sort=name&order=asc")
    assert resp.status_code == 200
    asset_ids = [a["id"] for a in resp.json()["assets"]]

    resp = client.post("/api/process/batch", json={"asset_ids": asset_ids})
    assert resp.status_code == 200

    body = resp.json()
    assert body["succeeded"] == 1
    assert body["failed"] == 1
    assert body["skipped"] == 0
    assert len(body["results"]) == 2


def test_batch_process_skipped(tmp_path: Path) -> None:
    """Batch processing reports skipped assets correctly."""
    mock_plugin = MagicMock()
    mock_plugin.name = "texture"
    mock_plugin.extensions = [".png"]
    mock_plugin.process.return_value = AssetResult(
        source=Path("a"),
        output=Path("a"),
        metadata={"processed": False, "reason": "tool not installed"},
    )

    client, _, _ = _setup_process(
        tmp_path,
        source_files={"a.png": b"PNG A"},
        mock_plugin=mock_plugin,
    )

    resp = client.get("/api/assets")
    asset_ids = [a["id"] for a in resp.json()["assets"]]

    resp = client.post("/api/process/batch", json={"asset_ids": asset_ids})
    assert resp.status_code == 200

    body = resp.json()
    assert body["succeeded"] == 0
    assert body["skipped"] == 1
    assert body["results"][0]["status"] == "skipped"
    assert "tool not installed" in body["results"][0]["message"]


# ---------------------------------------------------------------------------
# POST /api/rescan
# ---------------------------------------------------------------------------


def test_rescan(tmp_path: Path) -> None:
    """POST /api/rescan clears the cache and re-scans the source directory."""
    client, config = _setup(
        tmp_path, source_files={"a.png": b"PNG A", "b.png": b"PNG B"}
    )

    resp = client.post("/api/rescan")
    assert resp.status_code == 200

    body = resp.json()
    assert body["total"] == 2
    assert "by_type" in body
    assert "by_status" in body
    assert body["by_type"]["texture"] == 2
    assert body["by_status"]["new"] == 2


def test_rescan_picks_up_new_files(tmp_path: Path) -> None:
    """POST /api/rescan detects files added after initial scan."""
    client, config = _setup(tmp_path, source_files={"a.png": b"PNG A"})

    # Initial scan
    resp = client.get("/api/assets")
    assert resp.json()["total"] == 1

    # Add a new file on disk
    (config.source_dir / "b.png").write_bytes(b"PNG B")

    # Rescan should pick it up
    resp = client.post("/api/rescan")
    assert resp.status_code == 200
    assert resp.json()["total"] == 2


# ---------------------------------------------------------------------------
# POST /api/process
# ---------------------------------------------------------------------------


def test_process_all(tmp_path: Path) -> None:
    """POST /api/process processes all unprocessed assets."""
    client, config, mock_plugin = _setup_process(
        tmp_path,
        source_files={"a.png": b"PNG A", "b.png": b"PNG B"},
    )

    resp = client.post("/api/process")
    assert resp.status_code == 200

    body = resp.json()
    assert body["succeeded"] == 2
    assert body["failed"] == 0
    assert body["skipped"] == 0
    assert len(body["results"]) == 2
    assert mock_plugin.process.call_count == 2


def test_process_all_empty(tmp_path: Path) -> None:
    """POST /api/process returns empty batch when nothing to process."""
    source_data = b"PNG data"
    source_files = {"a.png": source_data}

    # Compute fingerprint for cache
    src = tmp_path / "pre"
    src.mkdir()
    (src / "a.png").write_bytes(source_data)
    fp = fingerprint_file(src / "a.png")
    cache_entries = {"a.png": fp}

    client, config, mock_plugin = _setup_process(
        tmp_path,
        source_files=source_files,
    )

    # Manually set up output and cache to make the asset "processed"
    out = config.output_dir / "a.png"
    out.write_bytes(b"output")
    fp_path = config.cache_dir / "fingerprints.json"
    fp_path.write_text(json.dumps(cache_entries), encoding="utf-8")

    resp = client.post("/api/process")
    assert resp.status_code == 200

    body = resp.json()
    assert body["succeeded"] == 0
    assert body["failed"] == 0
    assert body["skipped"] == 0
    assert len(body["results"]) == 0


def test_process_all_no_plugin(tmp_path: Path) -> None:
    """POST /api/process skips assets without a matching plugin."""
    # Create a client with no plugin for .txt files
    source_dir = tmp_path / "source"
    output_dir = tmp_path / "output"
    cache_dir = tmp_path / "cache"
    source_dir.mkdir()
    output_dir.mkdir()
    cache_dir.mkdir()
    (source_dir / "readme.txt").write_bytes(b"text file")

    config = PipelineConfig(
        source_dir=source_dir,
        output_dir=output_dir,
        cache_dir=cache_dir,
    )

    # Patch with a registry that returns no plugins for .txt
    with patch("pipeline.server.PluginRegistry") as MockRegistry:
        instance = MockRegistry.return_value
        instance.discover.return_value = 1
        instance.get_by_extension.return_value = []
        client = TestClient(create_app(config))

    resp = client.post("/api/process")
    assert resp.status_code == 200

    body = resp.json()
    # No plugin for .txt → nothing processable
    assert body["succeeded"] == 0
    assert len(body["results"]) == 0


# ---------------------------------------------------------------------------
# Dependency graph
# ---------------------------------------------------------------------------

_GLTF_WITH_TEXTURE = json.dumps(
    {
        "asset": {"version": "2.0"},
        "images": [{"uri": "brick.png"}],
        "textures": [{"source": 0}],
        "materials": [
            {
                "pbrMetallicRoughness": {
                    "baseColorTexture": {"index": 0},
                },
            }
        ],
    }
)


def test_dependencies_mesh_depends_on_texture(tmp_path: Path) -> None:
    """A glTF mesh should list its referenced textures in depends_on."""
    client, _ = _setup(
        tmp_path,
        source_files={
            "hero.gltf": _GLTF_WITH_TEXTURE.encode(),
            "brick.png": b"PNG data",
        },
    )
    resp = client.get("/api/assets/hero/dependencies")
    assert resp.status_code == 200

    body = resp.json()
    dep_ids = [d["id"] for d in body["depends_on"]]
    assert "brick" in dep_ids
    assert body["depended_by"] == []


def test_dependencies_texture_depended_by_mesh(tmp_path: Path) -> None:
    """A texture referenced by a glTF mesh should appear in depended_by."""
    client, _ = _setup(
        tmp_path,
        source_files={
            "hero.gltf": _GLTF_WITH_TEXTURE.encode(),
            "brick.png": b"PNG data",
        },
    )
    resp = client.get("/api/assets/brick/dependencies")
    assert resp.status_code == 200

    body = resp.json()
    dep_ids = [d["id"] for d in body["depended_by"]]
    assert "hero" in dep_ids
    assert body["depends_on"] == []


def test_dependencies_not_found(tmp_path: Path) -> None:
    """Requesting dependencies for an unknown asset returns 404."""
    client, _ = _setup(tmp_path, source_files={"a.png": b"data"})
    resp = client.get("/api/assets/nope/dependencies")
    assert resp.status_code == 404


def test_dependencies_no_deps(tmp_path: Path) -> None:
    """An asset with no dependencies returns empty lists."""
    client, _ = _setup(tmp_path, source_files={"solo.png": b"data"})
    resp = client.get("/api/assets/solo/dependencies")
    assert resp.status_code == 200

    body = resp.json()
    assert body["depends_on"] == []
    assert body["depended_by"] == []


def test_dependencies_scene_depends_on_asset(tmp_path: Path) -> None:
    """An asset used in an authored scene should list that scene in depended_by."""
    client, config = _setup(
        tmp_path,
        source_files={"hero.gltf": b'{"asset":{}}'},
    )
    # Create a scene that references the asset
    scene_resp = client.post("/api/scenes", json={"name": "test-scene"})
    assert scene_resp.status_code == 201
    scene_id = scene_resp.json()["id"]

    # Save a scene with an object referencing the asset
    scene_data = scene_resp.json()
    scene_data["objects"] = [
        {
            "id": "obj1",
            "name": "Hero",
            "asset_id": "hero",
            "position": [0, 0, 0],
            "rotation": [0, 0, 0, 1],
            "scale": [1, 1, 1],
            "parent_id": None,
            "visible": True,
        }
    ]
    put_resp = client.put(f"/api/scenes/{scene_id}", json=scene_data)
    assert put_resp.status_code == 200

    resp = client.get("/api/assets/hero/dependencies")
    assert resp.status_code == 200

    body = resp.json()
    scene_ids = [d["id"] for d in body["depended_by"]]
    assert any(s.startswith("scene--") for s in scene_ids)


def test_dependencies_gltf_subdirectory_texture(tmp_path: Path) -> None:
    """A glTF referencing a texture in a subdirectory resolves correctly."""
    gltf = json.dumps(
        {
            "asset": {"version": "2.0"},
            "images": [{"uri": "textures/albedo.png"}],
            "textures": [{"source": 0}],
            "materials": [
                {
                    "pbrMetallicRoughness": {
                        "baseColorTexture": {"index": 0},
                    },
                }
            ],
        }
    )
    client, _ = _setup(
        tmp_path,
        source_files={
            "models/hero.gltf": gltf.encode(),
            "models/textures/albedo.png": b"PNG data",
        },
    )
    resp = client.get("/api/assets/models--hero/dependencies")
    assert resp.status_code == 200

    body = resp.json()
    dep_ids = [d["id"] for d in body["depends_on"]]
    assert "models--textures--albedo" in dep_ids
