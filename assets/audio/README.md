# Audio Assets

WAV files for audio lessons. These files are **not checked into the
repository** — they are gitignored due to copyright and file size.

## Setup

Place WAV files in this directory. The audio lessons load them at runtime
using relative paths like `assets/audio/<filename>.wav`, resolved from
the working directory (the repository root).

## Required files

Audio lessons reference specific filenames. Each lesson's README lists
which files it needs. Any WAV file will work as a substitute — the
loader converts to F32 stereo 44100 Hz at runtime.

## Recommended sources

If you need royalty-free audio files:

- [freesound.org](https://freesound.org) — Creative Commons samples
- [opengameart.org](https://opengameart.org) — game-oriented CC audio
- [Sonniss](https://sonniss.com) — commercial sound effect bundles
