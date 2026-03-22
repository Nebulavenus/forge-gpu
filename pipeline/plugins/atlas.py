"""Atlas packing plugin — composites processed textures into a single atlas.

This is a **post-processing** plugin: it runs after the texture plugin has
produced individual processed textures with ``.meta.json`` sidecars.  The
plugin groups textures by material name, packs all materials into a single
atlas using the guillotine bin packer, composites the images with Pillow,
and writes the atlas image alongside an ``atlas.json`` metadata file.

Each material is represented by one image in the atlas (preferring the
albedo/base_color texture when available).  All texture slots for a
material share the same UV rectangle, so a single UV remap applies to
every texture lookup for that material.

Settings from ``[atlas]`` in ``pipeline.toml``::

    [atlas]
    atlas_enabled = false     # opt-in per project
    atlas_max_size = 4096     # maximum atlas dimension (pixels)
    atlas_padding = 4         # texel padding around each rect
"""

from __future__ import annotations

import json
import logging
from pathlib import Path

from PIL import Image

from pipeline.atlas import AtlasRect, pack_rects
from pipeline.plugin import AssetPlugin, AssetResult

log = logging.getLogger(__name__)

# Default settings matching the ATLAS_SETTINGS_SCHEMA defaults.
DEFAULT_MAX_SIZE = 4096
DEFAULT_PADDING = 4


