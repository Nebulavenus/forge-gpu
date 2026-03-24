"""Tests for pipeline.tool_finder — shared C tool discovery."""

from __future__ import annotations

import platform
from pathlib import Path
from unittest.mock import patch

import pytest

from pipeline.tool_finder import find_tool

# ---------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------


@pytest.fixture
def fake_build(tmp_path: Path) -> Path:
    """Create a fake build tree with tool executables."""
    suffix = ".exe" if platform.system() == "Windows" else ""

    mesh_dir = tmp_path / "build" / "tools" / "mesh"
    mesh_dir.mkdir(parents=True)
    (mesh_dir / f"forge_mesh_tool{suffix}").write_text("fake")

    basisu_dir = tmp_path / "build" / "_deps" / "basis_universal_full-src" / "bin"
    basisu_dir.mkdir(parents=True)
    (basisu_dir / f"basisu{suffix}").write_text("fake")

    return tmp_path


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------


def test_explicit_tool_path_returned_directly():
    """When tool_path is set in settings, return it without validation."""
    result = find_tool("forge_mesh_tool", {"tool_path": "/custom/path"})
    assert result == "/custom/path"


def test_explicit_custom_settings_key():
    """settings_key parameter controls which key is checked."""
    result = find_tool(
        "basisu", {"basisu_path": "/my/basisu"}, settings_key="basisu_path"
    )
    assert result == "/my/basisu"


def test_empty_tool_path_falls_through():
    """Empty tool_path in settings should not short-circuit."""
    with patch("pipeline.tool_finder.shutil.which", return_value=None):
        result = find_tool("nonexistent_tool", {"tool_path": ""})
    assert result is None


def test_build_directory_discovery(fake_build: Path):
    """Tool is found in build/tools/*/ when present."""
    with patch("pipeline.tool_finder._PROJECT_ROOT", fake_build):
        result = find_tool("forge_mesh_tool")
    assert result is not None
    assert "forge_mesh_tool" in result


def test_basisu_build_directory_discovery(fake_build: Path):
    """basisu is found in build/_deps/*/bin/ when present."""
    with patch("pipeline.tool_finder._PROJECT_ROOT", fake_build):
        result = find_tool("basisu")
    assert result is not None
    assert "basisu" in result


def test_falls_back_to_shutil_which():
    """When build dir has no match, falls back to shutil.which."""
    with (
        patch("pipeline.tool_finder._PROJECT_ROOT", Path("/nonexistent")),
        patch("pipeline.tool_finder.shutil.which", return_value="/usr/bin/mytool"),
    ):
        result = find_tool("mytool")
    assert result == "/usr/bin/mytool"


def test_underscore_hyphen_alternation():
    """Tries hyphenated name when underscored name not on PATH."""

    def mock_which(name):
        if name == "forge-mesh-tool":
            return "/usr/bin/forge-mesh-tool"
        return None

    with (
        patch("pipeline.tool_finder._PROJECT_ROOT", Path("/nonexistent")),
        patch("pipeline.tool_finder.shutil.which", side_effect=mock_which),
    ):
        result = find_tool("forge_mesh_tool")
    assert result == "/usr/bin/forge-mesh-tool"


def test_returns_none_when_not_found():
    """Returns None when tool is not found anywhere."""
    with (
        patch("pipeline.tool_finder._PROJECT_ROOT", Path("/nonexistent")),
        patch("pipeline.tool_finder.shutil.which", return_value=None),
    ):
        result = find_tool("totally_missing_tool")
    assert result is None


def test_settings_override_takes_priority(fake_build: Path):
    """Explicit tool_path takes priority over build directory search."""
    with patch("pipeline.tool_finder._PROJECT_ROOT", fake_build):
        result = find_tool("forge_mesh_tool", {"tool_path": "/override/path"})
    assert result == "/override/path"
