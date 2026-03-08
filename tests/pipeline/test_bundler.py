"""Tests for pipeline.bundler — bundle packing, reading, and dependency tracking."""

from __future__ import annotations

import json
from pathlib import Path

import pytest

from pipeline.bundler import (
    BUNDLE_MAGIC,
    BUNDLE_VERSION,
    HEADER_SIZE,
    BundleEntry,
    BundleError,
    BundleFormatError,
    BundleManifest,
    BundleReader,
    BundleWriter,
    DependencyGraph,
    create_bundle,
)

# Try importing zstandard — tests that need it are skipped if unavailable.
try:
    import zstandard  # noqa: F401

    HAS_ZSTD = True
except ImportError:
    HAS_ZSTD = False

needs_zstd = pytest.mark.skipif(not HAS_ZSTD, reason="zstandard not installed")


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _make_bundle(tmp_path: Path, entries: dict[str, bytes], **kwargs) -> Path:
    """Create a bundle from a dict of {path: data} pairs."""
    bundle_path = tmp_path / "test.forgepak"
    writer = BundleWriter(bundle_path, **kwargs)
    for path, data in entries.items():
        writer.add(path, data)
    writer.finalize()
    return bundle_path


# ---------------------------------------------------------------------------
# BundleEntry serialization
# ---------------------------------------------------------------------------


class TestBundleEntry:
    def test_to_dict_round_trip(self):
        entry = BundleEntry(
            path="textures/brick.png",
            offset=24,
            size=100,
            original_size=500,
            compression="zstd",
            fingerprint="abc123",
            dependencies=["textures/brick_normal.png"],
        )
        d = entry.to_dict()
        restored = BundleEntry.from_dict(d)
        assert restored.path == entry.path
        assert restored.offset == entry.offset
        assert restored.size == entry.size
        assert restored.original_size == entry.original_size
        assert restored.compression == entry.compression
        assert restored.fingerprint == entry.fingerprint
        assert restored.dependencies == entry.dependencies

    def test_to_dict_omits_empty_dependencies(self):
        entry = BundleEntry(
            path="a.txt",
            offset=0,
            size=10,
            original_size=10,
            compression="none",
            fingerprint="x",
        )
        d = entry.to_dict()
        assert "dependencies" not in d

    def test_from_dict_defaults_dependencies(self):
        d = {
            "path": "a.txt",
            "offset": 0,
            "size": 10,
            "original_size": 10,
            "compression": "none",
            "fingerprint": "x",
        }
        entry = BundleEntry.from_dict(d)
        assert entry.dependencies == []


# ---------------------------------------------------------------------------
# BundleManifest
# ---------------------------------------------------------------------------


class TestBundleManifest:
    def test_get_existing(self):
        e = BundleEntry("a.txt", 0, 10, 20, "none", "fp1")
        m = BundleManifest(version=1, entries=[e])
        assert m.get("a.txt") is e

    def test_get_missing(self):
        m = BundleManifest(version=1, entries=[])
        assert m.get("missing.txt") is None

    def test_paths(self):
        entries = [
            BundleEntry("b.txt", 0, 10, 20, "none", "fp1"),
            BundleEntry("a.txt", 10, 10, 20, "none", "fp2"),
        ]
        m = BundleManifest(version=1, entries=entries)
        assert m.paths == ["b.txt", "a.txt"]

    def test_totals(self):
        entries = [
            BundleEntry("a", 0, 50, 100, "zstd", "fp1"),
            BundleEntry("b", 50, 30, 80, "zstd", "fp2"),
        ]
        m = BundleManifest(version=1, entries=entries)
        assert m.total_compressed == 80
        assert m.total_original == 180


# ---------------------------------------------------------------------------
# BundleWriter — uncompressed
# ---------------------------------------------------------------------------


