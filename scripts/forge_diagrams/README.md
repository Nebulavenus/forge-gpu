# forge_diagrams

Matplotlib diagram generator for forge-gpu lessons. Produces dark-themed PNG
diagrams that illustrate GPU pipelines, math concepts, UI layouts, and more.

## Usage

```bash
python scripts/forge_diagrams --lesson gpu/04     # generate diagrams for one lesson
python scripts/forge_diagrams --lesson math/01    # math lesson
python scripts/forge_diagrams --all               # regenerate all diagrams
python scripts/forge_diagrams --list              # list all registered diagrams
```

Output PNGs are saved to each lesson's `assets/` directory at 200 DPI.

## Structure

Diagram functions are organized by track, one file per lesson:

```text
forge_diagrams/
├── __main__.py        CLI entry point, diagram registry (DIAGRAMS dict)
├── __init__.py        Package marker
├── _common.py         Shared theme (STYLE dict), helpers (setup_axes, save, etc.)
├── gpu/               GPU lesson diagrams (one file per lesson, as needed)
│   ├── __init__.py    Re-exports all GPU diagram functions
│   ├── lesson_03.py   Lesson 03 diagrams
│   ├── lesson_04.py   Lesson 04 diagrams
│   └── ...
├── math/              Math lesson diagrams
├── ui/                UI lesson diagrams
├── engine/            Engine lesson diagrams
├── audio/             Audio lesson diagrams
├── assets/            Asset pipeline lesson diagrams
└── physics/           Physics lesson diagrams
```

## Adding a new diagram

1. **Create or edit** `<track>/lesson_NN.py` — write a `diagram_your_name()`
   function using helpers from `_common.py`
2. **Re-export** from `<track>/__init__.py` — add to `__all__` and imports
3. **Register** in `__main__.py` — import the function and add a
   `(filename, function)` entry to the `DIAGRAMS` dict under the lesson key
4. **Generate** with `python scripts/forge_diagrams --lesson <track>/NN`

Use the `/dev-create-diagram` skill to automate this process.

## Theme

All diagrams use a shared dark theme defined in `_common.py`. Colors come from
the `STYLE` dictionary — never use hardcoded color values. Key colors:

| Key | Hex | Use |
|-----|-----|-----|
| `bg` | `#1a1a2e` | Figure and axes background |
| `text` | `#e0e0f0` | Primary labels |
| `text_dim` | `#8888aa` | Secondary annotations |
| `accent1` | `#4fc3f7` | Primary subject |
| `accent2` | `#81c784` | Secondary / comparison |
| `accent3` | `#ffb74d` | Tertiary / reference |
| `accent4` | `#f06292` | Special highlights |
| `warn` | `#ff8a65` | Warnings, sync points |

All text must use `path_effects=[pe.withStroke(...)]` for readability against
dark backgrounds.
