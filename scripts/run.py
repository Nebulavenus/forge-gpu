#!/usr/bin/env python
"""
run.py — Run a forge-gpu lesson by name or number.

Usage:
    python scripts/run.py 02                     # GPU lesson 02
    python scripts/run.py first-triangle          # by name fragment
    python scripts/run.py math/01                 # math lesson 01
    python scripts/run.py gpu/03                  # explicit GPU lesson 03
    python scripts/run.py engine/01               # engine lesson 01
    python scripts/run.py                         # list available lessons

Extra arguments after the lesson name are forwarded to the executable:
    python scripts/run.py 03 --screenshot out.bmp
"""

import os
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LESSONS_DIR = os.path.join(REPO_ROOT, "lessons")
BUILD_DIR = os.path.join(REPO_ROOT, "build")

# Build configurations to search, in priority order
BUILD_CONFIGS = ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"]


def discover_lessons():
    """Find all lesson directories, returning list of (type, dirname) tuples."""
    lessons = []
    for lesson_type in ["gpu", "math", "engine"]:
        type_dir = os.path.join(LESSONS_DIR, lesson_type)
        if not os.path.isdir(type_dir):
            continue
        for entry in sorted(os.listdir(type_dir)):
            full = os.path.join(type_dir, entry)
            if os.path.isdir(full) and entry[0].isdigit():
                lessons.append((lesson_type, entry))
    return lessons


def find_executable(lesson_type, dirname):
    """Locate the built executable for a lesson."""
    base = os.path.join(BUILD_DIR, "lessons", lesson_type, dirname)

    # Multi-config generators (MSVC, Xcode): look in Debug/, Release/, etc.
    for config in BUILD_CONFIGS:
        for ext in [".exe", ""]:
            exe = os.path.join(base, config, dirname + ext)
            if os.path.isfile(exe):
                return exe

    # Single-config generators (Ninja, Make): exe is directly in the folder
    for ext in [".exe", ""]:
        exe = os.path.join(base, dirname + ext)
        if os.path.isfile(exe):
            return exe

    return None


def match_lesson(query, lessons):
    """Match a user query to a lesson. Returns (type, dirname) or None."""
    query = query.strip().lower().replace("\\", "/")

    # Check for explicit type prefix: "gpu/02" or "math/01"
    forced_type = None
    if "/" in query:
        parts = query.split("/", 1)
        if parts[0] in ("gpu", "math", "engine"):
            forced_type = parts[0]
            query = parts[1]

    candidates = lessons
    if forced_type:
        candidates = [(t, d) for t, d in candidates if t == forced_type]

    # Exact number match: "02" matches "02-first-triangle"
    for lesson_type, dirname in candidates:
        num = dirname.split("-", 1)[0]
        if num == query.zfill(2):
            return (lesson_type, dirname)

    # Substring match on the name part: "triangle" matches "02-first-triangle"
    matches = []
    for lesson_type, dirname in candidates:
        if query in dirname.lower():
            matches.append((lesson_type, dirname))

    if len(matches) == 1:
        return matches[0]
    if len(matches) > 1:
        print(f"Ambiguous query '{query}' matches multiple lessons:")
        for t, d in matches:
            print(f"  {t}/{d}")
        print("Be more specific, e.g.: gpu/02, math/01, or engine/01")
        return None

    return None


def list_lessons(lessons):
    """Print all available lessons with their build status."""
    print("Available lessons:\n")

    current_type = None
    for lesson_type, dirname in lessons:
        if lesson_type != current_type:
            current_type = lesson_type
            print(f"  {lesson_type}/")

        exe = find_executable(lesson_type, dirname)
        status = "  (built)" if exe else "  (not built)"
        print(f"    {dirname}{status}")

    print("\nUsage: python scripts/run.py <name-or-number> [args...]")
    print("  e.g.: python scripts/run.py 02")
    print("        python scripts/run.py first-triangle")
    print("        python scripts/run.py math/01")
    print("        python scripts/run.py engine/01")


def main():
    lessons = discover_lessons()

    if len(sys.argv) < 2:
        list_lessons(lessons)
        return 0

    query = sys.argv[1]
    extra_args = sys.argv[2:]

    result = match_lesson(query, lessons)
    if result is None:
        print(f"No lesson matching '{query}'.")
        print("Run without arguments to see available lessons.")
        return 1

    lesson_type, dirname = result
    exe = find_executable(lesson_type, dirname)
    if exe is None:
        print(f"Lesson {lesson_type}/{dirname} found but not built.")
        print("Build it first:")
        print(f"  cmake --build build --target {dirname}")
        return 1

    print(f"Running {lesson_type}/{dirname}...")
    proc = subprocess.run([exe] + extra_args)
    return proc.returncode


if __name__ == "__main__":
    sys.exit(main())
