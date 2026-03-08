# Scripts

Helper scripts for building, running, and maintaining forge-gpu lessons.

## Scripts

| Script | Purpose |
|--------|---------|
| `run.py` | Run a lesson by number or name (`python scripts/run.py 01`) |
| `setup.py` | First-time setup — clone SDL, configure CMake, build |
| `compile_shaders.py` | Compile HLSL shaders to SPIR-V + DXIL with embedded C headers |
| `capture_lesson.py` | Capture screenshots and GIFs from running lessons |
| `bin_to_header.py` | Convert binary files to C byte-array headers |
| `equirect_to_cubemap.py` | Convert equirectangular HDR images to cube map faces |
| `check_math_blocks.py` | Validate math code blocks in lesson READMEs |
| `cloud-setup.sh` | Set up a headless Linux environment (Lavapipe + Mesa) |

## Subdirectories

| Directory | Purpose |
|-----------|---------|
| `forge_diagrams/` | Matplotlib diagram generator — see [forge_diagrams/README.md](forge_diagrams/README.md) |
