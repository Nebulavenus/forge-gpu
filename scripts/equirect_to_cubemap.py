#!/usr/bin/env python
"""
equirect_to_cubemap.py — Convert equirectangular panorama to 6 cube map face images.

For each output pixel on each face, computes the 3D direction vector, converts
to spherical coordinates, then samples the equirectangular source at the
corresponding UV.

Face naming follows SDL_GPUCubeMapFace enum order:
  px.png  (+X, right)
  nx.png  (-X, left)
  py.png  (+Y, up)
  ny.png  (-Y, down)
  pz.png  (+Z, front)
  nz.png  (-Z, back)

Usage:
    python scripts/equirect_to_cubemap.py <input.jpg> <output_dir> [--size 1024]
"""

import argparse
import os
import sys

try:
    import numpy as np
except ImportError:
    sys.exit("Missing dependency: numpy — install with: pip install numpy")

try:
    from PIL import Image
except ImportError:
    sys.exit("Missing dependency: Pillow — install with: pip install Pillow")

# Face order matching SDL_GPUCubeMapFace enum
FACES = ["px", "nx", "py", "ny", "pz", "nz"]


def face_directions(face, size):
    """Compute (size x size x 3) array of unit direction vectors for a cube face.

    Each pixel maps to a point on the unit cube, then gets normalized to a
    direction on the unit sphere.
    """
    # Normalized pixel coordinates in [-1, 1] inclusive.  Using the full range
    # ensures that edge pixels on adjacent faces compute the EXACT same 3D
    # direction — e.g. the +Z right edge and +X left edge both get (1, v, 1).
    # A half-pixel inset (linspace(-1+1/N, 1-1/N, N)) would produce slightly
    # different directions at face edges, creating visible seams.
    coords = np.linspace(-1.0, 1.0, size)
    # u varies across columns, v varies down rows
    u, v = np.meshgrid(coords, -coords)

    dirs = np.zeros((size, size, 3), dtype=np.float64)

    if face == "px":  # +X: look right
        dirs[..., 0] = 1.0
        dirs[..., 1] = v
        dirs[..., 2] = -u
    elif face == "nx":  # -X: look left
        dirs[..., 0] = -1.0
        dirs[..., 1] = v
        dirs[..., 2] = u
    elif face == "py":  # +Y: look up
        dirs[..., 0] = u
        dirs[..., 1] = 1.0
        dirs[..., 2] = -v
    elif face == "ny":  # -Y: look down
        dirs[..., 0] = u
        dirs[..., 1] = -1.0
        dirs[..., 2] = v
    elif face == "pz":  # +Z: look forward
        dirs[..., 0] = u
        dirs[..., 1] = v
        dirs[..., 2] = 1.0
    elif face == "nz":  # -Z: look backward
        dirs[..., 0] = -u
        dirs[..., 1] = v
        dirs[..., 2] = -1.0

    # Normalize to unit sphere
    length = np.sqrt(np.sum(dirs**2, axis=2, keepdims=True))
    dirs /= length
    return dirs


def direction_to_equirect_uv(dirs):
    """Convert 3D direction vectors to equirectangular UV coordinates.

    Returns (u, v) arrays in [0, 1] range.
    Longitude maps to u (0 = left edge, 1 = right edge).
    Latitude maps to v (0 = top/north, 1 = bottom/south).
    """
    x = dirs[..., 0]
    y = dirs[..., 1]
    z = dirs[..., 2]

    # Longitude: atan2(x, z) gives angle from +Z axis around Y
    # Range [-pi, pi] -> [0, 1]
    lon = np.arctan2(x, z)
    u = (lon / (2.0 * np.pi)) + 0.5

    # Latitude: asin(y) gives angle from equator
    # Range [-pi/2, pi/2] -> [0, 1] (top to bottom)
    lat = np.arcsin(np.clip(y, -1.0, 1.0))
    v = 0.5 - (lat / np.pi)

    return u, v


def sample_equirect(source, u, v):
    """Bilinear sample the equirectangular image at (u, v) coordinates.

    u, v are in [0, 1].  Returns an (H, W, C) uint8 array.
    """
    h, w, _c = source.shape

    # Convert UV to pixel coordinates (with wrapping for longitude)
    px = u * w - 0.5
    py = v * h - 0.5

    # Integer coordinates for the 4 surrounding pixels
    x0 = np.floor(px).astype(np.int32)
    y0 = np.floor(py).astype(np.int32)
    x1 = x0 + 1
    y1 = y0 + 1

    # Fractional part for interpolation weights
    fx = (px - x0).astype(np.float32)
    fy = (py - y0).astype(np.float32)

    # Wrap horizontally (longitude wraps), clamp vertically (poles)
    x0 = x0 % w
    x1 = x1 % w
    y0 = np.clip(y0, 0, h - 1)
    y1 = np.clip(y1, 0, h - 1)

    # Gather the 4 corner pixels
    p00 = source[y0, x0].astype(np.float32)
    p10 = source[y0, x1].astype(np.float32)
    p01 = source[y1, x0].astype(np.float32)
    p11 = source[y1, x1].astype(np.float32)

    # Bilinear interpolation
    fx = fx[..., np.newaxis]
    fy = fy[..., np.newaxis]
    result = (
        p00 * (1 - fx) * (1 - fy)
        + p10 * fx * (1 - fy)
        + p01 * (1 - fx) * fy
        + p11 * fx * fy
    )

    return np.clip(result, 0, 255).astype(np.uint8)


def convert(input_path, output_dir, size):
    """Convert an equirectangular image to 6 cube map face PNGs."""
    print(f"Loading: {input_path}")
    img = Image.open(input_path).convert("RGB")
    source = np.array(img)
    print(f"  Source size: {img.width}x{img.height}")

    os.makedirs(output_dir, exist_ok=True)

    for face in FACES:
        print(f"  Generating {face}.png ({size}x{size})...", end="", flush=True)

        dirs = face_directions(face, size)
        u, v = direction_to_equirect_uv(dirs)
        pixels = sample_equirect(source, u, v)

        face_img = Image.fromarray(pixels)
        out_path = os.path.join(output_dir, f"{face}.png")
        face_img.save(out_path)
        print(" done")

    print(f"\n6 faces written to {output_dir}/")


def main():
    parser = argparse.ArgumentParser(
        description="Convert equirectangular panorama to cube map faces."
    )
    parser.add_argument("input", help="Path to equirectangular image (JPG/PNG)")
    parser.add_argument("output_dir", help="Output directory for face PNGs")
    parser.add_argument(
        "--size",
        type=int,
        default=1024,
        help="Face size in pixels (default: 1024)",
    )
    args = parser.parse_args()

    if not os.path.isfile(args.input):
        print(f"Input file not found: {args.input}", file=sys.stderr)
        return 1

    if args.size <= 0:
        print(
            f"Face size must be a positive integer, got: {args.size}", file=sys.stderr
        )
        return 1

    # Validate that the input image has an approximately 2:1 aspect ratio
    # (expected for equirectangular projections).
    img = Image.open(args.input)
    ratio = img.width / img.height
    if abs(ratio - 2.0) > 0.02:
        print(
            f"Input image aspect ratio is {ratio:.3f}:1, expected ~2:1 "
            f"for equirectangular projection ({img.width}x{img.height})",
            file=sys.stderr,
        )
        return 1
    img.close()

    convert(args.input, args.output_dir, args.size)
    return 0


if __name__ == "__main__":
    sys.exit(main())
