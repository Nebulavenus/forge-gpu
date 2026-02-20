---
name: add-screenshot
description: Capture a screenshot for a forge-gpu lesson and update its README
argument-hint: "[lesson-path]"
disable-model-invocation: false
---

Capture a screenshot (PNG) from a lesson executable and embed it in the
lesson's README.

## When to use

- After creating or updating a GPU lesson
- When a lesson README has a `<!-- TODO: screenshot -->` placeholder
- When the visual output of a lesson has changed

## How it works

Lessons include `capture/forge_capture.h` which adds a `--screenshot`
command-line flag. The Python orchestration script runs the lesson, converts
the BMP output to PNG, and updates the README.

## Key API calls

- **Configure build:** `cmake -B build -DFORGE_CAPTURE=ON`
- **Build target:** `cmake --build build --config Debug --target <target-name>`
- **Screenshot:** `python scripts/capture_lesson.py lessons/gpu/<lesson-dir>`
- **Start frame:** `--capture-frame N` (default: 5, increase if output is black)
- **Skip README update:** `--no-update-readme`
- **Force rebuild:** `--build`

## Code template

```bash
# 1. Configure with capture support
cmake -B build -DFORGE_CAPTURE=ON

# 2. Build the lesson
cmake --build build --config Debug --target <target-name>

# 3. Capture a static screenshot
python scripts/capture_lesson.py lessons/gpu/<lesson-dir>
```

## Steps

### 1. Identify the lesson

From the user's argument, resolve the lesson directory path. Accept any of:

- `01` or `01-hello-window` (resolved to `lessons/gpu/01-hello-window`)
- `lessons/gpu/01-hello-window` (used directly)

### 2. Build the lesson

```bash
cmake --build build --config Debug --target <target-name>
```

### 3. Run the capture script

```bash
python scripts/capture_lesson.py lessons/gpu/<lesson-dir>
```

### 4. Verify the output

- Check that `lessons/gpu/<lesson-dir>/assets/screenshot.png` exists
- Verify the README was updated (TODO placeholder replaced with image markdown)

### 5. Report to the user

Show the output file path and size. If the user wants to inspect the image,
tell them where to find it.

## Capture script options

| Flag | Default | Description |
|---|---|---|
| `--capture-frame N` | 5 | Which frame to start capturing |
| `--no-update-readme` | off | Skip README placeholder replacement |
| `--build` | auto | Force rebuild before capturing |

## Output locations

- Screenshots: `lessons/gpu/<name>/assets/screenshot.png`

## Common issues

- **Black image**: The `--capture-frame` default of 5 skips the first few
  frames so the GPU pipeline is warmed up. If you still get black, try
  `--capture-frame 10`.
- **Build not found**: Run `cmake --build build --config Debug` first or
  pass `--build` to the capture script.
- **Wrong colors**: The capture uses the swapchain's sRGB format. If the
  lesson does not set `SDR_LINEAR`, colors may look washed out.
