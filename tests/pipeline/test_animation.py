"""Tests for the animation extraction plugin."""

from __future__ import annotations

import json
import subprocess
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from pipeline.plugins.animation import AnimationPlugin

# -- Helpers ----------------------------------------------------------------


def _make_gltf(path: Path) -> Path:
    """Create a minimal glTF file at *path*."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('{"asset": {"version": "2.0"}}', encoding="utf-8")
    return path


def _fake_meta(output_dir: Path, stem: str) -> None:
    """Create fake .meta.json, .fanim, and .fanims so plugin can read results."""
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / f"{stem}.fanim").write_bytes(b"FANM" + b"\x00" * 96)
    meta = {
        "source": f"{stem}.gltf",
        "clip_count": 1,
        "clips": [
            {
                "name": "Walk",
                "duration": 1.25,
                "sampler_count": 2,
                "channel_count": 2,
            }
        ],
    }
    (output_dir / f"{stem}.meta.json").write_text(json.dumps(meta), encoding="utf-8")
    # Split-mode manifest (the default) — matches what the real tool produces
    manifest = {
        "version": 1,
        "model": f"{stem}.gltf",
        "clips": {
            "Walk": {
                "file": f"{stem}.fanim",
                "duration": 1.25,
                "loop": False,
                "tags": [],
            }
        },
    }
    (output_dir / f"{stem}.fanims").write_text(json.dumps(manifest), encoding="utf-8")


def _mock_run_success(output_dir: Path, stem: str):
    """Return a side_effect for subprocess.run that creates output files."""

    def side_effect(*args, **kwargs):
        _fake_meta(output_dir, stem)
        return MagicMock(returncode=0, stderr="", stdout="extracted 1 clip")

    return side_effect


# -- Fixtures ---------------------------------------------------------------


@pytest.fixture
def plugin():
    return AnimationPlugin()


@pytest.fixture
def source_gltf(tmp_path: Path) -> Path:
    return _make_gltf(tmp_path / "src" / "model.gltf")


# -- 1. Plugin registration ------------------------------------------------


def test_animation_plugin_registered(plugin):
    """AnimationPlugin has name='animation' and correct extensions."""
    assert plugin.name == "animation"
    assert isinstance(plugin.extensions, list)
    assert len(plugin.extensions) >= 1


# -- 2. Extensions ----------------------------------------------------------


def test_animation_extensions(plugin):
    """.gltf and .glb are handled."""
    assert ".gltf" in plugin.extensions
    assert ".glb" in plugin.extensions


# -- 3. Default tool invocation ---------------------------------------------


@patch("pipeline.plugins.animation.subprocess.run")
@patch(
    "pipeline.plugins.animation._find_anim_tool",
    return_value="/usr/bin/forge-anim",
)
def test_anim_tool_invocation_default(
    mock_tool, mock_run, plugin, source_gltf, tmp_path
):
    """With default settings, verify CLI args include tool, source, output,
    and --verbose."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    plugin.process(source_gltf, output_dir, {})

    mock_run.assert_called_once()
    args = mock_run.call_args[0][0]
    assert args[0] == "/usr/bin/forge-anim"
    assert str(source_gltf) in args
    assert "--verbose" in args


# -- 4. Custom tool path ---------------------------------------------------


@patch("pipeline.plugins.animation.subprocess.run")
def test_anim_tool_custom_path(mock_run, plugin, source_gltf, tmp_path):
    """tool_path='/custom/path' uses that instead of _find_anim_tool()."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    plugin.process(source_gltf, output_dir, {"tool_path": "/custom/path"})

    args = mock_run.call_args[0][0]
    assert args[0] == "/custom/path"


# -- 5. Tool not installed warns -------------------------------------------


@patch("pipeline.plugins.animation._find_anim_tool", return_value=None)
def test_anim_tool_not_installed_warns(
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


@patch("pipeline.plugins.animation.subprocess.run")
@patch(
    "pipeline.plugins.animation._find_anim_tool",
    return_value="/usr/bin/forge-anim",
)
def test_anim_tool_failure_raises(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """subprocess returncode != 0 raises RuntimeError."""
    mock_run.return_value = MagicMock(
        returncode=1, stderr="animation extraction failed", stdout=""
    )
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError):
        plugin.process(source_gltf, output_dir, {})


# -- 7. Timeout raises RuntimeError ----------------------------------------


@patch("pipeline.plugins.animation.subprocess.run")
@patch(
    "pipeline.plugins.animation._find_anim_tool",
    return_value="/usr/bin/forge-anim",
)
def test_anim_tool_timeout_raises(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """TimeoutExpired from subprocess raises RuntimeError."""
    mock_run.side_effect = subprocess.TimeoutExpired(cmd="forge-anim", timeout=600)
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError):
        plugin.process(source_gltf, output_dir, {})


# -- 8. OSError on launch raises RuntimeError --------------------------------


@patch("pipeline.plugins.animation.subprocess.run")
@patch(
    "pipeline.plugins.animation._find_anim_tool",
    return_value="/bad/path",
)
def test_anim_tool_oserror_raises(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """OSError from subprocess raises RuntimeError."""
    mock_run.side_effect = OSError("No such file or directory")
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError, match="could not be launched"):
        plugin.process(source_gltf, output_dir, {})


# -- 9. Missing output file raises RuntimeError -----------------------------


@patch("pipeline.plugins.animation.subprocess.run")
@patch(
    "pipeline.plugins.animation._find_anim_tool",
    return_value="/usr/bin/forge-anim",
)
def test_anim_tool_missing_output_raises(
    mock_tool, mock_run, plugin, source_gltf, tmp_path
):
    """Tool exits 0 but no .fanim created → RuntimeError."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError, match="did not create"):
        plugin.process(source_gltf, output_dir, {})


