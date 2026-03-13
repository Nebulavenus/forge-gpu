#!/usr/bin/env python
"""
capture_lesson.py — Capture screenshots and animated GIFs from forge-gpu lessons.

Runs a lesson executable with capture flags, converts the resulting BMP
file(s) to PNG (or assembles an animated GIF) using Pillow, and updates
the lesson README.

Usage:
    python scripts/capture_lesson.py <lesson-dir> [options]

Examples:
    python scripts/capture_lesson.py lessons/gpu/01-hello-window
    python scripts/capture_lesson.py lessons/physics/01-point-particles --gif
    python scripts/capture_lesson.py lessons/gpu/02-first-triangle --no-update-readme

Options:
    --capture-frame N    Frame to start capturing (default: 5)
    --no-update-readme   Skip updating the lesson README
    --build              Build the lesson before capturing (default: auto-detect)
    --headless           Use lavapipe + Xvfb for headless capture (auto-detected)
    --gif                Capture an animated GIF instead of a screenshot
    --gif-frames N       Number of frames to capture for GIF (default: 120)
    --gif-fps N          Playback frame rate for the GIF (default: 30)
"""

import argparse
import contextlib
import glob
import os
import re
import shutil
import subprocess
import sys

LESSON_TRACKS = ["gpu", "physics", "ui", "math", "engine", "assets"]


def find_executable(target_name, lesson_dir=None):
    """Find the lesson executable in the build directory.

    Searches all lesson tracks (gpu, physics, ui, etc.) for the target.
    If lesson_dir is provided, its track is searched first.
    """
    # Determine track order — prioritize the track from lesson_dir if given
    tracks = list(LESSON_TRACKS)
    if lesson_dir:
        parts = os.path.normpath(lesson_dir).split(os.sep)
        for i, part in enumerate(parts):
            if part == "lessons" and i + 1 < len(parts):
                track = parts[i + 1]
                if track in tracks:
                    tracks.remove(track)
                    tracks.insert(0, track)
                break

    for track in tracks:
        base = os.path.join("build", "lessons", track, target_name)
        candidates = [
            # Multi-config generators (MSVC, Ninja Multi-Config)
            os.path.join(base, "Debug", f"{target_name}.exe"),
            os.path.join(base, "Debug", target_name),
            os.path.join(base, "Release", f"{target_name}.exe"),
            os.path.join(base, "Release", target_name),
            # Single-config generators (Unix Makefiles, Ninja)
            os.path.join(base, f"{target_name}.exe"),
            os.path.join(base, target_name),
        ]
        for path in candidates:
            if os.path.isfile(path):
                return path
    return None


def resolve_cmake():
    """Locate the cmake executable on PATH, or exit with a clear error."""
    cmake_path = shutil.which("cmake")
    if not cmake_path:
        print("Error: cmake not found on PATH.")
        sys.exit(1)
    return cmake_path


def build_lesson(target_name):
    """Build the lesson with FORGE_CAPTURE enabled."""
    cmake_path = resolve_cmake()

    print("Configuring with FORGE_CAPTURE=ON...")
    try:
        result = subprocess.run(
            [cmake_path, "-B", "build", "-DFORGE_CAPTURE=ON"],
            capture_output=True,
            text=True,
            timeout=120,
        )
    except subprocess.TimeoutExpired:
        print("CMake configure timed out after 120 seconds")
        return False
    if result.returncode != 0:
        print(f"Configure failed:\n{result.stderr}")
        return False

    print(f"Building {target_name}...")
    try:
        result = subprocess.run(
            [
                cmake_path,
                "--build",
                "build",
                "--config",
                "Debug",
                "--target",
                target_name,
            ],
            capture_output=True,
            text=True,
            timeout=300,
        )
    except subprocess.TimeoutExpired:
        print("Build timed out after 300 seconds")
        return False
    if result.returncode != 0:
        print(f"Build failed:\n{result.stderr}")
        return False
    print("Build succeeded.")
    return True


