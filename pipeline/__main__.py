"""CLI entry point for the forge asset pipeline.

Run with::

    python -m pipeline [OPTIONS]

The CLI ties together the core subsystems:

1. **Configuration** — load ``pipeline.toml`` for project settings.
2. **Plugin discovery** — scan a plugins directory, register handlers.
3. **Asset scanning** — walk the source tree, fingerprint files, and report
   what needs processing.
4. **Processing** — run each plugin on new/changed files, write outputs and
   metadata sidecars, then update the fingerprint cache.

Use ``--dry-run`` to scan and report without actually processing files.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import logging
import sys
from pathlib import Path

from pipeline.bundler import BundleError, BundleFormatError, BundleReader, create_bundle
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
        help="Scan and report without processing",
    )
    parser.add_argument(
        "--plugin",
        type=str,
        default=None,
        help="Only run this plugin (by name, e.g. --plugin animation)",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Enable debug logging",
    )

    # -- Subcommands --------------------------------------------------------
    subparsers = parser.add_subparsers(dest="command")

    bundle_cmd = subparsers.add_parser(
        "bundle",
        help="Pack processed assets into a .forgepak bundle",
    )
    bundle_cmd.add_argument(
        "-o",
        "--output",
        type=Path,
        default=None,
        help="Output bundle path (default: <output_dir>/assets.forgepak)",
    )
    bundle_cmd.add_argument(
        "--no-compress",
        action="store_true",
        help="Disable zstd compression",
    )
    bundle_cmd.add_argument(
        "--level",
        type=int,
        default=3,
        help="Zstd compression level, 1-22 (default: 3)",
    )
    bundle_cmd.add_argument(
        "--pattern",
        action="append",
        default=None,
        help="Glob pattern to filter files (repeatable, e.g. --pattern '*.png')",
    )

    info_cmd = subparsers.add_parser(
        "info",
        help="Show contents of a .forgepak bundle",
    )
    info_cmd.add_argument(
        "bundle_file",
        type=Path,
        help="Path to the .forgepak bundle file",
    )

    serve_cmd = subparsers.add_parser(
        "serve",
        help="Start the web UI server",
    )
    serve_cmd.add_argument(
        "--host",
        type=str,
        default="127.0.0.1",
        help="Host to bind to (default: 127.0.0.1)",
    )
    serve_cmd.add_argument(
        "--port",
        type=int,
        default=8000,
        help="Port to listen on (default: 8000)",
    )
    serve_cmd.add_argument(
        "--build",
        action="store_true",
        help="Rebuild the frontend before starting the server",
    )
    serve_cmd.add_argument(
        "--no-auto-build",
        action="store_true",
        help="Skip the automatic frontend rebuild when dist/ is stale",
    )

    return parser


# ---------------------------------------------------------------------------
# Settings-aware fingerprinting
# ---------------------------------------------------------------------------


def _fingerprint_with_settings(content_hash: str, settings: dict) -> str:
    """Combine a file's content hash with its plugin settings.

    This ensures that changing a setting (e.g. ``max_size``) invalidates
    previously processed assets even if the source file hasn't changed.
    """
    settings_json = json.dumps(settings, sort_keys=True)
    combined = hashlib.sha256(f"{content_hash}:{settings_json}".encode()).hexdigest()
    return combined


# ---------------------------------------------------------------------------
# Processing
# ---------------------------------------------------------------------------


def _process_files(
    files,
    registry,
    config,
    cache,
    *,
    plugin_filter: str | None = None,
) -> tuple[int, int, set]:
    """Run plugins on NEW and CHANGED files.

    Returns *(success_count, error_count, succeeded_keys)* where
    *succeeded_keys* is the set of ``"relative:plugin_name"`` cache keys
    that were processed without error.
    """
    to_process = [f for f in files if f.status is not FileStatus.UNCHANGED]
    if not to_process:
        return 0, 0, set()

    config.output_dir.mkdir(parents=True, exist_ok=True)

    success = 0
    errors = 0
    succeeded: set[str] = set()

    for f in to_process:
        plugins = registry.get_by_extension(f.extension)
        if not plugins:
            log.warning("No plugin for %s — skipping", f.relative)
            continue

        for plugin in plugins:
            # If --plugin was given, skip plugins that don't match.
            if plugin_filter is not None and plugin.name != plugin_filter:
                continue

            # Check per-plugin cache — skip if this (file, plugin) pair
            # is already up to date.
            settings = config.plugin_settings.get(plugin.name, {})
            combined = _fingerprint_with_settings(f.fingerprint, settings)
            cache_key = f"{f.relative}:{plugin.name}"
            cached = cache.get(Path(cache_key))
            if cached == combined:
                continue

            # Build the output subdirectory mirroring the source tree.
            relative_dir = f.relative.parent
            output_subdir = config.output_dir / relative_dir
            output_subdir.mkdir(parents=True, exist_ok=True)

            try:
                result = plugin.process(f.path, output_subdir, settings)
                processed = result.metadata.get("processed", True)
                if processed:
                    log.info(
                        "  [OK] %s (%s) -> %s",
                        f.relative,
                        plugin.name,
                        result.output.name,
                    )
                    success += 1
                    succeeded.add(cache_key)
                else:
                    log.info(
                        "  [SKIP] %s (%s): %s",
                        f.relative,
                        plugin.name,
                        result.metadata.get("reason", "not processed"),
                    )
            except Exception:
                log.exception("  [FAIL] %s (%s)", f.relative, plugin.name)
                errors += 1

    return success, errors, succeeded


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------


def _cmd_bundle(args, config) -> int:
    """Handle the ``bundle`` subcommand."""
    bundle_settings = config.plugin_settings.get("bundle", {})

    compress = not args.no_compress
    level = args.level
    patterns = args.pattern

    # Determine output path.
    bundle_dir = Path(bundle_settings.get("bundle_dir", config.output_dir))
    output_path = args.output
    if output_path is None:
        output_path = bundle_dir / "assets.forgepak"

    if not config.output_dir.is_dir():
        log.error("Output directory not found: %s", config.output_dir)
        log.error("Run 'forge-pipeline' first to process assets.")
        return 1

    print(f"\nBundling assets from {config.output_dir}...")
    try:
        manifest = create_bundle(
            config.output_dir,
            output_path,
            compress=compress,
            compression_level=level,
            patterns=patterns,
        )
    except BundleError as exc:
        log.error("Bundle failed: %s", exc)
        return 1

    # Report.
    ratio = (
        manifest.total_compressed / manifest.total_original * 100
        if manifest.total_original > 0
        else 100.0
    )
    print(f"\nBundle written: {output_path}")
    print(f"  Entries:        {len(manifest.entries)}")
    print(f"  Original size:  {manifest.total_original:,} bytes")
    print(f"  Compressed:     {manifest.total_compressed:,} bytes ({ratio:.1f}%)")
    print(f"  Bundle file:    {output_path.stat().st_size:,} bytes")
    return 0


def _cmd_info(args) -> int:
    """Handle the ``info`` subcommand."""
    try:
        with BundleReader(args.bundle_file) as reader:
            m = reader.manifest
            print(f"\nBundle: {args.bundle_file}")
            print(f"Version: {m.version}")
            print(f"Entries: {len(m.entries)}")
            if m.total_original > 0:
                ratio = m.total_compressed / m.total_original * 100
                print(
                    f"Total:   {m.total_original:,} bytes "
                    f"-> {m.total_compressed:,} bytes ({ratio:.1f}%)"
                )
            print()
            print(f"{'Path':<40} {'Original':>10} {'Compressed':>12} {'Ratio':>7}")
            print("-" * 73)
            for e in m.entries:
                r = e.size / e.original_size * 100 if e.original_size > 0 else 100.0
                print(f"{e.path:<40} {e.original_size:>10,} {e.size:>12,} {r:>6.1f}%")
                if e.dependencies:
                    for dep in e.dependencies:
                        print(f"  -> {dep}")
    except (BundleError, BundleFormatError) as exc:
        log.error("%s", exc)
        return 1
    return 0


def _build_frontend(web_dir: Path) -> bool:
    """Run ``npm run build`` in the web directory. Returns True on success."""
    import subprocess

    # npm is a .cmd on Windows — needs shell=True there, not on Unix
    _shell = sys.platform == "win32"

    if not (web_dir / "package.json").is_file():
        log.error("No package.json found at %s", web_dir)
        return False

    # Ensure node_modules exist
    if not (web_dir / "node_modules").is_dir():
        print("Installing frontend dependencies...")
        result = subprocess.run(
            ["npm", "install"],
            cwd=str(web_dir),
            shell=_shell,
        )
        if result.returncode != 0:
            log.error("npm install failed")
            return False

    print("Building frontend...")
    result = subprocess.run(
        ["npm", "run", "build"],
        cwd=str(web_dir),
        shell=_shell,
    )
    if result.returncode != 0:
        log.error("Frontend build failed")
        return False

    print("Frontend build complete.")
    return True


def _frontend_is_stale(web_dir: Path, dist_dir: Path) -> bool:
    """Check if any source file is newer than the dist/ build output."""
    if not dist_dir.is_dir():
        return True

    # Find the newest file in dist/
    dist_mtime = 0.0
    for f in dist_dir.rglob("*"):
        if f.is_file():
            dist_mtime = max(dist_mtime, f.stat().st_mtime)

    if dist_mtime == 0.0:
        return True

    # Check top-level config files that affect the build
    _BUILD_CONFIG_SUFFIXES = {".json", ".ts", ".js", ".mjs", ".cjs"}
    for f in web_dir.iterdir():
        if (
            f.is_file()
            and f.suffix in _BUILD_CONFIG_SUFFIXES
            and f.stat().st_mtime > dist_mtime
        ):
            return True

    # Check source files
    src_dir = web_dir / "src"
    if src_dir.is_dir():
        for f in src_dir.rglob("*"):
            if f.is_file() and f.stat().st_mtime > dist_mtime:
                return True

    return False


def _cmd_serve(args, config) -> int:
    """Handle the ``serve`` subcommand."""
    try:
        import uvicorn
    except ModuleNotFoundError:
        log.error("uvicorn is required for the web UI: pip install uvicorn")
        return 1

    web_dir = Path(__file__).resolve().parent / "web"
    dist_dir = web_dir / "dist"

    # Build frontend if requested or if dist/ is stale
    if args.build:
        if not _build_frontend(web_dir):
            return 1
    elif not args.no_auto_build and _frontend_is_stale(web_dir, dist_dir):
        print("Frontend build is stale or missing — rebuilding...")
        if not _build_frontend(web_dir):
            print("Frontend build failed — starting in API-only mode.")

    from pipeline.server import create_app

    app = create_app(config)
    url = f"http://{args.host}:{args.port}"
    print(f"\nForge pipeline web UI: {url}")
    print(f"  API docs:  {url}/api/docs")
    print("  Press Ctrl+C to stop.\n")
    uvicorn.run(app, host=args.host, port=args.port, log_level="info")
    return 0


def main(argv: list[str] | None = None) -> int:
    """Entry point.  Returns 0 on success, 1 on error."""
    args = build_parser().parse_args(argv)

    # -- Logging ------------------------------------------------------------
    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(name)s: %(message)s",
    )

    # -- Handle info subcommand (no config/plugins needed) ------------------
    if args.command == "info":
        return _cmd_info(args)

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

    # -- Handle bundle subcommand (config needed, plugins not needed) -------
    if args.command == "bundle":
        return _cmd_bundle(args, config)

    # -- Handle serve subcommand (config needed, plugins not needed) -------
    if args.command == "serve":
        return _cmd_serve(args, config)

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

    # Exclude the output directory from scanning — it may be inside source_dir.
    files = scan(
        config.source_dir,
        supported,
        cache,
        exclude_dirs=[config.output_dir.resolve()],
    )
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
            plugins = registry.get_by_extension(f.extension)
            plugin_names = ", ".join(p.name for p in plugins) if plugins else "?"
            print(f"  {label}  {f.relative}  ({plugin_names})")

    # -- Processing ---------------------------------------------------------
    if args.dry_run:
        if new_count + changed_count > 0:
            print(f"\n{new_count + changed_count} file(s) would be processed.")
        else:
            print("\nAll files up to date — nothing to process.")
    elif new_count + changed_count > 0:
        print(f"\nProcessing {new_count + changed_count} file(s)...")
        success, errors, succeeded = _process_files(
            files,
            registry,
            config,
            cache,
            plugin_filter=args.plugin,
        )

        # Update the fingerprint cache only for (file, plugin) pairs that
        # were processed successfully.  The cache key combines the source
        # content hash with a digest of the plugin settings so that config
        # changes (e.g. max_size, output_format) trigger reprocessing.
        for f in files:
            for plugin in registry.get_by_extension(f.extension):
                cache_key = f"{f.relative}:{plugin.name}"
                if cache_key in succeeded:
                    settings = config.plugin_settings.get(plugin.name, {})
                    combined = _fingerprint_with_settings(f.fingerprint, settings)
                    cache.set(Path(cache_key), combined)
        cache.save()

        print(
            f"\nDone: {success} processed, {errors} failed, "
            f"{unchanged_count} unchanged."
        )
        if errors > 0:
            return 1
    else:
        print("\nAll files up to date — nothing to process.")

    return 0


if __name__ == "__main__":
    sys.exit(main())
