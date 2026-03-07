"""Tests for pipeline.config."""

from pathlib import Path

import pytest

from pipeline.config import (
    DEFAULT_CACHE_DIR,
    DEFAULT_OUTPUT_DIR,
    DEFAULT_SOURCE_DIR,
    ConfigError,
    default_config,
    load_config,
)


def test_default_config_has_expected_paths():
    cfg = default_config()
    assert cfg.source_dir == Path(DEFAULT_SOURCE_DIR)
    assert cfg.output_dir == Path(DEFAULT_OUTPUT_DIR)
    assert cfg.cache_dir == Path(DEFAULT_CACHE_DIR)
    assert cfg.plugin_settings == {}


def test_load_config_minimal(tmp_path: Path):
    toml_file = tmp_path / "pipeline.toml"
    toml_file.write_text('[pipeline]\nsource_dir = "src"\n', encoding="utf-8")

    cfg = load_config(toml_file)
    assert cfg.source_dir == tmp_path / "src"
    assert cfg.output_dir == tmp_path / DEFAULT_OUTPUT_DIR


def test_load_config_with_plugin_section(tmp_path: Path):
    toml_file = tmp_path / "pipeline.toml"
    toml_file.write_text(
        '[pipeline]\nsource_dir = "art"\n\n[texture]\nmax_size = 2048\n',
        encoding="utf-8",
    )
    cfg = load_config(toml_file)
    assert cfg.source_dir == tmp_path / "art"
    assert "texture" in cfg.plugin_settings
    assert cfg.plugin_settings["texture"]["max_size"] == 2048


def test_load_config_missing_file(tmp_path: Path):
    with pytest.raises(ConfigError, match="not found"):
        load_config(tmp_path / "nope.toml")


def test_load_config_invalid_toml(tmp_path: Path):
    bad = tmp_path / "bad.toml"
    bad.write_text("not valid toml [[[", encoding="utf-8")
    with pytest.raises(ConfigError, match="Invalid TOML"):
        load_config(bad)


def test_load_config_empty_file(tmp_path: Path):
    """An empty TOML file is valid — all defaults apply."""
    toml_file = tmp_path / "pipeline.toml"
    toml_file.write_text("", encoding="utf-8")
    cfg = load_config(toml_file)
    assert cfg.source_dir == tmp_path / DEFAULT_SOURCE_DIR


def test_raw_dict_preserved(tmp_path: Path):
    toml_file = tmp_path / "pipeline.toml"
    toml_file.write_text('[pipeline]\nsource_dir = "x"\n', encoding="utf-8")
    cfg = load_config(toml_file)
    assert cfg.source_dir == tmp_path / "x"
    assert "pipeline" in cfg.raw


def test_load_config_directory_raises(tmp_path: Path):
    """Passing a directory instead of a file should raise ConfigError."""
    with pytest.raises(ConfigError, match="not found"):
        load_config(tmp_path)


def test_load_config_absolute_paths_preserved(tmp_path: Path):
    """Absolute paths in the config should not be rebased."""
    toml_file = tmp_path / "pipeline.toml"
    abs_dir = tmp_path / "absolute"
    # Use forward slashes for TOML string compatibility
    toml_file.write_text(
        f'[pipeline]\nsource_dir = "{abs_dir.as_posix()}"\n',
        encoding="utf-8",
    )
    cfg = load_config(toml_file)
    assert cfg.source_dir == abs_dir
