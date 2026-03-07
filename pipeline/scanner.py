"""Asset file scanning and content-hash fingerprinting.

The scanner walks a source directory, finds files whose extensions match a
registered plugin, and computes a SHA-256 content hash for each file.
Content hashes — not timestamps — drive incremental builds because they are:

- **Deterministic** — the same bytes always produce the same hash.
- **Portable** — hashes survive ``git clone``, file copies, and CI.
- **Correct** — a file touched without changing content is not reprocessed.

The fingerprint cache is a JSON file that maps relative paths to their last
known hash.  On subsequent runs the scanner compares current hashes against
the cache and reports which files are new, changed, or unchanged.
"""

from __future__ import annotations

import hashlib
import json
import logging
from dataclasses import dataclass
from enum import Enum, auto
from pathlib import Path

log = logging.getLogger(__name__)

# Read files in 64 KiB chunks — large enough for throughput, small enough
# to avoid excessive memory use on huge assets.
HASH_CHUNK_SIZE = 65536


# ---------------------------------------------------------------------------
# Types
# ---------------------------------------------------------------------------


class FileStatus(Enum):
    """Whether a scanned file needs processing."""

    NEW = auto()  # Not in the fingerprint cache — first time seeing it.
    CHANGED = auto()  # Hash differs from the cached value.
    UNCHANGED = auto()  # Hash matches the cache — skip processing.


@dataclass
class ScannedFile:
    """A source file discovered by the scanner."""

    path: Path  # Absolute path to the file.
    relative: Path  # Path relative to the source directory.
    extension: str  # Lowercase extension including the dot (e.g. ".png").
    fingerprint: str  # SHA-256 hex digest of the file contents.
    status: FileStatus  # Whether the file has changed since the last scan.


# ---------------------------------------------------------------------------
# Fingerprinting
# ---------------------------------------------------------------------------


def fingerprint_file(path: Path) -> str:
    """Return the SHA-256 hex digest of *path*'s contents."""
    h = hashlib.sha256()
    try:
        with open(path, "rb") as f:
            while chunk := f.read(HASH_CHUNK_SIZE):
                h.update(chunk)
    except OSError as exc:
        raise OSError(f"Failed to fingerprint {path}: {exc}") from exc
    return h.hexdigest()


# ---------------------------------------------------------------------------
# Cache
# ---------------------------------------------------------------------------


class FingerprintCache:
    """Persistent mapping of relative file paths to content hashes.

    Stored as a simple JSON object on disk.  Keys are POSIX-style relative
    paths (forward slashes) so the cache is cross-platform.
    """

    def __init__(self, cache_path: Path) -> None:
        self._path = cache_path
        self._data: dict[str, str] = {}
        if cache_path.exists():
            try:
                raw = json.loads(cache_path.read_text(encoding="utf-8"))
                if not isinstance(raw, dict) or not all(
                    isinstance(k, str) and isinstance(v, str) for k, v in raw.items()
                ):
                    raise ValueError("cache must be dict[str, str]")
                self._data = raw
            except (json.JSONDecodeError, OSError, ValueError):
                log.warning(
                    "Corrupt fingerprint cache at %s — starting fresh", cache_path
                )
                self._data = {}

    def get(self, relative: Path) -> str | None:
        """Return the cached hash for *relative*, or ``None``."""
        return self._data.get(relative.as_posix())

    def set(self, relative: Path, fingerprint: str) -> None:
        """Update the cached hash for *relative*."""
        self._data[relative.as_posix()] = fingerprint

    def save(self) -> None:
        """Write the cache to disk."""
        self._path.parent.mkdir(parents=True, exist_ok=True)
        self._path.write_text(
            json.dumps(self._data, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        log.debug(
            "Saved fingerprint cache to %s (%d entries)", self._path, len(self._data)
        )


# ---------------------------------------------------------------------------
# Scanner
# ---------------------------------------------------------------------------


def scan(
    source_dir: Path,
    supported_extensions: set[str],
    cache: FingerprintCache,
) -> list[ScannedFile]:
    """Walk *source_dir* recursively, fingerprint every file whose extension
    is in *supported_extensions*, and classify it as new / changed / unchanged
    by comparing against *cache*.

    Returns a list of ``ScannedFile`` objects sorted by relative path.
    """
    if not source_dir.is_dir():
        log.warning("Source directory does not exist: %s", source_dir)
        return []

    results: list[ScannedFile] = []

    for path in sorted(source_dir.rglob("*")):
        if not path.is_file():
            continue

        ext = path.suffix.lower()
        if ext not in supported_extensions:
            continue

        relative = path.relative_to(source_dir)
        fp = fingerprint_file(path)

        cached = cache.get(relative)
        if cached is None:
            status = FileStatus.NEW
        elif cached != fp:
            status = FileStatus.CHANGED
        else:
            status = FileStatus.UNCHANGED

        results.append(
            ScannedFile(
                path=path,
                relative=relative,
                extension=ext,
                fingerprint=fp,
                status=status,
            )
        )

    log.info(
        "Scanned %d file(s) in %s — %d new, %d changed, %d unchanged",
        len(results),
        source_dir,
        sum(1 for f in results if f.status is FileStatus.NEW),
        sum(1 for f in results if f.status is FileStatus.CHANGED),
        sum(1 for f in results if f.status is FileStatus.UNCHANGED),
    )
    return results
