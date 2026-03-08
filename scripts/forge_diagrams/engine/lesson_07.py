"""Diagrams for engine/07."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

from .._common import STYLE, save


# ---------------------------------------------------------------------------
# engine/07-using-a-debugger — debugger_workflow.png
# ---------------------------------------------------------------------------
def diagram_debugger_workflow():
    """Show the debugger workflow loop: build → run → pause → inspect → fix."""
    fig, ax = plt.subplots(figsize=(12, 5.5), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1, 25)
    ax.set_ylim(-1.5, 6)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        12,
        5.5,
        "Debugger Workflow",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Box definitions: (x, label, sublabel, color)
    boxes = [
        (0.0, "Build\n(Debug)", "-g / Debug config", STYLE["accent1"]),
        (5.0, "Set\nBreakpoint", "line or function", STYLE["accent2"]),
        (10.0, "Run", "program executes\nat full speed", STYLE["accent3"]),
        (15.0, "Paused", "inspect variables\nstep through code", STYLE["warn"]),
        (20.0, "Fix Bug", "edit source\nrebuild", STYLE["accent4"]),
    ]

    bw, bh = 4.0, 3.0
    y_mid = 0.8

    for x, label, sublabel, color in boxes:
        r = FancyBboxPatch(
            (x, y_mid),
            bw,
            bh,
            boxstyle="round,pad=0.15",
            facecolor=color,
            alpha=0.15,
            edgecolor=color,
            linewidth=2,
        )
        ax.add_patch(r)
        ax.text(
            x + bw / 2,
            y_mid + bh - 0.5,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=stroke,
        )
        ax.text(
            x + bw / 2,
            y_mid + 0.4,
            sublabel,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="center",
        )

    # Forward arrows between boxes
    for i in range(4):
        x_start = boxes[i][0] + bw + 0.1
        x_end = boxes[i + 1][0] - 0.1
        ax.annotate(
            "",
            xy=(x_end, y_mid + bh / 2),
            xytext=(x_start, y_mid + bh / 2),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.15",
                "color": STYLE["text"],
                "lw": 2,
            },
        )

    # Loop-back arrow from "Paused" back to "Run" (continue)
    loop_y = y_mid - 0.6
    # Horizontal line from under "Paused" to under "Run"
    ax.annotate(
        "",
        xy=(10.0 + bw / 2, y_mid - 0.05),
        xytext=(15.0 + bw / 2, y_mid - 0.05),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["accent3"],
            "lw": 1.5,
            "connectionstyle": "arc3,rad=0.4",
        },
    )
    ax.text(
        12.5 + bw / 2,
        loop_y - 0.15,
        "continue (F5)",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Step numbers
    for i, (x, _, _, _) in enumerate(boxes):
        ax.text(
            x + bw / 2,
            y_mid + bh + 0.15,
            f"Step {i + 1}",
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="bottom",
        )

    fig.tight_layout()
    save(fig, "engine/07-using-a-debugger", "debugger_workflow.png")


# ---------------------------------------------------------------------------
# engine/07-using-a-debugger — stepping_modes.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# engine/07-using-a-debugger — stepping_modes.png
# ---------------------------------------------------------------------------
def diagram_stepping_modes():
    """Visualize step over, step into, and step out with a call hierarchy."""
    fig, ax = plt.subplots(figsize=(12, 9), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1, 23)
    ax.set_ylim(-2, 12.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        11,
        12.0,
        "Stepping Modes Compared",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Source code lines (left column) — simulate a code listing
    code_x = 0.0
    code_w = 9.0
    line_h = 0.7

    code_lines = [
        ("main()", None, STYLE["text_dim"]),
        ("  normalize(v, 3);", "BREAKPOINT", STYLE["warn"]),
        ("  float d = dot(v, n, 3);", None, STYLE["text"]),
        ("  SDL_Log(d);", None, STYLE["text"]),
    ]

    inner_lines = [
        ("normalize()", None, STYLE["text_dim"]),
        ("  float len = dot(v, v, n);", None, STYLE["text"]),
        ("  float inv = 1.0 / sqrt(len);", None, STYLE["text"]),
        ("  v[i] *= inv;  // loop", None, STYLE["text"]),
    ]

    # Code block background
    code_y_top = 8.5
    code_bg = FancyBboxPatch(
        (code_x, code_y_top - len(code_lines) * line_h - 0.3),
        code_w,
        len(code_lines) * line_h + 0.5,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        alpha=0.6,
        edgecolor=STYLE["grid"],
        linewidth=1,
    )
    ax.add_patch(code_bg)

    for i, (text, marker, color) in enumerate(code_lines):
        y = code_y_top - i * line_h
        ax.text(
            code_x + 0.3,
            y,
            text,
            color=color,
            fontsize=10,
            fontfamily="monospace",
            va="center",
        )
        if marker:
            ax.text(
                code_x + code_w - 0.3,
                y,
                marker,
                color=STYLE["warn"],
                fontsize=8,
                fontweight="bold",
                ha="right",
                va="center",
                path_effects=stroke,
            )

    # Inner function block
    inner_y_top = code_y_top - len(code_lines) * line_h - 1.0
    inner_bg = FancyBboxPatch(
        (code_x, inner_y_top - len(inner_lines) * line_h - 0.3),
        code_w,
        len(inner_lines) * line_h + 0.5,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        alpha=0.6,
        edgecolor=STYLE["accent1"],
        linewidth=1,
    )
    ax.add_patch(inner_bg)

    for i, (text, _marker, color) in enumerate(inner_lines):
        y = inner_y_top - i * line_h
        ax.text(
            code_x + 0.3,
            y,
            text,
            color=color,
            fontsize=10,
            fontfamily="monospace",
            va="center",
        )

    # Right column: three stepping mode explanations
    info_x = 11.0
    info_w = 11.0

    modes = [
        (
            "Step Over (F10 / next)",
            STYLE["accent2"],
            "Executes normalize() to completion.\n"
            "Pauses on the NEXT line:\n"
            "  float d = dot(v, n, 3);",
            7.8,
        ),
        (
            "Step Into (F11 / step)",
            STYLE["accent1"],
            "Enters normalize() and pauses\n"
            "at its first line:\n"
            "  float len = dot(v, v, n);",
            4.5,
        ),
        (
            "Step Out (Shift+F11 / finish)",
            STYLE["accent4"],
            "Runs the rest of normalize()\n"
            "and pauses back in main():\n"
            "  float d = dot(v, n, 3);",
            1.2,
        ),
    ]

    for label, color, desc, y in modes:
        r = FancyBboxPatch(
            (info_x, y),
            info_w,
            2.4,
            boxstyle="round,pad=0.15",
            facecolor=color,
            alpha=0.1,
            edgecolor=color,
            linewidth=2,
        )
        ax.add_patch(r)
        ax.text(
            info_x + 0.4,
            y + 2.0,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            info_x + 0.4,
            y + 0.9,
            desc,
            color=STYLE["text_dim"],
            fontsize=9,
            fontfamily="monospace",
            va="center",
        )

    # Arrows from breakpoint line to each mode box
    bp_y = code_y_top - 1 * line_h  # y position of breakpoint line
    for _, color, _, y in modes:
        ax.annotate(
            "",
            xy=(info_x, y + 1.2),
            xytext=(code_x + code_w + 0.2, bp_y),
            arrowprops={
                "arrowstyle": "->,head_width=0.2,head_length=0.12",
                "color": color,
                "lw": 1.5,
                "connectionstyle": "arc3,rad=0.15",
            },
        )

    fig.tight_layout()
    save(fig, "engine/07-using-a-debugger", "stepping_modes.png")


# ---------------------------------------------------------------------------
# engine/07-using-a-debugger — call_stack.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# engine/07-using-a-debugger — call_stack.png
# ---------------------------------------------------------------------------
def diagram_call_stack():
    """Visualize the call stack as a series of stacked frames."""
    fig, ax = plt.subplots(figsize=(11, 7), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1, 22)
    ax.set_ylim(-1.5, 9)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        10.5,
        8.5,
        "The Call Stack (Backtrace)",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Stack frames from top (most recent) to bottom (main)
    frames = [
        (
            "#0  apply_damage()",
            "health=100, damage=60, armor=0.25",
            "YOU ARE HERE",
            STYLE["warn"],
        ),
        (
            "#1  process_hit()",
            "health=100, base_damage=30, armor=0.25",
            "called apply_damage()",
            STYLE["accent2"],
        ),
        (
            "#2  demo_call_stack()",
            "no parameters",
            "called process_hit()",
            STYLE["accent1"],
        ),
        (
            "#3  main()",
            "argc=1, argv=0x7fff...",
            "called demo_call_stack()",
            STYLE["accent3"],
        ),
    ]

    frame_w = 14.0
    frame_h = 1.6
    frame_x = 0.5
    y_top = 7.0

    for i, (name, params, note, color) in enumerate(frames):
        y = y_top - i * (frame_h + 0.3)
        alpha = 0.2 if i == 0 else 0.1

        r = FancyBboxPatch(
            (frame_x, y),
            frame_w,
            frame_h,
            boxstyle="round,pad=0.12",
            facecolor=color,
            alpha=alpha,
            edgecolor=color,
            linewidth=2.5 if i == 0 else 1.5,
        )
        ax.add_patch(r)

        # Frame name
        ax.text(
            frame_x + 0.4,
            y + frame_h - 0.35,
            name,
            color=color,
            fontsize=12,
            fontweight="bold",
            va="top",
            path_effects=stroke,
        )

        # Parameters
        ax.text(
            frame_x + 0.4,
            y + 0.3,
            params,
            color=STYLE["text_dim"],
            fontsize=9,
            fontfamily="monospace",
            va="center",
        )

        # Note on right side
        ax.text(
            frame_x + frame_w - 0.4,
            y + frame_h / 2,
            note,
            color=color if i == 0 else STYLE["text_dim"],
            fontsize=9,
            fontweight="bold" if i == 0 else "normal",
            ha="right",
            va="center",
            path_effects=stroke if i == 0 else [],
        )

    # Right side annotation: "read top to bottom"
    ann_x = frame_x + frame_w + 1.5

    ax.annotate(
        "",
        xy=(ann_x, y_top - 3 * (frame_h + 0.3) + frame_h / 2),
        xytext=(ann_x, y_top + frame_h / 2),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["text_dim"],
            "lw": 2,
        },
    )
    ax.text(
        ann_x + 0.4,
        y_top - 1 * (frame_h + 0.3),
        "Read\ntop\nto\nbottom",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="left",
        va="center",
    )

    # Insight annotation at the bottom
    ax.text(
        frame_x + frame_w / 2,
        -0.8,
        "damage=60 in #0 but base_damage=30 in #1"
        " -- process_hit() doubled it (crit hit)",
        color=STYLE["warn"],
        fontsize=10,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/07-using-a-debugger", "call_stack.png")


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — edge_functions.png
# ---------------------------------------------------------------------------
