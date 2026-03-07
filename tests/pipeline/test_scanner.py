"""Tests for pipeline.scanner."""

from pathlib import Path

from pipeline.scanner import FileStatus, FingerprintCache, fingerprint_file, scan

# -- Fingerprinting ---------------------------------------------------------


def test_fingerprint_deterministic(tmp_path: Path):
    f = tmp_path / "hello.txt"
    f.write_bytes(b"hello world")
    assert fingerprint_file(f) == fingerprint_file(f)


def test_fingerprint_changes_with_content(tmp_path: Path):
    f = tmp_path / "data.bin"
    f.write_bytes(b"version 1")
    h1 = fingerprint_file(f)
    f.write_bytes(b"version 2")
    h2 = fingerprint_file(f)
    assert h1 != h2


# -- Cache ------------------------------------------------------------------


def test_cache_round_trip(tmp_path: Path):
    cache_path = tmp_path / "cache" / "fp.json"
    cache = FingerprintCache(cache_path)
    cache.set(Path("a/b.png"), "abc123")
    cache.save()

    cache2 = FingerprintCache(cache_path)
    assert cache2.get(Path("a/b.png")) == "abc123"


def test_cache_miss_returns_none(tmp_path: Path):
    cache = FingerprintCache(tmp_path / "fp.json")
    assert cache.get(Path("nope.png")) is None


def test_cache_corrupt_file(tmp_path: Path):
    bad = tmp_path / "fp.json"
    bad.write_text("not json!!!", encoding="utf-8")
    cache = FingerprintCache(bad)
    # Should recover gracefully and start empty
    assert cache.get(Path("anything")) is None


# -- Scanner ----------------------------------------------------------------


def _make_source_tree(tmp_path: Path) -> Path:
    """Create a source directory with test files."""
    src = tmp_path / "assets"
    src.mkdir()
    (src / "hero.png").write_bytes(b"PNG fake data")
    (src / "model.obj").write_bytes(b"v 0 0 0")
    (src / "readme.txt").write_bytes(b"ignore me")  # unsupported extension
    sub = src / "subdir"
    sub.mkdir()
    (sub / "detail.png").write_bytes(b"another PNG")
    return src


def test_scan_finds_supported_files(tmp_path: Path):
    src = _make_source_tree(tmp_path)
    cache = FingerprintCache(tmp_path / "fp.json")

    files = scan(src, {".png", ".obj"}, cache)

    paths = {f.relative.as_posix() for f in files}
    assert "hero.png" in paths
    assert "model.obj" in paths
    assert "subdir/detail.png" in paths
    assert "readme.txt" not in paths  # not a supported extension


def test_scan_marks_new_files(tmp_path: Path):
    src = _make_source_tree(tmp_path)
    cache = FingerprintCache(tmp_path / "fp.json")

    files = scan(src, {".png"}, cache)
    assert all(f.status is FileStatus.NEW for f in files)


def test_scan_detects_unchanged(tmp_path: Path):
    src = _make_source_tree(tmp_path)
    cache_path = tmp_path / "fp.json"
    cache = FingerprintCache(cache_path)

    # First scan — all new
    files = scan(src, {".png"}, cache)
    for f in files:
        cache.set(f.relative, f.fingerprint)
    cache.save()

    # Second scan — all unchanged
    cache2 = FingerprintCache(cache_path)
    files2 = scan(src, {".png"}, cache2)
    assert all(f.status is FileStatus.UNCHANGED for f in files2)


def test_scan_detects_changed(tmp_path: Path):
    src = _make_source_tree(tmp_path)
    cache_path = tmp_path / "fp.json"
    cache = FingerprintCache(cache_path)

    # First scan
    files = scan(src, {".png"}, cache)
    for f in files:
        cache.set(f.relative, f.fingerprint)
    cache.save()

    # Modify a file
    (src / "hero.png").write_bytes(b"MODIFIED PNG data")

    # Second scan — hero.png should be CHANGED
    cache2 = FingerprintCache(cache_path)
    files2 = scan(src, {".png"}, cache2)
    hero = next(f for f in files2 if f.relative.as_posix() == "hero.png")
    assert hero.status is FileStatus.CHANGED


def test_scan_empty_directory(tmp_path: Path):
    src = tmp_path / "empty"
    src.mkdir()
    cache = FingerprintCache(tmp_path / "fp.json")
    assert scan(src, {".png"}, cache) == []


def test_scan_missing_directory(tmp_path: Path):
    cache = FingerprintCache(tmp_path / "fp.json")
    assert scan(tmp_path / "nope", {".png"}, cache) == []
