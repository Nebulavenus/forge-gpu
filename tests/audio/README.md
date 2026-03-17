# Audio Library Tests

Automated tests for `common/audio/forge_audio.h`.

Tests use synthetic sample data — no WAV files required. Covers buffer and
source operations, volume scaling, stereo panning, cursor advancement, looping,
end-of-buffer stop, additive mixing, playback progress reporting, and reset.

## Files

| File | Description |
|------|-------------|
| `test_audio.c` | All audio library tests |
| `CMakeLists.txt` | Build target definition |

## Running

```bash
# Build and run this suite
cmake --build build --target test_audio
ctest --test-dir build -R audio --output-on-failure

# Run all C tests
cmake --build build
ctest --test-dir build
```

## Exit codes

- **0** — All tests passed
- **1** — One or more tests failed
