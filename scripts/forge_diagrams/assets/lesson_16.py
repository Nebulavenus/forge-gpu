"""Diagrams for Asset Lesson 16 — Import Settings Editor."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# Diagram 1: Three-layer settings merge
# ---------------------------------------------------------------------------


def diagram_settings_merge():
    """Show the three-layer settings merge: schema → global → per-asset."""
    fig, ax = plt.subplots(figsize=(12, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False)
    ax.set_xlim(-0.5, 12.5)
    ax.set_ylim(-0.5, 5.5)
    ax.set_aspect("equal")
    ax.axis("off")

    # Layer boxes — left to right
    layers = [
        {
            "x": 0.0,
            "label": "Schema\nDefaults",
            "color": STYLE["accent4"],
            "detail": "max_size = 2048\nnormal_map = false\ncompression = none",
        },
        {
            "x": 4.0,
            "label": "Global Config\npipeline.toml",
            "color": STYLE["accent1"],
            "detail": "max_size = 1024\ncompression = basisu",
        },
        {
            "x": 8.0,
            "label": "Per-Asset\n.import.toml",
            "color": STYLE["accent2"],
            "detail": "normal_map = true",
        },
    ]

    box_w = 3.2
    box_h = 1.4
    detail_h = 1.6
    y_box = 3.5
    y_detail = 1.4

    for layer in layers:
        x = layer["x"]
        # Header box
        rect = mpatches.FancyBboxPatch(
            (x, y_box),
            box_w,
            box_h,
            boxstyle="round,pad=0.15",
            facecolor=layer["color"],
            edgecolor="none",
            alpha=0.9,
        )
        ax.add_patch(rect)
        ax.text(
            x + box_w / 2,
            y_box + box_h / 2,
            layer["label"],
            color="white",
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
        )

        # Detail box below
        detail_rect = mpatches.FancyBboxPatch(
            (x, y_detail),
            box_w,
            detail_h,
            boxstyle="round,pad=0.12",
            facecolor=STYLE["surface"],
            edgecolor=layer["color"],
            linewidth=1.5,
            alpha=0.8,
        )
        ax.add_patch(detail_rect)
        ax.text(
            x + box_w / 2,
            y_detail + detail_h / 2,
            layer["detail"],
            color=STYLE["text"],
            fontsize=8.5,
            fontfamily="monospace",
            ha="center",
            va="center",
            linespacing=1.6,
        )

    # Arrows between layers
    arrow_style = {
        "arrowstyle": "->,head_width=0.25,head_length=0.12",
        "color": STYLE["text_dim"],
        "lw": 2,
    }
    for i in range(2):
        x_start = layers[i]["x"] + box_w + 0.1
        x_end = layers[i + 1]["x"] - 0.1
        y_mid = y_box + box_h / 2
        ax.annotate(
            "",
            xy=(x_end, y_mid),
            xytext=(x_start, y_mid),
            arrowprops=arrow_style,
        )
        ax.text(
            (x_start + x_end) / 2,
            y_mid + 0.25,
            "overrides",
            color=STYLE["text_dim"],
            fontsize=7.5,
            ha="center",
            va="bottom",
        )

    # Result box at bottom
    result_y = -0.2
    result_rect = mpatches.FancyBboxPatch(
        (2.5, result_y),
        7.5,
        1.2,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["accent3"],
        edgecolor="none",
        alpha=0.85,
    )
    ax.add_patch(result_rect)
    ax.text(
        6.25,
        result_y + 0.6,
        "Effective:  max_size = 1024    normal_map = true    compression = basisu",
        color="white",
        fontsize=9,
        fontfamily="monospace",
        fontweight="bold",
        ha="center",
        va="center",
    )

    # Arrow from detail area to result
    ax.annotate(
        "",
        xy=(6.25, result_y + 1.2),
        xytext=(6.25, y_detail - 0.15),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.15",
            "color": STYLE["accent3"],
            "lw": 2.5,
        },
    )
    ax.text(
        6.7,
        (y_detail + result_y + 1.2) / 2,
        "merge",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="left",
        va="center",
    )

    save(fig, "assets/16-import-settings-editor", "lesson_16_settings_merge.png")


# ---------------------------------------------------------------------------
# Diagram 2: Settings data flow (frontend ↔ backend ↔ sidecar)
# ---------------------------------------------------------------------------


def diagram_settings_data_flow():
    """Show data flow between frontend, backend, and sidecar files."""
    fig, ax = plt.subplots(figsize=(12, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False)
    ax.set_xlim(-0.5, 12.5)
    ax.set_ylim(-0.5, 6.5)
    ax.set_aspect("equal")
    ax.axis("off")

    def draw_box(x, y, w, h, label, color, sublabel=None):
        rect = mpatches.FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.15",
            facecolor=color,
            edgecolor="none",
            alpha=0.9,
        )
        ax.add_patch(rect)
        text_y = y + h / 2 + (0.15 if sublabel else 0)
        ax.text(
            x + w / 2,
            text_y,
            label,
            color="white",
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
        )
        if sublabel:
            ax.text(
                x + w / 2,
                text_y - 0.4,
                sublabel,
                color="white",
                fontsize=7.5,
                alpha=0.8,
                ha="center",
                va="center",
            )

    def draw_file(x, y, label, color):
        """Draw a file-shaped box (rectangle with folded corner)."""
        fold = 0.3
        w, h = 2.4, 1.0
        # Main body
        verts = [
            (x, y),
            (x + w - fold, y),
            (x + w, y + fold),
            (x + w, y + h),
            (x, y + h),
            (x, y),
        ]
        poly = mpatches.Polygon(
            verts,
            closed=True,
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(poly)
        # Fold triangle
        fold_verts = [
            (x + w - fold, y),
            (x + w - fold, y + fold),
            (x + w, y + fold),
        ]
        fold_poly = mpatches.Polygon(
            fold_verts,
            closed=True,
            facecolor=color,
            edgecolor=color,
            linewidth=1,
            alpha=0.3,
        )
        ax.add_patch(fold_poly)
        ax.text(
            x + w / 2,
            y + h / 2,
            label,
            color=STYLE["text"],
            fontsize=8,
            fontfamily="monospace",
            ha="center",
            va="center",
        )

    # Frontend components
    draw_box(0, 5.0, 3.0, 1.2, "ImportSettings", STYLE["accent1"], "React component")

    # Backend
    draw_box(
        4.5, 5.0, 3.0, 1.2, "FastAPI Server", STYLE["accent4"], "pipeline/server.py"
    )

    # Processing
    draw_box(9.0, 5.0, 3.0, 1.2, "Plugin.process()", STYLE["accent2"], "texture / mesh")

    # Files on disk
    draw_file(0, 2.5, "pipeline.toml", STYLE["accent1"])
    draw_file(3.5, 2.5, ".import.toml", STYLE["accent2"])
    draw_file(7.0, 2.5, "brick.png", STYLE["text_dim"])
    draw_file(9.8, 2.5, "brick.png\n(processed)", STYLE["accent3"])

    # Merge box
    draw_box(3.0, 0.2, 4.0, 1.0, "get_effective_settings()", STYLE["accent3"])

    # Arrows: frontend → backend
    arrow_kw = {
        "arrowstyle": "->,head_width=0.2,head_length=0.1",
        "lw": 1.8,
    }

    # GET/PUT/DELETE settings
    ax.annotate(
        "",
        xy=(4.5, 5.6),
        xytext=(3.0, 5.6),
        arrowprops={**arrow_kw, "color": STYLE["accent1"]},
    )
    ax.text(
        3.75,
        5.85,
        "GET / PUT / DELETE",
        color=STYLE["accent1"],
        fontsize=7,
        ha="center",
        va="bottom",
    )

    # POST process
    ax.annotate(
        "",
        xy=(9.0, 5.6),
        xytext=(7.5, 5.6),
        arrowprops={**arrow_kw, "color": STYLE["accent2"]},
    )
    ax.text(
        8.25,
        5.85,
        "POST /process",
        color=STYLE["accent2"],
        fontsize=7,
        ha="center",
        va="bottom",
    )

    # Backend reads files
    ax.annotate(
        "",
        xy=(1.2, 3.5),
        xytext=(4.8, 5.0),
        arrowprops={**arrow_kw, "color": STYLE["text_dim"]},
    )
    ax.annotate(
        "",
        xy=(4.7, 3.5),
        xytext=(5.5, 5.0),
        arrowprops={**arrow_kw, "color": STYLE["text_dim"]},
    )

    # Files feed into merge
    ax.annotate(
        "",
        xy=(4.0, 1.2),
        xytext=(1.2, 2.5),
        arrowprops={**arrow_kw, "color": STYLE["accent1"]},
    )
    ax.annotate(
        "",
        xy=(5.5, 1.2),
        xytext=(4.7, 2.5),
        arrowprops={**arrow_kw, "color": STYLE["accent2"]},
    )

    # Merge feeds processing
    ax.annotate(
        "",
        xy=(10.0, 5.0),
        xytext=(6.5, 1.0),
        arrowprops={
            **arrow_kw,
            "color": STYLE["accent3"],
            "connectionstyle": "arc3,rad=-0.2",
        },
    )

    # Processing outputs file
    ax.annotate(
        "",
        xy=(11.0, 3.5),
        xytext=(10.5, 5.0),
        arrowprops={**arrow_kw, "color": STYLE["accent3"]},
    )

    save(fig, "assets/16-import-settings-editor", "lesson_16_settings_data_flow.png")


DIAGRAMS = [
    diagram_settings_merge,
    diagram_settings_data_flow,
]