# -- 10. Metadata recorded -------------------------------------------------


@patch("pipeline.plugins.animation.subprocess.run")
@patch(
    "pipeline.plugins.animation._find_anim_tool",
    return_value="/usr/bin/forge-anim",
)
def test_anim_metadata_recorded(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """Metadata from .meta.json is included in AssetResult."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    result = plugin.process(source_gltf, output_dir, {})

    assert result.metadata["clip_count"] == 1
    assert result.metadata["manifest_data"]["clips"]["Walk"]["duration"] == 1.25


# -- 11. Output extension --------------------------------------------------


@patch("pipeline.plugins.animation.subprocess.run")
@patch(
    "pipeline.plugins.animation._find_anim_tool",
    return_value="/usr/bin/forge-anim",
)
def test_anim_output_extension(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """Output has .fanim extension."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    result = plugin.process(source_gltf, output_dir, {})

    assert result.output.suffix == ".fanim"


# -- 12. Creates output dir ------------------------------------------------


@patch("pipeline.plugins.animation.subprocess.run")
@patch(
    "pipeline.plugins.animation._find_anim_tool",
    return_value="/usr/bin/forge-anim",
)
def test_anim_creates_output_dir(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """output_dir created if missing."""
    output_dir = tmp_path / "deep" / "nested" / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    result = plugin.process(source_gltf, output_dir, {})

    assert output_dir.is_dir()
    assert result.output.exists()


# -- 13. Timeout parameter -------------------------------------------------


@patch("pipeline.plugins.animation.subprocess.run")
@patch(
    "pipeline.plugins.animation._find_anim_tool",
    return_value="/usr/bin/forge-anim",
)
def test_anim_tool_timeout_value(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """subprocess.run called with timeout=600."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    plugin.process(source_gltf, output_dir, {})

    kwargs = mock_run.call_args[1]
    assert kwargs.get("timeout") == 600


# -- 14. Verbose flag always passed -----------------------------------------


@patch("pipeline.plugins.animation.subprocess.run")
@patch(
    "pipeline.plugins.animation._find_anim_tool",
    return_value="/usr/bin/forge-anim",
)
def test_anim_verbose_flag(mock_tool, mock_run, plugin, source_gltf, tmp_path):
    """--verbose is always passed."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "model")

    plugin.process(source_gltf, output_dir, {})

    args = mock_run.call_args[0][0]
    assert "--verbose" in args


# -- 15. Shared extension with mesh plugin ----------------------------------


def test_shared_extension_with_mesh():
    """AnimationPlugin and MeshPlugin can coexist on .gltf/.glb."""
    from pipeline.plugin import PluginRegistry
    from pipeline.plugins.mesh import MeshPlugin

    reg = PluginRegistry()
    reg.register(MeshPlugin())
    reg.register(AnimationPlugin())

    gltf_plugins = reg.get_by_extension(".gltf")
    assert len(gltf_plugins) == 2
    names = {p.name for p in gltf_plugins}
    assert "mesh" in names
    assert "animation" in names

    glb_plugins = reg.get_by_extension(".glb")
    assert len(glb_plugins) == 2


# -- 16. No processing when tool missing -----------------------------------


@patch("pipeline.plugins.animation._find_anim_tool", return_value=None)
def test_anim_no_processing_when_tool_missing(mock_tool, plugin, source_gltf, tmp_path):
    """Metadata shows no processing happened."""
    output_dir = tmp_path / "out"
    result = plugin.process(source_gltf, output_dir, {})

    assert result.metadata.get("processed") is False
    assert result.metadata.get("reason") == "tool_not_found"
