"""Diagrams for ui/05."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 05 — Immediate-Mode Basics
# ---------------------------------------------------------------------------


def diagram_hot_active_state_machine():
    """Show the hot/active two-ID state machine from Casey Muratori's IMGUI
    talk.  Four states (none, hot, active, click) with labeled transitions
    showing mouse events and conditions."""

    fig, ax = plt.subplots(figsize=(10, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-1, 11), ylim=(-1, 7), grid=False, aspect=None)
    ax.axis("off")

    # Title with vertical padding to avoid crowding content
    ax.set_title(
        "Hot / Active State Machine",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    # State positions and colors
    states = {
        "NONE": (1.5, 4.0, STYLE["text_dim"]),
        "HOT": (5.0, 4.0, STYLE["accent1"]),
        "ACTIVE": (8.5, 4.0, STYLE["accent2"]),
        "CLICK!": (8.5, 1.0, STYLE["accent3"]),
    }

    # Draw state circles
    for name, (x, y, color) in states.items():
        circle = mpatches.Circle(
            (x, y),
            0.9,
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
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=4,
        )

    # Descriptions below each state
    descs = {
        "NONE": "hot = NONE\nactive = NONE",
        "HOT": "hot = id\nactive = NONE",
        "ACTIVE": "hot = id\nactive = id",
        "CLICK!": "mouse released\nover active widget",
    }
    for name, (x, y, _color) in states.items():
        ax.text(
            x,
            y - 1.3,
            descs[name],
            color=STYLE["text_dim"],
            fontsize=7.5,
            ha="center",
            va="top",
            style="italic",
        )

    # Transition arrows
    arrow_style = dict(
        arrowstyle="->,head_width=0.25,head_length=0.15",
        lw=1.8,
    )

    # NONE -> HOT: mouse enters widget
    ax.annotate(
        "",
        xy=(4.1, 4.15),
        xytext=(2.4, 4.15),
        arrowprops=dict(**arrow_style, color=STYLE["accent1"]),
        zorder=5,
    )
    ax.text(
        3.25,
        4.55,
        "mouse enters\nwidget rect",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontweight="bold",
    )

    # HOT -> NONE: mouse leaves widget
    ax.annotate(
        "",
        xy=(2.4, 3.85),
        xytext=(4.1, 3.85),
        arrowprops=dict(**arrow_style, color=STYLE["text_dim"]),
        zorder=5,
    )
    ax.text(
        3.25,
        3.35,
        "mouse leaves",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="top",
    )

    # HOT -> ACTIVE: mouse pressed
    ax.annotate(
        "",
        xy=(7.6, 4.15),
        xytext=(5.9, 4.15),
        arrowprops=dict(**arrow_style, color=STYLE["accent2"]),
        zorder=5,
    )
    ax.text(
        6.75,
        4.55,
        "mouse button\npressed",
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontweight="bold",
    )

    # ACTIVE -> CLICK: mouse released while over widget
    ax.annotate(
        "",
        xy=(8.5, 1.9),
        xytext=(8.5, 3.1),
        arrowprops=dict(**arrow_style, color=STYLE["accent3"]),
        zorder=5,
    )
    ax.text(
        9.6,
        2.5,
        "mouse released\n(still over widget)",
        color=STYLE["accent3"],
        fontsize=8,
        ha="left",
        va="center",
        fontweight="bold",
    )

    # ACTIVE -> NONE: mouse released outside widget (no click)
    ax.annotate(
        "",
        xy=(2.1, 3.3),
        xytext=(7.8, 3.3),
        arrowprops=dict(
            **arrow_style,
            color=STYLE["warn"],
            connectionstyle="arc3,rad=0.25",
        ),
        zorder=5,
    )
    ax.text(
        5.0,
        2.15,
        "mouse released outside\n(no click -- cancelled)",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="top",
        fontweight="bold",
    )

    # CLICK -> NONE: return to idle
    ax.annotate(
        "",
        xy=(1.5, 3.1),
        xytext=(7.65, 1.0),
        arrowprops=dict(
            **arrow_style,
            color=STYLE["text_dim"],
            connectionstyle="arc3,rad=-0.3",
        ),
        zorder=5,
    )
    ax.text(
        4.5,
        0.55,
        "next frame",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="top",
    )

    save(fig, "ui/05-immediate-mode-basics", "hot_active_state_machine.png")


def diagram_declare_then_draw():
    """Show the immediate-mode frame loop: begin -> declare widgets ->
    end -> render.  Contrasts with retained-mode's create-once approach."""

    fig, axes = plt.subplots(1, 2, figsize=(12, 5.5))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Retained Mode vs Immediate Mode",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    # ---- Left: Retained mode ----
    ax = axes[0]
    setup_axes(ax, xlim=(-0.5, 6), ylim=(-0.5, 6.5), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Retained Mode",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        pad=12,
    )

    # Show create-once, update approach
    retained_steps = [
        ("Create widget tree\n(once at startup)", STYLE["accent2"], 5.5),
        ("Store in\npersistent tree", STYLE["text_dim"], 4.0),
        ("Update properties\n(on change)", STYLE["accent2"], 2.5),
        ("Library manages\nrender + state", STYLE["text_dim"], 1.0),
    ]
    for label, color, y in retained_steps:
        rect = mpatches.FancyBboxPatch(
            (0.5, y - 0.4),
            5.0,
            0.8,
            boxstyle="round,pad=0.1",
            facecolor=color + "20",
            edgecolor=color,
            linewidth=1.2,
        )
        ax.add_patch(rect)
        ax.text(
            3.0,
            y,
            label,
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            va="center",
        )

    # Down arrows between steps
    for i in range(len(retained_steps) - 1):
        y_from = retained_steps[i][2] - 0.4
        y_to = retained_steps[i + 1][2] + 0.4
        ax.annotate(
            "",
            xy=(3.0, y_to),
            xytext=(3.0, y_from),
            arrowprops=dict(
                arrowstyle="->,head_width=0.2,head_length=0.1",
                color=STYLE["text_dim"],
                lw=1.2,
            ),
        )

    # ---- Right: Immediate mode ----
    ax = axes[1]
    setup_axes(ax, xlim=(-0.5, 6), ylim=(-0.5, 6.5), grid=False, aspect=None)
    ax.axis("off")
    ax.set_title(
        "Immediate Mode",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        pad=12,
    )

    # Show per-frame declare-then-draw loop
    imm_steps = [
        ("ctx_begin()\ninput + reset buffers", STYLE["accent1"], 5.5),
        ("Declare widgets\nlabel(), button(), ...", STYLE["accent3"], 4.0),
        ("ctx_end()\nfinalize hot/active", STYLE["accent1"], 2.5),
        ("Render\nvertices + indices", STYLE["accent4"], 1.0),
    ]
    for label, color, y in imm_steps:
        rect = mpatches.FancyBboxPatch(
            (0.5, y - 0.4),
            5.0,
            0.8,
            boxstyle="round,pad=0.1",
            facecolor=color + "20",
            edgecolor=color,
            linewidth=1.2,
        )
        ax.add_patch(rect)
        ax.text(
            3.0,
            y,
            label,
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            va="center",
        )

    # Down arrows
    for i in range(len(imm_steps) - 1):
        y_from = imm_steps[i][2] - 0.4
        y_to = imm_steps[i + 1][2] + 0.4
        ax.annotate(
            "",
            xy=(3.0, y_to),
            xytext=(3.0, y_from),
            arrowprops=dict(
                arrowstyle="->,head_width=0.2,head_length=0.1",
                color=STYLE["accent1"],
                lw=1.2,
            ),
        )

    # Loop-back arrow from render to begin
    ax.annotate(
        "",
        xy=(5.7, 5.5),
        xytext=(5.7, 1.0),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.1",
            color=STYLE["warn"],
            lw=1.5,
            connectionstyle="arc3,rad=-0.3",
        ),
    )
    ax.text(
        5.9,
        3.25,
        "every\nframe",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="left",
        va="center",
    )

    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "ui/05-immediate-mode-basics", "declare_then_draw.png")


