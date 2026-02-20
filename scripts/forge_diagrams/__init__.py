"""forge_diagrams — Generate matplotlib diagrams for forge-gpu lesson READMEs.

Produces PNG diagrams at 200 DPI for embedding in markdown. Each lesson's
diagrams are placed in its assets/ directory (created automatically).

Usage:
    python scripts/forge_diagrams --lesson math/01    # one lesson
    python scripts/forge_diagrams --all               # all diagrams
    python scripts/forge_diagrams --list              # list available

Requires: pip install numpy matplotlib
"""
