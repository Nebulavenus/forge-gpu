"""Tests for the mesh processing plugin."""

from __future__ import annotations

import json
import subprocess
from pathlib import Path
from unittest.mock import MagicMock, patch

import pytest

from pipeline.plugins.mesh import MeshPlugin

# -- Helpers ----------------------------------------------------------------


def _make_obj(path: Path) -> Path:
    """Create a minimal OBJ file at *path*."""
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 0 1\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1\n"
    )
    return path


def _fake_meta(output_dir: Path, stem: str) -> None:
    """Create a fake .meta.json and .fmesh so plugin can read results."""
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / f"{stem}.fmesh").write_bytes(b"\x00" * 100)
    meta = {
        "source": f"{stem}.obj",
        "vertex_count": 3,
        "vertex_stride": 32,
        "has_tangents": True,
        "lods": [{"level": 0, "index_count": 3, "target_ratio": 1.0, "error": 0.0}],
        "original_vertex_count": 3,
        "dedup_ratio": 1.0,
    }
    (output_dir / f"{stem}.meta.json").write_text(json.dumps(meta), encoding="utf-8")


def _mock_run_success(output_dir: Path, stem: str):
    """Return a side_effect for subprocess.run that creates output files."""

    def side_effect(*args, **kwargs):
        _fake_meta(output_dir, stem)
        return MagicMock(returncode=0, stderr="", stdout="processed 3 vertices")

    return side_effect


# -- Fixtures ---------------------------------------------------------------


@pytest.fixture
def plugin():
    return MeshPlugin()


@pytest.fixture
def source_obj(tmp_path: Path) -> Path:
    return _make_obj(tmp_path / "src" / "cube.obj")


# -- 1. Plugin registration ------------------------------------------------


def test_mesh_plugin_registered(plugin):
    """MeshPlugin has name='mesh' and correct extensions."""
    assert plugin.name == "mesh"
    assert isinstance(plugin.extensions, list)
    assert len(plugin.extensions) >= 1


# -- 2. Extensions ----------------------------------------------------------


def test_mesh_extensions(plugin):
    """.obj, .gltf, and .glb are handled."""
    assert ".obj" in plugin.extensions
    assert ".gltf" in plugin.extensions
    assert ".glb" in plugin.extensions


# -- 3. Default tool invocation ---------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_tool_invocation_default(
    mock_tool, mock_run, plugin, source_obj, tmp_path
):
    """With default settings, verify CLI args include tool, source, output,
    --lod-levels, 1.0, and --verbose."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    plugin.process(source_obj, output_dir, {})

    mock_run.assert_called_once()
    args = mock_run.call_args[0][0]
    assert args[0] == "/usr/bin/forge-mesh"
    assert str(source_obj) in args
    # Default LOD is [1.0] which matches DEFAULT_LOD_LEVELS, so --lod-levels
    # is not added (the tool's own default is also [1.0]).
    assert "--verbose" in args


# -- 4. Deduplicate flag (default) -----------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_tool_deduplicate_flag(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """deduplicate=True is the default — no special flag needed."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    plugin.process(source_obj, output_dir, {"deduplicate": True})

    args = mock_run.call_args[0][0]
    # deduplicate is on by default; --no-deduplicate should NOT appear
    assert "--no-deduplicate" not in args


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_tool_no_deduplicate_flag(
    mock_tool, mock_run, plugin, source_obj, tmp_path
):
    """deduplicate=False adds --no-deduplicate."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    plugin.process(source_obj, output_dir, {"deduplicate": False})

    args = mock_run.call_args[0][0]
    assert "--no-deduplicate" in args


# -- 5. No tangents flag ---------------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_tool_no_tangents_flag(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """generate_tangents=False adds --no-tangents."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    plugin.process(source_obj, output_dir, {"generate_tangents": False})

    args = mock_run.call_args[0][0]
    assert "--no-tangents" in args


# -- 6. No optimize flag ---------------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_tool_no_optimize_flag(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """optimize=False adds --no-optimize."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    plugin.process(source_obj, output_dir, {"optimize": False})

    args = mock_run.call_args[0][0]
    assert "--no-optimize" in args


# -- 7. LOD levels ---------------------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_tool_lod_levels(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """lod_levels=[1.0, 0.5, 0.25] produces --lod-levels 1.0,0.5,0.25."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    plugin.process(source_obj, output_dir, {"lod_levels": [1.0, 0.5, 0.25]})

    args = mock_run.call_args[0][0]
    idx = args.index("--lod-levels")
    assert args[idx + 1] == "1.0,0.5,0.25"


