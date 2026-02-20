"""Shared style, helpers, and constants for forge-gpu diagram generation."""

import os

import matplotlib

matplotlib.use("Agg")

import matplotlib.patheffects as pe  # noqa: E402
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.colors import LinearSegmentedColormap  # noqa: E402

# ---------------------------------------------------------------------------
# Paths and settings
# ---------------------------------------------------------------------------

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LESSONS_DIR = os.path.join(REPO_ROOT, "lessons")
DPI = 200

# ---------------------------------------------------------------------------
# Dark theme style (matching forge-gpu visual identity)
# ---------------------------------------------------------------------------

STYLE = {
    "bg": "#1a1a2e",  # Dark blue-gray background
    "grid": "#2a2a4a",  # Subtle grid lines
    "axis": "#8888aa",  # Axis lines and labels
    "text": "#e0e0f0",  # Primary text
    "text_dim": "#8888aa",  # Secondary/dim text
    "accent1": "#4fc3f7",  # Cyan — primary vectors, highlights
    "accent2": "#ff7043",  # Orange — secondary vectors, results
    "accent3": "#66bb6a",  # Green — tertiary, normals
    "accent4": "#ab47bc",  # Purple — special elements
    "warn": "#ffd54f",  # Yellow — annotations, warnings
    "surface": "#252545",  # Slightly lighter surface for fills
}

# Custom colormap for texture filtering diagrams
FORGE_CMAP = LinearSegmentedColormap.from_list(
    "forge",
    [STYLE["bg"], STYLE["accent1"], STYLE["accent2"], STYLE["warn"]],
)


def setup_axes(ax, xlim=None, ylim=None, grid=True, aspect="equal"):
    """Apply consistent forge-gpu dark styling to axes."""
    ax.set_facecolor(STYLE["bg"])
    if xlim:
        ax.set_xlim(xlim)
    if ylim:
        ax.set_ylim(ylim)
    if aspect:
        ax.set_aspect(aspect)
    ax.tick_params(colors=STYLE["axis"], labelsize=9)
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    if grid:
        ax.grid(True, color=STYLE["grid"], linewidth=0.5, alpha=0.5)
    ax.set_axisbelow(True)


def draw_vector(ax, origin, vec, color, label=None, label_offset=(0.15, 0.15), lw=2.5):
    """Draw a labeled arrow vector with text stroke for readability."""
    ax.annotate(
        "",
        xy=(origin[0] + vec[0], origin[1] + vec[1]),
        xytext=origin,
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.15",
            "color": color,
            "lw": lw,
        },
    )
    if label:
        mid = (
            origin[0] + vec[0] / 2 + label_offset[0],
            origin[1] + vec[1] / 2 + label_offset[1],
        )
        ax.text(
            mid[0],
            mid[1],
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )


def save(fig, lesson_path, filename):
    """Save a figure to a lesson's assets/ directory."""
    assets_dir = os.path.join(LESSONS_DIR, lesson_path, "assets")
    os.makedirs(assets_dir, exist_ok=True)
    out = os.path.join(assets_dir, filename)
    fig.savefig(
        out,
        dpi=DPI,
        bbox_inches="tight",
        facecolor=STYLE["bg"],
        pad_inches=0.2,
    )
    plt.close(fig)
    rel = os.path.relpath(out, REPO_ROOT)
    print(f"  {rel}")
