"""Lint KaTeX math blocks in markdown files.

Checks:
1. Display math ($$) must use multi-line syntax — GitHub renders single-line
   $$...$$ as inline math, not block math.
2. No \\_ inside \\text{} — KaTeX rejects underscores in text mode with
   "'_' allowed only in math mode". Use \\text{a}\\_\\text{b} instead.
3. No \\; (LaTeX thin space) in math — GitHub renders it as a literal
   semicolon. Remove it; a comma alone provides sufficient separation.
"""

import re
import sys
from pathlib import Path

# Match lines where $$ opens AND closes on the same line with content between
SINGLE_LINE_DISPLAY = re.compile(r"^\$\$\s*\S.*\$\$\s*$")

# Match \text{...\_...} — underscore (escaped or bare) inside \text{}
TEXT_WITH_UNDERSCORE = re.compile(r"\\text\{[^}]*\\_[^}]*\}")

# Match \; inside math context (inline $...$ or display $$...$$)
LATEX_THIN_SPACE = re.compile(r"\\;")

IGNORE_DIRS = {"node_modules", "build", "third_party", ".git"}


def find_markdown_files(root: Path) -> list[Path]:
    """Find all .md files, skipping ignored directories."""
    results = []
    for path in root.rglob("*.md"):
        if any(part in IGNORE_DIRS for part in path.parts):
            continue
        results.append(path)
    return sorted(results)


def _line_has_math(line: str, in_display: bool) -> bool:
    """Return True if the line contains math (inside $$ block or inline $)."""
    if in_display:
        return True
    # Inline math: at least one $..$ span (not $$)
    # Simple heuristic: odd number of $ on the line means unclosed, but
    # any pair of $ with content between them counts as math.
    stripped = line.replace("$$", "")  # ignore display delimiters
    parts = stripped.split("$")
    # Parts at odd indices (1, 3, ...) are inside inline math
    return len(parts) >= 3


def check_file(path: Path) -> list[tuple[int, str, str]]:
    """Return list of (line_number, line_text, rule) for violations."""
    errors = []
    in_display = False
    for i, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if SINGLE_LINE_DISPLAY.match(line):
            errors.append((i, line, "single-line-display"))
        if TEXT_WITH_UNDERSCORE.search(line):
            errors.append((i, line, "text-underscore"))

        # Track display math blocks ($$-only lines toggle state)
        stripped = line.strip()
        if stripped == "$$":
            in_display = not in_display
            continue

        # Check for \; in math context
        if LATEX_THIN_SPACE.search(line) and _line_has_math(line, in_display):
            errors.append((i, line, "latex-thin-space"))

    return errors


MESSAGES = {
    "single-line-display": "single-line $$ block",
    "text-underscore": "\\_ inside \\text{} (use \\text{a}\\_\\text{b})",
    "latex-thin-space": "\\; in math (GitHub renders as literal semicolon)",
}

HINTS = {
    "single-line-display": ("Split $$...$$ onto three lines:\n  $$\n  <formula>\n  $$"),
    "text-underscore": (
        "Break out of \\text{} for underscores:\n"
        "  Bad:  \\text{num\\_levels}\n"
        "  Good: \\text{num}\\_\\text{levels}"
    ),
    "latex-thin-space": (
        "Remove \\; (LaTeX thin space) — GitHub shows it as a semicolon:\n"
        "  Bad:  \\max(x,\\; 0)\n"
        "  Good: \\max(x, 0)"
    ),
}


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    files = find_markdown_files(root)
    total_errors = 0
    rules_seen: set[str] = set()

    for path in files:
        errors = check_file(path)
        for lineno, line, rule in errors:
            rel = path.relative_to(root)
            print(f"{rel}:{lineno}: {MESSAGES[rule]}: {line.strip()}")
            total_errors += 1
            rules_seen.add(rule)

    if total_errors:
        print(f"\n{total_errors} math block issue(s) found.")
        for rule in sorted(rules_seen):
            print(f"\n{MESSAGES[rule]}:")
            print(HINTS[rule])
        return 1

    print("All math blocks OK.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