LAVAPIPE_ICD = "/usr/share/vulkan/icd.d/lvp_icd.json"


def _should_use_headless(explicit):
    """Decide whether to use headless mode (lavapipe + Xvfb).

    Returns True when explicitly requested or when auto-detected: no DISPLAY
    environment variable and both xvfb-run and the lavapipe ICD are available.
    """
    if explicit:
        return True
    if os.environ.get("DISPLAY"):
        return False
    return shutil.which("xvfb-run") and os.path.isfile(LAVAPIPE_ICD)


def _build_headless_env():
    """Build environment dict for headless capture."""
    env = os.environ.copy()
    env["VK_ICD_FILENAMES"] = LAVAPIPE_ICD
    env["VK_DRIVER_FILES"] = LAVAPIPE_ICD
    return env


def _wrap_headless(cmd):
    """Wrap a command list with xvfb-run for headless execution."""
    return ["xvfb-run", "-a", "--server-args=-screen 0 1280x720x24"] + cmd


def capture_screenshot(
    exe_path, output_bmp, capture_frame, headless=False, extra_args=None
):
    """Run the lesson and capture a single frame."""
    exe_path = os.path.abspath(exe_path)
    output_bmp = os.path.abspath(output_bmp)
    headless = _should_use_headless(headless)

    # Run from the executable's directory so asset paths resolve correctly.
    exe_dir = os.path.dirname(exe_path)

    cmd = [exe_path, "--screenshot", output_bmp, "--capture-frame", str(capture_frame)]
    if extra_args:
        cmd.extend(extra_args)
    env = os.environ.copy()

    if headless:
        if not os.path.isfile(LAVAPIPE_ICD):
            print(f"Error: lavapipe ICD not found at {LAVAPIPE_ICD}")
            print("Install with: apt install mesa-vulkan-drivers")
            return False
        if not shutil.which("xvfb-run"):
            print("Error: xvfb-run not found on PATH")
            print("Install with: apt install xvfb")
            return False
        env = _build_headless_env()
        cmd = _wrap_headless(cmd)
        print(f"Capturing screenshot headless via lavapipe (frame {capture_frame})...")
    else:
        print(f"Capturing screenshot (frame {capture_frame})...")

    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,
            env=env,
            cwd=exe_dir,
        )
    except subprocess.TimeoutExpired:
        print("Capture timed out after 120 seconds")
        return False
    if result.returncode != 0:
        print(f"Capture failed (exit code {result.returncode})")
        if result.stderr:
            print(result.stderr)
        return False
    return os.path.isfile(output_bmp)


def capture_gif_frames(
    exe_path, output_dir, num_frames, capture_frame, headless=False, extra_args=None
):
    """Run the lesson and capture multiple consecutive frames as BMPs."""
    if num_frames < 1:
        print("Error: GIF frame count must be positive")
        return False

    exe_path = os.path.abspath(exe_path)
    output_dir = os.path.abspath(output_dir)
    headless = _should_use_headless(headless)

    # Run from the executable's directory so asset paths resolve correctly.
    exe_dir = os.path.dirname(exe_path)

    os.makedirs(output_dir, exist_ok=True)

    # Clean up stale frame BMPs from a previous failed run
    stale = glob.glob(os.path.join(output_dir, "frame_*.bmp"))
    for f in stale:
        os.remove(f)

    cmd = [
        exe_path,
        "--gif-frames",
        output_dir,
        str(num_frames),
        "--capture-frame",
        str(capture_frame),
    ]
    if extra_args:
        cmd.extend(extra_args)
    env = os.environ.copy()

    if headless:
        if not os.path.isfile(LAVAPIPE_ICD):
            print(f"Error: lavapipe ICD not found at {LAVAPIPE_ICD}")
            print("Install with: apt install mesa-vulkan-drivers")
            return False
        if not shutil.which("xvfb-run"):
            print("Error: xvfb-run not found on PATH")
            print("Install with: apt install xvfb")
            return False
        env = _build_headless_env()
        cmd = _wrap_headless(cmd)
        print(
            f"Capturing {num_frames} GIF frames headless via lavapipe "
            f"(starting at frame {capture_frame})..."
        )
    else:
        print(
            f"Capturing {num_frames} GIF frames (starting at frame {capture_frame})..."
        )

    # GIF capture runs longer — allow up to 5 minutes
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=300,
            env=env,
            cwd=exe_dir,
        )
    except subprocess.TimeoutExpired:
        print("GIF capture timed out after 300 seconds")
        return False
    if result.returncode != 0:
        print(f"GIF capture failed (exit code {result.returncode})")
        if result.stderr:
            print(result.stderr)
        return False

    # Verify the expected number of frames were captured
    captured = sorted(glob.glob(os.path.join(output_dir, "frame_*.bmp")))
    if len(captured) != num_frames:
        print(f"Error: expected {num_frames} frame BMPs, found {len(captured)}")
        return False

    print(f"Captured {len(captured)} frames")
    return True


