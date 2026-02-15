#!/usr/bin/env python
"""
setup.py — Verify forge-gpu development environment and configure the build.

Checks that all required tools are installed, reports versions, and optionally
runs the CMake configure step.

Usage:
    python scripts/setup.py            # check everything
    python scripts/setup.py --build    # check + configure + build
    python scripts/setup.py --fix      # attempt to install missing Python packages
"""

import argparse
import os
import shutil
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = os.path.join(REPO_ROOT, "build")

# ANSI colors (disabled on Windows without VT support)
try:
    os.system("")  # enable VT100 on Windows 10+
    GREEN = "\033[92m"
    YELLOW = "\033[93m"
    RED = "\033[91m"
    BOLD = "\033[1m"
    RESET = "\033[0m"
except Exception:
    GREEN = YELLOW = RED = BOLD = RESET = ""

PASS = f"{GREEN}OK{RESET}"
WARN = f"{YELLOW}WARN{RESET}"
FAIL = f"{RED}MISSING{RESET}"


def check_command(name, args):
    """Check if a command is available and optionally parse its version."""
    path = shutil.which(name)
    if path is None:
        return None, None, None

    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=10)
        output = (result.stdout + result.stderr).strip()
    except Exception:
        output = ""

    return path, output, True


def parse_version_line(output):
    """Extract a version-like string from command output."""
    import re

    match = re.search(r"(\d+\.\d+[\.\d]*)", output)
    return match.group(1) if match else output.split("\n")[0][:60]


def check_git():
    """Check git installation and repo status."""
    _, output, ok = check_command("git", ["git", "--version"])
    if not ok:
        print(f"  [{FAIL}] git — not found")
        print("         Install from https://git-scm.com/")
        return False

    version = parse_version_line(output)
    print(f"  [{PASS}] git {version}")

    # Check submodules
    submodule_dir = os.path.join(REPO_ROOT, "third_party", "SDL")
    if os.path.isdir(submodule_dir):
        entries = os.listdir(submodule_dir)
        if len(entries) > 0:
            print("         SDL submodule present (third_party/SDL)")
        else:
            print(
                f"  [{WARN}] SDL submodule directory empty — run: git submodule update --init"
            )
    return True


def check_cmake():
    """Check CMake installation."""
    _, output, ok = check_command("cmake", ["cmake", "--version"])
    if not ok:
        print(f"  [{FAIL}] CMake — not found")
        print("         Install from https://cmake.org/download/")
        print("         Required: 3.24+")
        return False

    version = parse_version_line(output)
    print(f"  [{PASS}] CMake {version}")

    # Check version requirement
    parts = version.split(".")
    if len(parts) >= 2:
        major, minor = int(parts[0]), int(parts[1])
        if major < 3 or (major == 3 and minor < 24):
            print(f"  [{WARN}] CMake 3.24+ required, found {version}")
            return False
    return True


def check_compiler():
    """Check for C compiler (MSVC or GCC/Clang)."""
    # Check for MSVC (cl.exe)
    cl_path = shutil.which("cl")
    if cl_path:
        try:
            result = subprocess.run(["cl"], capture_output=True, text=True, timeout=10)
            # cl prints version to stderr
            version_line = result.stderr.strip().split("\n")[0]
            print(f"  [{PASS}] MSVC — {version_line[:70]}")
            return True
        except Exception:
            pass

    # Check for GCC
    gcc_path = shutil.which("gcc")
    if gcc_path:
        _, output, ok = check_command("gcc", ["gcc", "--version"])
        if ok:
            version = parse_version_line(output)
            print(f"  [{PASS}] GCC {version}")
            return True

    # Check for Clang
    clang_path = shutil.which("clang")
    if clang_path:
        _, output, ok = check_command("clang", ["clang", "--version"])
        if ok:
            version = parse_version_line(output)
            print(f"  [{PASS}] Clang {version}")
            return True

    print(f"  [{FAIL}] C compiler — no cl, gcc, or clang found")
    print("         On Windows: install Visual Studio or Build Tools")
    print("         On Linux/macOS: install gcc or clang")
    return False


def check_python():
    """Check Python installation."""
    version = (
        f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}"
    )
    print(f"  [{PASS}] Python {version} ({sys.executable})")

    # Check for Pillow (needed by capture_lesson.py)
    try:
        import PIL

        pil_version = PIL.__version__
        print(f"  [{PASS}] Pillow {pil_version} (for screenshot/GIF capture)")
    except ImportError:
        print(f"  [{WARN}] Pillow — not installed (needed for screenshot/GIF capture)")
        print("         Install with: pip install Pillow")

    return True


def check_vulkan_sdk():
    """Check Vulkan SDK and dxc shader compiler."""
    vulkan_sdk = os.environ.get("VULKAN_SDK")

    if vulkan_sdk and os.path.isdir(vulkan_sdk):
        print(f"  [{PASS}] Vulkan SDK — {vulkan_sdk}")

        # Check for dxc in Vulkan SDK
        dxc_path = os.path.join(vulkan_sdk, "Bin", "dxc.exe")
        if not os.path.isfile(dxc_path):
            dxc_path = os.path.join(vulkan_sdk, "bin", "dxc")
        if os.path.isfile(dxc_path):
            print(f"  [{PASS}] dxc (Vulkan SDK) — {dxc_path}")
            return True
        else:
            print(f"  [{WARN}] dxc not found in Vulkan SDK")
    else:
        print(f"  [{WARN}] VULKAN_SDK not set or directory missing")
        print("         Download from https://vulkan.lunarg.com/sdk/home")

    # Check for dxc on PATH as fallback
    dxc_path = shutil.which("dxc")
    if dxc_path:
        print(f"  [{PASS}] dxc (PATH) — {dxc_path}")
        return True

    print(f"  [{WARN}] dxc — not found (needed for shader compilation)")
    print("         Install Vulkan SDK or add dxc to PATH")
    return False