def diagram_button_draw_data():
    """Show how a button generates draw data: a background rectangle quad
    using white_uv plus centered text quads using glyph UVs -- all in the
    same vertex/index buffer with a shared atlas texture."""

    fig, ax = plt.subplots(figsize=(10, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12), ylim=(-1, 8), grid=False, aspect=None)
    ax.axis("off")

    # Title with vertical padding to avoid crowding content
    ax.set_title(
        "Button Draw Data: Background + Text in One Draw Call",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    # ---- Button visualization (left side) ----
    # Background rect
    btn_x, btn_y, btn_w, btn_h = 0.5, 3.5, 4.5, 1.8
    btn_rect = mpatches.FancyBboxPatch(
        (btn_x, btn_y),
        btn_w,
        btn_h,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["accent4"] + "40",
        edgecolor=STYLE["accent4"],
        linewidth=2.0,
    )
    ax.add_patch(btn_rect)
    ax.text(
        btn_x + btn_w / 2,
        btn_y + btn_h / 2,
        "OK",
        color=STYLE["text"],
        fontsize=18,
        fontweight="bold",
        ha="center",
        va="center",
        family="monospace",
    )

    # Label the button parts
    ax.annotate(
        "background quad\n(white_uv region)",
        xy=(btn_x + btn_w, btn_y + btn_h - 0.1),
        xytext=(btn_x + btn_w + 0.8, btn_y + btn_h + 1.0),
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["accent4"],
            lw=1.2,
        ),
        color=STYLE["accent4"],
        fontsize=8.5,
        fontweight="bold",
    )
    ax.annotate(
        "text quads\n(glyph UVs)",
        xy=(btn_x + btn_w / 2, btn_y + 0.3),
        xytext=(btn_x + btn_w + 0.8, btn_y - 0.8),
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["accent1"],
            lw=1.2,
        ),
        color=STYLE["accent1"],
        fontsize=8.5,
        fontweight="bold",
    )

    # ---- Atlas texture visualization (right side) ----
    atlas_x, atlas_y = 7.0, 2.0
    atlas_w, atlas_h = 4.5, 4.5

    atlas_bg = mpatches.FancyBboxPatch(
        (atlas_x, atlas_y),
        atlas_w,
        atlas_h,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["text_dim"],
        linewidth=1.0,
    )
    ax.add_patch(atlas_bg)
    ax.text(
        atlas_x + atlas_w / 2,
        atlas_y + atlas_h + 0.3,
        "Font Atlas Texture",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
    )

    # Glyph regions in the atlas
    glyphs = [
        ("A", atlas_x + 0.3, atlas_y + 3.2, 0.8, 1.0),
        ("B", atlas_x + 1.3, atlas_y + 3.2, 0.8, 1.0),
        ("O", atlas_x + 2.3, atlas_y + 3.2, 0.9, 1.0),
        ("K", atlas_x + 3.3, atlas_y + 3.2, 0.8, 1.0),
        ("...", atlas_x + 0.3, atlas_y + 1.8, 3.9, 1.0),
    ]
    for label, gx, gy, gw, gh in glyphs:
        glyph_rect = mpatches.FancyBboxPatch(
            (gx, gy),
            gw,
            gh,
            boxstyle="round,pad=0.02",
            facecolor=STYLE["accent1"] + "30",
            edgecolor=STYLE["accent1"],
            linewidth=0.8,
        )
        ax.add_patch(glyph_rect)
        ax.text(
            gx + gw / 2,
            gy + gh / 2,
            label,
            color=STYLE["text"],
            fontsize=9 if label != "..." else 12,
            ha="center",
            va="center",
            family="monospace",
        )

    # White pixel region
    wp_x = atlas_x + 0.3
    wp_y = atlas_y + 0.3
    wp_rect = mpatches.FancyBboxPatch(
        (wp_x, wp_y),
        1.0,
        1.0,
        boxstyle="round,pad=0.02",
        facecolor=STYLE["warn"] + "50",
        edgecolor=STYLE["warn"],
        linewidth=1.2,
    )
    ax.add_patch(wp_rect)
    ax.text(
        wp_x + 0.5,
        wp_y + 0.5,
        "white\npixel",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
    )

    # Arrows from button parts to atlas regions
    # Background -> white pixel
    ax.annotate(
        "",
        xy=(wp_x + 0.5, wp_y + 1.0),
        xytext=(btn_x + btn_w, btn_y + btn_h * 0.75),
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["warn"],
            lw=1.5,
            connectionstyle="arc3,rad=-0.2",
            linestyle="--",
        ),
    )

    # Text -> glyph O and K
    ax.annotate(
        "",
        xy=(atlas_x + 2.75, atlas_y + 3.2),
        xytext=(btn_x + btn_w * 0.4, btn_y + btn_h * 0.25),
        arrowprops=dict(
            arrowstyle="->",
            color=STYLE["accent1"],
            lw=1.5,
            connectionstyle="arc3,rad=-0.15",
            linestyle="--",
        ),
    )

    # ---- Vertex buffer visualization (bottom) ----
    vb_y = 0.2
    ax.text(
        6.0,
        vb_y + 0.8,
        "Vertex Buffer (shared format: pos, UV, color)",
        color=STYLE["text"],
        fontsize=9,
        fontweight="bold",
        ha="center",
    )

    buf_entries = [
        ("bg v0", STYLE["accent4"]),
        ("bg v1", STYLE["accent4"]),
        ("bg v2", STYLE["accent4"]),
        ("bg v3", STYLE["accent4"]),
        ("'O' v0", STYLE["accent1"]),
        ("'O' v1", STYLE["accent1"]),
        ("...", STYLE["text_dim"]),
        ("'K' v3", STYLE["accent1"]),
    ]
    cell_w = 1.3
    start_x = 6.0 - (len(buf_entries) * cell_w) / 2
    for i, (label, color) in enumerate(buf_entries):
        cx = start_x + i * cell_w
        rect = mpatches.FancyBboxPatch(
            (cx, vb_y - 0.3),
            cell_w - 0.1,
            0.6,
            boxstyle="round,pad=0.03",
            facecolor=color + "25",
            edgecolor=color,
            linewidth=0.8,
        )
        ax.add_patch(rect)
        ax.text(
            cx + (cell_w - 0.1) / 2,
            vb_y,
            label,
            color=STYLE["text"],
            fontsize=6.5,
            ha="center",
            va="center",
            family="monospace",
        )

    save(fig, "ui/05-immediate-mode-basics", "button_draw_data.png")