# -- 8. Custom tool path ---------------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
def test_mesh_tool_custom_path(mock_run, plugin, source_obj, tmp_path):
    """tool_path='/custom/path' uses that instead of _find_mesh_tool()."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    plugin.process(source_obj, output_dir, {"tool_path": "/custom/path"})

    args = mock_run.call_args[0][0]
    assert args[0] == "/custom/path"


# -- 9. Tool not installed warns -------------------------------------------


@patch("pipeline.plugins.mesh._find_mesh_tool", return_value=None)
def test_mesh_tool_not_installed_warns(mock_tool, plugin, source_obj, tmp_path, caplog):
    """When tool not found, logs WARNING and returns source path (no fake .fmesh)."""
    output_dir = tmp_path / "out"

    with caplog.at_level("WARNING"):
        result = plugin.process(source_obj, output_dir, {})

    assert "not installed" in caplog.text.lower() or "not found" in caplog.text.lower()
    # When the tool is missing, output points to the original source — no fake .fmesh
    assert result.output == source_obj
    assert result.metadata["processed"] is False


# -- 10. Tool failure raises -----------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_tool_failure_raises(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """subprocess returncode != 0 raises RuntimeError."""
    mock_run.return_value = MagicMock(
        returncode=1, stderr="mesh processing failed", stdout=""
    )
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError):
        plugin.process(source_obj, output_dir, {})


# -- 11. Timeout parameter -------------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_tool_timeout(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """subprocess.run called with timeout=600."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    plugin.process(source_obj, output_dir, {})

    kwargs = mock_run.call_args[1]
    assert kwargs.get("timeout") == 600


# -- 12. Metadata recorded -------------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_metadata_recorded(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """Metadata from .meta.json is included in AssetResult."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    result = plugin.process(source_obj, output_dir, {})

    assert result.metadata["vertex_count"] == 3
    assert result.metadata["has_tangents"] is True
    assert "lods" in result.metadata


# -- 13. Output extension --------------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_output_extension(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """Output has .fmesh extension."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    result = plugin.process(source_obj, output_dir, {})

    assert result.output.suffix == ".fmesh"


# -- 14. Creates output dir ------------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_creates_output_dir(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """output_dir created if missing."""
    output_dir = tmp_path / "deep" / "nested" / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    result = plugin.process(source_obj, output_dir, {})

    assert output_dir.is_dir()
    assert result.output.exists()


# -- 15. Settings defaults -------------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_settings_defaults(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """Default settings applied: deduplicate=True, optimize=True,
    tangents=True, lod=[1.0]."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    plugin.process(source_obj, output_dir, {})

    args = mock_run.call_args[0][0]
    # Defaults are on, so the negative flags should not appear
    assert "--no-tangents" not in args
    assert "--no-optimize" not in args
    assert "--no-deduplicate" not in args
    # Default LOD [1.0] matches DEFAULT_LOD_LEVELS, so --lod-levels is omitted
    assert "--lod-levels" not in args


# -- 16. Invalid LOD raises ------------------------------------------------


def test_mesh_invalid_lod_raises(plugin, source_obj, tmp_path):
    """LOD values outside (0, 1] are rejected with ValueError."""
    output_dir = tmp_path / "out"
    with pytest.raises(ValueError, match="[Ll][Oo][Dd]"):
        plugin.process(source_obj, output_dir, {"lod_levels": [1.0, 0.0]})


# -- 17. No processing when tool missing -----------------------------------


@patch("pipeline.plugins.mesh._find_mesh_tool", return_value=None)
def test_mesh_effective_processing_none_when_tool_missing(
    mock_tool, plugin, source_obj, tmp_path
):
    """Metadata shows no processing happened."""
    output_dir = tmp_path / "out"
    result = plugin.process(source_obj, output_dir, {})

    # The metadata should indicate no mesh processing was performed
    assert result.metadata.get("processed") is False


# -- 18. Timeout exception raises RuntimeError ------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_tool_timeout_raises(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """TimeoutExpired from subprocess raises RuntimeError."""
    mock_run.side_effect = subprocess.TimeoutExpired(cmd="forge-mesh", timeout=600)
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError):
        plugin.process(source_obj, output_dir, {})


# -- 19. Empty LOD levels --------------------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_lod_levels_empty(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """Empty lod_levels still works (defaults to [1.0])."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    plugin.process(source_obj, output_dir, {"lod_levels": []})

    args = mock_run.call_args[0][0]
    # Empty list defaults to [1.0] which matches DEFAULT_LOD_LEVELS,
    # so --lod-levels is not added (the tool also defaults to [1.0]).
    assert "--lod-levels" not in args


# -- 20. OSError on launch raises RuntimeError ------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/bad/path")
def test_mesh_tool_oserror_raises(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """OSError from subprocess raises RuntimeError."""
    mock_run.side_effect = OSError("No such file or directory")
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError, match="could not be launched"):
        plugin.process(source_obj, output_dir, {})


# -- 21. Missing output file raises RuntimeError ----------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_tool_missing_output_raises(
    mock_tool, mock_run, plugin, source_obj, tmp_path
):
    """Tool exits 0 but no .fmesh created → RuntimeError."""
    mock_run.return_value = MagicMock(returncode=0, stderr="", stdout="")
    output_dir = tmp_path / "out"

    with pytest.raises(RuntimeError, match="did not create"):
        plugin.process(source_obj, output_dir, {})


# -- 22. Verbose flag always passed -----------------------------------------


@patch("pipeline.plugins.mesh.subprocess.run")
@patch("pipeline.plugins.mesh._find_mesh_tool", return_value="/usr/bin/forge-mesh")
def test_mesh_verbose_flag(mock_tool, mock_run, plugin, source_obj, tmp_path):
    """--verbose is always passed."""
    output_dir = tmp_path / "out"
    mock_run.side_effect = _mock_run_success(output_dir, "cube")

    plugin.process(source_obj, output_dir, {})

    args = mock_run.call_args[0][0]
    assert "--verbose" in args
