"""Tests for the scene hierarchy extraction plugin."""

from __future__ import annotations

import json
import subprocess
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from pipeline.plugins.scene import ScenePlugin

# -- Helpers ----------------------------------------------------------------


def _make_gltf(path: Path) -> Path:
    """Create a minimal glTF file at *path*."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('{"asset": {"version": "2.0"}}', encoding="utf-8")
    return path


def _fake_meta(output_dir: Path, stem: str) -> None:
    """Create a fake .meta.json and .fscene so plugin can read results."""
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / f"{stem}.fscene").write_bytes(b"FSCN" + b"\x00" * 96)
    meta = {
        "source": f"{stem}.gltf",
        "format_version": 1,
        "node_count": 3,
        "mesh_count": 1,
        "root_count": 1,
        "mesh_node_count": 2,
        "total_children": 2,
        "meshes": [
            {
                "name": "Wheel",
                "first_submesh": 0,
                "submesh_count": 1,
            }
        ],
        "nodes": [
            {"name": "Root", "parent": -1, "mesh_index": -1, "child_count": 1},
            {"name": "Body", "parent": 0, "mesh_index": 0, "child_count": 1},
            {"name": "Wheel", "parent": 1, "mesh_index": 0, "child_count": 0},
        ],
    }
    (output_dir / f"{stem}.meta.json").write_text(json.dumps(meta), encoding="utf-8")


def _mock_run_success(output_dir: Path, stem: str):
    """Return a side_effect for subprocess.run that creates output files."""

    def side_effect(*args, **kwargs):
        _fake_meta(output_dir, stem)
        return MagicMock(returncode=0, stderr="", stdout="extracted 3 node(s)")

    return side_effect


# -- Fixtures ---------------------------------------------------------------


@pytest.fixture
def plugin():
    return ScenePlugin()


@pytest.fixture
def source_gltf(tmp_path: Path) -> Path:
    return _make_gltf(tmp_path / "src" / "model.gltf")


# -- 1. Plugin registration ------------------------------------------------


def test_scene_plugin_registered(plugin):
    """ScenePlugin has name='scene' and correct extensions."""
    assert plugin.name == "scene"
    assert isinstance(plugin.extensions, list)
    assert len(plugin.extensions) >= 1


# -- 2. Extensions ----------------------------------------------------------


def test_scene_extensions(plugin):
    """.gltf and .glb are handled."""
    assert ".gltf" in plugin.extensions
    assert ".glb" in plugin.extensions


# -- 3. Default tool invocation ---------------------------------------------


@patch("pipeline.plugins.scene.subprocess.run")
@patch(
    "pipeline.plugins.scene._find_scene_tool",
    return_value="/usr/bin/forge-scene",
)
def test_scene_tool_invocation_default(
    mock_tool, mock_run, plugin, source_gltf, tmp_path
):
    """With default settings, verify CLI args include tool, source, output,
    and --verbose."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    plugin.process(source_gltf, output_dir, {})

    mock_run.assert_called_once()
    args = mock_run.call_args[0][0]
    assert args[0] == "/usr/bin/forge-scene"
    assert str(source_gltf) in args
    assert "--verbose" in args


# -- 4. Custom tool path ---------------------------------------------------


@patch("pipeline.plugins.scene.subprocess.run")
def test_scene_tool_custom_path(mock_run, plugin, source_gltf, tmp_path):
    """tool_path='/custom/path' uses that instead of _find_scene_tool()."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    plugin.process(source_gltf, output_dir, {"tool_path": "/custom/path"})

    args = mock_run.call_args[0][0]
    assert args[0] == "/custom/path"


# -- 5. Tool not installed warns -------------------------------------------


@patch("pipeline.plugins.scene._find_scene_tool", return_value=None)
def test_scene_tool_not_installed_warns(
    mock_tool, plugin, source_gltf, tmp_path, caplog
):
    """When tool not found, logs WARNING and returns source path."""
    output_dir = tmp_path / "out"

    with caplog.at_level("WARNING"):
        result = plugin.process(source_gltf, output_dir, {})

    assert "not installed" in caplog.text.lower() or "not found" in caplog.text.lower()
    assert result.output == source_gltf
    assert result.metadata["processed"] is False


# -- 6. Tool failure raises -----------------------------------------------


@patch("pipeline.plugins.scene.subprocess.run")
@patch(
    "pipeline.plugins.scene._find_scene_tool",
    return_value="/usr/bin/forge-scene",
)
def test_scene_tool_failure_raises(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """subprocess returncode != 0 raises RuntimeError."""
    mock_run.return_value = MagicMock(
        returncode=1, stderr="scene extraction failed", stdout=""
    )
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError):
        plugin.process(source_gltf, output_dir, {})


# -- 7. Timeout raises RuntimeError ----------------------------------------


@patch("pipeline.plugins.scene.subprocess.run")
@patch(
    "pipeline.plugins.scene._find_scene_tool",
    return_value="/usr/bin/forge-scene",
)
def test_scene_tool_timeout_raises(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """TimeoutExpired from subprocess raises RuntimeError."""
    mock_run.side_effect = subprocess.TimeoutExpired(cmd="forge-scene", timeout=600)
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError):
        plugin.process(source_gltf, output_dir, {})


# -- 8. OSError on launch raises RuntimeError --------------------------------


@patch("pipeline.plugins.scene.subprocess.run")
@patch(
    "pipeline.plugins.scene._find_scene_tool",
    return_value="/bad/path",
)
def test_scene_tool_oserror_raises(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """OSError from subprocess raises RuntimeError."""
    mock_run.side_effect = OSError("No such file or directory")
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError, match="could not be launched"):
        plugin.process(source_gltf, output_dir, {})


# -- 9. Missing output file raises RuntimeError -----------------------------


@patch("pipeline.plugins.scene.subprocess.run")
@patch(
    "pipeline.plugins.scene._find_scene_tool",
    return_value="/usr/bin/forge-scene",
)
def test_scene_tool_missing_output_raises(
    mock_tool, mock_run, plugin, source_gltf, tmp_path
):
    """Tool exits 0 but no .fscene created -> RuntimeError."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError, match="did not create"):
        plugin.process(source_gltf, output_dir, {})