def check_gpu():
    """Check for a Vulkan-capable GPU (best-effort)."""
    vulkaninfo = shutil.which("vulkaninfo")
    if vulkaninfo:
        try:
            result = subprocess.run(
                ["vulkaninfo", "--summary"], capture_output=True, text=True, timeout=10
            )
            if result.returncode == 0:
                # Try to find GPU name
                import re

                match = re.search(r"deviceName\s*=\s*(.+)", result.stdout)
                if match:
                    gpu_name = match.group(1).strip()
                    print(f"  [{PASS}] Vulkan GPU — {gpu_name}")
                else:
                    print(f"  [{PASS}] Vulkan — supported")
                return True
        except Exception:
            pass

    # Can't verify, but that's OK — SDL will detect at runtime
    print(f"  [{WARN}] vulkaninfo not found — cannot verify GPU support")
    print("         SDL will detect a compatible GPU at runtime (Vulkan/D3D12/Metal)")
    return True


def check_build_state():
    """Check if the project has been configured/built."""
    if os.path.isdir(BUILD_DIR):
        cache_file = os.path.join(BUILD_DIR, "CMakeCache.txt")
        if os.path.isfile(cache_file):
            print(f"  [{PASS}] Build directory configured (build/)")
            return True
        else:
            print(f"  [{WARN}] Build directory exists but not configured")
            return False
    else:
        print("  [    ] Build directory not created yet")
        print("         Run: cmake -B build")
        return False


def try_fix():
    """Attempt to install missing Python packages."""
    print(f"\n{BOLD}Attempting to install missing packages...{RESET}\n")

    packages = []
    try:
        import PIL  # noqa: F401
    except ImportError:
        packages.append("Pillow")

    if not packages:
        print("  Nothing to install — all Python packages present.")
        return True

    for pkg in packages:
        print(f"  Installing {pkg}...")
        result = subprocess.run(
            [sys.executable, "-m", "pip", "install", pkg],
            capture_output=True,
            text=True,
        )
        if result.returncode == 0:
            print(f"  [{PASS}] {pkg} installed")
        else:
            print(f"  [{FAIL}] Failed to install {pkg}")
            print(f"         {result.stderr.strip()[:200]}")
            return False
    return True


def configure_and_build():
    """Run CMake configure and build."""
    print(f"\n{BOLD}Configuring build...{RESET}\n")

    result = subprocess.run(["cmake", "-B", "build"], cwd=REPO_ROOT)
    if result.returncode != 0:
        print(f"\n  [{FAIL}] CMake configure failed")
        return False

    print(f"\n{BOLD}Building...{RESET}\n")

    result = subprocess.run(["cmake", "--build", "build"], cwd=REPO_ROOT)
    if result.returncode != 0:
        print(f"\n  [{FAIL}] Build failed")
        return False

    print(f"\n  [{PASS}] Build succeeded")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Verify forge-gpu development environment."
    )
    parser.add_argument(
        "--build", action="store_true", help="Configure and build after checks pass"
    )
    parser.add_argument(
        "--fix", action="store_true", help="Attempt to install missing Python packages"
    )
    args = parser.parse_args()

    print(f"\n{BOLD}forge-gpu environment check{RESET}")
    print(f"{'=' * 40}\n")

    all_ok = True
    warnings = []

    # Required tools
    print(f"{BOLD}Required:{RESET}")
    if not check_git():
        all_ok = False
    if not check_cmake():
        all_ok = False
    if not check_compiler():
        all_ok = False
    if not check_python():
        all_ok = False

    # Shader tools
    print(f"\n{BOLD}Shader compilation:{RESET}")
    if not check_vulkan_sdk():
        warnings.append("Shader compilation (dxc) not available")

    # GPU
    print(f"\n{BOLD}GPU:{RESET}")
    check_gpu()

    # Build state
    print(f"\n{BOLD}Build:{RESET}")
    build_ready = check_build_state()

    # Summary
    print(f"\n{'=' * 40}")
    if all_ok and not warnings:
        print(f"{GREEN}All checks passed!{RESET}")
    elif all_ok:
        print(f"{YELLOW}Checks passed with warnings:{RESET}")
        for w in warnings:
            print(f"  - {w}")
    else:
        print(f"{RED}Some required tools are missing.{RESET}")
        print("Install them and run this script again.")

    if not build_ready and all_ok:
        print("\nTo configure and build:")
        print("  cmake -B build && cmake --build build")
        print("Or run: python scripts/setup.py --build")

    # Optional actions
    if args.fix:
        try_fix()

    if args.build and all_ok:
        configure_and_build()

    print()
    return 0 if all_ok else 1


if __name__ == "__main__":
    sys.exit(main())
