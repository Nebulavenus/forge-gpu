#!/bin/bash
# forge-gpu — Claude Code on the web setup script
# Runs once when a new session starts (skipped on resume).
# Paste this into the "Setup script" field when adding an environment.
#
# What this does:
#   1. Verifies CMake 3.24+ and a C compiler are available
#   2. Installs Python packages needed by helper scripts
#   3. Installs GitHub CLI (gh) for issue/PR management
#   4. Downloads pre-built SDL3 (falls back to FetchContent if unavailable)
#   5. Configures the CMake build
#   6. Builds all math and engine lessons (headless — no GPU or display needed)
#   7. Runs the test suite to confirm everything works
#
# Network: "Trusted" is sufficient.
#   - github.com    → pre-built SDL3 release, gh CLI, FetchContent fallback
#   - pypi.org      → pip installs Python packages
#   - ubuntu repos  → apt if anything is missing
set -euo pipefail
echo "=== forge-gpu cloud environment setup ==="

# ── 1. Verify toolchain ────────────────────────────────────────────────────
echo ""
echo "--- Checking toolchain ---"

# CMake
if ! command -v cmake &>/dev/null; then
    echo "ERROR: cmake not found. The universal image should include it."
    echo "Attempting install via apt..."
    sudo apt-get update -qq && sudo apt-get install -y -qq cmake
fi
CMAKE_VERSION=$(cmake --version | head -1 | sed 's/[^0-9]*\([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/')
echo "CMake: $CMAKE_VERSION"

# C compiler — prefer gcc, fall back to clang
if command -v gcc &>/dev/null; then
    echo "C compiler: $(gcc --version | head -1)"
elif command -v clang &>/dev/null; then
    echo "C compiler: $(clang --version | head -1)"
else
    echo "ERROR: No C compiler found. Installing gcc..."
    sudo apt-get update -qq && sudo apt-get install -y -qq gcc
fi

# Python
echo "Python: $(python3 --version 2>&1)"

# ── 2. Python dependencies ─────────────────────────────────────────────────
echo ""
echo "--- Installing Python packages ---"
pip install --quiet --break-system-packages Pillow numpy matplotlib 2>/dev/null \
    || pip install --quiet Pillow numpy matplotlib
echo "Python packages installed."

# ── 3. GitHub CLI ───────────────────────────────────────────────────────────
echo ""
echo "--- Installing GitHub CLI ---"
GH_VERSION="2.74.0"
if ! command -v gh &>/dev/null; then
    curl -sSL https://github.com/cli/cli/releases/download/v2.65.0/gh_2.65.0_linux_amd64.tar.gz -o /tmp/gh.tar.gz
    tar -xzf /tmp/gh.tar.gz -C /tmp
    sudo cp /tmp/gh_2.65.0_linux_amd64/bin/gh /usr/local/bin/gh
    rm -rf /tmp/gh.tar.gz /tmp/gh_2.65.0_linux_amd64
fi
echo "gh: $(gh --version | head -1)"
if [ -n "${GH_TOKEN:-}" ]; then
    echo "GH_TOKEN is set — gh auth ready."
else
    echo "WARNING: GH_TOKEN not set. gh commands will not authenticate."
fi

