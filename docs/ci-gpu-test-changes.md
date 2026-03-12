# CI Changes Needed for GPU Tests (Lavapipe)

These changes enable GPU-integration tests on GitHub Actions using Mesa's
Lavapipe software Vulkan driver. Copy the relevant sections into
`.github/workflows/build-and-test.yml`.

## 1. Add mesa-vulkan-drivers and xvfb to the install step

In the Ubuntu job's package install step, add `mesa-vulkan-drivers` and `xvfb`:

```yaml
- name: Install dependencies
  run: |
    sudo apt-get update
    sudo apt-get install -y cmake ninja-build mesa-vulkan-drivers xvfb
```

`mesa-vulkan-drivers` provides Lavapipe (software Vulkan ICD).
`xvfb` provides a virtual framebuffer — SDL needs a display to create a window.

## 2. Run CTest under xvfb-run

Change the test execution step to use `xvfb-run`:

```yaml
- name: Run tests
  run: xvfb-run ctest --test-dir build --output-on-failure
```

This creates a virtual X display so SDL can create GPU windows headless.

## 3. Set environment variables (optional but recommended)

Force SDL to use Vulkan (Lavapipe is the only ICD installed, so it is
selected automatically):

```yaml
- name: Run tests
  env:
    SDL_GPU_DRIVER: vulkan
  run: xvfb-run ctest --test-dir build --output-on-failure
```

## Notes

- Lavapipe is a CPU-based Vulkan implementation — no GPU hardware needed
- GPU-integration tests in `test_scene` gracefully skip if no Vulkan device
  is available, so existing CI won't break even without these changes
- The non-GPU tests (config defaults, math, struct sizes) always run regardless
