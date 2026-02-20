#!/usr/bin/env python
"""
capture_lesson.py — Capture screenshots from forge-gpu lessons.

Runs a lesson executable with capture flags, converts the resulting BMP
file to PNG using Pillow, and updates the lesson README.

Usage:
    python scripts/capture_lesson.py <lesson-dir> [options]

Examples:
    python scripts/capture_lesson.py lessons/gpu/01-hello-window
    python scripts/capture_lesson.py lessons/gpu/02-first-triangle --no-update-readme

Options:
    --capture-frame N    Frame to start capturing (default: 5)
    --no-update-readme   Skip updating the lesson README
    --build              Build the lesson before capturing (default: auto-detect)
"""

import argparse
import os
import re
import shutil
import subprocess
import sys


def find_executable(target_name):
    """Find the lesson executable in the build directory."""
    # Common build output locations for CMake + MSVC
    candidates = [
        os.path.join(
            "build", "lessons", "gpu", target_name, "Debug", f"{target_name}.exe"
        ),
        os.path.join(
            "build", "lessons", "gpu", target_name, "Release", f"{target_name}.exe"
        ),
        os.path.join("build", "lessons", "gpu", target_name, target_name),
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
    result = subprocess.run(
        [cmake_path, "-B", "build", "-DFORGE_CAPTURE=ON"],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"Configure failed:\n{result.stderr}")
        return False

    print(f"Building {target_name}...")
    result = subprocess.run(
        [cmake_path, "--build", "build", "--config", "Debug", "--target", target_name],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print(f"Build failed:\n{result.stderr}")
        return False
    print("Build succeeded.")
    return True


def capture_screenshot(exe_path, output_bmp, capture_frame):
    """Run the lesson and capture a single frame."""
    exe_path = os.path.abspath(exe_path)
    print(f"Capturing screenshot (frame {capture_frame})...")
    result = subprocess.run(
        [exe_path, "--screenshot", output_bmp, "--capture-frame", str(capture_frame)],
        capture_output=True,
        text=True,
        timeout=30,
    )
    if result.returncode != 0:
        print(f"Capture failed (exit code {result.returncode})")
        if result.stderr:
            print(result.stderr)
        return False
    return os.path.isfile(output_bmp)


def bmp_to_png(bmp_path, png_path):
    """Convert a BMP file to optimized PNG using Pillow."""
    from PIL import Image

    img = Image.open(bmp_path)
    # Drop alpha channel if fully opaque (smaller PNG)
    if img.mode == "RGBA":
        alpha = img.getchannel("A")
        if alpha.getextrema() == (255, 255):
            img = img.convert("RGB")
    img.save(png_path, optimize=True)
    size_kb = os.path.getsize(png_path) / 1024
    print(f"Saved {png_path} ({size_kb:.1f} KB)")


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
        description="Capture screenshots from forge-gpu lessons"
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
    args = parser.parse_args()

    # Derive target name from lesson directory
    lesson_dir = args.lesson_dir.rstrip("/\\")
    target_name = os.path.basename(lesson_dir)

    # Derive a readable lesson name from the directory (e.g. "Lesson 01")
    match = re.match(r"(\d+)", target_name)
    lesson_name = f"Lesson {match.group(1)}" if match else target_name

    # Find or build the executable
    exe_path = find_executable(target_name)
    if not exe_path or args.build:
        if not build_lesson(target_name):
            sys.exit(1)
        exe_path = find_executable(target_name)

    if not exe_path:
        print(f"Could not find executable for {target_name}.")
        print("Try: cmake --build build --config Debug --target " + target_name)
        sys.exit(1)

    print(f"Using executable: {exe_path}")

    # Create assets directory
    assets_dir = os.path.join(lesson_dir, "assets")
    os.makedirs(assets_dir, exist_ok=True)

    # Capture single frame
    bmp_path = os.path.join(assets_dir, "_capture.bmp")
    if not capture_screenshot(exe_path, bmp_path, args.capture_frame):
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