def _find_material_textures(
    output_dir: Path,
) -> dict[str, list[Path]]:
    """Scan *output_dir* for processed textures with ``.meta.json`` sidecars.

    Groups texture files by material name.  The material name is derived
    from the filename by stripping known suffixes (``_albedo``, ``_normal``,
    ``_metallic_roughness``, ``_ao``, ``_emissive``).  Textures without a
    recognised suffix are assigned to a material named after their stem.

    Returns a dict mapping material names to lists of texture file paths.
    """
    # Known texture slot suffixes (order matters — match longest first).
    slot_suffixes = [
        "_metallic_roughness",
        "_base_color",
        "_albedo",
        "_normal",
        "_emissive",
        "_ao",
        "_mr",
    ]

    materials: dict[str, list[Path]] = {}

    for meta_path in sorted(output_dir.rglob("*.meta.json")):
        try:
            meta = json.loads(meta_path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            continue

        if not isinstance(meta, dict):
            continue
        output_name = meta.get("output")
        if not isinstance(output_name, str) or not output_name:
            continue

        texture_path = (meta_path.parent / output_name).resolve()
        if not texture_path.is_file():
            continue
        # Reject paths that resolve outside the output directory.
        if not texture_path.is_relative_to(output_dir.resolve()):
            log.warning(
                "Sidecar %s points outside output dir — skipping", meta_path.name
            )
            continue

        # Derive material name from the texture stem.
        stem = texture_path.stem
        material_name = stem
        for suffix in slot_suffixes:
            if stem.lower().endswith(suffix):
                material_name = stem[: -len(suffix)]
                break

        # Include relative directory to avoid collisions when textures
        # in different subdirectories share the same stem (e.g.
        # characters/wood_albedo.png vs props/wood_albedo.png).
        rel_dir = meta_path.parent.relative_to(output_dir)
        material_key = (
            material_name if rel_dir == Path(".") else str(rel_dir / material_name)
        )
        materials.setdefault(material_key, []).append(texture_path)

    return materials


def _material_max_dimension(textures: list[Path]) -> tuple[int, int]:
    """Return the maximum (width, height) across all texture images."""
    max_w, max_h = 0, 0
    for tex_path in textures:
        try:
            with Image.open(tex_path) as img:
                if img.width > max_w:
                    max_w = img.width
                if img.height > max_h:
                    max_h = img.height
        except Exception:
            log.warning("Could not read dimensions from %s", tex_path.name)
    return max_w, max_h


def build_atlas(
    output_dir: Path,
    *,
    max_size: int = DEFAULT_MAX_SIZE,
    padding: int = DEFAULT_PADDING,
) -> AssetResult | None:
    """Build a texture atlas from processed textures in *output_dir*.

    Returns an :class:`AssetResult` pointing to the atlas image, or
    ``None`` if there are fewer than two materials to pack.
    """
    materials = _find_material_textures(output_dir)

    # Determine the packing rect for each material (largest texture size).
    # Filter here rather than on raw sidecar count — unreadable or non-image
    # outputs should not count toward the 2-material minimum.
    material_dims: dict[str, tuple[int, int]] = {}
    for mat_name, textures in materials.items():
        w, h = _material_max_dimension(textures)
        if w > 0 and h > 0:
            material_dims[mat_name] = (w, h)

    if len(material_dims) < 2:
        log.info("Fewer than 2 atlasable materials — skipping atlas packing")
        return None

    # Pack rects.
    input_rects = [(name, w, h) for name, (w, h) in material_dims.items()]
    result = pack_rects(input_rects, max_size=max_size, padding=padding)

    # Build a lookup from material name to atlas rect.
    rect_map: dict[str, AtlasRect] = {r.name: r for r in result.rects}

    # Composite one representative texture per material into the atlas.
    # We pick the first albedo/base_color texture if available, otherwise
    # the first texture in the list.  Full multi-slot atlases (separate
    # color, normal, MR atlases sharing UV layout) are left as an exercise.
    _preferred_suffixes = ("_albedo", "_base_color")
    atlas_img = Image.new("RGBA", (result.width, result.height), (0, 0, 0, 0))
    failed_materials: set[str] = set()

    for mat_name, textures in materials.items():
        rect = rect_map.get(mat_name)
        if rect is None:
            continue

        # Pick the preferred texture slot for compositing.
        tex_path = textures[0]
        for candidate in textures:
            if any(candidate.stem.lower().endswith(s) for s in _preferred_suffixes):
                tex_path = candidate
                break

        try:
            with Image.open(tex_path) as tex:
                resized = tex.resize(
                    (rect.width, rect.height), Image.Resampling.LANCZOS
                )
                if resized.mode != "RGBA":
                    resized = resized.convert("RGBA")
                atlas_img.paste(resized, (rect.x, rect.y))
        except Exception:
            log.warning(
                "Could not composite %s into atlas — excluding from metadata",
                tex_path.name,
            )
            failed_materials.add(mat_name)

    # Build metadata entries — exclude materials that failed compositing.
    entries: dict[str, dict] = {}
    for rect in result.rects:
        if rect.name in failed_materials:
            continue
        entries[rect.name] = {
            "x": rect.x,
            "y": rect.y,
            "width": rect.width,
            "height": rect.height,
            "u_offset": round(rect.x / result.width, 6),
            "v_offset": round(rect.y / result.height, 6),
            "u_scale": round(rect.width / result.width, 6),
            "v_scale": round(rect.height / result.height, 6),
        }

    # If composite failures reduced entries below the 2-material minimum,
    # do not write an incomplete atlas.
    if len(entries) < 2:
        log.warning(
            "Only %d material(s) composited successfully — skipping atlas",
            len(entries),
        )
        return None

    # Write atlas image.
    atlas_path = output_dir / "atlas.png"
    atlas_img.save(atlas_path, format="PNG")
    log.info("Atlas written: %s (%dx%d)", atlas_path.name, result.width, result.height)

    # Recompute utilization based on the entries that actually made it into
    # the atlas (excludes failed composites).
    total_area = result.width * result.height
    used_area = sum(
        (e["width"] + padding * 2) * (e["height"] + padding * 2)
        for e in entries.values()
    )
    effective_utilization = round(used_area / total_area, 4) if total_area > 0 else 0.0

    atlas_meta = {
        "version": 1,
        "width": result.width,
        "height": result.height,
        "padding": padding,
        "utilization": effective_utilization,
        "entries": entries,
    }

    meta_path = output_dir / "atlas.json"
    meta_path.write_text(
        json.dumps(atlas_meta, indent=2) + "\n",
        encoding="utf-8",
    )
    log.info(
        "Atlas metadata: %s (%.0f%% utilization)",
        meta_path.name,
        effective_utilization * 100,
    )

    return AssetResult(
        source=output_dir,
        output=atlas_path,
        metadata={
            "width": result.width,
            "height": result.height,
            "materials": len(entries),
            "utilization": effective_utilization,
            "atlas_meta": str(meta_path.name),
        },
    )


class AtlasPlugin(AssetPlugin):
    """Post-processing plugin that packs textures into an atlas.

    Unlike other plugins, this does not process individual files — it
    operates on the entire output directory after textures have been
    processed.  The ``process()`` method expects *source* to be the
    output directory (or a sentinel path) and builds the atlas from
    all ``.meta.json`` sidecars found there.
    """

    name = "atlas"
    extensions: list[str] = []  # Not triggered by file extension.

    def process(self, source: Path, output_dir: Path, settings: dict) -> AssetResult:
        """Build a texture atlas from processed textures in *output_dir*."""
        enabled = bool(settings.get("atlas_enabled", False))
        if not enabled:
            return AssetResult(
                source=source,
                output=output_dir,
                metadata={"processed": False, "reason": "atlas_enabled is false"},
            )

        max_size = int(settings.get("atlas_max_size", DEFAULT_MAX_SIZE))
        padding = int(settings.get("atlas_padding", DEFAULT_PADDING))

        result = build_atlas(output_dir, max_size=max_size, padding=padding)

        if result is None:
            return AssetResult(
                source=source,
                output=output_dir,
                metadata={"processed": False, "reason": "fewer than 2 materials"},
            )

        return result
