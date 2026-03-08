"""Diagrams for ui/07."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 07 — Text Input
# ---------------------------------------------------------------------------


def diagram_focus_state_machine():
    """Two-state focus machine (Unfocused / Focused) with a transient key-event
    processing phase shown as a dashed node, reflecting the single focused-ID
    model in forge_ui_ctx.h."""

    fig, ax = plt.subplots(figsize=(10, 5.5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 11), ylim=(-0.5, 6), grid=False, aspect=None)
    ax.axis("off")

    ax.set_title(
        "Focus State Machine",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    # Persistent state positions and colors
    states = {
        "UNFOCUSED": (1.5, 3.0, STYLE["text_dim"]),
        "FOCUSED": (5.5, 3.0, STYLE["accent1"]),
    }

    # Draw persistent state circles (solid border)
    for name, (x, y, color) in states.items():
        circle = mpatches.Circle(
            (x, y),
            1.0,
            facecolor=color + "25",
            edgecolor=color,
            linewidth=2.0,
            zorder=3,
        )
        ax.add_patch(circle)
        ax.text(
            x,
            y,
            name,
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=4,
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Transient key-event node (dashed border to signal non-persistent)
    ke_x, ke_y, ke_color = 9.5, 3.0, STYLE["accent2"]
    ke_circle = mpatches.Circle(
        (ke_x, ke_y),
        1.0,
        facecolor=ke_color + "15",
        edgecolor=ke_color,
        linewidth=1.5,
        linestyle="--",
        zorder=3,
    )
    ax.add_patch(ke_circle)
    ax.text(
        ke_x,
        ke_y + 0.15,
        "KEY EVENT",
        color=STYLE["text"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        zorder=4,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        ke_x,
        ke_y - 0.25,
        "(transient)",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="center",
        zorder=4,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Descriptions below each node
    descs = {
        "UNFOCUSED": "focused = NONE\nno keyboard input",
        "FOCUSED": "focused = id\ncursor visible",
    }
    for name, (x, y, _) in states.items():
        ax.text(
            x,
            y - 1.5,
            descs[name],
            color=STYLE["text_dim"],
            fontsize=7.5,
            ha="center",
            va="top",
            style="italic",
        )
    ax.text(
        ke_x,
        ke_y - 1.5,
        "momentary key processing\nwhile focused (same frame)",
        color=STYLE["text_dim"],
        fontsize=7.5,
        ha="center",
        va="top",
        style="italic",
    )

    arrow_style = dict(
        arrowstyle="->,head_width=0.25,head_length=0.15",
        lw=1.8,
    )

    # UNFOCUSED -> FOCUSED: click on text input
    ax.annotate(
        "",
        xy=(4.5, 3.2),
        xytext=(2.5, 3.2),
        arrowprops=dict(**arrow_style, color=STYLE["accent1"]),
        zorder=5,
    )
    ax.text(
        3.5,
        3.7,
        "click on\ntext input",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # FOCUSED -> UNFOCUSED: click outside / Escape
    ax.annotate(
        "",
        xy=(2.5, 2.8),
        xytext=(4.5, 2.8),
        arrowprops=dict(**arrow_style, color=STYLE["text_dim"]),
        zorder=5,
    )
    ax.text(
        3.5,
        2.25,
        "click outside\nor Escape",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # FOCUSED -> KEY EVENT: key arrives while focused
    ax.annotate(
        "",
        xy=(8.5, 3.2),
        xytext=(6.5, 3.2),
        arrowprops=dict(**arrow_style, color=STYLE["accent2"]),
        zorder=5,
    )
    ax.text(
        7.5,
        3.7,
        "key event\n(text/arrow/del)",
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # KEY EVENT -> FOCUSED: processed, returns to focused
    ax.annotate(
        "",
        xy=(6.5, 2.8),
        xytext=(8.5, 2.8),
        arrowprops=dict(**arrow_style, color=STYLE["accent3"]),
        zorder=5,
    )
    ax.text(
        7.5,
        2.25,
        "input processed\n(returns immediately)",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # KEY EVENT -> UNFOCUSED: Escape key
    ax.annotate(
        "",
        xy=(2.2, 4.0),
        xytext=(9.2, 4.0),
        arrowprops=dict(
            **arrow_style,
            color=STYLE["warn"],
            connectionstyle="arc3,rad=0.35",
        ),
        zorder=5,
    )
    ax.text(
        5.5,
        5.3,
        "Escape",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "focus_state_machine.png")


def diagram_text_input_anatomy():
    """Labeled parts of a text input widget: background rect, text baseline,
    cursor bar, left padding, with annotations for focused vs unfocused."""

    fig, axes = plt.subplots(1, 2, figsize=(11, 4))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Text Input Anatomy",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    titles = ["Unfocused", "Focused"]
    border_colors = [STYLE["text_dim"] + "40", STYLE["accent1"]]
    bg_colors = [STYLE["surface"], STYLE["surface"]]
    show_cursor = [False, True]

    for idx, ax in enumerate(axes):
        setup_axes(ax, xlim=(-0.5, 12), ylim=(-1, 5), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(
            titles[idx],
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            pad=8,
        )

        # Background rect
        bg = mpatches.FancyBboxPatch(
            (0, 1.0),
            11,
            2.5,
            boxstyle="round,pad=0.08",
            facecolor=bg_colors[idx],
            edgecolor=border_colors[idx],
            linewidth=2.0 if idx == 1 else 1.0,
            zorder=2,
        )
        ax.add_patch(bg)

        # Text "Hello"
        text_x = 0.8
        baseline_y = 2.0
        letters = list("Hello")
        widths = [1.2, 1.0, 0.6, 0.6, 1.2]
        pen_x = text_x
        for li, letter in enumerate(letters):
            ax.text(
                pen_x,
                baseline_y,
                letter,
                color=STYLE["text"],
                fontsize=18,
                fontweight="bold",
                ha="left",
                va="baseline",
                zorder=3,
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )
            pen_x += widths[li]

        # Annotations
        # Left padding
        ax.annotate(
            "",
            xy=(0.8, 0.6),
            xytext=(0, 0.6),
            arrowprops=dict(
                arrowstyle="<->",
                color=STYLE["warn"],
                lw=1.2,
            ),
        )
        ax.text(
            0.4,
            0.15,
            "padding",
            color=STYLE["warn"],
            fontsize=7,
            ha="center",
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Baseline indicator
        ax.plot(
            [0.5, 10.5],
            [baseline_y, baseline_y],
            color=STYLE["accent3"] + "60",
            linewidth=0.8,
            linestyle="--",
            zorder=2,
        )
        ax.text(
            10.7,
            baseline_y,
            "baseline",
            color=STYLE["accent3"],
            fontsize=7,
            ha="left",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Cursor bar (focused only)
        if show_cursor[idx]:
            cursor_x = pen_x + 0.1
            cursor_rect = mpatches.Rectangle(
                (cursor_x, 1.3),
                0.15,
                2.0,
                facecolor=STYLE["accent1"],
                zorder=4,
            )
            ax.add_patch(cursor_rect)
            ax.annotate(
                "cursor bar\n(2px wide)",
                xy=(cursor_x, 3.5),
                xytext=(cursor_x + 2.0, 4.3),
                color=STYLE["accent1"],
                fontsize=7.5,
                fontweight="bold",
                ha="center",
                arrowprops=dict(
                    arrowstyle="->",
                    color=STYLE["accent1"],
                    lw=1.2,
                ),
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

        # Border annotation (focused only)
        if idx == 1:
            ax.annotate(
                "accent border",
                xy=(0, 3.5),
                xytext=(-0.3, 4.5),
                color=STYLE["accent1"],
                fontsize=7.5,
                fontweight="bold",
                ha="center",
                arrowprops=dict(
                    arrowstyle="->",
                    color=STYLE["accent1"],
                    lw=1.0,
                ),
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )
        else:
            ax.text(
                5.5,
                4.3,
                "dark background\nsubtle border",
                color=STYLE["text_dim"],
                fontsize=7.5,
                ha="center",
                va="bottom",
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "text_input_anatomy.png")


def diagram_cursor_positioning():
    """The word 'Hello' with cursor bar drawn after the first 'l'
    (between the two l's), showing pen_x = sum(widths[:3]) for 'Hel'."""

    fig, ax = plt.subplots(figsize=(9, 4))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 13), ylim=(-1.5, 5), grid=False, aspect=None)
    ax.axis("off")

    ax.set_title(
        "Cursor Positioning via Substring Measurement",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    # Draw text input background
    bg = mpatches.FancyBboxPatch(
        (0, 0.5),
        12,
        2.8,
        boxstyle="round,pad=0.08",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
        zorder=2,
    )
    ax.add_patch(bg)

    # Draw letters with individual bounding boxes
    padding_x = 0.6
    baseline_y = 1.5
    letters = list("Hello")
    widths = [1.6, 1.3, 0.8, 0.8, 1.5]
    pen_x = padding_x

    for i, (letter, w) in enumerate(zip(letters, widths, strict=True)):
        color = STYLE["accent1"] if i < 3 else STYLE["text_dim"]
        ax.text(
            pen_x + 0.1,
            baseline_y,
            letter,
            color=color,
            fontsize=22,
            fontweight="bold",
            ha="left",
            va="baseline",
            zorder=4,
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

        # Glyph advance box
        box = mpatches.Rectangle(
            (pen_x, 0.8),
            w,
            2.2,
            facecolor="none",
            edgecolor=color + "40",
            linewidth=0.8,
            linestyle="--",
            zorder=3,
        )
        ax.add_patch(box)
        pen_x += w

    # Cursor position (after first 'l', between the two l's)
    cursor_x = padding_x + sum(widths[:3])
    cursor_rect = mpatches.Rectangle(
        (cursor_x, 0.7),
        0.12,
        2.4,
        facecolor=STYLE["accent1"],
        zorder=5,
    )
    ax.add_patch(cursor_rect)

    # Measurement bracket under "Hel"
    meas_start = padding_x
    meas_end = cursor_x
    bracket_y = 0.2
    ax.plot(
        [meas_start, meas_end],
        [bracket_y, bracket_y],
        color=STYLE["accent2"],
        linewidth=2.0,
    )
    ax.plot(
        [meas_start, meas_start],
        [bracket_y - 0.2, bracket_y + 0.2],
        color=STYLE["accent2"],
        linewidth=2.0,
    )
    ax.plot(
        [meas_end, meas_end],
        [bracket_y - 0.2, bracket_y + 0.2],
        color=STYLE["accent2"],
        linewidth=2.0,
    )

    ax.text(
        (meas_start + meas_end) / 2,
        bracket_y - 0.4,
        'text_measure("Hel").width',
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Formula annotation
    ax.text(
        6.5,
        4.2,
        "cursor_x  =  padding  +  text_measure( buffer[0..cursor] ).width",
        color=STYLE["text"],
        fontsize=8.5,
        ha="center",
        va="bottom",
        fontfamily="monospace",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Cursor index annotation
    ax.annotate(
        "cursor = 3",
        xy=(cursor_x, 3.3),
        xytext=(cursor_x + 2.5, 4.0),
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["accent1"],
            lw=1.2,
        ),
    )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "cursor_positioning.png")


def diagram_character_insertion():
    """Before/after byte-array diagram showing 'Helo' becoming 'Hello' with
    splice at index 3 — trailing bytes shift right then 'l' written at cursor."""

    fig, axes = plt.subplots(2, 1, figsize=(10, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Character Insertion (splice at cursor)",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.98,
    )

    labels = ["Before:  cursor = 3,  insert 'l'", "After:   cursor = 4"]
    before_chars = list("Helo") + ["\\0"]
    after_chars = list("Hello") + ["\\0"]
    char_sets = [before_chars, after_chars]

    for idx, ax in enumerate(axes):
        setup_axes(ax, xlim=(-1.5, 12), ylim=(-1, 3), grid=False, aspect=None)
        ax.axis("off")

        chars = char_sets[idx]
        cell_w = 1.5
        start_x = 0.5
        y = 0.5

        ax.text(
            -1.3,
            1.3,
            labels[idx],
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            ha="left",
            va="center",
        )

        for i, ch in enumerate(chars):
            x = start_x + i * cell_w

            # Highlight the inserted/affected cells
            if idx == 0 and i == 3:
                # Cursor position (before insertion)
                fc = STYLE["accent2"] + "30"
                ec = STYLE["accent2"]
            elif idx == 1 and i == 3:
                # Newly inserted character
                fc = STYLE["accent1"] + "30"
                ec = STYLE["accent1"]
            elif idx == 1 and i == 4:
                # Shifted character
                fc = STYLE["accent2"] + "20"
                ec = STYLE["accent2"] + "80"
            else:
                fc = STYLE["surface"]
                ec = STYLE["text_dim"] + "60"

            cell = mpatches.FancyBboxPatch(
                (x, y),
                cell_w - 0.15,
                1.2,
                boxstyle="round,pad=0.05",
                facecolor=fc,
                edgecolor=ec,
                linewidth=1.5,
                zorder=3,
            )
            ax.add_patch(cell)

            # Character label
            ax.text(
                x + (cell_w - 0.15) / 2,
                y + 0.6,
                ch,
                color=STYLE["text"],
                fontsize=14,
                fontweight="bold",
                ha="center",
                va="center",
                fontfamily="monospace",
                zorder=4,
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

            # Index label below
            ax.text(
                x + (cell_w - 0.15) / 2,
                y - 0.3,
                str(i),
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                va="top",
            )

        # Cursor indicator
        if idx == 0:
            cx = start_x + 3 * cell_w - 0.1
            ax.plot(
                [cx, cx],
                [y - 0.1, y + 1.3],
                color=STYLE["accent1"],
                linewidth=2.5,
                zorder=5,
            )
            ax.text(
                cx,
                y + 1.6,
                "cursor",
                color=STYLE["accent1"],
                fontsize=8,
                fontweight="bold",
                ha="center",
                va="bottom",
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )
        else:
            cx = start_x + 4 * cell_w - 0.1
            ax.plot(
                [cx, cx],
                [y - 0.1, y + 1.3],
                color=STYLE["accent1"],
                linewidth=2.5,
                zorder=5,
            )

    # Arrow between panels showing the operation
    axes[0].annotate(
        "1. shift 'o','\\0' right\n2. write 'l' at [3]",
        xy=(8.5, -0.5),
        xytext=(10, 0.5),
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "character_insertion.png")


def diagram_character_deletion():
    """Side-by-side Backspace vs Delete on 'Hello' with cursor at index 3,
    showing which byte is removed and how trailing bytes shift left."""

    fig, axes = plt.subplots(1, 2, figsize=(12, 4.5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Character Deletion: Backspace vs Delete",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.98,
    )

    titles = ["Backspace (cursor = 3)", "Delete (cursor = 3)"]
    # Backspace removes char BEFORE cursor (index 2 = 'l'), result: "Helo"
    # Delete removes char AT cursor (index 3 = 'l'), result: "Helo"
    removed_indices = [2, 3]
    result_texts = ["Helo", "Helo"]
    result_cursors = [2, 3]
    descriptions = [
        "removes byte BEFORE cursor\ncursor moves left by 1",
        "removes byte AT cursor\ncursor stays at same index",
    ]

    before_chars = list("Hello") + ["\\0"]

    for idx, ax in enumerate(axes):
        setup_axes(ax, xlim=(-0.5, 10), ylim=(-3.5, 4), grid=False, aspect=None)
        ax.axis("off")
        ax.set_title(
            titles[idx],
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            pad=8,
        )

        cell_w = 1.3
        start_x = 0.5

        # Before row
        y_before = 2.0
        ax.text(
            -0.3,
            y_before + 0.5,
            "Before:",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="right",
            va="center",
        )

        for i, ch in enumerate(before_chars):
            x = start_x + i * cell_w
            removed = i == removed_indices[idx]
            fc = STYLE["accent2"] + "35" if removed else STYLE["surface"]
            ec = STYLE["accent2"] if removed else STYLE["text_dim"] + "60"

            cell = mpatches.FancyBboxPatch(
                (x, y_before),
                cell_w - 0.1,
                1.0,
                boxstyle="round,pad=0.04",
                facecolor=fc,
                edgecolor=ec,
                linewidth=1.5 if removed else 1.0,
                zorder=3,
            )
            ax.add_patch(cell)
            ax.text(
                x + (cell_w - 0.1) / 2,
                y_before + 0.5,
                ch,
                color=STYLE["text"] if not removed else STYLE["accent2"],
                fontsize=12,
                fontweight="bold",
                ha="center",
                va="center",
                fontfamily="monospace",
                zorder=4,
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

        # Cursor line (before)
        cx_before = start_x + 3 * cell_w - 0.08
        ax.plot(
            [cx_before, cx_before],
            [y_before - 0.1, y_before + 1.1],
            color=STYLE["accent1"],
            linewidth=2.5,
            zorder=5,
        )

        # After row
        y_after = -0.5
        ax.text(
            -0.3,
            y_after + 0.5,
            "After:",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="right",
            va="center",
        )

        after_chars = list(result_texts[idx]) + ["\\0"]
        for i, ch in enumerate(after_chars):
            x = start_x + i * cell_w
            fc = STYLE["surface"]
            ec = STYLE["text_dim"] + "60"
            cell = mpatches.FancyBboxPatch(
                (x, y_after),
                cell_w - 0.1,
                1.0,
                boxstyle="round,pad=0.04",
                facecolor=fc,
                edgecolor=ec,
                linewidth=1.0,
                zorder=3,
            )
            ax.add_patch(cell)
            ax.text(
                x + (cell_w - 0.1) / 2,
                y_after + 0.5,
                ch,
                color=STYLE["text"],
                fontsize=12,
                fontweight="bold",
                ha="center",
                va="center",
                fontfamily="monospace",
                zorder=4,
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

        # Cursor line (after)
        cx_after = start_x + result_cursors[idx] * cell_w - 0.08
        ax.plot(
            [cx_after, cx_after],
            [y_after - 0.1, y_after + 1.1],
            color=STYLE["accent1"],
            linewidth=2.5,
            zorder=5,
        )

        # Arrow from removed cell
        rx = start_x + removed_indices[idx] * cell_w + (cell_w - 0.1) / 2
        ax.annotate(
            "",
            xy=(rx, y_before - 0.15),
            xytext=(rx, y_before - 0.05),
            arrowprops=dict(
                arrowstyle="-",
                color=STYLE["accent2"],
                lw=2.0,
            ),
        )
        ax.text(
            rx,
            y_before - 0.35,
            "X",
            color=STYLE["accent2"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Description
        ax.text(
            start_x + 2.5 * cell_w,
            y_after - 0.8,
            descriptions[idx],
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="top",
            style="italic",
        )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "character_deletion.png")


def diagram_keyboard_input_flow():
    """Vertical data-flow diagram showing simulated key events feeding into
    ForgeUiContext begin, then flowing to the focused ForgeUiTextInputState
    through the text input widget function."""

    fig, ax = plt.subplots(figsize=(7, 8))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-2, 9), ylim=(-1, 11), grid=False, aspect=None)
    ax.axis("off")

    ax.set_title(
        "Keyboard Input Flow",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    # Flow boxes from top to bottom
    boxes = [
        (
            3.5,
            9.5,
            "Simulated Key Events",
            STYLE["warn"],
            "text_input, key_backspace,\nkey_left, key_right, ...",
        ),
        (
            3.5,
            7.5,
            "forge_ui_ctx_set_keyboard()",
            STYLE["accent1"],
            "stores keyboard state\nin ForgeUiContext",
        ),
        (
            3.5,
            5.5,
            "forge_ui_ctx_text_input()",
            STYLE["accent2"],
            "if focused == id:\n  process keyboard input",
        ),
        (
            3.5,
            3.5,
            "ForgeUiTextInputState",
            STYLE["accent3"],
            "buffer modified, cursor\nupdated, length changed",
        ),
        (
            3.5,
            1.5,
            "Draw Data (vertices/indices)",
            STYLE["accent4"],
            "background + text quads\n+ cursor bar",
        ),
    ]

    box_w = 5.5
    box_h = 1.3

    for cx, cy, title, color, desc in boxes:
        rect = mpatches.FancyBboxPatch(
            (cx - box_w / 2, cy - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.12",
            facecolor=color + "20",
            edgecolor=color,
            linewidth=1.5,
            zorder=3,
        )
        ax.add_patch(rect)
        ax.text(
            cx,
            cy + 0.15,
            title,
            color=STYLE["text"],
            fontsize=9.5,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=4,
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )
        ax.text(
            cx,
            cy - 0.35,
            desc,
            color=STYLE["text_dim"],
            fontsize=7.5,
            ha="center",
            va="center",
            style="italic",
            zorder=4,
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Arrows between boxes
    arrow_style = dict(
        arrowstyle="->,head_width=0.2,head_length=0.12",
        lw=2.0,
    )

    arrow_pairs = [
        (9.5, 7.5, STYLE["accent1"]),
        (7.5, 5.5, STYLE["accent2"]),
        (5.5, 3.5, STYLE["accent3"]),
        (3.5, 1.5, STYLE["accent4"]),
    ]

    for from_y, to_y, color in arrow_pairs:
        ax.annotate(
            "",
            xy=(3.5, to_y + box_h / 2 + 0.05),
            xytext=(3.5, from_y - box_h / 2 - 0.05),
            arrowprops=dict(**arrow_style, color=color),
            zorder=5,
        )

    # Side annotation: "only if focused == id"
    ax.annotate(
        "only runs if\nfocused == id",
        xy=(6.25, 5.5),
        xytext=(8.0, 5.5),
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["accent2"],
            lw=1.0,
        ),
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "ui/07-text-input", "keyboard_input_flow.png")


# ---------------------------------------------------------------------------
# UI Lesson 08 — Layout
# ---------------------------------------------------------------------------
