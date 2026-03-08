"""Asset bundle packing with random-access table of contents.

A bundle (``.forgepak``) packs multiple processed assets into a single file.
Each entry is compressed independently with zstd so the reader can decompress
any single entry without reading the rest — true random access, not sequential
unpacking like ZIP.

File layout::

    ┌──────────────────────────────────────┐
    │ Header              (24 bytes)       │
    ├──────────────────────────────────────┤
    │ Entry 0 data        (zstd or raw)    │
    │ Entry 1 data        (zstd or raw)    │
    │ …                                    │
    ├──────────────────────────────────────┤
    │ Table of Contents   (zstd JSON)      │
    └──────────────────────────────────────┘

Header (little-endian):

=============  =====  ===================================
Field          Bytes  Description
=============  =====  ===================================
magic          4      ``b"FPAK"``
version        4      Format version (currently 1)
entry_count    4      Number of entries in the bundle
toc_offset     8      Byte offset of the TOC from file start
toc_size       4      Compressed size of the TOC in bytes
=============  =====  ===================================

The TOC sits at the end of the file so entries can be written sequentially
without knowing the final TOC size in advance.  After all entries are written,
the writer serializes the TOC, writes it, then seeks back to patch the header
with the real ``toc_offset`` and ``toc_size``.

Why per-entry compression?
    Whole-file compression (e.g. gzip a tar) achieves better ratios but
    forces sequential decompression to reach any single entry.  Per-entry
    compression trades a few percent of ratio for O(1) random access — the
    reader seeks to an offset, reads ``compressed_size`` bytes, and
    decompresses that block alone.  For a game loading screen that needs
    one texture, this is dramatically faster than decompressing the entire
    bundle.
"""

from __future__ import annotations

import hashlib
import json
import logging
import struct
from dataclasses import dataclass, field
from pathlib import Path

log = logging.getLogger(__name__)

# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

BUNDLE_MAGIC = b"FPAK"
BUNDLE_VERSION = 1
HEADER_SIZE = 24  # 4 + 4 + 4 + 8 + 4
HEADER_FORMAT = "<4sIIQI"  # magic, version, entry_count, toc_offset, toc_size
HASH_CHUNK_SIZE = 65536  # 64 KiB — same as scanner.py
DEFAULT_COMPRESSION_LEVEL = 3  # zstd level (1-22, 3 is a good speed/ratio trade-off)


# ---------------------------------------------------------------------------
# Data types
# ---------------------------------------------------------------------------


