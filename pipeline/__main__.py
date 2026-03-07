"""CLI entry point for the forge asset pipeline.

Run with::

    python -m pipeline [OPTIONS]

The CLI ties together the three core subsystems built in this lesson:

1. **Configuration** — load ``pipeline.toml`` for project settings.
2. **Plugin discovery** — scan a plugins directory, register handlers.
3. **Asset scanning** — walk the source tree, fingerprint files, and report
   what needs processing.

No actual processing happens yet — this lesson builds the scaffold that
later lessons fill in with real texture, mesh, and scene processing plugins.
"""

from __future__ import annotations

import argparse
import logging
import sys
from pathlib import Path

from pipeline.config import ConfigError, default_config, load_config
from pipeline.plugin import PluginRegistry
from pipeline.scanner import FileStatus, FingerprintCache, scan

log = logging.getLogger("pipeline")


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="forge-pipeline",
        description="Asset processing pipeline for forge-gpu.",
    )
    parser.add_argument(
        "-c",
        "--config",
        type=Path,
        default=None,
        help="Path to the TOML configuration file (default: pipeline.toml)",
    )
    parser.add_argument(
        "--plugins-dir",
        type=Path,
        default=None,
        help="Directory containing plugin .py files (default: built-in pipeline/plugins/)",
    )
    parser.add_argument(
        "--source-dir",
        type=Path,
        default=None,
        help="Override the source directory from the config file",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Scan and report without processing (always true in this lesson)",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Enable debug logging",
    )
    return parser


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def main(argv: list[str] | None = None) -> int:
    """Entry point.  Returns 0 on success, 1 on error."""
    args = build_parser().parse_args(argv)

    # -- Logging ------------------------------------------------------------
    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(name)s: %(message)s",
    )

    # -- Configuration ------------------------------------------------------
    config_path = args.config or Path("pipeline.toml")
    if config_path.exists():
        try:
            config = load_config(config_path)
            log.info("Loaded config from %s", config_path)
        except ConfigError as exc:
            log.error("%s", exc)
            return 1
    elif args.config is None:
        config = default_config()
        log.info("No config file found — using defaults")
    else:
        log.error("Configuration file not found: %s", config_path)
        return 1

    # CLI overrides
    if args.source_dir is not None:
        config.source_dir = args.source_dir

    # -- Plugin discovery ---------------------------------------------------
    plugins_dir = args.plugins_dir
    if plugins_dir is None:
        # Default to the built-in plugins shipped with the pipeline package.
        plugins_dir = Path(__file__).resolve().parent / "plugins"

    registry = PluginRegistry()
    if plugins_dir.is_dir():
        count = registry.discover(plugins_dir)
        log.info("Loaded %d plugin(s)", count)
    elif args.plugins_dir is not None:
        # User explicitly provided a path that doesn't exist — fail fast.
        log.error("Plugins directory not found: %s", plugins_dir)
        return 1
    else:
        log.info("No plugins directory at %s — running with no plugins", plugins_dir)

    # Show registered plugins
    for plugin in registry.plugins:
        exts = ", ".join(plugin.extensions)
        log.info("  %-12s  %s", plugin.name, exts)

    supported = registry.supported_extensions
    if not supported:
        log.warning("No plugins registered — nothing to scan")
        print("\nNo plugins found.  Create a plugins/ directory with .py files")
        print("that define AssetPlugin subclasses.  See the lesson README for details.")
        return 0

    # -- Scanning -----------------------------------------------------------
    cache_path = config.cache_dir / "fingerprints.json"
    cache = FingerprintCache(cache_path)

    if not config.source_dir.is_dir():
        log.error("Source directory not found: %s", config.source_dir)
        return 1

    files = scan(config.source_dir, supported, cache)
    if not files:
        print(f"\nNo supported files found in {config.source_dir}")
        return 0

    # -- Report -------------------------------------------------------------
    new_count = sum(1 for f in files if f.status is FileStatus.NEW)
    changed_count = sum(1 for f in files if f.status is FileStatus.CHANGED)
    unchanged_count = sum(1 for f in files if f.status is FileStatus.UNCHANGED)

    print(f"\nScanned {len(files)} file(s) in {config.source_dir}:")
    print(f"  {new_count} new")
    print(f"  {changed_count} changed")
    print(f"  {unchanged_count} unchanged")

    # Detailed listing
    if args.verbose or new_count + changed_count > 0:
        print()
        status_labels = {
            FileStatus.NEW: "[NEW]    ",
            FileStatus.CHANGED: "[CHANGED]",
            FileStatus.UNCHANGED: "[OK]     ",
        }
        for f in files:
            label = status_labels[f.status]
            plugin = registry.get_by_extension(f.extension)
            plugin_name = plugin.name if plugin else "?"
            print(f"  {label}  {f.relative}  ({plugin_name})")

    # Update the fingerprint cache so the next run sees unchanged files as
    # UNCHANGED.  This demonstrates incremental detection — the core value of
    # content-hash fingerprinting.  Later lessons will tie cache updates to
    # successful processing; for now the scan itself is the "processing step".
    for f in files:
        if f.status is not FileStatus.UNCHANGED:
            cache.set(f.relative, f.fingerprint)
    cache.save()

    if new_count + changed_count > 0:
        print(f"\n{new_count + changed_count} file(s) would be processed.")
    else:
        print("\nAll files up to date — nothing to process.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