def bmp_to_png(bmp_path, png_path):
    """Convert a BMP file to optimized PNG using Pillow."""
    from PIL import Image

    with Image.open(bmp_path) as img:
        # Drop alpha channel if fully opaque (smaller PNG)
        out = img
        if img.mode == "RGBA":
            alpha = img.getchannel("A")
            try:
                if alpha.getextrema() == (255, 255):
                    out = img.convert("RGB")
            finally:
                alpha.close()
        out.save(png_path, optimize=True)
        if out is not img:
            out.close()
    size_kb = os.path.getsize(png_path) / 1024
    print(f"Saved {png_path} ({size_kb:.1f} KB)")


def assemble_gif(frame_dir, gif_path, fps=30):
    """Assemble numbered BMP frames into an animated GIF using Pillow."""
    from PIL import Image

    frame_paths = sorted(glob.glob(os.path.join(frame_dir, "frame_*.bmp")))
    if not frame_paths:
        print("Error: no frame BMPs found to assemble")
        return False

    frames = []
    for fp in frame_paths:
        try:
            with Image.open(fp) as img:
                # Convert RGBA to RGB for smaller GIF
                frame = img.convert("RGB") if img.mode == "RGBA" else img.copy()
                frames.append(frame)
        except OSError as e:
            print(f"Warning: skipping {fp}: {e}")

    if not frames:
        print("Error: all frame BMPs failed to open")
        return False

    fps = max(fps, 1)  # guard against zero division
    duration_ms = int(1000 / fps)
    frames[0].save(
        gif_path,
        save_all=True,
        append_images=frames[1:],
        duration=duration_ms,
        loop=0,
        optimize=True,
    )

    size_kb = os.path.getsize(gif_path) / 1024
    print(f"Saved {gif_path} ({size_kb:.1f} KB, {len(frames)} frames, {fps} fps)")

    # Close all Pillow image handles
    for frame in frames:
        frame.close()

    # Clean up temp BMPs
    for fp in frame_paths:
        os.remove(fp)

    return True


