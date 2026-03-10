"""Plugin discovery and registration.

The pipeline uses a simple plugin system: each asset type (texture, mesh,
scene, etc.) is handled by a plugin class that inherits from ``AssetPlugin``.
Plugins declare which file extensions they handle and implement a ``process``
method that transforms a source file into a processed output.

Discovery works by scanning a *plugins directory* for Python modules and
importing them.  Any ``AssetPlugin`` subclass found is registered
automatically.  This means adding a new asset type is as simple as dropping
a new ``.py`` file into the plugins folder — no core code changes required.

Why a class-based registry instead of entry points?
  Entry points are the standard mechanism for installed packages, but they
  require ``pip install`` to update.  For a learning project where files
  change constantly, file-based discovery with importlib is simpler to
  understand and faster to iterate on.
"""

from __future__ import annotations

import importlib
import importlib.util
import logging
from dataclasses import dataclass, field
from pathlib import Path

log = logging.getLogger(__name__)


# ---------------------------------------------------------------------------
# Result type
# ---------------------------------------------------------------------------


@dataclass
class AssetResult:
    """Outcome of processing a single asset file.

    Plugins return this from ``process()`` so the pipeline can track what was
    produced.
    """

    source: Path
    output: Path
    metadata: dict = field(default_factory=dict)


# ---------------------------------------------------------------------------
# Base class
# ---------------------------------------------------------------------------


class AssetPlugin:
    """Base class that every asset plugin must inherit from.

    Subclasses set ``name`` and ``extensions`` as class attributes, then
    override ``process`` to perform the actual work.
    """

    # Human-readable name shown in logs and CLI output.
    name: str = ""

    # File extensions this plugin handles, including the leading dot.
    # Example: [".png", ".jpg", ".tga"]
    extensions: list[str] = []

    def process(self, source: Path, output_dir: Path, settings: dict) -> AssetResult:
        """Transform *source* into a processed asset under *output_dir*.

        *settings* comes from the ``[plugin-name]`` section of
        ``pipeline.toml``.  The base implementation raises
        ``NotImplementedError`` — subclasses must override this.
        """
        raise NotImplementedError(f"Plugin {self.name!r} has not implemented process()")


# ---------------------------------------------------------------------------
# Registry
# ---------------------------------------------------------------------------


class PluginRegistry:
    """Discovers, stores, and looks up ``AssetPlugin`` instances."""

    def __init__(self) -> None:
        self._plugins: dict[str, AssetPlugin] = {}
        self._ext_map: dict[str, list[AssetPlugin]] = {}

    # -- Registration -------------------------------------------------------

    def register(self, plugin: AssetPlugin) -> None:
        """Add *plugin* to the registry.

        Raises ``ValueError`` if a plugin with the same name is already
        registered.  Multiple plugins may handle the same extension —
        all of them will be invoked when a file with that extension is
        processed.
        """
        if not plugin.name:
            raise ValueError("Plugin must have a non-empty 'name' attribute")

        if plugin.name in self._plugins:
            raise ValueError(f"Duplicate plugin name: {plugin.name!r}")

        # Commit after name validation passes.
        self._plugins[plugin.name] = plugin
        for ext in plugin.extensions:
            self._ext_map.setdefault(ext.lower(), []).append(plugin)

        log.debug(
            "Registered plugin %r (extensions: %s)", plugin.name, plugin.extensions
        )

    # -- Lookup -------------------------------------------------------------

    def get_by_name(self, name: str) -> AssetPlugin | None:
        """Return the plugin with *name*, or ``None``."""
        return self._plugins.get(name)

    def get_by_extension(self, ext: str) -> list[AssetPlugin]:
        """Return all plugins that handle *ext* (e.g. ``".png"``).

        Returns an empty list if no plugin handles the extension.
        Multiple plugins may share an extension — for example, a mesh
        plugin and an animation plugin both handle ``.gltf``.
        """
        return list(self._ext_map.get(ext.lower(), []))

    @property
    def plugins(self) -> list[AssetPlugin]:
        """All registered plugins, in registration order."""
        return list(self._plugins.values())

    @property
    def supported_extensions(self) -> set[str]:
        """All file extensions that have a registered handler."""
        return set(self._ext_map.keys())

    # -- Discovery ----------------------------------------------------------

    def discover(self, plugins_dir: Path) -> int:
        """Import every ``.py`` file in *plugins_dir* and register any
        ``AssetPlugin`` subclasses found.

        Returns the number of newly registered plugins.
        """
        if not plugins_dir.is_dir():
            log.warning("Plugins directory does not exist: %s", plugins_dir)
            return 0

        count_before = len(self._plugins)

        for py_file in sorted(plugins_dir.glob("*.py")):
            if py_file.name.startswith("_"):
                continue
            self._import_and_register(py_file)

        registered = len(self._plugins) - count_before
        log.info(
            "Discovered %d plugin(s) from %s",
            registered,
            plugins_dir,
        )
        return registered

    def _import_and_register(self, py_file: Path) -> None:
        """Import *py_file* as a module and register any ``AssetPlugin``
        subclasses it defines.
        """
        module_name = f"pipeline.plugins.{py_file.stem}"
        spec = importlib.util.spec_from_file_location(module_name, py_file)
        if spec is None or spec.loader is None:
            log.warning("Could not create import spec for %s", py_file)
            return

        module = importlib.util.module_from_spec(spec)
        try:
            spec.loader.exec_module(module)
        except Exception:
            log.exception("Failed to import plugin %s", py_file)
            return

        # Find AssetPlugin subclasses defined in this module (not imported ones).
        for attr_name, attr in vars(module).items():
            if (
                isinstance(attr, type)
                and issubclass(attr, AssetPlugin)
                and attr is not AssetPlugin
                and attr.__module__ == module.__name__
                and attr.name  # skip abstract subclasses without a name
            ):
                try:
                    self.register(attr())
                except ValueError:
                    log.exception("Failed to register %s from %s", attr_name, py_file)
