# Building forge-gpu

This guide covers everything you need to build and run the lessons. If
you're new to CMake or C compilers, the
[engine track](../lessons/engine/) walks through these tools in detail —
especially [Lesson 02 (CMake Fundamentals)](../lessons/engine/02-cmake-fundamentals/)
and [Lesson 06 (Reading Error Messages)](../lessons/engine/06-reading-error-messages/).

## Prerequisites

- **CMake 3.24+**
- **Ninja** — the build system used by CI and recommended for local builds
- **A C compiler** — MSVC on Windows, GCC or Clang on Linux/macOS
- **A GPU** with Vulkan, Direct3D 12, or Metal support
- **Python 3.10+** — for helper scripts and the asset pipeline
- **Git LFS** — binary assets (`.bin`, `.glb`, `.gif`) are stored with
  [Git Large File Storage](https://git-lfs.com)

SDL3 is fetched automatically via CMake's FetchContent — no manual
installation required.

### Installing Git LFS

Most Git installations include LFS, but it needs to be initialized once:

```bash
git lfs install
```

If the `git lfs` command is not found, install it first:

**Windows:**

```bash
winget install GitHub.GitLFS
```

**Linux:**

```bash
sudo apt install git-lfs    # Debian / Ubuntu
sudo dnf install git-lfs    # Fedora
```

**macOS:**

```bash
brew install git-lfs
```

After cloning the repo, run `git lfs pull` to download the binary assets.
Without this, lessons that load 3D models will fail at runtime because the
`.bin` files will be LFS pointer files instead of actual data.

### Installing Python

**Windows:** Download from [python.org](https://www.python.org/downloads/).
Check "Add python.exe to PATH" during installation. Alternatively:

```bash
winget install Python.Python.3.12
```

**Linux:**

```bash
sudo apt install python3 python3-pip    # Debian / Ubuntu
sudo dnf install python3 python3-pip    # Fedora
```

**macOS:** Python 3 is included with Xcode Command Line Tools. Or:

```bash
brew install python
```

### Installing Ninja

**Windows:** Visual Studio 2022+ includes Ninja. If you open a
"Developer Command Prompt" or "Developer PowerShell", Ninja is already on
your PATH. You can also install it standalone:

```bash
winget install Ninja-build.Ninja
```

**Linux:**

```bash
sudo apt install ninja-build    # Debian / Ubuntu
sudo dnf install ninja-build    # Fedora
```

**macOS:**

```bash
brew install ninja
```

### Verify your environment

The setup script checks all required tools and reports anything missing:

```bash
python scripts/setup.py
```

Use `--fix` to install missing Python packages, or `--build` to configure
and build in one step:

```bash
python scripts/setup.py --fix
python scripts/setup.py --build
```

## Building

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

The first build takes longer because CMake downloads and builds SDL3. Subsequent
builds are incremental.

Using Ninja (instead of the Visual Studio generator on Windows or Make on
Linux) produces a `compile_commands.json` in the build directory, which
gives editors and language servers (clangd, VS Code, etc.) full knowledge
of include paths and compiler flags.

### Common issues

**"CMake not found"** — Install from [cmake.org](https://cmake.org/download/)
or via your package manager. On Windows, make sure "Add CMake to PATH" is
checked during installation.

**"No C compiler found"** — On Windows, install
[Visual Studio](https://visualstudio.microsoft.com/) (the Community edition
is free) with the "Desktop development with C++" workload. On Linux,
`sudo apt install build-essential` (Debian/Ubuntu) or equivalent. On macOS,
`xcode-select --install`.

**"Ninja not found"** — See the [Installing Ninja](#installing-ninja) section
above. On Windows, make sure you're in a Developer Command Prompt (which adds
Ninja to PATH), or install it via `winget`.

**"Generator not found" or build errors on Windows** — If Ninja is not
available, CMake can fall back to the Visual Studio generator:

```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Debug
```

Note: the Visual Studio generator is multi-config, so you pass `--config Debug`
at build time instead of `-DCMAKE_BUILD_TYPE=Debug` at configure time. It does
not produce `compile_commands.json`.

**FetchContent download fails** — Check your internet connection. If you're
behind a proxy, configure git: `git config --global http.proxy http://proxy:port`.

For a deeper understanding of what CMake is doing and how to read build errors,
see [Engine Lesson 02](../lessons/engine/02-cmake-fundamentals/) and
[Engine Lesson 06](../lessons/engine/06-reading-error-messages/).

## Running lessons

The easiest way to run a lesson is with the run script:

```bash
python scripts/run.py 01                  # by number
python scripts/run.py first-triangle      # by name
python scripts/run.py math/01             # math lesson
python scripts/run.py                     # list all lessons
```

You can also run executables directly:

```bash
# Ninja (single-config) — executable is directly in the lesson folder
build/lessons/gpu/01-hello-window/01-hello-window

# Visual Studio (multi-config) — executable is in a Debug/ subfolder
build\lessons\gpu\01-hello-window\Debug\01-hello-window.exe
```

## Shader compilation

Pre-compiled shader bytecodes are checked in, so you **don't need any extra
tools just to build and run the lessons**. If you want to modify the HLSL
shader source, you'll need:

- **[Vulkan SDK](https://vulkan.lunarg.com/)** — provides `dxc` with SPIR-V
  support (the Windows SDK `dxc` can only compile DXIL, not SPIR-V)

After installing, make sure the `VULKAN_SDK` environment variable is set
(the installer does this automatically). On Windows the default location is:

```text
C:\VulkanSDK\<version>\Bin\dxc.exe
```

> **Heads up:** If you just type `dxc` and get *"SPIR-V CodeGen not
> available"*, you're hitting the Windows SDK `dxc` instead of the Vulkan
> SDK one. Use the full path to the Vulkan SDK `dxc` or put its `Bin/`
> directory earlier on your PATH.

The shader compilation script handles everything — finds `dxc`, compiles
each HLSL file to both SPIR-V and DXIL, and generates C byte-array headers:

```bash
python scripts/compile_shaders.py            # all lessons
python scripts/compile_shaders.py 02         # just lesson 02
python scripts/compile_shaders.py -v         # verbose (show dxc commands)
```

## SDL source (optional)

You can browse the SDL headers and GPU backend code locally by initializing
the submodule. This is for reference only — the build uses FetchContent:

```bash
git submodule update --init
```

## Testing

Run all C tests:

```bash
ctest --test-dir build --output-on-failure
```

Run a specific test suite:

```bash
cmake --build build --target test_math
ctest --test-dir build -R math
```

Run Python pipeline tests:

```bash
pip install -e ".[dev]"
pytest tests/pipeline/ -v
```