class TestBundleWriterUncompressed:
    def test_write_single_entry(self, tmp_path):
        bundle_path = tmp_path / "test.forgepak"
        writer = BundleWriter(bundle_path, compress=False)
        data = b"hello world"
        entry = writer.add("greeting.txt", data)
        manifest = writer.finalize()

        assert entry.path == "greeting.txt"
        assert entry.original_size == len(data)
        assert entry.size == len(data)
        assert entry.compression == "none"
        assert entry.offset == HEADER_SIZE
        assert len(manifest.entries) == 1
        assert bundle_path.exists()

    def test_write_multiple_entries(self, tmp_path):
        entries = {"a.txt": b"aaa", "b.txt": b"bbbbb", "c.txt": b"c"}
        bundle_path = _make_bundle(tmp_path, entries, compress=False)

        with BundleReader(bundle_path) as reader:
            assert len(reader.manifest.entries) == 3
            assert reader.read("a.txt") == b"aaa"
            assert reader.read("b.txt") == b"bbbbb"
            assert reader.read("c.txt") == b"c"

    def test_header_magic_and_version(self, tmp_path):
        bundle_path = _make_bundle(tmp_path, {"x.txt": b"x"}, compress=False)
        raw = bundle_path.read_bytes()
        assert raw[:4] == BUNDLE_MAGIC
        # Version is bytes 4-8 (little-endian uint32).
        import struct

        version = struct.unpack_from("<I", raw, 4)[0]
        assert version == BUNDLE_VERSION

    def test_duplicate_path_raises(self, tmp_path):
        bundle_path = tmp_path / "dup.forgepak"
        writer = BundleWriter(bundle_path, compress=False)
        writer.add("same.txt", b"first")
        with pytest.raises(BundleError, match="Duplicate"):
            writer.add("same.txt", b"second")

    def test_add_after_finalize_raises(self, tmp_path):
        bundle_path = tmp_path / "fin.forgepak"
        writer = BundleWriter(bundle_path, compress=False)
        writer.finalize()
        with pytest.raises(BundleError, match="finalize"):
            writer.add("late.txt", b"oops")

    def test_double_finalize_raises(self, tmp_path):
        bundle_path = tmp_path / "fin2.forgepak"
        writer = BundleWriter(bundle_path, compress=False)
        writer.finalize()
        with pytest.raises(BundleError, match="finalize"):
            writer.finalize()

    def test_add_file(self, tmp_path):
        src = tmp_path / "source.bin"
        src.write_bytes(b"file data here")
        bundle_path = tmp_path / "test.forgepak"
        writer = BundleWriter(bundle_path, compress=False)
        entry = writer.add_file(src, "data/source.bin")
        writer.finalize()

        assert entry.path == "data/source.bin"
        assert entry.original_size == 14

        with BundleReader(bundle_path) as reader:
            assert reader.read("data/source.bin") == b"file data here"

    def test_path_normalization(self, tmp_path):
        bundle_path = tmp_path / "norm.forgepak"
        writer = BundleWriter(bundle_path, compress=False)
        writer.add("dir\\file.txt", b"backslash")
        manifest = writer.finalize()
        assert manifest.entries[0].path == "dir/file.txt"

    def test_fingerprint_auto_generated(self, tmp_path):
        bundle_path = tmp_path / "fp.forgepak"
        writer = BundleWriter(bundle_path, compress=False)
        entry = writer.add("data.bin", b"deterministic")
        writer.finalize()
        assert len(entry.fingerprint) == 64  # SHA-256 hex

    def test_fingerprint_custom(self, tmp_path):
        bundle_path = tmp_path / "fp2.forgepak"
        writer = BundleWriter(bundle_path, compress=False)
        entry = writer.add("data.bin", b"x", fingerprint="custom_fp")
        writer.finalize()
        assert entry.fingerprint == "custom_fp"

    def test_dependencies_stored(self, tmp_path):
        bundle_path = tmp_path / "deps.forgepak"
        writer = BundleWriter(bundle_path, compress=False)
        writer.add("mesh.bin", b"mesh", dependencies=["tex.png", "mat.json"])
        writer.finalize()

        with BundleReader(bundle_path) as reader:
            entry = reader.manifest.get("mesh.bin")
            assert entry is not None
            assert entry.dependencies == ["tex.png", "mat.json"]

    def test_empty_data(self, tmp_path):
        bundle_path = _make_bundle(tmp_path, {"empty.bin": b""}, compress=False)
        with BundleReader(bundle_path) as reader:
            assert reader.read("empty.bin") == b""


# ---------------------------------------------------------------------------
# BundleWriter — compressed
# ---------------------------------------------------------------------------


class TestBundleWriterCompressed:
    @needs_zstd
    def test_compressed_smaller(self, tmp_path):
        # Repetitive data compresses well.
        data = b"A" * 10000
        bundle_path = tmp_path / "compressed.forgepak"
        writer = BundleWriter(bundle_path, compress=True)
        entry = writer.add("big.txt", data)
        writer.finalize()
        assert entry.compression == "zstd"
        assert entry.size < entry.original_size

    @needs_zstd
    def test_compressed_round_trip(self, tmp_path):
        entries = {
            "text.txt": b"The quick brown fox " * 100,
            "binary.bin": bytes(range(256)) * 40,
        }
        bundle_path = _make_bundle(tmp_path, entries, compress=True)

        with BundleReader(bundle_path) as reader:
            for path, expected in entries.items():
                assert reader.read(path) == expected

    @needs_zstd
    def test_compression_level(self, tmp_path):
        data = b"B" * 5000
        path_low = tmp_path / "low.forgepak"
        path_high = tmp_path / "high.forgepak"

        w1 = BundleWriter(path_low, compress=True, compression_level=1)
        w1.add("data", data)
        w1.finalize()

        w2 = BundleWriter(path_high, compress=True, compression_level=19)
        w2.add("data", data)
        w2.finalize()

        # Higher level should produce smaller (or equal) output.
        assert path_high.stat().st_size <= path_low.stat().st_size


