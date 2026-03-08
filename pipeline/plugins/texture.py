"""Texture processing plugin — resize, format conversion, mipmap generation,
and optional GPU texture compression.

Processes image files (PNG, JPG, TGA, BMP) through a configurable pipeline:

1. **Load** the source image with Pillow.
2. **Resize** to fit within ``max_size`` while preserving aspect ratio.
3. **Convert** to the output format (default: PNG).
4. **Generate mipmaps** — a series of progressively halved images down to 1x1.
5. **Compress** (optional) — encode into a GPU-native block format (BC7, ASTC,
   ETC2) via Basis Universal or astcenc, outputting KTX2 or ``.astc`` files.
6. **Write metadata** — a ``.meta.json`` sidecar describing the source, output
   dimensions, mip levels, compression info, and processing settings.

Settings are read from the ``[texture]`` section of ``pipeline.toml``::

    [texture]
    max_size = 2048          # Clamp width and height to this limit
    generate_mipmaps = true  # Create mip chain alongside the base image
    output_format = "png"    # Output format (png, jpg, bmp)
    jpg_quality = 90         # JPEG quality (1-100, only used for jpg output)
    compression = "none"     # none, basisu, or astc
    basisu_format = "uastc"  # etc1s (smaller) or uastc (higher quality)
    basisu_quality = 128     # 1-255, higher = better + slower
    astc_block_size = "6x6"  # 4x4, 5x5, 6x6, 8x8
    astc_quality = "medium"  # fastest, fast, medium, thorough, exhaustive
    normal_map = false       # true for normal maps (BC5/linear encoding)
"""

from __future__ import annotations

import json
import logging
import math
import shutil
import subprocess
from pathlib import Path

from PIL import Image

from pipeline.plugin import AssetPlugin, AssetResult

log = logging.getLogger(__name__)

# Reasonable defaults matching the pipeline.toml example.
DEFAULT_MAX_SIZE = 2048
DEFAULT_OUTPUT_FORMAT = "png"
DEFAULT_JPG_QUALITY = 90

# Valid compression modes.
_VALID_COMPRESSION = {"none", "basisu", "astc"}

# Valid ASTC quality presets (mapped to astcenc CLI flags).
_ASTC_QUALITIES = {"fastest", "fast", "medium", "thorough", "exhaustive"}

# Valid ASTC block sizes.
_ASTC_BLOCK_SIZES = {"4x4", "5x5", "6x6", "8x8"}

# Pillow format strings for each output extension.
_FORMAT_MAP = {
    "png": "PNG",
    "jpg": "JPEG",
    "jpeg": "JPEG",
    "bmp": "BMP",
}


def _clamp_dimensions(width: int, height: int, max_size: int) -> tuple[int, int]:
    """Return *(new_w, new_h)* that fits within *max_size* on both axes,
    preserving aspect ratio.  Returns the original size if already within
    limits.
    """
    if width <= max_size and height <= max_size:
        return width, height

    scale = min(max_size / width, max_size / height)
    new_w = max(1, int(width * scale))
    new_h = max(1, int(height * scale))
    return new_w, new_h


def _mip_count(width: int, height: int) -> int:
    """Number of mip levels for a texture of the given dimensions,
    including the base level (mip 0).  Halves until the smallest side is 1.
    """
    if width < 1 or height < 1:
        return 1
    return 1 + int(math.log2(max(width, height)))


def _generate_mipmaps(
    img: Image.Image,
    output_stem: Path,
    output_ext: str,
    pil_format: str,
    save_kwargs: dict,
) -> list[dict]:
    """Write mip levels 1..N (mip 0 is the base image written separately).

    Returns a list of dicts describing each mip level (including mip 0).
    """
    w, h = img.size
    levels = _mip_count(w, h)
    mip_info: list[dict] = [{"level": 0, "width": w, "height": h}]

    current = img
    for level in range(1, levels):
        mip_w = max(1, w >> level)
        mip_h = max(1, h >> level)
        current = current.resize((mip_w, mip_h), Image.Resampling.LANCZOS)

        mip_path = output_stem.parent / f"{output_stem.name}_mip{level}{output_ext}"
        current.save(mip_path, format=pil_format, **save_kwargs)
        mip_info.append({"level": level, "width": mip_w, "height": mip_h})
        log.debug("  mip %d: %dx%d -> %s", level, mip_w, mip_h, mip_path.name)

    return mip_info


