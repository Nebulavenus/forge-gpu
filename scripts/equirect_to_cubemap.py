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

# Optional: imageio for HDR (.hdr / .exr) support.
try:
    import imageio.v3 as iio  # type: ignore[import-unresolved]

    _HAS_IMAGEIO = True
except ImportError:
    _HAS_IMAGEIO = False

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


def sample_equirect_float(source, u, v):
    """Bilinear sample an equirectangular image stored as float32.

    Same algorithm as sample_equirect but operates on float data and
    returns a float array (no clamping to uint8).  Used for HDR sources.
    """
    h, w, _c = source.shape

    px = u * w - 0.5
    py = v * h - 0.5

    x0 = np.floor(px).astype(np.int32)
    y0 = np.floor(py).astype(np.int32)
    x1 = x0 + 1
    y1 = y0 + 1

    fx = (px - x0).astype(np.float32)
    fy = (py - y0).astype(np.float32)

    x0 = x0 % w
    x1 = x1 % w
    y0 = np.clip(y0, 0, h - 1)
    y1 = np.clip(y1, 0, h - 1)

    p00 = source[y0, x0]
    p10 = source[y0, x1]
    p01 = source[y1, x0]
    p11 = source[y1, x1]

    fx = fx[..., np.newaxis]
    fy = fy[..., np.newaxis]
    result = (
        p00 * (1 - fx) * (1 - fy)
        + p10 * fx * (1 - fy)
        + p01 * (1 - fx) * fy
        + p11 * fx * fy
    )

    return result


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


def _is_hdr(path):
    """Return True if the file is an HDR format (Radiance .hdr or OpenEXR)."""
    ext = os.path.splitext(path)[1].lower()
    return ext in (".hdr", ".exr")


def _tone_map_reinhard(hdr, exposure=1.0):
    """Apply Reinhard tone mapping and sRGB gamma to an HDR float array.

    Returns a uint8 array suitable for saving as PNG.
    """
    # Apply exposure and sanitize invalid/negative radiance
    hdr = np.nan_to_num(hdr * exposure, nan=0.0, posinf=1e6, neginf=0.0)
    hdr = np.clip(hdr, 0.0, None)
    # Reinhard: L / (1 + L)
    mapped = hdr / (1.0 + hdr)
    # sRGB gamma (simplified pow 1/2.2)
    mapped = np.power(np.clip(mapped, 0.0, 1.0), 1.0 / 2.2)
    return np.clip(mapped * 255.0, 0, 255).astype(np.uint8)


def convert(input_path, output_dir, size, preloaded_hdr=None):
    """Convert an equirectangular image to 6 cube map face PNGs.

    If *preloaded_hdr* is provided (a float32 ndarray from the preflight check),
    it is reused directly, avoiding a second decode of the HDR file.
    """
    is_hdr = _is_hdr(input_path)

    if is_hdr:
        if not _HAS_IMAGEIO:
            sys.exit("HDR input requires imageio — install with: pip install imageio")
        print(f"Loading HDR: {input_path}")
        if preloaded_hdr is not None:
            source = preloaded_hdr
        else:
            try:
                source = iio.imread(input_path).astype(np.float32)  # type: ignore[reportPossiblyUnbound]
            except Exception as exc:
                sys.exit(f"Failed to decode HDR/EXR image '{input_path}': {exc}")
        # Normalize channels: ensure 3-channel (H, W, 3) output
        if source.ndim == 2:
            # Grayscale (H, W) → broadcast to (H, W, 3)
            source = np.stack([source] * 3, axis=-1)
        elif source.ndim == 3:
            if source.shape[2] == 0:
                sys.exit("Unsupported HDR channel count: 0")
            if source.shape[2] == 1:
                # Single channel (H, W, 1) → broadcast to (H, W, 3)
                source = np.broadcast_to(
                    source, (source.shape[0], source.shape[1], 3)
                ).copy()
            elif source.shape[2] == 2:
                # Two channels (H, W, 2) → take first, broadcast to (H, W, 3)
                source = np.stack([source[:, :, 0]] * 3, axis=-1)
            elif source.shape[2] >= 3:
                # Three or more channels → take first three
                source = source[:, :, :3]
        else:
            sys.exit(f"Unsupported HDR image rank: {source.ndim}")
        print(f"  Source size: {source.shape[1]}x{source.shape[0]} (HDR float)")
    else:
        print(f"Loading: {input_path}")
        img = Image.open(input_path).convert("RGB")
        source = np.array(img)
        print(f"  Source size: {img.width}x{img.height}")

    os.makedirs(output_dir, exist_ok=True)

    for face in FACES:
        print(f"  Generating {face}.png ({size}x{size})...", end="", flush=True)

        dirs = face_directions(face, size)
        u, v = direction_to_equirect_uv(dirs)

        if is_hdr:
            pixels_float = sample_equirect_float(source, u, v)
            pixels = _tone_map_reinhard(pixels_float, exposure=1.0)
        else:
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
    parser.add_argument("input", help="Path to equirectangular image (JPG/PNG/HDR/EXR)")
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
    preloaded_hdr = None
    if _is_hdr(args.input):
        if not _HAS_IMAGEIO:
            print(
                "HDR input requires imageio — install with: pip install imageio",
                file=sys.stderr,
            )
            return 1
        # Load once and reuse for both validation and conversion.
        try:
            preloaded_hdr = iio.imread(args.input).astype(np.float32)  # type: ignore[reportPossiblyUnbound]
        except Exception as exc:
            print(
                f"Failed to decode HDR/EXR image '{args.input}': {exc}",
                file=sys.stderr,
            )
            return 1
        if preloaded_hdr.ndim < 2:
            print(
                f"Unsupported HDR image rank: {preloaded_hdr.ndim}",
                file=sys.stderr,
            )
            return 1
        if preloaded_hdr.shape[0] == 0 or preloaded_hdr.shape[1] == 0:
            print("HDR image has invalid dimensions", file=sys.stderr)
            return 1
        ratio = preloaded_hdr.shape[1] / preloaded_hdr.shape[0]
        dims_str = f"{preloaded_hdr.shape[1]}x{preloaded_hdr.shape[0]}"
    else:
        img = Image.open(args.input)
        ratio = img.width / img.height
        dims_str = f"{img.width}x{img.height}"
        img.close()

    if abs(ratio - 2.0) > 0.02:
        print(
            f"Input image aspect ratio is {ratio:.3f}:1, expected ~2:1 "
            f"for equirectangular projection ({dims_str})",
            file=sys.stderr,
        )
        return 1

    convert(args.input, args.output_dir, args.size, preloaded_hdr=preloaded_hdr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
