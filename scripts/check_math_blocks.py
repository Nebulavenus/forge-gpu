"""Lint KaTeX math blocks in markdown files.

Checks:
1. Display math ($$) must use multi-line syntax — GitHub renders single-line
   $$...$$ as inline math, not block math.
2. No \\_ inside \\text{} — KaTeX rejects underscores in text mode with
   "'_' allowed only in math mode". Use \\text{a}\\_\\text{b} instead.
"""

import re
import sys
from pathlib import Path

# Match lines where $$ opens AND closes on the same line with content between
SINGLE_LINE_DISPLAY = re.compile(r"^\$\$\s*\S.*\$\$\s*$")

# Match \text{...\_...} — underscore (escaped or bare) inside \text{}
TEXT_WITH_UNDERSCORE = re.compile(r"\\text\{[^}]*\\_[^}]*\}")

IGNORE_DIRS = {"node_modules", "build", "third_party", ".git"}


def find_markdown_files(root: Path) -> list[Path]:
    """Find all .md files, skipping ignored directories."""
    results = []
    for path in root.rglob("*.md"):
        if any(part in IGNORE_DIRS for part in path.parts):
            continue
        results.append(path)
    return sorted(results)


def check_file(path: Path) -> list[tuple[int, str, str]]:
    """Return list of (line_number, line_text, rule) for violations."""
    errors = []
    for i, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if SINGLE_LINE_DISPLAY.match(line):
            errors.append((i, line, "single-line-display"))
        if TEXT_WITH_UNDERSCORE.search(line):
            errors.append((i, line, "text-underscore"))
    return errors


MESSAGES = {
    "single-line-display": "single-line $$ block",
    "text-underscore": "\\_ inside \\text{} (use \\text{a}\\_\\text{b})",
}

HINTS = {
    "single-line-display": ("Split $$...$$ onto three lines:\n  $$\n  <formula>\n  $$"),
    "text-underscore": (
        "Break out of \\text{} for underscores:\n"
        "  Bad:  \\text{num\\_levels}\n"
        "  Good: \\text{num}\\_\\text{levels}"
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