def diagram_hit_testing():
    """Show how hit testing works: mouse position checked against widget
    bounding rectangles to determine the hot widget."""

    fig, ax = plt.subplots(figsize=(8, 5.5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 10), ylim=(-0.5, 7), grid=False, aspect=None)
    ax.axis("off")

    # Title with vertical padding to avoid crowding content
    ax.set_title(
        "Hit Testing: Point-in-Rectangle",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    # Draw three buttons
    buttons = [
        ("Start", 1.0, 4.5, 3.0, 1.2, STYLE["accent1"]),
        ("Options", 1.0, 2.8, 3.0, 1.2, STYLE["accent2"]),
        ("Quit", 1.0, 1.1, 3.0, 1.2, STYLE["text_dim"]),
    ]

    for label, bx, by, bw, bh, color in buttons:
        is_hit = label == "Options"  # Mouse is over Options
        rect = mpatches.FancyBboxPatch(
            (bx, by),
            bw,
            bh,
            boxstyle="round,pad=0.05",
            facecolor=color + ("40" if is_hit else "20"),
            edgecolor=color,
            linewidth=2.5 if is_hit else 1.2,
        )
        ax.add_patch(rect)
        ax.text(
            bx + bw / 2,
            by + bh / 2,
            label,
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
        )
        if is_hit:
            ax.text(
                bx + bw + 0.3,
                by + bh / 2,
                "HOT",
                color=STYLE["accent1"],
                fontsize=10,
                fontweight="bold",
                ha="left",
                va="center",
            )

    # Mouse cursor
    mx, my = 2.5, 3.4
    ax.plot(
        mx,
        my,
        marker="x",
        markersize=14,
        color=STYLE["warn"],
        markeredgewidth=2.5,
        zorder=10,
    )
    ax.text(
        mx + 0.3,
        my + 0.3,
        "mouse (px, py)",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="left",
    )

    # Hit test formula
    ax.text(
        6.5,
        4.8,
        "Hit test passes when:",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="left",
    )
    conditions = [
        "px >= rect.x",
        "px <  rect.x + rect.w",
        "py >= rect.y",
        "py <  rect.y + rect.h",
    ]
    for i, cond in enumerate(conditions):
        ax.text(
            6.8,
            4.2 - i * 0.45,
            cond,
            color=STYLE["accent1"],
            fontsize=9,
            ha="left",
            family="monospace",
        )

    # Result annotation
    ax.text(
        6.5,
        2.0,
        "Last widget passing\nhit test becomes hot",
        color=STYLE["text_dim"],
        fontsize=8.5,
        ha="left",
        va="top",
        style="italic",
    )

    save(fig, "ui/05-immediate-mode-basics", "hit_testing.png")


# ---------------------------------------------------------------------------
# UI Lesson 06 — Checkboxes and Sliders
# ---------------------------------------------------------------------------