def _find_tool(name: str) -> str | None:
    """Return the full path to an external tool, or None if not installed."""
    return shutil.which(name)


def _run_basisu(
    input_path: Path,
    output_dir: Path,
    *,
    basisu_format: str = "uastc",
    quality: int = 128,
    mipmaps: bool = True,
    normal_map: bool = False,
) -> Path:
    """Compress a PNG into a KTX2 file using Basis Universal.

    Returns the path to the generated .ktx2 file.
    """
    tool = _find_tool("basisu")
    if tool is None:
        raise FileNotFoundError(
            "basisu not found — install Basis Universal to enable GPU "
            "texture compression (https://github.com/BinomialLLC/basis_universal)"
        )

    args = [tool, "-ktx2", "-q", str(quality)]
    if basisu_format == "uastc":
        args.append("-uastc")
    if mipmaps:
        args.append("-mipmap")
    if normal_map:
        args.extend(["-normal_map", "-separate_rg_to_color_alpha"])
    args.extend(["-file", str(input_path), "-output_path", str(output_dir)])

    try:
        result = subprocess.run(
            args, capture_output=True, text=True, check=False, timeout=600
        )
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError(
            f"basisu timed out after 600 s compressing {input_path.name}"
        ) from exc
    if result.returncode != 0:
        raise RuntimeError(
            f"basisu failed (exit {result.returncode}): {result.stderr.strip()}"
        )

    ktx2_path = output_dir / f"{input_path.stem}.ktx2"
    log.debug("  basisu -> %s", ktx2_path.name)
    return ktx2_path


def _run_astcenc(
    input_path: Path,
    output_path: Path,
    *,
    block_size: str = "6x6",
    quality: str = "medium",
) -> Path:
    """Compress an image into an ASTC file using astcenc.

    Returns the path to the generated .astc file.
    """
    tool = _find_tool("astcenc")
    if tool is None:
        raise FileNotFoundError(
            "astcenc not found — install the ASTC Encoder to enable ASTC "
            "compression (https://github.com/ARM-software/astc-encoder)"
        )

    args = [
        tool,
        "-cl",
        str(input_path),
        str(output_path),
        block_size,
        f"-{quality}",
    ]

    try:
        result = subprocess.run(
            args, capture_output=True, text=True, check=False, timeout=600
        )
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError(
            f"astcenc timed out after 600 s compressing {input_path.name}"
        ) from exc
    if result.returncode != 0:
        raise RuntimeError(
            f"astcenc failed (exit {result.returncode}): {result.stderr.strip()}"
        )

    log.debug("  astcenc -> %s", output_path.name)
    return output_path


def _write_metadata(
    meta_path: Path,
    source: Path,
    output: Path,
    original_size: tuple[int, int],
    final_size: tuple[int, int],
    mip_levels: list[dict],
    settings_used: dict,
    compression_info: dict | None = None,
) -> None:
    """Write a JSON sidecar with processing metadata."""
    meta = {
        "source": source.name,
        "output": output.name,
        "original_width": original_size[0],
        "original_height": original_size[1],
        "output_width": final_size[0],
        "output_height": final_size[1],
        "mip_levels": mip_levels,
        "settings": settings_used,
    }
    if compression_info is not None:
        meta["compression"] = compression_info
    meta_path.write_text(
        json.dumps(meta, indent=2) + "\n",
        encoding="utf-8",
    )
    log.debug("  metadata -> %s", meta_path.name)


