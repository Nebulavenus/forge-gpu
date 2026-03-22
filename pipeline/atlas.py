"""Guillotine bin-packing algorithm for texture atlas generation.

Packs variable-size rectangles into a single atlas image.  The algorithm
sorts inputs by area (largest first) and uses best-area-fit placement in
a list of free rectangles, splitting the remaining space with guillotine
cuts after each placement.

This approach produces better utilization than shelf packing for textures
with varied aspect ratios — typical of game material sets where albedo,
normal, and metallic-roughness maps share dimensions within a material
but differ across materials.

The output is an :class:`AtlasResult` containing the packed positions,
the final atlas dimensions (shrunk to the smallest power-of-two that
fits), and a utilization metric.
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

# ---------------------------------------------------------------------------
# Public data types
# ---------------------------------------------------------------------------


@dataclass
class AtlasRect:
    """A single rectangle placed in the atlas."""

    name: str
    x: int
    y: int
    width: int
    height: int


@dataclass
class AtlasResult:
    """The output of a packing operation."""

    width: int
    height: int
    rects: list[AtlasRect] = field(default_factory=list)
    utilization: float = 0.0


# ---------------------------------------------------------------------------
# Internal helpers
# ---------------------------------------------------------------------------


def _next_power_of_two(n: int) -> int:
    """Return the smallest power of two >= *n* (minimum 1)."""
    if n <= 1:
        return 1
    return 1 << (n - 1).bit_length()


@dataclass
class _FreeRect:
    """An axis-aligned free region inside the atlas."""

    x: int
    y: int
    w: int
    h: int


# ---------------------------------------------------------------------------
# Guillotine packer
# ---------------------------------------------------------------------------


def pack_rects(
    rects: list[tuple[str, int, int]],
    max_size: int = 4096,
    padding: int = 4,
) -> AtlasResult:
    """Pack *rects* into the smallest power-of-two atlas that fits.

    Parameters
    ----------
    rects:
        Input rectangles as ``(name, width, height)`` tuples.
    max_size:
        Maximum atlas dimension on either axis (pixels).
    padding:
        Pixels of empty space added around each rectangle to prevent
        texture bleeding during bilinear filtering.

    Returns
    -------
    AtlasResult
        Packed positions, atlas dimensions, and utilization.  If any
        rectangle cannot fit, ``ValueError`` is raised.
    """
    if not rects:
        return AtlasResult(width=0, height=0)

    # Pad each rect so the padding is consumed from the available space.
    padded = [(name, w + padding * 2, h + padding * 2) for name, w, h in rects]

    # Sort by area descending — large items first reduces fragmentation.
    padded.sort(key=lambda r: r[1] * r[2], reverse=True)

    # Validate: the largest single rect must fit within max_size.
    for name, pw, ph in padded:
        if pw > max_size or ph > max_size:
            raise ValueError(
                f"Rectangle {name!r} ({pw - padding * 2}x{ph - padding * 2}) "
                f"exceeds max_size {max_size} even with padding"
            )

    # Start with an atlas at max_size and shrink later.
    free: list[_FreeRect] = [_FreeRect(0, 0, max_size, max_size)]
    placed: list[AtlasRect] = []

    for name, pw, ph in padded:
        # Best-area-fit: find the free rect with the smallest area that
        # can contain (pw, ph).
        best_idx = -1
        best_area = math.inf

        for i, fr in enumerate(free):
            if pw <= fr.w and ph <= fr.h:
                area = fr.w * fr.h
                if area < best_area:
                    best_area = area
                    best_idx = i

        if best_idx < 0:
            raise ValueError(
                f"Cannot fit rectangle {name!r} ({pw - padding * 2}x"
                f"{ph - padding * 2}) into atlas of max_size {max_size}"
            )

        fr = free.pop(best_idx)

        # Place the rect at the top-left of the free region (with padding
        # offset so the usable area starts inside the padding border).
        placed.append(
            AtlasRect(
                name=name,
                x=fr.x + padding,
                y=fr.y + padding,
                width=pw - padding * 2,
                height=ph - padding * 2,
            )
        )

        # Guillotine split: create two free rects from the remainder.
        # Split along the axis that leaves the larger remaining piece.
        right_w = fr.w - pw
        bottom_h = fr.h - ph

        if right_w > 0 and bottom_h > 0:
            # Choose the split that maximises the larger remainder.
            # Horizontal split: right rect spans full remaining height.
            # Vertical split: bottom rect spans full remaining width.
            if right_w * fr.h >= fr.w * bottom_h:
                # Split vertically — right gets full height.
                free.append(_FreeRect(fr.x + pw, fr.y, right_w, fr.h))
                free.append(_FreeRect(fr.x, fr.y + ph, pw, bottom_h))
            else:
                # Split horizontally — bottom gets full width.
                free.append(_FreeRect(fr.x, fr.y + ph, fr.w, bottom_h))
                free.append(_FreeRect(fr.x + pw, fr.y, right_w, ph))
        elif right_w > 0:
            free.append(_FreeRect(fr.x + pw, fr.y, right_w, fr.h))
        elif bottom_h > 0:
            free.append(_FreeRect(fr.x, fr.y + ph, fr.w, bottom_h))

    # Shrink to the smallest power-of-two that contains all placed rects.
    max_x = 0
    max_y = 0
    for r in placed:
        # Account for padding on the right/bottom edge.
        max_x = max(max_x, r.x + r.width + padding)
        max_y = max(max_y, r.y + r.height + padding)

    atlas_w = min(max_size, _next_power_of_two(max_x))
    atlas_h = min(max_size, _next_power_of_two(max_y))

    # Utilization: ratio of used pixels to total atlas pixels.
    used_area = sum((r.width + padding * 2) * (r.height + padding * 2) for r in placed)
    total_area = atlas_w * atlas_h
    utilization = used_area / total_area if total_area > 0 else 0.0

    return AtlasResult(
        width=atlas_w,
        height=atlas_h,
        rects=placed,
        utilization=round(utilization, 4),
    )