@dataclass
class BundleEntry:
    """Metadata for a single asset stored in a bundle.

    This is one row in the table of contents.  The reader uses ``offset``
    and ``size`` to seek directly to the compressed data, then decompresses
    ``size`` bytes to recover the original ``original_size`` bytes.
    """

    path: str  # POSIX-style relative path within the bundle
    offset: int  # byte offset from file start to compressed data
    size: int  # compressed size in bytes
    original_size: int  # uncompressed size in bytes
    compression: str  # "zstd" or "none"
    fingerprint: str  # SHA-256 hex digest of uncompressed content
    dependencies: list[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        """Serialize to a JSON-friendly dict."""
        d: dict = {
            "path": self.path,
            "offset": self.offset,
            "size": self.size,
            "original_size": self.original_size,
            "compression": self.compression,
            "fingerprint": self.fingerprint,
        }
        if self.dependencies:
            d["dependencies"] = self.dependencies
        return d

    @classmethod
    def from_dict(cls, d: dict) -> BundleEntry:
        """Deserialize from a parsed JSON dict."""
        return cls(
            path=d["path"],
            offset=d["offset"],
            size=d["size"],
            original_size=d["original_size"],
            compression=d["compression"],
            fingerprint=d["fingerprint"],
            dependencies=d.get("dependencies", []),
        )


@dataclass
class BundleManifest:
    """Table of contents for a bundle file."""

    version: int
    entries: list[BundleEntry]

    def get(self, path: str) -> BundleEntry | None:
        """Look up an entry by path, or return ``None``."""
        for entry in self.entries:
            if entry.path == path:
                return entry
        return None

    @property
    def paths(self) -> list[str]:
        """All entry paths in insertion order."""
        return [e.path for e in self.entries]

    @property
    def total_compressed(self) -> int:
        """Sum of all compressed entry sizes."""
        return sum(e.size for e in self.entries)

    @property
    def total_original(self) -> int:
        """Sum of all original entry sizes."""
        return sum(e.original_size for e in self.entries)


# ---------------------------------------------------------------------------
# Errors
# ---------------------------------------------------------------------------


class BundleError(Exception):
    """Raised when a bundle operation fails."""


class BundleFormatError(BundleError):
    """Raised when a bundle file has an invalid format."""


# ---------------------------------------------------------------------------
# Writer
# ---------------------------------------------------------------------------


def _fingerprint_bytes(data: bytes) -> str:
    """Return the SHA-256 hex digest of *data*."""
    return hashlib.sha256(data).hexdigest()


def _try_import_zstd():
    """Import zstandard and return the module, or None if unavailable."""
    try:
        import zstandard

        return zstandard
    except ImportError:
        return None


class BundleWriter:
    """Creates a ``.forgepak`` bundle file from asset data.

    Usage::

        writer = BundleWriter(Path("assets.forgepak"))
        writer.add("textures/brick.png", brick_bytes)
        writer.add_file(Path("processed/mesh.bin"), "meshes/hero.bin")
        manifest = writer.finalize()
    """

    def __init__(
        self,
        output_path: Path,
        *,
        compression_level: int = DEFAULT_COMPRESSION_LEVEL,
        compress: bool = True,
    ) -> None:
        self._output_path = output_path
        self._compression_level = compression_level
        self._compress = compress
        self._entries: list[BundleEntry] = []
        self._finalized = False

        # Open the output file and write a placeholder header.
        output_path.parent.mkdir(parents=True, exist_ok=True)
        self._file = open(output_path, "wb")  # noqa: SIM115
        self._file.write(b"\x00" * HEADER_SIZE)

        self._zstd = _try_import_zstd() if compress else None
        if compress and self._zstd is None:
            log.warning(
                "zstandard not installed — bundling without compression. "
                "Install with: pip install zstandard"
            )
            self._compress = False

    def add(
        self,
        path: str,
        data: bytes,
        *,
        fingerprint: str = "",
        dependencies: list[str] | None = None,
    ) -> BundleEntry:
        """Add raw bytes to the bundle under *path*.

        Returns the ``BundleEntry`` created for this asset.
        """
        if self._finalized:
            raise BundleError("Cannot add entries after finalize()")

        # Normalize path to POSIX forward slashes (handle Windows backslashes).
        posix_path = path.replace("\\", "/")

        # Check for duplicate paths.
        if any(e.path == posix_path for e in self._entries):
            raise BundleError(f"Duplicate entry path: {posix_path!r}")

        fp = fingerprint or _fingerprint_bytes(data)
        original_size = len(data)

        # Compress the data.
        if self._compress and self._zstd is not None:
            cctx = self._zstd.ZstdCompressor(level=self._compression_level)
            compressed = cctx.compress(data)
            compression = "zstd"
        else:
            compressed = data
            compression = "none"

        # Write to file and record the offset.
        offset = self._file.tell()
        self._file.write(compressed)

        entry = BundleEntry(
            path=posix_path,
            offset=offset,
            size=len(compressed),
            original_size=original_size,
            compression=compression,
            fingerprint=fp,
            dependencies=dependencies or [],
        )
        self._entries.append(entry)
        log.debug(
            "Added %s (%d -> %d bytes, %s)",
            posix_path,
            original_size,
            len(compressed),
            compression,
        )
        return entry

    def add_file(
        self,
        source: Path,
        bundle_path: str,
        *,
        fingerprint: str = "",
        dependencies: list[str] | None = None,
    ) -> BundleEntry:
        """Read *source* from disk and add it to the bundle under *bundle_path*."""
        data = source.read_bytes()
        return self.add(
            bundle_path,
            data,
            fingerprint=fingerprint,
            dependencies=dependencies,
        )

    def finalize(self) -> BundleManifest:
        """Write the TOC, patch the header, and close the file.

        Returns the ``BundleManifest`` describing the completed bundle.
        Must be called exactly once.
        """
        if self._finalized:
            raise BundleError("finalize() already called")
        self._finalized = True

        # Serialize and compress the TOC.
        toc_json = json.dumps(
            [e.to_dict() for e in self._entries],
            separators=(",", ":"),  # compact JSON
        ).encode("utf-8")

        if self._compress and self._zstd is not None:
            cctx = self._zstd.ZstdCompressor(level=self._compression_level)
            toc_data = cctx.compress(toc_json)
        else:
            toc_data = toc_json

        toc_offset = self._file.tell()
        toc_size = len(toc_data)
        self._file.write(toc_data)

        # Seek back and write the real header.
        header = struct.pack(
            HEADER_FORMAT,
            BUNDLE_MAGIC,
            BUNDLE_VERSION,
            len(self._entries),
            toc_offset,
            toc_size,
        )
        self._file.seek(0)
        self._file.write(header)
        self._file.close()

        manifest = BundleManifest(version=BUNDLE_VERSION, entries=self._entries)
        log.info(
            "Bundle written: %s (%d entries, %d bytes)",
            self._output_path,
            len(self._entries),
            toc_offset + toc_size,
        )
        return manifest


# ---------------------------------------------------------------------------
# Reader
# ---------------------------------------------------------------------------


class BundleReader:
    """Reads entries from a ``.forgepak`` bundle with random access.

    Usage::

        with BundleReader(Path("assets.forgepak")) as reader:
            data = reader.read("textures/brick.png")
            print(reader.manifest.paths)
    """

    def __init__(self, bundle_path: Path) -> None:
        if not bundle_path.is_file():
            raise BundleError(f"Bundle file not found: {bundle_path}")

        self._path = bundle_path
        self._file = open(bundle_path, "rb")  # noqa: SIM115
        self._manifest = self._read_manifest()
        self._zstd = _try_import_zstd()

    def _read_manifest(self) -> BundleManifest:
        """Parse the header and TOC from the bundle file."""
        # Read and validate header.
        header_bytes = self._file.read(HEADER_SIZE)
        if len(header_bytes) < HEADER_SIZE:
            raise BundleFormatError("File too small for a valid bundle header")

        magic, version, entry_count, toc_offset, toc_size = struct.unpack(
            HEADER_FORMAT, header_bytes
        )

        if magic != BUNDLE_MAGIC:
            raise BundleFormatError(
                f"Invalid magic bytes: expected {BUNDLE_MAGIC!r}, got {magic!r}"
            )

        if version != BUNDLE_VERSION:
            raise BundleFormatError(
                f"Unsupported bundle version: {version} (expected {BUNDLE_VERSION})"
            )

        # Read and decompress the TOC.
        self._file.seek(toc_offset)
        toc_raw = self._file.read(toc_size)
        if len(toc_raw) < toc_size:
            raise BundleFormatError("Truncated TOC data")

        # Try decompressing as zstd first; fall back to raw JSON.
        toc_json = self._decompress_or_raw(toc_raw)

        try:
            toc_list = json.loads(toc_json)
            if not isinstance(toc_list, list):
                raise TypeError("TOC root must be a list")
            entries = [BundleEntry.from_dict(d) for d in toc_list]
        except (json.JSONDecodeError, KeyError, TypeError, ValueError) as exc:
            raise BundleFormatError(f"Invalid TOC data: {exc}") from exc

        if len(entries) != entry_count:
            raise BundleFormatError(
                f"Header says {entry_count} entries but TOC has {len(entries)}"
            )

        return BundleManifest(version=version, entries=entries)

    def _decompress_or_raw(self, data: bytes) -> bytes:
        """Decompress zstd data, or return as-is if it is raw JSON."""
        zstd_mod = _try_import_zstd()
        if zstd_mod is not None and len(data) >= 4 and data[:4] == b"\x28\xb5\x2f\xfd":  # noqa: E501
            # Zstd magic bytes (0xFD2FB528) — decompress.
            dctx = zstd_mod.ZstdDecompressor()
            return dctx.decompress(data)
        return data

    @property
    def manifest(self) -> BundleManifest:
        """The bundle's table of contents."""
        return self._manifest

    def read(self, path: str) -> bytes:
        """Read and decompress the entry at *path*.

        Raises ``BundleError`` if the path is not in the bundle.
        """
        entry = self._manifest.get(path)
        if entry is None:
            raise BundleError(f"Entry not found: {path!r}")
        return self.read_entry(entry)

    def read_entry(self, entry: BundleEntry) -> bytes:
        """Read and decompress a specific ``BundleEntry``."""
        self._file.seek(entry.offset)
        raw = self._file.read(entry.size)
        if len(raw) < entry.size:
            raise BundleFormatError(
                f"Truncated entry data for {entry.path!r}: "
                f"expected {entry.size} bytes, got {len(raw)}"
            )

        if entry.compression == "none":
            return raw

        if entry.compression == "zstd":
            zstd_mod = _try_import_zstd()
            if zstd_mod is None:
                raise BundleError(
                    "zstandard not installed — cannot decompress entry. "
                    "Install with: pip install zstandard"
                )
            dctx = zstd_mod.ZstdDecompressor()
            return dctx.decompress(raw, max_output_size=entry.original_size)

        raise BundleFormatError(f"Unknown compression: {entry.compression!r}")

    def close(self) -> None:
        """Close the underlying file handle."""
        self._file.close()

    def __enter__(self) -> BundleReader:
        return self

    def __exit__(self, *args: object) -> None:
        self.close()


# ---------------------------------------------------------------------------
# Dependency graph
# ---------------------------------------------------------------------------


class DependencyGraph:
    """Tracks directed dependencies between assets.

    An edge ``A -> B`` means "asset A depends on asset B".  When B changes,
    A must be reprocessed (or re-bundled).

    The graph supports:
    - Forward queries: *what does A depend on?*
    - Reverse queries: *what depends on B?*  (i.e. what is invalidated)
    - Cycle detection
    - Topological ordering for correct processing order
    """

    def __init__(self) -> None:
        self._forward: dict[str, set[str]] = {}  # asset -> its dependencies
        self._reverse: dict[str, set[str]] = {}  # asset -> its dependents

    def add(self, asset: str, depends_on: list[str]) -> None:
        """Record that *asset* depends on each path in *depends_on*."""
        if asset not in self._forward:
            self._forward[asset] = set()
        for dep in depends_on:
            self._forward[asset].add(dep)
            if dep not in self._reverse:
                self._reverse[dep] = set()
            self._reverse[dep].add(asset)

    def dependencies_of(self, asset: str) -> set[str]:
        """Return the set of assets that *asset* directly depends on."""
        return set(self._forward.get(asset, set()))

    def dependents_of(self, asset: str) -> set[str]:
        """Return all assets that directly depend on *asset*.

        When *asset* changes, every asset in this set is invalidated.
        """
        return set(self._reverse.get(asset, set()))

    def all_dependents_of(self, asset: str) -> set[str]:
        """Return all assets transitively depending on *asset*."""
        result: set[str] = set()
        queue = list(self.dependents_of(asset))
        while queue:
            dep = queue.pop()
            if dep not in result:
                result.add(dep)
                queue.extend(self.dependents_of(dep))
        return result

    @property
    def assets(self) -> set[str]:
        """All known asset paths."""
        return set(self._forward.keys()) | set(self._reverse.keys())

    def has_cycles(self) -> bool:
        """Return ``True`` if the dependency graph contains a cycle."""
        try:
            self.topological_order()
            return False
        except BundleError:
            return True

    def topological_order(self) -> list[str]:
        """Return assets in dependency order (dependencies before dependents).

        Raises ``BundleError`` if the graph contains a cycle.
        """
        # Kahn's algorithm.
        in_degree: dict[str, int] = {a: 0 for a in self.assets}
        for asset, deps in self._forward.items():
            for dep in deps:
                if dep in in_degree:
                    in_degree[asset] = in_degree.get(asset, 0)
                # in_degree counts how many deps each asset has that are
                # also in the graph — but for topological sort we want
                # "how many predecessors".
        # Rebuild: in_degree[X] = number of edges Y -> X (Y depends on X
        # means X must come first, so X has in_degree from reverse perspective).
        # Actually: edge A -> B means A depends on B, so B must come before A.
        # In Kahn's: in_degree[A] = |dependencies of A that are in the graph|.
        in_degree = {a: 0 for a in self.assets}
        for asset, deps in self._forward.items():
            for dep in deps:
                if dep in in_degree:
                    in_degree[asset] += 1

        queue = [a for a, d in in_degree.items() if d == 0]
        result: list[str] = []

        while queue:
            node = queue.pop(0)
            result.append(node)
            # For every asset that depends on node, decrement its in_degree.
            for dependent in self._reverse.get(node, set()):
                if dependent in in_degree:
                    in_degree[dependent] -= 1
                    if in_degree[dependent] == 0:
                        queue.append(dependent)

        if len(result) != len(in_degree):
            raise BundleError("Dependency cycle detected")

        return result

    @classmethod
    def from_meta_files(cls, output_dir: Path) -> DependencyGraph:
        """Build a dependency graph from ``.meta.json`` sidecar files.

        Each sidecar may contain a ``"dependencies"`` key listing relative
        paths of assets that the source file references (e.g. a mesh that
        references a texture).
        """
        graph = cls()
        if not output_dir.is_dir():
            return graph

        for meta_path in sorted(output_dir.rglob("*.meta.json")):
            try:
                meta = json.loads(meta_path.read_text(encoding="utf-8"))
            except (json.JSONDecodeError, OSError):
                log.warning("Skipping unreadable meta file: %s", meta_path)
                continue

            # Key by the processed asset's relative path (matching the key
            # used by create_bundle), not meta["source"] which is the raw
            # input filename.
            asset = meta.get("output", "")
            if not asset:
                asset = (
                    meta_path.relative_to(output_dir)
                    .as_posix()
                    .removesuffix(".meta.json")
                )
            deps = meta.get("dependencies", [])
            if asset and deps:
                graph.add(asset, deps)

        return graph


# ---------------------------------------------------------------------------
# CLI helpers
# ---------------------------------------------------------------------------


def create_bundle(
    output_dir: Path,
    bundle_path: Path,
    *,
    compress: bool = True,
    compression_level: int = DEFAULT_COMPRESSION_LEVEL,
    patterns: list[str] | None = None,
) -> BundleManifest:
    """Pack all files in *output_dir* into a bundle at *bundle_path*.

    If *patterns* is given, only files matching those glob patterns are
    included (e.g. ``["*.png", "*.fmesh"]``).

    Returns the manifest of the completed bundle.
    """
    if not output_dir.is_dir():
        raise BundleError(f"Output directory not found: {output_dir}")

    # Collect files to bundle.
    files: list[Path] = []
    if patterns:
        for pattern in patterns:
            files.extend(sorted(output_dir.rglob(pattern)))
    else:
        files = sorted(output_dir.rglob("*"))

    bundle_path_abs = bundle_path.resolve()

    # Filter to regular files, exclude .meta.json sidecars and the output
    # bundle itself, and deduplicate (overlapping globs can match the same
    # file twice).
    asset_files = list(
        dict.fromkeys(
            f
            for f in files
            if f.is_file()
            and not f.name.endswith(".meta.json")
            and f.resolve() != bundle_path_abs
        )
    )

    if not asset_files:
        raise BundleError(f"No files to bundle in {output_dir}")

    # Build the dependency graph from .meta.json files.
    dep_graph = DependencyGraph.from_meta_files(output_dir)

    writer = BundleWriter(
        bundle_path,
        compression_level=compression_level,
        compress=compress,
    )

    for file_path in asset_files:
        relative = file_path.relative_to(output_dir)
        bundle_path_str = relative.as_posix()

        # Look up dependencies for this asset.
        deps = list(dep_graph.dependencies_of(bundle_path_str))

        writer.add_file(file_path, bundle_path_str, dependencies=deps)

    return writer.finalize()