def update_readme(readme_path, image_rel_path, lesson_name):
    """Replace the TODO screenshot placeholder in a lesson README."""
    if not os.path.isfile(readme_path):
        print(f"README not found: {readme_path}")
        return False

    with open(readme_path, encoding="utf-8") as f:
        content = f.read()

    # Match TODO comments about screenshots
    pattern = r"<!--\s*TODO:.*?screenshot.*?-->"
    replacement = f"![{lesson_name} result]({image_rel_path})"

    new_content, count = re.subn(pattern, replacement, content, flags=re.IGNORECASE)
    if count == 0:
        # Check if already has an image
        if "![" in content and "assets/" in content:
            print("README already has a screenshot — skipping update.")
            return True
        print("No TODO screenshot placeholder found in README.")
        return False

    with open(readme_path, "w", encoding="utf-8") as f:
        f.write(new_content)
    print(f"Updated {readme_path} ({count} placeholder(s) replaced)")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Capture screenshots and animated GIFs from forge-gpu lessons"
    )
    parser.add_argument(
        "lesson_dir",
        help="Path to the lesson directory (e.g. lessons/gpu/01-hello-window)",
    )
    parser.add_argument(
        "--capture-frame",
        type=int,
        default=5,
        help="Frame to start capturing (default: 5)",
    )
    parser.add_argument(
        "--no-update-readme",
        action="store_true",
        help="Skip updating the lesson README",
    )
    parser.add_argument(
        "--build", action="store_true", help="Build the lesson before capturing"
    )
    parser.add_argument(
        "--headless",
        action="store_true",
        help="Use lavapipe + Xvfb for headless capture (auto-detected when DISPLAY is unset)",
    )
    parser.add_argument(
        "--gif",
        action="store_true",
        help="Capture an animated GIF instead of a screenshot",
    )
    parser.add_argument(
        "--gif-frames",
        type=int,
        default=120,
        help="Number of frames to capture for GIF (default: 120)",
    )
    parser.add_argument(
        "--gif-fps",
        type=int,
        default=30,
        help="Playback frame rate for the GIF (default: 30)",
    )
    parser.add_argument(
        "--exe-args",
        nargs=argparse.REMAINDER,
        default=[],
        help="Extra arguments passed to the lesson executable (e.g. --display-mode 1)",
    )
    args = parser.parse_args()

    # Derive target name from lesson directory
    lesson_dir = args.lesson_dir.rstrip("/\\")
    target_name = os.path.basename(lesson_dir)

    # Derive a readable lesson name from the directory (e.g. "Lesson 01")
    match = re.match(r"(\d+)", target_name)
    lesson_name = f"Lesson {match.group(1)}" if match else target_name

    # Find or build the executable
    exe_path = find_executable(target_name, lesson_dir)
    if not exe_path or args.build:
        if not build_lesson(target_name):
            sys.exit(1)
        exe_path = find_executable(target_name, lesson_dir)

    if not exe_path:
        print(f"Could not find executable for {target_name}.")
        print("Try: cmake --build build --config Debug --target " + target_name)
        sys.exit(1)

    print(f"Using executable: {exe_path}")

    # Create assets directory
    assets_dir = os.path.join(lesson_dir, "assets")
    os.makedirs(assets_dir, exist_ok=True)

    if args.gif:
        # --- GIF capture mode ---
        frame_dir = os.path.join(assets_dir, "_gif_frames")
        if not capture_gif_frames(
            exe_path,
            frame_dir,
            args.gif_frames,
            args.capture_frame,
            args.headless,
            args.exe_args,
        ):
            sys.exit(1)

        gif_path = os.path.join(assets_dir, "animation.gif")
        if not assemble_gif(frame_dir, gif_path, args.gif_fps):
            sys.exit(1)

        # Remove the temp frame directory (ignore if non-empty)
        with contextlib.suppress(OSError):
            os.rmdir(frame_dir)

        print("Done! (GIF)")
    else:
        # --- Screenshot capture mode ---
        bmp_path = os.path.join(assets_dir, "_capture.bmp")
        if not capture_screenshot(
            exe_path, bmp_path, args.capture_frame, args.headless, args.exe_args
        ):
            sys.exit(1)

        # Convert BMP to PNG
        png_path = os.path.join(assets_dir, "screenshot.png")
        bmp_to_png(bmp_path, png_path)

        # Clean up temp BMP
        os.remove(bmp_path)
        image_rel_path = "assets/screenshot.png"

        # Update README
        if not args.no_update_readme:
            readme_path = os.path.join(lesson_dir, "README.md")
            update_readme(readme_path, image_rel_path, lesson_name)

        print("Done!")


if __name__ == "__main__":
    main()
