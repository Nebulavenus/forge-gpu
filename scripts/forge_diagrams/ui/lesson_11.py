"""Diagrams for ui/11."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 11 — Widget ID System
# ---------------------------------------------------------------------------


def diagram_id_collision():
    """Show how the old integer ID system caused collisions when windows
    reserved id+1 (scrollbar) and id+2 (toggle), and callers unknowingly
    assigned overlapping IDs."""

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # ── Left panel: Old system with collisions ──
    setup_axes(ax1, xlim=(-0.5, 10), ylim=(-0.5, 8.5), grid=False, aspect=None)
    ax1.axis("off")

    ax1.text(
        5.0,
        8.0,
        "Old System: Integer IDs",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Window block
    win_rect = mpatches.FancyBboxPatch(
        (0.5, 4.5),
        4.0,
        3.0,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
    )
    ax1.add_patch(win_rect)
    ax1.text(
        2.5,
        7.0,
        "Window (id=100)",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Reserved IDs inside window
    reserved = [
        (1.0, 6.0, "Scrollbar (id=101)", STYLE["accent2"]),
        (1.0, 5.2, "Toggle (id=102)", STYLE["accent2"]),
    ]
    for rx, ry, rlabel, rcolor in reserved:
        r = mpatches.FancyBboxPatch(
            (rx, ry),
            3.0,
            0.6,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["bg"],
            edgecolor=rcolor,
            linewidth=1.5,
            linestyle="--",
        )
        ax1.add_patch(r)
        ax1.text(
            rx + 1.5,
            ry + 0.3,
            rlabel,
            color=rcolor,
            fontsize=9,
            ha="center",
            path_effects=stroke,
        )

    # User widget that collides
    collision_rect = mpatches.FancyBboxPatch(
        (5.5, 5.2),
        4.0,
        0.8,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
    )
    ax1.add_patch(collision_rect)
    ax1.text(
        7.5,
        5.6,
        "Checkbox (id=102)",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Collision arrow
    ax1.annotate(
        "COLLISION!",
        xy=(4.5, 5.5),
        xytext=(5.5, 4.2),
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        arrowprops={
            "arrowstyle": "->,head_width=0.25",
            "color": STYLE["accent2"],
            "lw": 2,
        },
        path_effects=stroke,
    )

    # Explanation
    ax1.text(
        5.0,
        0.8,
        "Callers must know that id+1\nand id+2 are reserved.\n"
        "Collision causes input bugs.",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    # ── Right panel: New system, no collisions ──
    setup_axes(ax2, xlim=(-0.5, 10), ylim=(-0.5, 8.5), grid=False, aspect=None)
    ax2.axis("off")

    ax2.text(
        5.0,
        8.0,
        "New System: Hashed String IDs",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Window block
    win_rect2 = mpatches.FancyBboxPatch(
        (0.5, 4.0),
        4.0,
        3.5,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent3"],
        linewidth=2,
    )
    ax2.add_patch(win_rect2)
    ax2.text(
        2.5,
        7.0,
        'Window "Settings"',
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    scoped = [
        (1.0, 6.0, '"__scrollbar"  → 0xA3F1...', STYLE["accent1"]),
        (1.0, 5.2, '"__toggle"     → 0x7B2C...', STYLE["accent1"]),
        (1.0, 4.4, '"Enable"       → 0xE48D...', STYLE["accent1"]),
    ]
    for sx, sy, slabel, scolor in scoped:
        r = mpatches.FancyBboxPatch(
            (sx, sy),
            3.0,
            0.6,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["bg"],
            edgecolor=scolor,
            linewidth=1.5,
        )
        ax2.add_patch(r)
        ax2.text(
            sx + 1.5,
            sy + 0.3,
            slabel,
            color=scolor,
            fontsize=8,
            ha="center",
            family="monospace",
            path_effects=stroke,
        )

    # External widget, no collision
    ok_rect = mpatches.FancyBboxPatch(
        (5.5, 5.2),
        4.0,
        0.8,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent3"],
        linewidth=2,
    )
    ax2.add_patch(ok_rect)
    ax2.text(
        7.5,
        5.6,
        '"Enable"  → 0xD917...',
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        family="monospace",
        path_effects=stroke,
    )
    ax2.text(
        7.5,
        4.6,
        "(different scope → different hash)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    # Check mark
    ax2.text(
        7.5,
        3.5,
        "No collision possible",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    ax2.text(
        5.0,
        0.8,
        "Scoped hashing eliminates\nall ID collisions automatically.",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    fig.suptitle(
        "ID Collision: Old Integer IDs vs New Hashed String IDs",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, "ui/11-widget-id-system", "id_collision.png")


def diagram_id_scope_stack():
    """Show the hierarchical ID scope stack with push/pop operations and
    how the FNV-1a seed chains through nested scopes."""

    fig, ax = plt.subplots(figsize=(10, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12), ylim=(-0.5, 9), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Title
    ax.text(
        6.0,
        8.5,
        "ID Scope Stack — Hierarchical Hashing",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Stack visualization (left side)
    stack_x = 0.5
    stack_w = 3.5
    levels = [
        ("Root seed: 0x811c9dc5", STYLE["text_dim"], 0),
        ('push_id("Settings")', STYLE["accent1"], 1),
        ('push_id("__panel")', STYLE["accent2"], 2),
    ]

    for i, (label, color, depth) in enumerate(levels):
        y = 6.0 - i * 1.5
        indent = depth * 0.3
        r = mpatches.FancyBboxPatch(
            (stack_x + indent, y),
            stack_w - indent,
            0.9,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["surface"] if depth == 0 else STYLE["bg"],
            edgecolor=color,
            linewidth=2,
        )
        ax.add_patch(r)
        ax.text(
            stack_x + indent + (stack_w - indent) / 2,
            y + 0.45,
            label,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            family="monospace",
            path_effects=stroke,
        )

        # Depth label
        ax.text(
            stack_x - 0.3,
            y + 0.45,
            f"[{depth}]",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="right",
            family="monospace",
            path_effects=stroke,
        )

    # Arrow showing seed chaining
    ax.annotate(
        "",
        xy=(2.3, 5.1),
        xytext=(2.3, 5.55),
        arrowprops={
            "arrowstyle": "->,head_width=0.2",
            "color": STYLE["accent1"],
            "lw": 1.5,
        },
    )
    ax.annotate(
        "",
        xy=(2.6, 3.6),
        xytext=(2.6, 4.05),
        arrowprops={
            "arrowstyle": "->,head_width=0.2",
            "color": STYLE["accent2"],
            "lw": 1.5,
        },
    )

    # Hash pipeline (right side)
    pipeline_x = 5.5

    ax.text(
        8.5,
        7.2,
        "Hash Pipeline",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    steps = [
        ('Label: "Enable"', STYLE["accent1"]),
        ("Find ## separator?  No → use full label", STYLE["text_dim"]),
        ("seed = stack[depth-1]", STYLE["accent2"]),
        ("hash = FNV-1a(label, seed)", STYLE["accent3"]),
        ("if hash == 0 → return 1", STYLE["text_dim"]),
        ("→ Uint32 widget ID", STYLE["accent3"]),
    ]

    for i, (step, color) in enumerate(steps):
        y = 6.5 - i * 1.0
        r = mpatches.FancyBboxPatch(
            (pipeline_x, y),
            6.0,
            0.7,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(r)
        ax.text(
            pipeline_x + 3.0,
            y + 0.35,
            step,
            color=color,
            fontsize=9,
            ha="center",
            family="monospace",
            path_effects=stroke,
        )

        # Flow arrows between steps
        if i < len(steps) - 1:
            ax.annotate(
                "",
                xy=(pipeline_x + 3.0, y - 0.05),
                xytext=(pipeline_x + 3.0, y + 0.0),
                arrowprops={
                    "arrowstyle": "->,head_width=0.15",
                    "color": STYLE["grid"],
                    "lw": 1,
                },
            )

    # ## separator example at bottom
    ax.text(
        6.0,
        0.3,
        '## separator:  "Delete##audio"  →  displays "Delete",  hashes "##audio"',
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        family="monospace",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "ui/11-widget-id-system", "id_scope_stack.png")


# ---------------------------------------------------------------------------
# UI Lesson 12 — Font Scaling and Spacing
# ---------------------------------------------------------------------------