# ---------------------------------------------------------------------------
# BundleReader
# ---------------------------------------------------------------------------


class TestBundleReader:
    def test_read_missing_entry_raises(self, tmp_path):
        bundle_path = _make_bundle(tmp_path, {"a.txt": b"a"}, compress=False)
        with (
            BundleReader(bundle_path) as reader,
            pytest.raises(BundleError, match="not found"),
        ):
            reader.read("nonexistent.txt")

    def test_nonexistent_file_raises(self, tmp_path):
        with pytest.raises(BundleError, match="not found"):
            BundleReader(tmp_path / "missing.forgepak")

    def test_invalid_magic_raises(self, tmp_path):
        bad = tmp_path / "bad.forgepak"
        bad.write_bytes(b"NOPE" + b"\x00" * 20)
        with pytest.raises(BundleFormatError, match="magic"):
            BundleReader(bad)

    def test_truncated_header_raises(self, tmp_path):
        bad = tmp_path / "short.forgepak"
        bad.write_bytes(b"FPAK")  # only 4 bytes
        with pytest.raises(BundleFormatError, match="too small"):
            BundleReader(bad)

    def test_context_manager(self, tmp_path):
        bundle_path = _make_bundle(tmp_path, {"x.txt": b"x"}, compress=False)
        reader = BundleReader(bundle_path)
        reader.close()
        # Double close should not raise.
        reader.close()

    def test_manifest_property(self, tmp_path):
        entries = {"a.bin": b"aaa", "b.bin": b"bbb"}
        bundle_path = _make_bundle(tmp_path, entries, compress=False)
        with BundleReader(bundle_path) as reader:
            m = reader.manifest
            assert m.version == BUNDLE_VERSION
            assert set(m.paths) == {"a.bin", "b.bin"}


# ---------------------------------------------------------------------------
# DependencyGraph
# ---------------------------------------------------------------------------


class TestDependencyGraph:
    def test_empty_graph(self):
        g = DependencyGraph()
        assert g.assets == set()
        assert g.topological_order() == []
        assert not g.has_cycles()

    def test_add_and_query(self):
        g = DependencyGraph()
        g.add("mesh.bin", ["texture.png", "material.json"])
        assert g.dependencies_of("mesh.bin") == {"texture.png", "material.json"}
        assert g.dependents_of("texture.png") == {"mesh.bin"}
        assert g.dependents_of("material.json") == {"mesh.bin"}

    def test_transitive_dependents(self):
        g = DependencyGraph()
        g.add("scene.json", ["mesh.bin"])
        g.add("mesh.bin", ["texture.png"])
        assert g.all_dependents_of("texture.png") == {"mesh.bin", "scene.json"}

    def test_topological_order(self):
        g = DependencyGraph()
        g.add("c", ["b"])
        g.add("b", ["a"])
        order = g.topological_order()
        assert order.index("a") < order.index("b")
        assert order.index("b") < order.index("c")

    def test_cycle_detection(self):
        g = DependencyGraph()
        g.add("a", ["b"])
        g.add("b", ["a"])
        assert g.has_cycles()
        with pytest.raises(BundleError, match="cycle"):
            g.topological_order()

    def test_diamond_dependency(self):
        g = DependencyGraph()
        g.add("d", ["b", "c"])
        g.add("b", ["a"])
        g.add("c", ["a"])
        assert not g.has_cycles()
        order = g.topological_order()
        assert order.index("a") < order.index("b")
        assert order.index("a") < order.index("c")
        assert order.index("b") < order.index("d")
        assert order.index("c") < order.index("d")

    def test_no_dependencies(self):
        g = DependencyGraph()
        g.add("standalone.bin", [])
        assert g.dependencies_of("standalone.bin") == set()
        assert g.dependents_of("standalone.bin") == set()

    def test_from_meta_files(self, tmp_path):
        # Create mock .meta.json files.
        meta_dir = tmp_path / "processed"
        meta_dir.mkdir()

        (meta_dir / "mesh.bin.meta.json").write_text(
            json.dumps(
                {
                    "source": "mesh.gltf",
                    "output": "mesh.bin",
                    "dependencies": ["texture.png"],
                }
            )
        )
        (meta_dir / "scene.json.meta.json").write_text(
            json.dumps(
                {
                    "source": "scene.gltf",
                    "output": "scene.json",
                    "dependencies": ["mesh.bin"],
                }
            )
        )

        g = DependencyGraph.from_meta_files(meta_dir)
        assert g.dependencies_of("mesh.bin") == {"texture.png"}
        assert g.dependencies_of("scene.json") == {"mesh.bin"}
        assert g.all_dependents_of("texture.png") == {"mesh.bin", "scene.json"}

    def test_from_meta_files_empty_dir(self, tmp_path):
        g = DependencyGraph.from_meta_files(tmp_path / "nonexistent")
        assert g.assets == set()

    def test_from_meta_files_corrupt_json(self, tmp_path):
        meta_dir = tmp_path / "processed"
        meta_dir.mkdir()
        (meta_dir / "bad.meta.json").write_text("not json{{{")
        # Should not raise — just skips the bad file.
        g = DependencyGraph.from_meta_files(meta_dir)
        assert g.assets == set()


