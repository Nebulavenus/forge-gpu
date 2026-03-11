"""Diagrams for engine/12-memory-arenas."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle

from .._common import STYLE, save


# ---------------------------------------------------------------------------
# engine/12-memory-arenas — bump_allocation.png
# ---------------------------------------------------------------------------
def diagram_bump_allocation():
    """Show how bump allocation advances a pointer through a memory block."""
    fig, ax = plt.subplots(figsize=(11, 7), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1.5, 18)
    ax.set_ylim(-1, 9.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        8.25,
        9.2,
        "Bump Allocation: Three Allocations in One Block",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    block_x = 0.0
    block_w = 16.0
    cell_h = 1.3

    # --- State 0: Empty arena ---
    y0 = 7.0
    ax.text(
        -1.2,
        y0 + cell_h / 2,
        "Initial",
        color=STYLE["text_dim"],
        fontsize=9,
        fontweight="bold",
        ha="right",
        va="center",
    )

    # Full block outline
    r = Rectangle(
        (block_x, y0),
        block_w,
        cell_h,
        facecolor=STYLE["surface"],
        alpha=0.3,
        edgecolor=STYLE["grid"],
        linewidth=1.5,
        linestyle="--",
    )
    ax.add_patch(r)
    ax.text(
        block_w / 2,
        y0 + cell_h / 2,
        "(free space)",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
    )
    # Pointer arrow
    ax.annotate(
        "",
        xy=(0, y0 - 0.05),
        xytext=(0, y0 - 0.55),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["warn"],
            "lw": 2,
        },
    )
    ax.text(
        0,
        y0 - 0.75,
        "offset = 0",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- State 1: After alloc(32) — 2 cells wide ---
    y1 = 4.8
    alloc1_w = 4.0  # 32 bytes = 4 cells of 8 bytes each, visually
    ax.text(
        -1.2,
        y1 + cell_h / 2,
        "alloc(32)",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="right",
        va="center",
    )

    # Allocated region
    r1 = Rectangle(
        (block_x, y1),
        alloc1_w,
        cell_h,
        facecolor=STYLE["accent1"],
        alpha=0.2,
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
    )
    ax.add_patch(r1)
    ax.text(
        alloc1_w / 2,
        y1 + cell_h / 2,
        "32 bytes",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Free region
    r1f = Rectangle(
        (alloc1_w, y1),
        block_w - alloc1_w,
        cell_h,
        facecolor=STYLE["surface"],
        alpha=0.3,
        edgecolor=STYLE["grid"],
        linewidth=1.5,
        linestyle="--",
    )
    ax.add_patch(r1f)

    # Pointer
    ax.annotate(
        "",
        xy=(alloc1_w, y1 - 0.05),
        xytext=(alloc1_w, y1 - 0.55),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["warn"],
            "lw": 2,
        },
    )
    ax.text(
        alloc1_w,
        y1 - 0.75,
        "offset = 32",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- State 2: After alloc(64) — 8 cells wide ---
    y2 = 2.6
    alloc2_w = 8.0
    ax.text(
        -1.2,
        y2 + cell_h / 2,
        "alloc(64)",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="right",
        va="center",
    )

    # First alloc still there
    r2a = Rectangle(
        (block_x, y2),
        alloc1_w,
        cell_h,
        facecolor=STYLE["accent1"],
        alpha=0.2,
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
    )
    ax.add_patch(r2a)
    ax.text(
        alloc1_w / 2,
        y2 + cell_h / 2,
        "32 B",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Second alloc
    r2b = Rectangle(
        (alloc1_w, y2),
        alloc2_w,
        cell_h,
        facecolor=STYLE["accent3"],
        alpha=0.2,
        edgecolor=STYLE["accent3"],
        linewidth=1.5,
    )
    ax.add_patch(r2b)
    ax.text(
        alloc1_w + alloc2_w / 2,
        y2 + cell_h / 2,
        "64 bytes",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Free region
    r2f = Rectangle(
        (alloc1_w + alloc2_w, y2),
        block_w - alloc1_w - alloc2_w,
        cell_h,
        facecolor=STYLE["surface"],
        alpha=0.3,
        edgecolor=STYLE["grid"],
        linewidth=1.5,
        linestyle="--",
    )
    ax.add_patch(r2f)

    # Pointer
    ptr2_x = alloc1_w + alloc2_w
    ax.annotate(
        "",
        xy=(ptr2_x, y2 - 0.05),
        xytext=(ptr2_x, y2 - 0.55),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["warn"],
            "lw": 2,
        },
    )
    ax.text(
        ptr2_x,
        y2 - 0.75,
        "offset = 96",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- State 3: After alloc(16) ---
    y3 = 0.4
    alloc3_w = 2.0
    ax.text(
        -1.2,
        y3 + cell_h / 2,
        "alloc(16)",
        color=STYLE["accent4"],
        fontsize=9,
        fontweight="bold",
        ha="right",
        va="center",
    )

    r3a = Rectangle(
        (block_x, y3),
        alloc1_w,
        cell_h,
        facecolor=STYLE["accent1"],
        alpha=0.2,
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
    )
    ax.add_patch(r3a)
    ax.text(
        alloc1_w / 2,
        y3 + cell_h / 2,
        "32 B",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    r3b = Rectangle(
        (alloc1_w, y3),
        alloc2_w,
        cell_h,
        facecolor=STYLE["accent3"],
        alpha=0.2,
        edgecolor=STYLE["accent3"],
        linewidth=1.5,
    )
    ax.add_patch(r3b)
    ax.text(
        alloc1_w + alloc2_w / 2,
        y3 + cell_h / 2,
        "64 B",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    r3c = Rectangle(
        (ptr2_x, y3),
        alloc3_w,
        cell_h,
        facecolor=STYLE["accent4"],
        alpha=0.2,
        edgecolor=STYLE["accent4"],
        linewidth=1.5,
    )
    ax.add_patch(r3c)
    ax.text(
        ptr2_x + alloc3_w / 2,
        y3 + cell_h / 2,
        "16 B",
        color=STYLE["accent4"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Remaining free
    free_x = ptr2_x + alloc3_w
    r3f = Rectangle(
        (free_x, y3),
        block_w - free_x,
        cell_h,
        facecolor=STYLE["surface"],
        alpha=0.3,
        edgecolor=STYLE["grid"],
        linewidth=1.5,
        linestyle="--",
    )
    ax.add_patch(r3f)

    # Pointer
    ax.annotate(
        "",
        xy=(free_x, y3 - 0.05),
        xytext=(free_x, y3 - 0.55),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["warn"],
            "lw": 2,
        },
    )
    ax.text(
        free_x,
        y3 - 0.75,
        "offset = 112",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Key insight at bottom
    ax.text(
        8.25,
        -0.7,
        "Each alloc bumps the offset forward \u2014 no free list, no fragmentation",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/12-memory-arenas", "bump_allocation.png")


# ---------------------------------------------------------------------------
# engine/12-memory-arenas — alignment_padding.png
# ---------------------------------------------------------------------------
def diagram_alignment_padding():
    """Show how alignment rounds up the allocation pointer, creating padding."""
    fig, ax = plt.subplots(figsize=(12, 7), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1, 19)
    ax.set_ylim(-1.5, 9)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        9,
        8.7,
        "Alignment Padding in Arena Allocation",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Each visual cell = 1 byte.  Show 16 bytes of a block.
    cell_w = 1.0
    cell_h = 1.3
    num_bytes = 16

    def draw_byte_row(y, fills, pointer_at, label_left, note_right=None):
        """Draw a row of byte cells with coloring and a pointer arrow."""
        for i in range(num_bytes):
            color, alpha_val, txt = fills.get(i, (STYLE["surface"], 0.2, ""))
            r = Rectangle(
                (i * cell_w, y),
                cell_w,
                cell_h,
                facecolor=color,
                alpha=alpha_val,
                edgecolor=STYLE["grid"],
                linewidth=0.8,
            )
            ax.add_patch(r)
            if txt:
                ax.text(
                    i * cell_w + cell_w / 2,
                    y + cell_h / 2,
                    txt,
                    color=color,
                    fontsize=8,
                    ha="center",
                    va="center",
                    path_effects=stroke,
                )

        # Label on left
        ax.text(
            -0.3,
            y + cell_h / 2,
            label_left,
            color=STYLE["text_dim"],
            fontsize=9,
            fontweight="bold",
            ha="right",
            va="center",
        )

        # Pointer arrow below
        if pointer_at is not None:
            ax.annotate(
                "",
                xy=(pointer_at * cell_w, y - 0.05),
                xytext=(pointer_at * cell_w, y - 0.5),
                arrowprops={
                    "arrowstyle": "->,head_width=0.18,head_length=0.1",
                    "color": STYLE["warn"],
                    "lw": 1.8,
                },
            )
            ax.text(
                pointer_at * cell_w,
                y - 0.65,
                f"offset {pointer_at}",
                color=STYLE["warn"],
                fontsize=7,
                fontweight="bold",
                ha="center",
                va="top",
                path_effects=stroke,
            )

        if note_right:
            ax.text(
                num_bytes * cell_w + 0.3,
                y + cell_h / 2,
                note_right,
                color=STYLE["text_dim"],
                fontsize=8,
                ha="left",
                va="center",
            )

    # Byte offset ruler at top
    for i in range(num_bytes + 1):
        ax.text(
            i * cell_w,
            8.0,
            str(i),
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="bottom",
        )

    # --- Row 1: alloc(3) with default 8-byte alignment ---
    fills1 = {}
    for i in range(3):
        fills1[i] = (STYLE["accent1"], 0.3, "D")
    draw_byte_row(
        6.2,
        fills1,
        pointer_at=3,
        label_left="alloc(3)",
        note_right="3 bytes used",
    )

    # --- Row 2: Next alloc(4), align=8 — must skip to offset 8 ---
    fills2 = {}
    for i in range(3):
        fills2[i] = (STYLE["accent1"], 0.2, "")  # prior alloc (dimmed)
    for i in range(3, 8):
        fills2[i] = (STYLE["warn"], 0.15, "\u00d7")  # padding
    for i in range(8, 12):
        fills2[i] = (STYLE["accent3"], 0.3, "D")  # new alloc
    draw_byte_row(
        3.8,
        fills2,
        pointer_at=12,
        label_left="alloc(4)\nalign=8",
    )

    # Annotate the padding span
    pad_y = 3.8 + cell_h + 0.1
    ax.annotate(
        "",
        xy=(3 * cell_w, pad_y),
        xytext=(8 * cell_w, pad_y),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.2,widthB=0.2",
            "color": STYLE["warn"],
            "lw": 1.2,
        },
    )
    ax.text(
        5.5 * cell_w,
        pad_y + 0.2,
        "5 bytes padding (skip to align 8)",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # Annotate the aligned start
    ax.text(
        8 * cell_w,
        3.8 - 0.85,
        "\u2191 offset 8 is divisible by 8",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- Row 3: Contrast — alloc(4), align=4 from same offset 3 ---
    fills3 = {}
    for i in range(3):
        fills3[i] = (STYLE["accent1"], 0.2, "")
    fills3[3] = (STYLE["warn"], 0.15, "\u00d7")  # 1 byte padding
    for i in range(4, 8):
        fills3[i] = (STYLE["accent2"], 0.3, "D")
    draw_byte_row(
        1.4,
        fills3,
        pointer_at=8,
        label_left="alloc(4)\nalign=4",
    )

    # Annotate smaller padding
    pad_y2 = 1.4 + cell_h + 0.1
    ax.annotate(
        "",
        xy=(3 * cell_w, pad_y2),
        xytext=(4 * cell_w, pad_y2),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.2,widthB=0.2",
            "color": STYLE["warn"],
            "lw": 1.2,
        },
    )
    ax.text(
        3.5 * cell_w,
        pad_y2 + 0.2,
        "1 byte",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # Legend
    legend_y = -0.3
    items = [
        ("D", STYLE["accent1"], "Allocated data"),
        ("\u00d7", STYLE["warn"], "Alignment padding (wasted)"),
    ]
    x_leg = 2.0
    for symbol, color, desc in items:
        r = Rectangle(
            (x_leg, legend_y),
            0.7,
            0.7,
            facecolor=color,
            alpha=0.25,
            edgecolor=color,
            linewidth=1,
        )
        ax.add_patch(r)
        ax.text(
            x_leg + 0.35,
            legend_y + 0.35,
            symbol,
            color=color,
            fontsize=9,
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            x_leg + 1.0,
            legend_y + 0.35,
            desc,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="left",
            va="center",
        )
        x_leg += 6.0

    # Key insight
    ax.text(
        9,
        -1.2,
        "Higher alignment = more potential padding.  Default 8-byte alignment matches"
        " most CPU data types.",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/12-memory-arenas", "alignment_padding.png")


# ---------------------------------------------------------------------------
# engine/12-memory-arenas — block_chain_growth.png
# ---------------------------------------------------------------------------
def diagram_block_chain_growth():
    """Show how an arena grows via a linked list of blocks."""
    fig, ax = plt.subplots(figsize=(12, 6), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-2, 18)
    ax.set_ylim(-1.5, 7)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        8,
        6.7,
        "Arena Block Chain: Automatic Growth",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # ForgeArena struct box
    arena_x, arena_y = -1.5, 2.5
    arena_w, arena_h = 3.0, 2.5
    ra = FancyBboxPatch(
        (arena_x, arena_y),
        arena_w,
        arena_h,
        boxstyle="round,pad=0.12",
        facecolor=STYLE["surface"],
        alpha=0.5,
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
    )
    ax.add_patch(ra)
    ax.text(
        arena_x + arena_w / 2,
        arena_y + arena_h - 0.25,
        "ForgeArena",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )
    ax.text(
        arena_x + 0.2,
        arena_y + arena_h - 0.7,
        "first \u2192\ncurrent \u2192\ndefault_size",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="left",
        va="top",
        family="monospace",
    )

    # --- Block 1 (full) ---
    b1_x, b1_y = 3.5, 3.0
    b1_w, b1_h = 4.0, 2.0
    r1 = FancyBboxPatch(
        (b1_x, b1_y),
        b1_w,
        b1_h,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["accent1"],
        alpha=0.12,
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
    )
    ax.add_patch(r1)
    ax.text(
        b1_x + b1_w / 2,
        b1_y + b1_h - 0.25,
        "Block 1 (256 B)",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Show block as filled
    fill1 = Rectangle(
        (b1_x + 0.15, b1_y + 0.2),
        b1_w - 0.3,
        0.7,
        facecolor=STYLE["accent1"],
        alpha=0.25,
        edgecolor=STYLE["accent1"],
        linewidth=0.8,
    )
    ax.add_patch(fill1)
    ax.text(
        b1_x + b1_w / 2,
        b1_y + 0.55,
        "FULL",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # "next" pointer from block 1 to block 2
    ax.text(
        b1_x + b1_w - 0.3,
        b1_y + b1_h - 0.6,
        "next \u2192",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="right",
        va="center",
        family="monospace",
    )

    # --- Block 2 (full) ---
    b2_x = b1_x + b1_w + 1.5
    b2_w = 4.0
    r2 = FancyBboxPatch(
        (b2_x, b1_y),
        b2_w,
        b1_h,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["accent3"],
        alpha=0.12,
        edgecolor=STYLE["accent3"],
        linewidth=1.5,
    )
    ax.add_patch(r2)
    ax.text(
        b2_x + b2_w / 2,
        b1_y + b1_h - 0.25,
        "Block 2 (256 B)",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    fill2 = Rectangle(
        (b2_x + 0.15, b1_y + 0.2),
        b2_w - 0.3,
        0.7,
        facecolor=STYLE["accent3"],
        alpha=0.25,
        edgecolor=STYLE["accent3"],
        linewidth=0.8,
    )
    ax.add_patch(fill2)
    ax.text(
        b2_x + b2_w / 2,
        b1_y + 0.55,
        "FULL",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    ax.text(
        b2_x + b2_w - 0.3,
        b1_y + b1_h - 0.6,
        "next \u2192",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="right",
        va="center",
        family="monospace",
    )

    # --- Block 3 (current, partially filled) ---
    b3_x = b2_x + b2_w + 1.5
    b3_w = 4.0
    r3 = FancyBboxPatch(
        (b3_x, b1_y),
        b3_w,
        b1_h,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["accent2"],
        alpha=0.12,
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
    )
    ax.add_patch(r3)
    ax.text(
        b3_x + b3_w / 2,
        b1_y + b1_h - 0.25,
        "Block 3 (256 B)",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Partial fill
    fill_frac = 0.4
    fill3 = Rectangle(
        (b3_x + 0.15, b1_y + 0.2),
        (b3_w - 0.3) * fill_frac,
        0.7,
        facecolor=STYLE["accent2"],
        alpha=0.25,
        edgecolor=STYLE["accent2"],
        linewidth=0.8,
    )
    ax.add_patch(fill3)
    free3 = Rectangle(
        (b3_x + 0.15 + (b3_w - 0.3) * fill_frac, b1_y + 0.2),
        (b3_w - 0.3) * (1 - fill_frac),
        0.7,
        facecolor=STYLE["surface"],
        alpha=0.3,
        edgecolor=STYLE["grid"],
        linewidth=0.8,
        linestyle="--",
    )
    ax.add_patch(free3)
    ax.text(
        b3_x + b3_w / 2,
        b1_y + 0.55,
        "40% used",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    ax.text(
        b3_x + b3_w - 0.3,
        b1_y + b1_h - 0.6,
        "next = NULL",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="right",
        va="center",
        family="monospace",
    )

    # --- Arrows connecting blocks ---
    arrow_y = b1_y + b1_h / 2
    for sx, sw, tx in [(b1_x, b1_w, b2_x), (b2_x, b2_w, b3_x)]:
        ax.annotate(
            "",
            xy=(tx - 0.05, arrow_y),
            xytext=(sx + sw + 0.05, arrow_y),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.15",
                "color": STYLE["text"],
                "lw": 2,
                "connectionstyle": "arc3,rad=0",
            },
        )

    # Arrow from arena.first to block 1
    ax.annotate(
        "",
        xy=(b1_x - 0.05, b1_y + b1_h - 0.5),
        xytext=(arena_x + arena_w + 0.05, arena_y + arena_h - 0.65),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["accent1"],
            "lw": 1.5,
        },
    )

    # Arrow from arena.current to block 3
    ax.annotate(
        "",
        xy=(b3_x + 0.5, b1_y + b1_h + 0.05),
        xytext=(arena_x + arena_w + 0.05, arena_y + arena_h - 1.1),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["accent2"],
            "lw": 1.5,
            "connectionstyle": "arc3,rad=-0.25",
        },
    )

    # Labels for the arrows
    ax.text(
        2.2,
        5.15,
        "first",
        color=STYLE["accent1"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )
    ax.text(
        8.5,
        5.7,
        "current",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # Timeline annotation
    ax.text(
        b1_x + b1_w / 2,
        b1_y - 0.3,
        "allocated first",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="top",
    )
    ax.text(
        b2_x + b2_w / 2,
        b1_y - 0.3,
        "grew when\nblock 1 filled",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="top",
    )
    ax.text(
        b3_x + b3_w / 2,
        b1_y - 0.3,
        "grew when\nblock 2 filled",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="top",
    )

    # Key insight
    ax.text(
        8,
        -0.8,
        "destroy() walks the chain and frees every block \u2014 one call frees"
        " all allocations",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/12-memory-arenas", "block_chain_growth.png")
