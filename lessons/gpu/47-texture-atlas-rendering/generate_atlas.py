"""Generate a texture atlas from individual material PNGs.

Resizes all PNGs in assets/materials/ to 256x256, packs them into a grid
atlas, and writes:

  assets/atlas.png   — the packed atlas image (RGBA)
  assets/atlas.json  — per-material UV offset/scale metadata

All textures are the same size, so a simple grid layout is used instead of
the guillotine bin packer (which is designed for variable-size rects and
produces poor utilization when all inputs are identical).

Usage:
    python lessons/gpu/47-texture-atlas-rendering/generate_atlas.py
"""

from __future__ import annotations

import json
import math
import sys
from pathlib import Path

from PIL import Image

MATERIAL_SIZE = 256  # resize each material to this square size
ATLAS_PADDING = 4  # texel padding between packed rects

LESSON_DIR = Path(__file__).resolve().parent
MATERIALS_DIR = LESSON_DIR / "assets" / "materials"
ATLAS_IMAGE_PATH = LESSON_DIR / "assets" / "atlas.png"
ATLAS_JSON_PATH = LESSON_DIR / "assets" / "atlas.json"


def _next_power_of_two(n: int) -> int:
    """Return the smallest power of two >= n."""
    if n <= 0:
        return 1
    return 1 << (n - 1).bit_length()


def main() -> None:
    # Collect and resize material PNGs
    png_files = sorted(MATERIALS_DIR.glob("*.png"))
    if not png_files:
        print(f"No PNGs found in {MATERIALS_DIR}")
        sys.exit(1)

    count = len(png_files)
    print(f"Found {count} material textures")

    # Resize to MATERIAL_SIZE x MATERIAL_SIZE
    images: dict[str, Image.Image] = {}
    for p in png_files:
        name = p.stem
        img = Image.open(p).convert("RGBA")
        if img.size != (MATERIAL_SIZE, MATERIAL_SIZE):
            img = img.resize((MATERIAL_SIZE, MATERIAL_SIZE), Image.Resampling.LANCZOS)
            img.save(p)
            print(f"  Resized {name} to {MATERIAL_SIZE}x{MATERIAL_SIZE}")
        images[name] = img

    # Grid layout: find the most square arrangement
    cell = MATERIAL_SIZE + ATLAS_PADDING * 2  # cell size with padding
    cols = math.ceil(math.sqrt(count))
    rows = math.ceil(count / cols)

    # Atlas dimensions (shrunk to power-of-two)
    atlas_w = _next_power_of_two(cols * cell)
    atlas_h = _next_power_of_two(rows * cell)

    used_area = count * cell * cell
    total_area = atlas_w * atlas_h
    utilization = used_area / total_area if total_area > 0 else 0.0

    print(
        f"Atlas grid: {cols}x{rows} cells, "
        f"{atlas_w}x{atlas_h} pixels, "
        f"{utilization:.1%} utilization"
    )

    # Composite atlas image
    atlas = Image.new("RGBA", (atlas_w, atlas_h), (0, 0, 0, 0))
    names = sorted(images.keys())
    entries: dict[str, dict] = {}

    for i, name in enumerate(names):
        col = i % cols
        row = i // cols
        # Pixel position: padding offset within the cell
        px = col * cell + ATLAS_PADDING
        py = row * cell + ATLAS_PADDING

        img = images[name]
        atlas.paste(img, (px, py))

        # Extrude tile edges into padding to prevent mipmap/filter seams
        if ATLAS_PADDING > 0:
            # Left edge: 1px column replicated into left padding
            left_edge = img.crop((0, 0, 1, MATERIAL_SIZE))
            for p in range(ATLAS_PADDING):
                atlas.paste(left_edge, (px - 1 - p, py))
            # Right edge
            right_edge = img.crop((MATERIAL_SIZE - 1, 0, MATERIAL_SIZE, MATERIAL_SIZE))
            for p in range(ATLAS_PADDING):
                atlas.paste(right_edge, (px + MATERIAL_SIZE + p, py))
            # Top edge
            top_edge = img.crop((0, 0, MATERIAL_SIZE, 1))
            for p in range(ATLAS_PADDING):
                atlas.paste(top_edge, (px, py - 1 - p))
            # Bottom edge
            bottom_edge = img.crop((0, MATERIAL_SIZE - 1, MATERIAL_SIZE, MATERIAL_SIZE))
            for p in range(ATLAS_PADDING):
                atlas.paste(bottom_edge, (px, py + MATERIAL_SIZE + p))
            # Corners: replicate corner pixel into padding corners
            for dx in range(-ATLAS_PADDING, 0):
                for dy in range(-ATLAS_PADDING, 0):
                    atlas.putpixel((px + dx, py + dy), img.getpixel((0, 0)))
            for dx in range(ATLAS_PADDING):
                for dy in range(-ATLAS_PADDING, 0):
                    atlas.putpixel((px + MATERIAL_SIZE + dx, py + dy),
                                   img.getpixel((MATERIAL_SIZE - 1, 0)))
            for dx in range(-ATLAS_PADDING, 0):
                for dy in range(ATLAS_PADDING):
                    atlas.putpixel((px + dx, py + MATERIAL_SIZE + dy),
                                   img.getpixel((0, MATERIAL_SIZE - 1)))
            for dx in range(ATLAS_PADDING):
                for dy in range(ATLAS_PADDING):
                    atlas.putpixel((px + MATERIAL_SIZE + dx, py + MATERIAL_SIZE + dy),
                                   img.getpixel((MATERIAL_SIZE - 1, MATERIAL_SIZE - 1)))
        entries[name] = {
            "x": px,
            "y": py,
            "width": MATERIAL_SIZE,
            "height": MATERIAL_SIZE,
            "u_offset": px / atlas_w,
            "v_offset": py / atlas_h,
            "u_scale": MATERIAL_SIZE / atlas_w,
            "v_scale": MATERIAL_SIZE / atlas_h,
        }

    atlas.save(ATLAS_IMAGE_PATH)
    print(f"Wrote {ATLAS_IMAGE_PATH}")

    # Write metadata JSON
    meta = {
        "version": 1,
        "width": atlas_w,
        "height": atlas_h,
        "padding": ATLAS_PADDING,
        "utilization": round(utilization, 4),
        "entries": entries,
    }

    ATLAS_JSON_PATH.write_text(json.dumps(meta, indent=2) + "\n")
    print(f"Wrote {ATLAS_JSON_PATH}")


if __name__ == "__main__":
    main()