# -- 10. Metadata recorded -------------------------------------------------


@patch("pipeline.plugins.scene.subprocess.run")
@patch(
    "pipeline.plugins.scene._find_scene_tool",
    return_value="/usr/bin/forge-scene",
)
def test_scene_metadata_recorded(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """Metadata from .meta.json is included in AssetResult."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    result = plugin.process(source_gltf, output_dir, {})

    assert result.metadata["node_count"] == 3
    assert result.metadata["mesh_count"] == 1
    assert result.metadata["root_count"] == 1
    assert result.metadata["nodes"][0]["name"] == "Root"


# -- 11. Output extension --------------------------------------------------


@patch("pipeline.plugins.scene.subprocess.run")
@patch(
    "pipeline.plugins.scene._find_scene_tool",
    return_value="/usr/bin/forge-scene",
)
def test_scene_output_extension(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """Output has .fscene extension."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    result = plugin.process(source_gltf, output_dir, {})

    assert result.output.suffix == ".fscene"


# -- 12. Creates output dir ------------------------------------------------


@patch("pipeline.plugins.scene.subprocess.run")
@patch(
    "pipeline.plugins.scene._find_scene_tool",
    return_value="/usr/bin/forge-scene",
)
def test_scene_creates_output_dir(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """output_dir created if missing."""
    output_dir = tmp_path / "deep" / "nested" / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    result = plugin.process(source_gltf, output_dir, {})

    assert output_dir.is_dir()
    assert result.output.exists()


# -- 13. Timeout parameter -------------------------------------------------


@patch("pipeline.plugins.scene.subprocess.run")
@patch(
    "pipeline.plugins.scene._find_scene_tool",
    return_value="/usr/bin/forge-scene",
)
def test_scene_tool_timeout_value(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """subprocess.run called with timeout=600."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    plugin.process(source_gltf, output_dir, {})

    kwargs = mock_run.call_args[1]
    assert kwargs.get("timeout") == 600


# -- 14. Verbose flag always passed -----------------------------------------


@patch("pipeline.plugins.scene.subprocess.run")
@patch(
    "pipeline.plugins.scene._find_scene_tool",
    return_value="/usr/bin/forge-scene",
)
def test_scene_verbose_flag(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """--verbose is always passed."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    plugin.process(source_gltf, output_dir, {})

    args = mock_run.call_args[0][0]
    assert "--verbose" in args


# -- 15. Shared extension with mesh and animation plugins -------------------


def test_shared_extension_with_mesh_and_animation():
    """ScenePlugin coexists with MeshPlugin and AnimationPlugin on .gltf/.glb."""
    from pipeline.plugin import PluginRegistry
    from pipeline.plugins.animation import AnimationPlugin
    from pipeline.plugins.mesh import MeshPlugin

    reg = PluginRegistry()
    reg.register(MeshPlugin())
    reg.register(AnimationPlugin())
    reg.register(ScenePlugin())

    gltf_plugins = reg.get_by_extension(".gltf")
    assert len(gltf_plugins) == 3
    names = {p.name for p in gltf_plugins}
    assert "mesh" in names
    assert "animation" in names
    assert "scene" in names

    glb_plugins = reg.get_by_extension(".glb")
    assert len(glb_plugins) == 3


# -- 16. No processing when tool missing -----------------------------------


@patch("pipeline.plugins.scene._find_scene_tool", return_value=None)
def test_scene_no_processing_when_tool_missing(
    mock_tool, plugin, source_gltf, tmp_path
):
    """Metadata shows no processing happened."""
    output_dir = tmp_path / "out"
    result = plugin.process(source_gltf, output_dir, {})

    assert result.metadata.get("processed") is False
    assert result.metadata.get("reason") == "tool_not_found"


# -- 17. Stale output removed before tool invocation -----------------------


@patch("pipeline.plugins.scene.subprocess.run")
@patch(
    "pipeline.plugins.scene._find_scene_tool",
    return_value="/usr/bin/forge-scene",
)
def test_scene_stale_output_removed(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """Pre-existing .fscene and .meta.json are removed before the tool runs."""
    output_dir = tmp_path / "out"
    output_dir.mkdir(parents=True, exist_ok=True)

    stale_fscene = output_dir / "model.fscene"
    stale_meta = output_dir / "model.meta.json"
    stale_fscene.write_bytes(b"OLD_DATA")
    stale_meta.write_text("{}", encoding="utf-8")

    # Tool fails — so the stale files should have been deleted before the call
    mock_run.return_value = MagicMock(returncode=1, stderr="fail", stdout="")

    with pytest.raises(RuntimeError):
        plugin.process(source_gltf, output_dir, {})

    # Stale outputs must be gone regardless of tool success/failure
    assert not stale_fscene.exists()
    assert not stale_meta.exists()
