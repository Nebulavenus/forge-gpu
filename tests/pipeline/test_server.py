"""Tests for pipeline.server."""

from __future__ import annotations

import json
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

from pipeline.config import PipelineConfig
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
    "textures/brick.png": b"PNG fake",
    "models/hero.gltf": b'{"asset":{}}',
    "anims/walk.fanim": b"FANIM data",
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