# ── 4. Pre-built SDL3 ──────────────────────────────────────────────────────
# Download pre-built SDL3 to avoid compiling from source every session.
# This saves ~2-3 minutes and avoids parallel-build corruption issues.
# If the download fails, FetchContent will build SDL3 from source during
# the CMake configure step (step 5).
SDL3_PREFIX="/opt/sdl3"
if [ ! -d "$SDL3_PREFIX/lib" ]; then
    echo ""
    echo "--- Downloading pre-built SDL3 ---"
    SDL3_URL="https://github.com/RosyGameStudio/forge-gpu/releases/download/sdl3-prebuilt/sdl3-linux-amd64.tar.gz"
    if curl -fsSL "$SDL3_URL" -o /tmp/sdl3.tar.gz 2>/dev/null; then
        sudo mkdir -p "$SDL3_PREFIX"
        sudo tar -xzf /tmp/sdl3.tar.gz -C "$SDL3_PREFIX"
        rm /tmp/sdl3.tar.gz
        # Verify the expected layout; retry with --strip-components=1 if the
        # archive has a top-level directory wrapping the files.
        if [ ! -d "$SDL3_PREFIX/lib" ]; then
            echo "SDL3 layout unexpected — retrying with --strip-components=1"
            sudo rm -rf "$SDL3_PREFIX"
            sudo mkdir -p "$SDL3_PREFIX"
            curl -fsSL "$SDL3_URL" -o /tmp/sdl3.tar.gz
            sudo tar -xzf /tmp/sdl3.tar.gz -C "$SDL3_PREFIX" --strip-components=1
            rm /tmp/sdl3.tar.gz
        fi
        if [ ! -d "$SDL3_PREFIX/lib" ]; then
            echo "ERROR: SDL3 extraction failed — $SDL3_PREFIX/lib not found"
            echo "FetchContent will build SDL3 from source during configure."
        else
            echo "SDL3 installed to $SDL3_PREFIX"
        fi
    else
        echo "WARNING: Pre-built SDL3 download failed."
        echo "FetchContent will build SDL3 from source during configure."
        echo "Installing SDL3 build dependencies..."
        sudo apt-get update -qq && sudo apt-get install -y -qq \
            libx11-dev libxcursor-dev libxi-dev libxrandr-dev libxss-dev \
            libxext-dev libxfixes-dev libxtst-dev libwayland-dev \
            libxkbcommon-dev libegl-dev libdrm-dev libgbm-dev libdecor-0-dev
    fi
else
    echo "SDL3 already present at $SDL3_PREFIX"
fi

# ── 5. CMake configure ─────────────────────────────────────────────────────
echo ""
echo "--- Configuring CMake build ---"
CMAKE_ARGS=("-DCMAKE_BUILD_TYPE=Debug")
if [ -d "$SDL3_PREFIX/lib" ]; then
    CMAKE_ARGS+=("-DCMAKE_PREFIX_PATH=$SDL3_PREFIX")
fi
cmake -B build "${CMAKE_ARGS[@]}"
echo "CMake configure complete."

# ── 6. Build math + engine lessons ─────────────────────────────────────────
# These all call SDL_Init(0) — no window, no GPU, no display server needed.
# GPU lessons will also compile (no display needed at compile time) but we
# focus on the headless targets so the build is fast.
echo ""
echo "--- Building math lessons ---"
MATH_TARGETS=(
    01-vectors
    02-coordinate-spaces
    03-bilinear-interpolation
    04-mipmaps-and-lod
    05-matrices
    06-projections
    07-floating-point
    08-orientation
    09-view-matrix
    10-anisotropy
    11-color-spaces
)
for target in "${MATH_TARGETS[@]}"; do
    echo "  Building math/$target..."
    cmake --build build --target "$target" -j"$(nproc)" --quiet 2>&1 || {
        echo "  WARNING: Failed to build $target (non-fatal, continuing)"
    }
done

echo ""
echo "--- Building engine lessons ---"
ENGINE_TARGETS=(
    01-intro-to-c
    02-cmake-fundamentals
)
for target in "${ENGINE_TARGETS[@]}"; do
    echo "  Building engine/$target..."
    cmake --build build --target "$target" -j"$(nproc)" --quiet 2>&1 || {
        echo "  WARNING: Failed to build $target (non-fatal, continuing)"
    }
done

# ── 7. Build and run tests ─────────────────────────────────────────────────
echo ""
echo "--- Building and running tests ---"
cmake --build build --target test_math -j"$(nproc)" --quiet 2>&1 || true
cmake --build build --target test_obj -j"$(nproc)" --quiet 2>&1 || true
cmake --build build --target test_gltf -j"$(nproc)" --quiet 2>&1 || true
ctest --test-dir build -C Debug --output-on-failure 2>&1 || {
    echo "WARNING: Some tests failed (non-fatal for setup)"
}

# ── 8. Summary ─────────────────────────────────────────────────────────────
echo ""
echo "=== Setup complete ==="
echo ""
echo "Ready to work on math and engine lessons."
echo "  Build a lesson:   cmake --build build --target <name>"
echo "  Run a lesson:     python3 scripts/run.py math/01"
echo "  Run tests:        cd build && ctest -C Debug --output-on-failure"
echo "  Compile shaders:  Not available (no Vulkan SDK in cloud)"
echo ""
echo "NOTE: GPU lessons (lessons/gpu/) will compile but cannot run"
echo "without a display server. Math and engine lessons run headless."
