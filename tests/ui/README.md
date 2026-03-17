# UI Library Tests

Automated tests for `common/ui/forge_ui.h`.

Multiple test binaries cover the UI library's layers: TTF parsing, glyph
rasterization, font atlas construction, text layout, immediate-mode widget
behavior, context isolation, state isolation between widget IDs, theme contrast
ratios, window management, and text input. A fuzz driver exercises text input
robustness under arbitrary byte sequences.

## Files

| File | Description |
|------|-------------|
| `test_ui.c` | Core tests: TTF parsing, hmtx metrics, cmap lookups, atlas packing, text layout, BMP writing |
| `test_ui_ctx.c` | Context lifecycle and multi-context isolation tests |
| `test_ui_state_isolation.c` | Widget state isolation — each widget ID gets independent state |
| `test_ui_theme_contrast.c` | Theme color contrast ratio verification (WCAG thresholds) |
| `test_ui_window.c` | Window creation, dragging, stacking, and close behavior |
| `fuzz_text_input.c` | libFuzzer driver for text input processing |
| `CMakeLists.txt` | Build target definitions |

## Running

```bash
# Build and run all UI suites
cmake --build build --target test_ui
ctest --test-dir build -R ui --output-on-failure

# Run all C tests
cmake --build build
ctest --test-dir build
```

## Exit codes

- **0** — All tests passed
- **1** — One or more tests failed