class TexturePlugin(AssetPlugin):
    """Process image files: resize, convert format, generate mipmaps."""

    name = "texture"
    extensions = [".png", ".jpg", ".jpeg", ".tga", ".bmp"]

    def process(self, source: Path, output_dir: Path, settings: dict) -> AssetResult:
        """Load *source*, resize, convert, generate mipmaps, compress, write metadata.

        *settings* comes from ``[texture]`` in ``pipeline.toml``.
        """
        max_size = int(settings.get("max_size", DEFAULT_MAX_SIZE))
        if max_size < 1:
            raise ValueError(f"texture.max_size must be >= 1, got {max_size}")
        generate_mips = bool(settings.get("generate_mipmaps", True))
        out_fmt = str(settings.get("output_format", DEFAULT_OUTPUT_FORMAT)).lower()
        jpg_quality = int(settings.get("jpg_quality", DEFAULT_JPG_QUALITY))

        # Compression settings (optional, additive).
        compression = str(settings.get("compression", "none")).lower()
        if compression not in _VALID_COMPRESSION:
            raise ValueError(
                f"Unsupported compression {compression!r} — "
                f"choose from: {', '.join(sorted(_VALID_COMPRESSION))}"
            )
        basisu_format = str(settings.get("basisu_format", "uastc")).lower()
        basisu_quality = int(settings.get("basisu_quality", 128))
        astc_block_size = str(settings.get("astc_block_size", "6x6"))
        astc_quality = str(settings.get("astc_quality", "medium")).lower()

        # Validate only the selected backend's settings.
        if compression == "basisu":
            if basisu_format not in {"etc1s", "uastc"}:
                raise ValueError(
                    f"Unsupported basisu_format {basisu_format!r} — "
                    "choose from: etc1s, uastc"
                )
            if not 1 <= basisu_quality <= 255:
                raise ValueError(
                    f"basisu_quality must be between 1 and 255, got {basisu_quality}"
                )
        elif compression == "astc":
            if astc_block_size not in _ASTC_BLOCK_SIZES:
                raise ValueError(
                    f"Unsupported astc_block_size {astc_block_size!r} — "
                    f"choose from: {', '.join(sorted(_ASTC_BLOCK_SIZES))}"
                )
            if astc_quality not in _ASTC_QUALITIES:
                raise ValueError(
                    f"Unsupported astc_quality {astc_quality!r} — "
                    f"choose from: {', '.join(sorted(_ASTC_QUALITIES))}"
                )
        normal_map = bool(settings.get("normal_map", False))

        pil_format = _FORMAT_MAP.get(out_fmt)
        if pil_format is None:
            raise ValueError(
                f"Unsupported output format {out_fmt!r} — "
                f"choose from: {', '.join(sorted(_FORMAT_MAP))}"
            )

        out_ext = f".{out_fmt}" if out_fmt != "jpeg" else ".jpg"

        # Build the output path: preserve the relative directory structure
        # from the source, but replace the extension with the output format.
        output_dir.mkdir(parents=True, exist_ok=True)
        output_stem = output_dir / source.stem
        output_path = output_dir / f"{source.stem}{out_ext}"

        # Guard against collisions: if a file already exists at the output path
        # from a different source (e.g. foo.png and foo.tga in the same dir),
        # raise rather than silently overwriting.
        if output_path.exists():
            existing_meta = output_dir / f"{source.stem}.meta.json"
            if existing_meta.exists():
                try:
                    prev = json.loads(existing_meta.read_text(encoding="utf-8"))
                    if prev.get("source") != source.name:
                        raise FileExistsError(
                            f"Output collision: {output_path.name} already "
                            f"produced from {prev['source']!r}, cannot "
                            f"overwrite with {source.name!r}"
                        )
                except (json.JSONDecodeError, KeyError):
                    pass  # Corrupted meta — allow overwrite

        # -- Load ---------------------------------------------------------------
        img = Image.open(source)
        original_size = img.size
        log.debug("  loaded %s (%dx%d)", source.name, *original_size)

        # Ensure RGB(A) mode for consistent processing.
        # Check both alpha bands and palette/tRNS transparency metadata.
        has_alpha = "A" in img.mode or "transparency" in img.info
        target_mode = "RGBA" if has_alpha else "RGB"
        if img.mode != target_mode:
            img = img.convert(target_mode)

        # -- Resize -------------------------------------------------------------
        new_w, new_h = _clamp_dimensions(img.width, img.height, max_size)
        if (new_w, new_h) != img.size:
            img = img.resize((new_w, new_h), Image.Resampling.LANCZOS)
            log.debug("  resized to %dx%d (max_size=%d)", new_w, new_h, max_size)

        # -- Save base image (mip 0) -------------------------------------------
        save_kwargs: dict = {}
        if pil_format == "JPEG":
            save_kwargs["quality"] = jpg_quality
            # JPEG does not support alpha — drop to RGB.
            if img.mode == "RGBA":
                img = img.convert("RGB")

        img.save(output_path, format=pil_format, **save_kwargs)
        log.debug("  base -> %s", output_path.name)

        # -- Mipmaps ------------------------------------------------------------
        mip_levels: list[dict] = [
            {"level": 0, "width": img.width, "height": img.height}
        ]
        if generate_mips:
            mip_levels = _generate_mipmaps(
                img, output_stem, out_ext, pil_format, save_kwargs
            )

        # -- GPU compression (optional) -----------------------------------------
        compression_info: dict | None = None
        if compression == "basisu":
            tool_path = _find_tool("basisu")
            if tool_path is None:
                log.warning(
                    "basisu not installed — skipping GPU texture compression for %s",
                    source.name,
                )
            else:
                uncompressed_size = output_path.stat().st_size
                compressed_path = _run_basisu(
                    output_path,
                    output_dir,
                    basisu_format=basisu_format,
                    quality=basisu_quality,
                    mipmaps=generate_mips,
                    normal_map=normal_map,
                )
                if not compressed_path.exists():
                    log.warning(
                        "basisu reported success but output file %s not found",
                        compressed_path,
                    )
                else:
                    compressed_size = compressed_path.stat().st_size
                    compression_info = {
                        "codec": basisu_format,
                        "container": "ktx2",
                        "compressed_file": compressed_path.name,
                        "uncompressed_bytes": uncompressed_size,
                        "compressed_bytes": compressed_size,
                        "ratio": round(uncompressed_size / compressed_size, 2),
                        "normal_map": normal_map,
                    }

        elif compression == "astc":
            tool_path = _find_tool("astcenc")
            if tool_path is None:
                log.warning(
                    "astcenc not installed — skipping ASTC compression for %s",
                    source.name,
                )
            else:
                uncompressed_size = output_path.stat().st_size
                astc_output = output_dir / f"{source.stem}.astc"
                _run_astcenc(
                    output_path,
                    astc_output,
                    block_size=astc_block_size,
                    quality=astc_quality,
                )
                if not astc_output.exists():
                    log.warning(
                        "astcenc reported success but output file %s not found",
                        astc_output,
                    )
                else:
                    compressed_size = astc_output.stat().st_size
                    compression_info = {
                        "codec": "astc",
                        "block_size": astc_block_size,
                        "quality": astc_quality,
                        "compressed_file": astc_output.name,
                        "uncompressed_bytes": uncompressed_size,
                        "compressed_bytes": compressed_size,
                        "ratio": round(uncompressed_size / compressed_size, 2),
                    }

        # -- Metadata sidecar ---------------------------------------------------
        meta_path = output_dir / f"{source.stem}.meta.json"
        settings_used = {
            "max_size": max_size,
            "generate_mipmaps": generate_mips,
            "output_format": out_fmt,
        }
        if pil_format == "JPEG":
            settings_used["jpg_quality"] = jpg_quality
        if compression != "none":
            settings_used["compression"] = compression

        _write_metadata(
            meta_path,
            source,
            output_path,
            original_size,
            img.size,
            mip_levels,
            settings_used,
            compression_info=compression_info,
        )

        effective_compression = compression if compression_info is not None else "none"
        return AssetResult(
            source=source,
            output=output_path,
            metadata={
                "original_size": list(original_size),
                "output_size": [img.width, img.height],
                "mip_levels": len(mip_levels),
                "format": out_fmt,
                "meta_file": str(meta_path.name),
                "compression": effective_compression,
            },
        )