# ---------------------------------------------------------------------------
# create_bundle helper
# ---------------------------------------------------------------------------


class TestCreateBundle:
    def test_basic(self, tmp_path):
        output_dir = tmp_path / "processed"
        output_dir.mkdir()
        (output_dir / "a.txt").write_bytes(b"alpha")
        (output_dir / "b.txt").write_bytes(b"bravo")

        bundle_path = tmp_path / "assets.forgepak"
        manifest = create_bundle(output_dir, bundle_path, compress=False)

        assert len(manifest.entries) == 2
        with BundleReader(bundle_path) as reader:
            assert reader.read("a.txt") == b"alpha"
            assert reader.read("b.txt") == b"bravo"

    def test_excludes_meta_json(self, tmp_path):
        output_dir = tmp_path / "processed"
        output_dir.mkdir()
        (output_dir / "asset.bin").write_bytes(b"data")
        (output_dir / "asset.meta.json").write_text('{"source": "asset.bin"}')

        bundle_path = tmp_path / "assets.forgepak"
        manifest = create_bundle(output_dir, bundle_path, compress=False)

        assert len(manifest.entries) == 1
        assert manifest.entries[0].path == "asset.bin"

    def test_preserves_subdirectories(self, tmp_path):
        output_dir = tmp_path / "processed"
        (output_dir / "textures").mkdir(parents=True)
        (output_dir / "meshes").mkdir()
        (output_dir / "textures" / "brick.png").write_bytes(b"png data")
        (output_dir / "meshes" / "hero.bin").write_bytes(b"mesh data")

        bundle_path = tmp_path / "assets.forgepak"
        manifest = create_bundle(output_dir, bundle_path, compress=False)

        assert set(manifest.paths) == {"textures/brick.png", "meshes/hero.bin"}

    def test_patterns_filter(self, tmp_path):
        output_dir = tmp_path / "processed"
        output_dir.mkdir()
        (output_dir / "a.txt").write_bytes(b"text")
        (output_dir / "b.png").write_bytes(b"image")
        (output_dir / "c.txt").write_bytes(b"more text")

        bundle_path = tmp_path / "assets.forgepak"
        manifest = create_bundle(
            output_dir, bundle_path, compress=False, patterns=["*.txt"]
        )
        assert set(manifest.paths) == {"a.txt", "c.txt"}

    def test_empty_dir_raises(self, tmp_path):
        output_dir = tmp_path / "empty"
        output_dir.mkdir()
        with pytest.raises(BundleError, match="No files"):
            create_bundle(output_dir, tmp_path / "out.forgepak", compress=False)

    def test_nonexistent_dir_raises(self, tmp_path):
        with pytest.raises(BundleError, match="not found"):
            create_bundle(tmp_path / "nope", tmp_path / "out.forgepak", compress=False)

    def test_with_dependencies(self, tmp_path):
        output_dir = tmp_path / "processed"
        output_dir.mkdir()
        (output_dir / "mesh.bin").write_bytes(b"mesh")
        (output_dir / "tex.png").write_bytes(b"tex")
        (output_dir / "mesh.bin.meta.json").write_text(
            json.dumps(
                {
                    "source": "mesh.gltf",
                    "output": "mesh.bin",
                    "dependencies": ["tex.png"],
                }
            )
        )

        bundle_path = tmp_path / "assets.forgepak"
        manifest = create_bundle(output_dir, bundle_path, compress=False)

        mesh_entry = manifest.get("mesh.bin")
        assert mesh_entry is not None
        assert mesh_entry.dependencies == ["tex.png"]

    @needs_zstd
    def test_compressed_round_trip(self, tmp_path):
        output_dir = tmp_path / "processed"
        output_dir.mkdir()
        data = b"repetitive " * 500
        (output_dir / "big.bin").write_bytes(data)

        bundle_path = tmp_path / "compressed.forgepak"
        manifest = create_bundle(output_dir, bundle_path, compress=True)

        assert manifest.entries[0].compression == "zstd"
        assert manifest.entries[0].size < manifest.entries[0].original_size

        with BundleReader(bundle_path) as reader:
            assert reader.read("big.bin") == data
