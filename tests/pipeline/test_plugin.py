"""Tests for pipeline.plugin."""

from pathlib import Path

import pytest

from pipeline.plugin import AssetPlugin, PluginRegistry

# -- Fixtures ---------------------------------------------------------------


class TexturePlugin(AssetPlugin):
    name = "texture"
    extensions = [".png", ".jpg", ".tga"]


class MeshPlugin(AssetPlugin):
    name = "mesh"
    extensions = [".obj", ".gltf"]


class NoNamePlugin(AssetPlugin):
    extensions = [".xyz"]


# -- Registration tests -----------------------------------------------------


def test_register_and_lookup():
    reg = PluginRegistry()
    reg.register(TexturePlugin())

    assert reg.get_by_name("texture") is not None
    assert len(reg.get_by_extension(".png")) == 1
    assert reg.get_by_extension(".png")[0].name == "texture"
    assert len(reg.get_by_extension(".PNG")) == 1  # case-insensitive


def test_duplicate_name_raises():
    reg = PluginRegistry()
    reg.register(TexturePlugin())
    with pytest.raises(ValueError, match="Duplicate plugin name"):
        reg.register(TexturePlugin())


def test_multiple_plugins_per_extension():
    """Two plugins can handle the same extension without conflict."""
    reg = PluginRegistry()
    reg.register(MeshPlugin())  # handles .gltf

    class AnimPlugin(AssetPlugin):
        name = "animation"
        extensions = [".gltf", ".glb"]

    reg.register(AnimPlugin())

    plugins = reg.get_by_extension(".gltf")
    assert len(plugins) == 2
    names = {p.name for p in plugins}
    assert names == {"mesh", "animation"}


def test_no_name_raises():
    reg = PluginRegistry()
    with pytest.raises(ValueError, match="non-empty"):
        reg.register(NoNamePlugin())


def test_plugins_list():
    reg = PluginRegistry()
    reg.register(TexturePlugin())
    reg.register(MeshPlugin())
    assert len(reg.plugins) == 2
    names = {p.name for p in reg.plugins}
    assert names == {"texture", "mesh"}


def test_supported_extensions():
    reg = PluginRegistry()
    reg.register(TexturePlugin())
    assert ".png" in reg.supported_extensions
    assert ".jpg" in reg.supported_extensions
    assert ".obj" not in reg.supported_extensions


def test_get_by_name_miss():
    reg = PluginRegistry()
    assert reg.get_by_name("nope") is None


def test_get_by_extension_miss():
    reg = PluginRegistry()
    assert reg.get_by_extension(".xyz") == []


def test_get_by_extension_returns_list():
    """get_by_extension always returns a list, even for a single plugin."""
    reg = PluginRegistry()
    reg.register(TexturePlugin())

    result = reg.get_by_extension(".png")
    assert isinstance(result, list)
    assert len(result) == 1

    result_miss = reg.get_by_extension(".xyz")
    assert isinstance(result_miss, list)
    assert len(result_miss) == 0


def test_get_by_extension_returns_copy():
    """Mutating the returned list must not affect the registry."""
    reg = PluginRegistry()
    reg.register(TexturePlugin())

    result = reg.get_by_extension(".png")
    result.clear()

    # Internal state should be unaffected.
    assert len(reg.get_by_extension(".png")) == 1


# -- Discovery tests --------------------------------------------------------


def test_discover_from_directory(tmp_path: Path):
    """Write a plugin file to disk and verify it gets discovered."""
    plugins_dir = tmp_path / "plugins"
    plugins_dir.mkdir()
    (plugins_dir / "demo.py").write_text(
        """\
from pipeline.plugin import AssetPlugin

class DemoPlugin(AssetPlugin):
    name = "demo"
    extensions = [".demo"]
""",
        encoding="utf-8",
    )

    reg = PluginRegistry()
    count = reg.discover(plugins_dir)

    assert count == 1
    assert reg.get_by_name("demo") is not None
    assert len(reg.get_by_extension(".demo")) == 1


def test_discover_skips_underscored_files(tmp_path: Path):
    plugins_dir = tmp_path / "plugins"
    plugins_dir.mkdir()
    (plugins_dir / "_internal.py").write_text(
        "from pipeline.plugin import AssetPlugin\n"
        "class Hidden(AssetPlugin):\n"
        "    name = 'hidden'\n"
        "    extensions = ['.hid']\n",
        encoding="utf-8",
    )

    reg = PluginRegistry()
    assert reg.discover(plugins_dir) == 0


def test_discover_missing_directory(tmp_path: Path):
    reg = PluginRegistry()
    assert reg.discover(tmp_path / "nonexistent") == 0
