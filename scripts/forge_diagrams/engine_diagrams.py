"""Diagram functions for engine lessons (lessons/engine/)."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Polygon, Rectangle

from ._common import STYLE, save, setup_axes


# ---------------------------------------------------------------------------
# engine/04-pointers-and-memory — stack_vs_heap.png
# ---------------------------------------------------------------------------
def diagram_stack_vs_heap():
    """Stack vs heap memory layout with address ranges and growth directions."""
    fig, ax = plt.subplots(figsize=(10, 8), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 11.5)
    ax.set_ylim(-0.5, 12.0)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Title
    ax.text(
        5.5,
        11.5,
        "Memory Layout of a Running Program",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- Stack column (right side) ---
    stack_x = 6.5
    stack_w = 3.5
    stack_blocks = [
        ("main()", "argc, argv\nstack_var, arr[4]", STYLE["accent1"], 8.0, 1.6),
        ("demo_pointers()", "x, p, null_ptr", STYLE["accent1"], 6.0, 1.6),
        ("(free space)", "", STYLE["grid"], 2.5, 3.1),
    ]

    ax.text(
        stack_x + stack_w / 2,
        10.8,
        "STACK",
        color=STYLE["accent1"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )
    ax.text(
        stack_x + stack_w / 2,
        10.55,
        "grows downward",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="bottom",
    )

    # Down arrow for stack growth
    ax.annotate(
        "",
        xy=(stack_x + stack_w + 0.3, 5.2),
        xytext=(stack_x + stack_w + 0.3, 9.8),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["accent1"],
            "lw": 2,
        },
    )

    for label, detail, color, y, h in stack_blocks:
        r = FancyBboxPatch(
            (stack_x, y),
            stack_w,
            h,
            boxstyle="round,pad=0.1",
            facecolor=color,
            alpha=0.15,
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(r)
        ax.text(
            stack_x + 0.2,
            y + h - 0.25,
            label,
            color=color if color != STYLE["grid"] else STYLE["text_dim"],
            fontsize=10,
            fontweight="bold",
            va="top",
            path_effects=stroke,
        )
        if detail:
            ax.text(
                stack_x + 0.2,
                y + h - 0.65,
                detail,
                color=STYLE["text_dim"],
                fontsize=8,
                va="top",
            )

    # --- Heap column (left side) ---
    heap_x = 0.5
    heap_w = 3.5

    ax.text(
        heap_x + heap_w / 2,
        1.0,
        "HEAP",
        color=STYLE["accent2"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )
    ax.text(
        heap_x + heap_w / 2,
        0.75,
        "grows upward",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="bottom",
    )

    heap_blocks = [
        ("SDL_malloc(16)", "heap_array\n4 floats", STYLE["accent2"], 1.5, 1.4),
        ("SDL_malloc(60)", "vertex buffer\n3 vertices", STYLE["accent3"], 3.2, 1.6),
        ("(free space)", "", STYLE["grid"], 5.1, 3.3),
    ]

    # Up arrow for heap growth
    ax.annotate(
        "",
        xy=(heap_x - 0.3, 5.7),
        xytext=(heap_x - 0.3, 1.5),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["accent2"],
            "lw": 2,
        },
    )

    for label, detail, color, y, h in heap_blocks:
        r = FancyBboxPatch(
            (heap_x, y),
            heap_w,
            h,
            boxstyle="round,pad=0.1",
            facecolor=color,
            alpha=0.15,
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(r)
        ax.text(
            heap_x + 0.2,
            y + h - 0.25,
            label,
            color=color if color != STYLE["grid"] else STYLE["text_dim"],
            fontsize=10,
            fontweight="bold",
            va="top",
            path_effects=stroke,
        )
        if detail:
            ax.text(
                heap_x + 0.2,
                y + h - 0.65,
                detail,
                color=STYLE["text_dim"],
                fontsize=8,
                va="top",
            )

    # Address labels on edges
    ax.text(
        stack_x + stack_w + 0.6,
        9.5,
        "0x7FFF...\n(high)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
    )
    ax.text(
        heap_x - 0.3,
        1.3,
        "0x0055...\n(low)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
    )

    # Center divider + labels
    ax.plot([5.0, 5.0], [0.5, 10.5], "--", color=STYLE["grid"], lw=0.8, alpha=0.5)

    # Key properties
    props_y = 9.8
    ax.text(
        heap_x + heap_w / 2,
        props_y,
        "Manual lifetime",
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )
    ax.text(
        heap_x + heap_w / 2,
        props_y - 0.35,
        "malloc / free",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )
    ax.text(
        heap_x + heap_w / 2,
        props_y - 0.65,
        "Large allocations OK",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )

    ax.text(
        stack_x + stack_w / 2,
        1.5,
        "Automatic lifetime",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )
    ax.text(
        stack_x + stack_w / 2,
        1.15,
        "Freed on function return",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )
    ax.text(
        stack_x + stack_w / 2,
        0.85,
        "Limited size (1-8 MB)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )

    fig.tight_layout()
    save(fig, "engine/04-pointers-and-memory", "stack_vs_heap.png")


# ---------------------------------------------------------------------------
# engine/04-pointers-and-memory — vertex_memory_layout.png
# ---------------------------------------------------------------------------
def diagram_vertex_memory_layout():
    """Show byte-level layout of a vertex struct as the GPU sees it."""
    fig, ax = plt.subplots(figsize=(11, 6), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1, 22)
    ax.set_ylim(-2.5, 7)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        10.5,
        6.5,
        "Vertex Struct Memory Layout",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- Single vertex layout (top row) ---
    y_top = 3.5
    cell_w = 1.0
    cell_h = 1.2

    # Draw byte cells for one vertex (20 bytes)
    fields = [
        ("px", 0, 4, STYLE["accent1"]),
        ("py", 4, 4, STYLE["accent1"]),
        ("r", 8, 4, STYLE["accent2"]),
        ("g", 12, 4, STYLE["accent2"]),
        ("b", 16, 4, STYLE["accent2"]),
    ]

    for name, offset, size, color in fields:
        for i in range(size):
            x = offset + i
            r = Rectangle(
                (x, y_top),
                cell_w,
                cell_h,
                facecolor=color,
                alpha=0.2,
                edgecolor=color,
                linewidth=1.0,
            )
            ax.add_patch(r)

        # Field name centered in the field
        cx = offset + size / 2
        ax.text(
            cx,
            y_top + cell_h / 2,
            name,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Offset labels below
    for offset in [0, 4, 8, 12, 16, 20]:
        ax.text(
            offset,
            y_top - 0.25,
            str(offset),
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="top",
        )

    # Bracket labels
    ax.annotate(
        "",
        xy=(0, y_top + cell_h + 0.1),
        xytext=(8, y_top + cell_h + 0.1),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.3,widthB=0.3",
            "color": STYLE["accent1"],
            "lw": 1.5,
        },
    )
    ax.text(
        4,
        y_top + cell_h + 0.35,
        "position (8 bytes)",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    ax.annotate(
        "",
        xy=(8, y_top + cell_h + 0.1),
        xytext=(20, y_top + cell_h + 0.1),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.3,widthB=0.3",
            "color": STYLE["accent2"],
            "lw": 1.5,
        },
    )
    ax.text(
        14,
        y_top + cell_h + 0.35,
        "color (12 bytes)",
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # Total size
    ax.annotate(
        "",
        xy=(0, y_top - 0.6),
        xytext=(20, y_top - 0.6),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.3,widthB=0.3",
            "color": STYLE["text"],
            "lw": 1.5,
        },
    )
    ax.text(
        10,
        y_top - 0.85,
        "sizeof(Vertex) = 20 bytes  =  stride",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- Three vertices in a buffer (bottom row) ---
    y_bot = 0.3
    cell_h2 = 1.0

    ax.text(
        -0.5,
        y_bot + cell_h2 + 0.6,
        "Vertex buffer (3 vertices = 60 bytes):",
        color=STYLE["text"],
        fontsize=11,
        ha="left",
        path_effects=stroke,
    )

    vert_colors = [STYLE["accent1"], STYLE["accent3"], STYLE["accent4"]]
    for v in range(3):
        x_start = v * 7  # visual spacing between vertices
        for byte in range(20):
            alpha = (
                0.25 if byte < 8 else 0.12
            )  # Alternate shading for position vs color

            x = x_start + byte * (7.0 / 20.0)
            r = Rectangle(
                (x, y_bot),
                7.0 / 20.0,
                cell_h2,
                facecolor=vert_colors[v],
                alpha=alpha,
                edgecolor=vert_colors[v],
                linewidth=0.5,
            )
            ax.add_patch(r)

        # Vertex label
        ax.text(
            x_start + 3.5,
            y_bot + cell_h2 / 2,
            f"Vertex {v}",
            color=vert_colors[v],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

        # Byte offset label
        byte_offset = v * 20
        ax.text(
            x_start,
            y_bot - 0.2,
            f"byte {byte_offset}",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="left",
            va="top",
        )

    ax.text(
        21,
        y_bot - 0.2,
        "byte 60",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="left",
        va="top",
    )

    # Side annotation
    ax.text(
        -0.5,
        y_bot - 0.8,
        "GPU reads: stride = 20, offset(position) = 0, offset(color) = 8",
        color=STYLE["warn"],
        fontsize=9,
        ha="left",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/04-pointers-and-memory", "vertex_memory_layout.png")


# ---------------------------------------------------------------------------
# engine/04-pointers-and-memory — pointer_arithmetic.png
# ---------------------------------------------------------------------------
def diagram_pointer_arithmetic():
    """Visualize how pointer arithmetic scales by element size."""
    fig, ax = plt.subplots(figsize=(11, 6.5), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1, 22)
    ax.set_ylim(-0.8, 8.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        10.5,
        8.2,
        "Pointer Arithmetic: Scaling by Element Size",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- int array (4 bytes per element) ---
    y_int = 3.5
    cell_w = 3.0
    cell_h = 1.2

    ax.text(
        -0.8,
        y_int + cell_h / 2,
        "int arr[5]",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="right",
        va="center",
        path_effects=stroke,
    )

    int_vals = [10, 20, 30, 40, 50]
    for i, val in enumerate(int_vals):
        x = i * cell_w
        r = Rectangle(
            (x, y_int),
            cell_w,
            cell_h,
            facecolor=STYLE["accent1"],
            alpha=0.15,
            edgecolor=STYLE["accent1"],
            linewidth=1.5,
        )
        ax.add_patch(r)
        ax.text(
            x + cell_w / 2,
            y_int + cell_h / 2,
            str(val),
            color=STYLE["accent1"],
            fontsize=12,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        # Byte address below
        ax.text(
            x,
            y_int - 0.2,
            f"+{i * 4}",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="top",
        )

    # p+0, p+1, etc. labels above
    for i in range(5):
        x = i * cell_w + cell_w / 2
        ax.text(
            x,
            y_int + cell_h + 0.1,
            f"p+{i}",
            color=STYLE["accent2"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="bottom",
            path_effects=stroke,
        )

    # Arrow showing +4 bytes per step
    for i in range(4):
        x1 = i * cell_w + cell_w / 2
        x2 = (i + 1) * cell_w + cell_w / 2
        ax.annotate(
            "",
            xy=(x2, y_int + cell_h + 0.55),
            xytext=(x1, y_int + cell_h + 0.55),
            arrowprops={
                "arrowstyle": "->,head_width=0.15,head_length=0.1",
                "color": STYLE["warn"],
                "lw": 1.5,
            },
        )

    ax.text(
        7.5,
        y_int + cell_h + 0.9,
        "+4 bytes each step (sizeof(int))",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    # --- Vertex array (20 bytes per element) ---
    y_vert = 0.5
    vert_w = 5.0
    vert_h = 1.2

    ax.text(
        -0.8,
        y_vert + vert_h / 2,
        "Vertex v[3]",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="right",
        va="center",
        path_effects=stroke,
    )

    for i in range(3):
        x = i * vert_w
        r = Rectangle(
            (x, y_vert),
            vert_w,
            vert_h,
            facecolor=STYLE["accent3"],
            alpha=0.15,
            edgecolor=STYLE["accent3"],
            linewidth=1.5,
        )
        ax.add_patch(r)
        ax.text(
            x + vert_w / 2,
            y_vert + vert_h / 2,
            f"Vertex {i}  (20 B)",
            color=STYLE["accent3"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            x,
            y_vert - 0.2,
            f"+{i * 20}",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="top",
        )

    # v+0, v+1, v+2 labels above
    for i in range(3):
        x = i * vert_w + vert_w / 2
        ax.text(
            x,
            y_vert + vert_h + 0.1,
            f"v+{i}",
            color=STYLE["accent2"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="bottom",
            path_effects=stroke,
        )

    # Arrow showing +20 bytes per step
    for i in range(2):
        x1 = i * vert_w + vert_w / 2
        x2 = (i + 1) * vert_w + vert_w / 2
        ax.annotate(
            "",
            xy=(x2, y_vert + vert_h + 0.55),
            xytext=(x1, y_vert + vert_h + 0.55),
            arrowprops={
                "arrowstyle": "->,head_width=0.15,head_length=0.1",
                "color": STYLE["warn"],
                "lw": 1.5,
            },
        )

    ax.text(
        5.0,
        y_vert + vert_h + 0.9,
        "+20 bytes each step (sizeof(Vertex))",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/04-pointers-and-memory", "pointer_arithmetic.png")


# ---------------------------------------------------------------------------
# engine/04-pointers-and-memory — struct_padding.png
# ---------------------------------------------------------------------------
def diagram_struct_padding():
    """Show struct member alignment with padding bytes highlighted."""
    fig, axes = plt.subplots(1, 2, figsize=(12, 5), facecolor=STYLE["bg"])

    for ax in axes:
        ax.set_facecolor(STYLE["bg"])
        ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    fig.suptitle(
        "Struct Padding and Alignment",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=0.97,
    )

    # --- Left: PaddedExample (bad ordering) ---
    ax = axes[0]
    ax.set_xlim(-1, 13)
    ax.set_ylim(-2, 6)

    ax.text(
        6,
        5.5,
        "PaddedExample (12 bytes)",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )
    ax.text(
        6,
        5.0,
        "char, float, char",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
    )

    # Draw 12 byte cells
    cell_w = 1.0
    cell_h = 1.3
    y = 2.5

    byte_info = [
        (0, "tag", STYLE["accent1"]),
        (1, "pad", STYLE["warn"]),
        (2, "pad", STYLE["warn"]),
        (3, "pad", STYLE["warn"]),
        (4, "value", STYLE["accent3"]),
        (5, "value", STYLE["accent3"]),
        (6, "value", STYLE["accent3"]),
        (7, "value", STYLE["accent3"]),
        (8, "flag", STYLE["accent4"]),
        (9, "pad", STYLE["warn"]),
        (10, "pad", STYLE["warn"]),
        (11, "pad", STYLE["warn"]),
    ]

    for offset, name, color in byte_info:
        alpha = 0.3 if name != "pad" else 0.15
        r = Rectangle(
            (offset, y),
            cell_w,
            cell_h,
            facecolor=color,
            alpha=alpha,
            edgecolor=color,
            linewidth=1.0,
        )
        ax.add_patch(r)
        if name == "pad":
            ax.text(
                offset + 0.5,
                y + cell_h / 2,
                "X",
                color=STYLE["warn"],
                fontsize=10,
                ha="center",
                va="center",
                alpha=0.6,
            )

    # Field labels with brackets
    ax.annotate(
        "",
        xy=(0, y + cell_h + 0.05),
        xytext=(1, y + cell_h + 0.05),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.2,widthB=0.2",
            "color": STYLE["accent1"],
            "lw": 1,
        },
    )
    ax.text(
        0.5,
        y + cell_h + 0.25,
        "tag",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="bottom",
    )

    ax.annotate(
        "",
        xy=(1, y + cell_h + 0.05),
        xytext=(4, y + cell_h + 0.05),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.2,widthB=0.2",
            "color": STYLE["warn"],
            "lw": 1,
        },
    )
    ax.text(
        2.5,
        y + cell_h + 0.25,
        "3B pad",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="bottom",
    )

    ax.annotate(
        "",
        xy=(4, y + cell_h + 0.05),
        xytext=(8, y + cell_h + 0.05),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.2,widthB=0.2",
            "color": STYLE["accent3"],
            "lw": 1,
        },
    )
    ax.text(
        6,
        y + cell_h + 0.25,
        "value (float)",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="bottom",
    )

    ax.annotate(
        "",
        xy=(8, y + cell_h + 0.05),
        xytext=(9, y + cell_h + 0.05),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.2,widthB=0.2",
            "color": STYLE["accent4"],
            "lw": 1,
        },
    )
    ax.text(
        8.5,
        y + cell_h + 0.25,
        "flag",
        color=STYLE["accent4"],
        fontsize=8,
        ha="center",
        va="bottom",
    )

    ax.annotate(
        "",
        xy=(9, y + cell_h + 0.05),
        xytext=(12, y + cell_h + 0.05),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.2,widthB=0.2",
            "color": STYLE["warn"],
            "lw": 1,
        },
    )
    ax.text(
        10.5,
        y + cell_h + 0.25,
        "3B trail",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="bottom",
    )

    # Byte offset numbers
    for i in range(13):
        ax.text(
            i,
            y - 0.15,
            str(i),
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="top",
        )

    ax.text(
        6,
        y - 0.7,
        "6 bytes wasted on padding!",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # --- Right: Optimal ordering ---
    ax = axes[1]
    ax.set_xlim(-1, 13)
    ax.set_ylim(-2, 6)

    ax.text(
        4,
        5.5,
        "OptimalLayout (8 bytes)",
        color=STYLE["accent3"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )
    ax.text(
        4,
        5.0,
        "float, char, char",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
    )

    opt_bytes = [
        (0, "value", STYLE["accent3"]),
        (1, "value", STYLE["accent3"]),
        (2, "value", STYLE["accent3"]),
        (3, "value", STYLE["accent3"]),
        (4, "tag", STYLE["accent1"]),
        (5, "flag", STYLE["accent4"]),
        (6, "pad", STYLE["warn"]),
        (7, "pad", STYLE["warn"]),
    ]

    for offset, name, color in opt_bytes:
        alpha = 0.3 if name != "pad" else 0.15
        r = Rectangle(
            (offset, y),
            cell_w,
            cell_h,
            facecolor=color,
            alpha=alpha,
            edgecolor=color,
            linewidth=1.0,
        )
        ax.add_patch(r)
        if name == "pad":
            ax.text(
                offset + 0.5,
                y + cell_h / 2,
                "X",
                color=STYLE["warn"],
                fontsize=10,
                ha="center",
                va="center",
                alpha=0.6,
            )

    # Labels
    ax.annotate(
        "",
        xy=(0, y + cell_h + 0.05),
        xytext=(4, y + cell_h + 0.05),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.2,widthB=0.2",
            "color": STYLE["accent3"],
            "lw": 1,
        },
    )
    ax.text(
        2,
        y + cell_h + 0.25,
        "value (float)",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="bottom",
    )

    ax.annotate(
        "",
        xy=(4, y + cell_h + 0.05),
        xytext=(5, y + cell_h + 0.05),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.2,widthB=0.2",
            "color": STYLE["accent1"],
            "lw": 1,
        },
    )
    ax.text(
        4.5,
        y + cell_h + 0.25,
        "tag",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="bottom",
    )

    ax.annotate(
        "",
        xy=(5, y + cell_h + 0.05),
        xytext=(6, y + cell_h + 0.05),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.2,widthB=0.2",
            "color": STYLE["accent4"],
            "lw": 1,
        },
    )
    ax.text(
        5.5,
        y + cell_h + 0.25,
        "flag",
        color=STYLE["accent4"],
        fontsize=8,
        ha="center",
        va="bottom",
    )

    ax.annotate(
        "",
        xy=(6, y + cell_h + 0.05),
        xytext=(8, y + cell_h + 0.05),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.2,widthB=0.2",
            "color": STYLE["warn"],
            "lw": 1,
        },
    )
    ax.text(
        7,
        y + cell_h + 0.25,
        "2B pad",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="bottom",
    )

    for i in range(9):
        ax.text(
            i,
            y - 0.15,
            str(i),
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="top",
        )

    ax.text(
        4,
        y - 0.7,
        "Only 2 bytes of padding",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/04-pointers-and-memory", "struct_padding.png")


# ---------------------------------------------------------------------------
# engine/04-pointers-and-memory — gpu_upload_pipeline.png
# ---------------------------------------------------------------------------
def diagram_gpu_upload_pipeline():
    """Show the CPU-to-GPU transfer buffer upload pipeline."""
    fig, ax = plt.subplots(figsize=(12, 5), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1, 24)
    ax.set_ylim(-1, 6)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        11.5,
        5.5,
        "Vertex Data Upload: CPU to GPU",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Box dimensions
    bw, bh = 4.5, 2.5
    y_mid = 1.2

    # --- Box 1: CPU vertex array ---
    x1 = 0.0
    r1 = FancyBboxPatch(
        (x1, y_mid),
        bw,
        bh,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["accent1"],
        alpha=0.15,
        edgecolor=STYLE["accent1"],
        linewidth=2,
    )
    ax.add_patch(r1)
    ax.text(
        x1 + bw / 2,
        y_mid + bh - 0.35,
        "Vertex Array",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )
    ax.text(
        x1 + bw / 2,
        y_mid + bh / 2 - 0.3,
        "CPU memory\n(stack/heap)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
    )
    ax.text(
        x1 + bw / 2,
        y_mid + 0.2,
        "Vertex triangle[3]",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        alpha=0.8,
    )

    # --- Arrow 1: memcpy ---
    ax.annotate(
        "",
        xy=(x1 + bw + 2.8, y_mid + bh / 2),
        xytext=(x1 + bw + 0.3, y_mid + bh / 2),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["warn"],
            "lw": 2.5,
        },
    )
    ax.text(
        x1 + bw + 1.55,
        y_mid + bh / 2 + 0.35,
        "SDL_memcpy",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # --- Box 2: Transfer buffer ---
    x2 = x1 + bw + 3.0
    r2 = FancyBboxPatch(
        (x2, y_mid),
        bw,
        bh,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["accent3"],
        alpha=0.15,
        edgecolor=STYLE["accent3"],
        linewidth=2,
    )
    ax.add_patch(r2)
    ax.text(
        x2 + bw / 2,
        y_mid + bh - 0.35,
        "Transfer Buffer",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )
    ax.text(
        x2 + bw / 2,
        y_mid + bh / 2 - 0.3,
        "CPU-visible,\nGPU-readable staging",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
    )
    ax.text(
        x2 + bw / 2,
        y_mid + 0.2,
        "void *mapped = Map(...)",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        alpha=0.8,
    )

    # --- Arrow 2: GPU copy pass ---
    ax.annotate(
        "",
        xy=(x2 + bw + 2.8, y_mid + bh / 2),
        xytext=(x2 + bw + 0.3, y_mid + bh / 2),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["warn"],
            "lw": 2.5,
        },
    )
    ax.text(
        x2 + bw + 1.55,
        y_mid + bh / 2 + 0.35,
        "GPU Copy Pass",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # --- Box 3: GPU buffer ---
    x3 = x2 + bw + 3.0
    r3 = FancyBboxPatch(
        (x3, y_mid),
        bw,
        bh,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["accent2"],
        alpha=0.15,
        edgecolor=STYLE["accent2"],
        linewidth=2,
    )
    ax.add_patch(r3)
    ax.text(
        x3 + bw / 2,
        y_mid + bh - 0.35,
        "GPU Buffer",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )
    ax.text(
        x3 + bw / 2,
        y_mid + bh / 2 - 0.3,
        "Device-local VRAM\n(fast for GPU)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
    )
    ax.text(
        x3 + bw / 2,
        y_mid + 0.2,
        "Vertex shader reads",
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        alpha=0.8,
    )

    # Step numbers
    ax.text(
        x1 + bw / 2,
        y_mid - 0.3,
        "Step 1",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
    )
    ax.text(
        x2 + bw / 2,
        y_mid - 0.3,
        "Step 2",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
    )
    ax.text(
        x3 + bw / 2,
        y_mid - 0.3,
        "Step 3",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
    )

    # Bottom annotation
    ax.text(
        11.5,
        -0.5,
        "Pointers and memcpy are used in Steps 1-2 (the CPU side)",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/04-pointers-and-memory", "gpu_upload_pipeline.png")


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
def diagram_edge_functions():
    """Triangle with three edge function half-planes and orient2d labels."""
    fig, ax = plt.subplots(figsize=(8, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-1, 11), ylim=(-1, 11), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Triangle vertices (CCW)
    v0 = np.array([5.0, 9.0])
    v1 = np.array([1.5, 1.5])
    v2 = np.array([9.0, 2.5])

    # Fill the triangle with a subtle surface color
    tri = Polygon(
        [v0, v1, v2],
        closed=True,
        facecolor=STYLE["surface"],
        edgecolor="none",
        alpha=0.6,
    )
    ax.add_patch(tri)

    # Draw the three edges with different accent colors
    edges = [
        (v0, v1, STYLE["accent1"], "edge 0"),
        (v1, v2, STYLE["accent2"], "edge 1"),
        (v2, v0, STYLE["accent3"], "edge 2"),
    ]

    for a, b, color, _label in edges:
        ax.plot([a[0], b[0]], [a[1], b[1]], color=color, lw=2.5, zorder=3)
        mid = (a + b) / 2
        # Normal direction (pointing inward for CCW)
        d = b - a
        n = np.array([-d[1], d[0]])
        n = n / np.linalg.norm(n) * 0.6
        ax.annotate(
            "",
            xy=mid + n,
            xytext=mid,
            arrowprops={"arrowstyle": "->,head_width=0.15", "color": color, "lw": 1.5},
            zorder=4,
        )

    # Vertex labels
    for v, name, offset in [
        (v0, "v0", (0, 0.5)),
        (v1, "v1", (-0.5, -0.5)),
        (v2, "v2", (0.5, -0.5)),
    ]:
        ax.plot(v[0], v[1], "o", color=STYLE["text"], markersize=7, zorder=5)
        ax.text(
            v[0] + offset[0],
            v[1] + offset[1],
            name,
            color=STYLE["text"],
            fontsize=13,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # orient2d formula labels along each edge
    orient_labels = [
        (v0, v1, STYLE["accent1"], "orient2d(v0, v1, p)"),
        (v1, v2, STYLE["accent2"], "orient2d(v1, v2, p)"),
        (v2, v0, STYLE["accent3"], "orient2d(v2, v0, p)"),
    ]
    for a, b, color, label in orient_labels:
        mid = (a + b) / 2
        d = b - a
        n = np.array([-d[1], d[0]])
        n = n / np.linalg.norm(n) * 1.3
        ax.text(
            mid[0] + n[0],
            mid[1] + n[1],
            label,
            color=color,
            fontsize=9,
            fontstyle="italic",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Test points: one inside, one outside
    p_in = np.array([5.0, 4.5])
    p_out = np.array([1.5, 7.0])

    ax.plot(p_in[0], p_in[1], "s", color=STYLE["accent3"], markersize=10, zorder=5)
    ax.text(
        p_in[0] + 0.5,
        p_in[1] + 0.3,
        "inside\nall >= 0",
        color=STYLE["accent3"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    ax.plot(p_out[0], p_out[1], "X", color=STYLE["accent2"], markersize=10, zorder=5)
    ax.text(
        p_out[0] + 0.4,
        p_out[1] + 0.3,
        "outside\nmixed signs",
        color=STYLE["accent2"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # Title
    ax.text(
        5.0,
        10.5,
        "Edge Function Inside Test",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Formula below
    ax.text(
        5.0,
        -0.3,
        "orient2d(a, b, p) = (b.x-a.x)(p.y-a.y) - (b.y-a.y)(p.x-a.x)",
        color=STYLE["text_dim"],
        fontsize=10,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "edge_functions.png")


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — barycentric_coords.png
# ---------------------------------------------------------------------------
def diagram_barycentric_coords():
    """RGB triangle showing barycentric coordinate weights as smooth color."""
    fig, ax = plt.subplots(figsize=(8, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 10.5), ylim=(-0.5, 10.5), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Triangle vertices
    v0 = np.array([5.0, 9.5])  # red (top)
    v1 = np.array([0.5, 0.8])  # green (bottom-left)
    v2 = np.array([9.5, 0.8])  # blue (bottom-right)

    # Rasterize the triangle with barycentric colors using matplotlib
    # Create a fine grid and compute barycentric coordinates
    resolution = 400
    x = np.linspace(-0.5, 10.5, resolution)
    y = np.linspace(-0.5, 10.5, resolution)
    xx, yy = np.meshgrid(x, y)

    # Compute orient2d for each point
    def orient2d(ax_, ay, bx, by, px, py):
        return (bx - ax_) * (py - ay) - (by - ay) * (px - ax_)

    area = orient2d(v0[0], v0[1], v1[0], v1[1], v2[0], v2[1])
    w0 = orient2d(v1[0], v1[1], v2[0], v2[1], xx, yy)
    w1 = orient2d(v2[0], v2[1], v0[0], v0[1], xx, yy)
    w2 = orient2d(v0[0], v0[1], v1[0], v1[1], xx, yy)

    b0 = w0 / area
    b1 = w1 / area
    b2 = w2 / area

    # Inside test
    inside = (b0 >= 0) & (b1 >= 0) & (b2 >= 0)

    # Build RGB image
    rgb = np.zeros((resolution, resolution, 4))
    rgb[inside, 0] = b0[inside]  # red from v0
    rgb[inside, 1] = b1[inside]  # green from v1
    rgb[inside, 2] = b2[inside]  # blue from v2
    rgb[inside, 3] = 1.0
    rgb = np.clip(rgb, 0, 1)

    ax.imshow(
        rgb,
        extent=[-0.5, 10.5, -0.5, 10.5],
        origin="lower",
        interpolation="bilinear",
        zorder=1,
    )

    # Vertex labels
    labels = [
        (v0, "v0\nb0 = 1.0", (0, 0.4), "#ff4444"),
        (v1, "v1\nb1 = 1.0", (-0.3, -0.5), "#44ff44"),
        (v2, "v2\nb2 = 1.0", (0.3, -0.5), "#4488ff"),
    ]
    for v, name, offset, color in labels:
        ax.plot(
            v[0],
            v[1],
            "o",
            color=color,
            markersize=9,
            markeredgecolor=STYLE["text"],
            markeredgewidth=1.5,
            zorder=5,
        )
        ax.text(
            v[0] + offset[0],
            v[1] + offset[1],
            name,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Center annotation
    center = (v0 + v1 + v2) / 3
    ax.plot(
        center[0],
        center[1],
        "+",
        color=STYLE["text"],
        markersize=12,
        markeredgewidth=2,
        zorder=5,
    )
    ax.text(
        center[0] + 0.8,
        center[1] + 0.3,
        "centroid\nb0 = b1 = b2 = 1/3",
        color=STYLE["text"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # Title
    ax.text(
        5.0,
        10.2,
        "Barycentric Coordinates",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    ax.text(
        5.0,
        -0.2,
        "color(p) = b0 * color(v0) + b1 * color(v1) + b2 * color(v2)",
        color=STYLE["text_dim"],
        fontsize=10,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "barycentric_coords.png")


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — rasterization_pipeline.png
# ---------------------------------------------------------------------------
def diagram_rasterization_pipeline():
    """Rasterization pipeline flow: vertices -> triangles -> pixels."""
    fig, ax = plt.subplots(figsize=(12, 4), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1.0, 14)
    ax.set_ylim(-0.5, 3.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    stages = [
        ("Vertices", STYLE["accent1"], 0.5),
        ("Triangles", STYLE["accent2"], 3.5),
        ("Bounding\nBox", STYLE["accent3"], 6.5),
        ("Edge\nTest", STYLE["accent4"], 9.5),
        ("Pixels", STYLE["warn"], 12.5),
    ]

    for label, color, x in stages:
        box = FancyBboxPatch(
            (x - 0.9, 0.6),
            1.8,
            1.8,
            boxstyle="round,pad=0.15",
            facecolor=color + "25",
            edgecolor=color,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(box)
        ax.text(
            x,
            1.5,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Arrows between stages
    for i in range(len(stages) - 1):
        x_start = stages[i][2] + 1.0
        x_end = stages[i + 1][2] - 1.0
        ax.annotate(
            "",
            xy=(x_end, 1.5),
            xytext=(x_start, 1.5),
            arrowprops={
                "arrowstyle": "->,head_width=0.2",
                "color": STYLE["text_dim"],
                "lw": 2,
            },
        )

    # Sub-labels
    sublabels = [
        (0.5, "position, color,\nUV per vertex"),
        (3.5, "3 indices per\ntriangle"),
        (6.5, "clamp AABB\nto framebuffer"),
        (9.5, "orient2d >= 0\nfor all 3 edges"),
        (12.5, "interpolate,\nblend, write"),
    ]
    for x, text in sublabels:
        ax.text(
            x,
            -0.1,
            text,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="top",
            path_effects=stroke,
        )

    # Title
    ax.text(
        6.75,
        3.2,
        "CPU Rasterization Pipeline",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "rasterization_pipeline.png")


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — alpha_blending.png
# ---------------------------------------------------------------------------
def diagram_alpha_blending():
    """Source-over alpha compositing formula with visual example."""
    fig, axes = plt.subplots(
        1,
        2,
        figsize=(12, 5),
        facecolor=STYLE["bg"],
        gridspec_kw={"width_ratios": [1.2, 1]},
    )

    # --- Left panel: formula ---
    ax = axes[0]
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 10)
    ax.set_ylim(-0.5, 7)
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        5,
        6.5,
        "Source-Over Compositing",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    formulas = [
        (5.2, "out.rgb = src.rgb * src.a + dst.rgb * (1 - src.a)"),
        (4.2, "out.a   = src.a + dst.a * (1 - src.a)"),
    ]
    for y, text in formulas:
        ax.text(
            5,
            y,
            text,
            color=STYLE["accent1"],
            fontsize=11,
            fontfamily="monospace",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    cases = [
        (2.8, "src.a = 1.0", "fully opaque  ->  dst is replaced", STYLE["accent3"]),
        (1.8, "src.a = 0.5", "50% blend  ->  equal mix", STYLE["warn"]),
        (0.8, "src.a = 0.0", "fully transparent  ->  dst unchanged", STYLE["text_dim"]),
    ]
    for y, left, right, color in cases:
        ax.text(
            2,
            y,
            left,
            color=color,
            fontsize=10,
            fontfamily="monospace",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            7,
            y,
            right,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # --- Right panel: visual demo ---
    ax2 = axes[1]
    ax2.set_facecolor(STYLE["bg"])
    ax2.set_xlim(-0.5, 6)
    ax2.set_ylim(-0.5, 7)
    ax2.axis("off")

    ax2.text(
        2.75,
        6.5,
        "Visual Example",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Destination (white background)
    dst = Rectangle(
        (0.5, 3.5),
        2,
        2,
        facecolor="#e0e0e0",
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
        zorder=1,
    )
    ax2.add_patch(dst)
    ax2.text(
        1.5,
        5.8,
        "dst (white)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Source (red, semi-transparent)
    src = Rectangle(
        (1.5, 2.5),
        2,
        2,
        facecolor="#ff4444",
        alpha=0.6,
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
        zorder=2,
    )
    ax2.add_patch(src)
    ax2.text(
        3.8,
        2.8,
        "src (red, a=0.6)",
        color=STYLE["accent2"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # Result arrow
    ax2.annotate(
        "",
        xy=(2.75, 1.2),
        xytext=(2.75, 2.3),
        arrowprops={
            "arrowstyle": "->,head_width=0.15",
            "color": STYLE["warn"],
            "lw": 2,
        },
    )

    # Blended result
    blended_r = 1.0 * 0.6 + 0.88 * 0.4
    blended_g = 0.27 * 0.6 + 0.88 * 0.4
    blended_b = 0.27 * 0.6 + 0.88 * 0.4
    result_color = (blended_r, blended_g, blended_b)
    result = Rectangle(
        (1.75, 0.0),
        2,
        1,
        facecolor=result_color,
        edgecolor=STYLE["warn"],
        linewidth=2,
        zorder=2,
    )
    ax2.add_patch(result)
    ax2.text(
        2.75,
        0.5,
        "result",
        color=STYLE["bg"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "alpha_blending.png")


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — bounding_box.png
# ---------------------------------------------------------------------------
def diagram_bounding_box():
    """Bounding box optimization: only test pixels inside the AABB."""
    fig, ax = plt.subplots(figsize=(8, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12.5), ylim=(-0.5, 12.5), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Triangle vertices
    v0 = np.array([6.0, 11.0])
    v1 = np.array([2.0, 3.0])
    v2 = np.array([10.5, 4.5])

    # Bounding box
    bb_min = np.array([min(v0[0], v1[0], v2[0]), min(v0[1], v1[1], v2[1])])
    bb_max = np.array([max(v0[0], v1[0], v2[0]), max(v0[1], v1[1], v2[1])])

    # Draw pixel grid inside bounding box
    for gx in range(int(bb_min[0]), int(bb_max[0]) + 1):
        for gy in range(int(bb_min[1]), int(bb_max[1]) + 1):
            px, py = gx + 0.5, gy + 0.5

            # Check if inside triangle
            def orient2d(ax_, ay, bx, by, ppx, ppy):
                return (bx - ax_) * (ppy - ay) - (by - ay) * (ppx - ax_)

            w0 = orient2d(v1[0], v1[1], v2[0], v2[1], px, py)
            w1 = orient2d(v2[0], v2[1], v0[0], v0[1], px, py)
            w2 = orient2d(v0[0], v0[1], v1[0], v1[1], px, py)
            inside = w0 >= 0 and w1 >= 0 and w2 >= 0

            color = STYLE["accent3"] + "40" if inside else STYLE["grid"]
            rect = Rectangle(
                (gx, gy),
                1,
                1,
                facecolor=color,
                edgecolor=STYLE["grid"],
                linewidth=0.5,
                zorder=1,
            )
            ax.add_patch(rect)

            if inside:
                ax.plot(px, py, ".", color=STYLE["accent3"], markersize=4, zorder=3)

    # Bounding box outline
    bb_rect = Rectangle(
        (bb_min[0], bb_min[1]),
        bb_max[0] - bb_min[0] + 1,
        bb_max[1] - bb_min[1] + 1,
        facecolor="none",
        edgecolor=STYLE["warn"],
        linewidth=2,
        linestyle="--",
        zorder=4,
    )
    ax.add_patch(bb_rect)

    # Triangle outline
    tri = Polygon(
        [v0, v1, v2],
        closed=True,
        facecolor="none",
        edgecolor=STYLE["accent1"],
        linewidth=2.5,
        zorder=5,
    )
    ax.add_patch(tri)

    # Vertex markers
    for v, name in [(v0, "v0"), (v1, "v1"), (v2, "v2")]:
        ax.plot(v[0], v[1], "o", color=STYLE["accent1"], markersize=8, zorder=6)
        ax.text(
            v[0] + 0.4,
            v[1] + 0.3,
            name,
            color=STYLE["accent1"],
            fontsize=11,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke,
        )

    # Legend
    ax.text(
        0.2,
        0.8,
        "AABB (bounding box)",
        color=STYLE["warn"],
        fontsize=10,
        ha="left",
        va="center",
        path_effects=stroke,
    )
    ax.plot(0.0, 0.8, "s", color=STYLE["warn"], markersize=6)

    ax.text(
        0.2,
        0.0,
        "inside (fragment emitted)",
        color=STYLE["accent3"],
        fontsize=10,
        ha="left",
        va="center",
        path_effects=stroke,
    )
    ax.plot(0.0, 0.0, "s", color=STYLE["accent3"] + "60", markersize=6)

    # Title
    ax.text(
        6.0,
        12.2,
        "Bounding Box Optimization",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "bounding_box.png")


# ---------------------------------------------------------------------------
# engine/10-cpu-rasterization — indexed_quad.png
# ---------------------------------------------------------------------------
def diagram_indexed_quad():
    """Indexed drawing: 4 vertices + 6 indices = 1 quad (2 triangles)."""
    fig, ax = plt.subplots(figsize=(9, 6), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1, 13)
    ax.set_ylim(-1.5, 7)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Left: Vertex buffer ---
    ax.text(
        2.5,
        6.5,
        "Vertex Buffer (4 vertices)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    vert_data = ["v0 (TL)", "v1 (TR)", "v2 (BR)", "v3 (BL)"]
    for i, label in enumerate(vert_data):
        y = 5.0 - i * 1.2
        box = FancyBboxPatch(
            (0.5, y - 0.4),
            4,
            0.8,
            boxstyle="round,pad=0.05",
            facecolor=STYLE["accent1"] + "25",
            edgecolor=STYLE["accent1"],
            linewidth=1.5,
        )
        ax.add_patch(box)
        ax.text(
            0.7,
            y,
            f"[{i}]",
            color=STYLE["accent1"],
            fontsize=10,
            fontfamily="monospace",
            ha="left",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            2.5,
            y,
            label,
            color=STYLE["text"],
            fontsize=10,
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # --- Right: Index buffer ---
    ax.text(
        9.5,
        6.5,
        "Index Buffer (6 indices)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Triangle 0
    tri0_indices = [
        ("0", STYLE["accent2"]),
        ("1", STYLE["accent2"]),
        ("2", STYLE["accent2"]),
    ]
    ax.text(
        9.5,
        5.5,
        "Triangle 0",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    for j, (idx, color) in enumerate(tri0_indices):
        x = 8.0 + j * 1.0
        box = FancyBboxPatch(
            (x - 0.3, 4.3),
            0.6,
            0.6,
            boxstyle="round,pad=0.05",
            facecolor=color + "25",
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(box)
        ax.text(
            x,
            4.6,
            idx,
            color=color,
            fontsize=11,
            fontfamily="monospace",
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Triangle 1
    tri1_indices = [
        ("0", STYLE["accent3"]),
        ("2", STYLE["accent3"]),
        ("3", STYLE["accent3"]),
    ]
    ax.text(
        9.5,
        3.5,
        "Triangle 1",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    for j, (idx, color) in enumerate(tri1_indices):
        x = 8.0 + j * 1.0
        box = FancyBboxPatch(
            (x - 0.3, 2.3),
            0.6,
            0.6,
            boxstyle="round,pad=0.05",
            facecolor=color + "25",
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(box)
        ax.text(
            x,
            2.6,
            idx,
            color=color,
            fontsize=11,
            fontfamily="monospace",
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # --- Bottom: visual quad ---
    ax.text(
        5.5,
        0.7,
        "Result: 1 Quad",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    qverts = np.array([[3.5, 0.0], [7.5, 0.0], [7.5, -1.0], [3.5, -1.0]])
    tri_a = Polygon(
        [qverts[0], qverts[1], qverts[2]],
        closed=True,
        facecolor=STYLE["accent2"] + "30",
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
    )
    tri_b = Polygon(
        [qverts[0], qverts[2], qverts[3]],
        closed=True,
        facecolor=STYLE["accent3"] + "30",
        edgecolor=STYLE["accent3"],
        linewidth=1.5,
    )
    ax.add_patch(tri_a)
    ax.add_patch(tri_b)

    # Diagonal line
    ax.plot(
        [qverts[0][0], qverts[2][0]],
        [qverts[0][1], qverts[2][1]],
        color=STYLE["text_dim"],
        linewidth=1,
        linestyle="--",
    )

    fig.tight_layout()
    save(fig, "engine/10-cpu-rasterization", "indexed_quad.png")


# ---------------------------------------------------------------------------
# engine/11-git-version-control — three_areas.png
# ---------------------------------------------------------------------------
def diagram_three_areas():
    """Git's three areas: working directory, staging area (index), and HEAD."""
    fig, ax = plt.subplots(figsize=(12, 6), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 12.5)
    ax.set_ylim(-0.5, 6.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Title
    ax.text(
        6.0,
        6.0,
        "Git's Three Areas",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Box parameters: (x, y, width, height, label, subtitle, color)
    boxes = [
        (0.2, 1.5, 3.2, 3.0, "Working\nDirectory", "Files on disk", STYLE["accent2"]),
        (
            4.4,
            1.5,
            3.2,
            3.0,
            "Staging Area\n(Index)",
            "Next commit\npreview",
            STYLE["warn"],
        ),
        (
            8.6,
            1.5,
            3.2,
            3.0,
            "Repository\n(HEAD)",
            "Committed\nhistory",
            STYLE["accent3"],
        ),
    ]

    for bx, by, bw, bh, label, sub, color in boxes:
        rect = FancyBboxPatch(
            (bx, by),
            bw,
            bh,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.5,
        )
        ax.add_patch(rect)
        ax.text(
            bx + bw / 2,
            by + bh * 0.65,
            label,
            color=color,
            fontsize=13,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            bx + bw / 2,
            by + bh * 0.2,
            sub,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="center",
        )

    # Arrows between boxes
    arrow_props = {
        "arrowstyle": "->,head_width=0.3,head_length=0.2",
        "lw": 2.5,
    }

    # Working -> Staging
    ax.annotate(
        "",
        xy=(4.2, 3.0),
        xytext=(3.6, 3.0),
        arrowprops={**arrow_props, "color": STYLE["accent1"]},
    )
    ax.text(
        3.9,
        3.7,
        "git add",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # Staging -> HEAD
    ax.annotate(
        "",
        xy=(8.4, 3.0),
        xytext=(7.8, 3.0),
        arrowprops={**arrow_props, "color": STYLE["accent1"]},
    )
    ax.text(
        8.1,
        3.7,
        "git commit",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # HEAD -> Working (checkout)
    ax.annotate(
        "",
        xy=(0.4, 1.3),
        xytext=(8.8, 1.3),
        arrowprops={
            **arrow_props,
            "color": STYLE["text_dim"],
            "connectionstyle": "arc3,rad=-0.3",
        },
    )
    ax.text(
        4.6,
        0.1,
        "git checkout",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
    )

    # Status comparison annotations
    ax.text(
        3.9,
        5.2,
        "git diff\n(unstaged)",
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
    )
    ax.text(
        8.1,
        5.2,
        "git diff --staged\n(staged)",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
    )

    save(fig, "engine/11-git-version-control", "three_areas.png")


# ---------------------------------------------------------------------------
# engine/11-git-version-control — worktree_architecture.png
# ---------------------------------------------------------------------------
def diagram_worktree_architecture():
    """Git worktrees sharing a single .git repository."""
    fig, ax = plt.subplots(figsize=(12, 7), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 12.5)
    ax.set_ylim(-0.5, 7.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Title
    ax.text(
        6.0,
        7.0,
        "Git Worktrees: Shared History, Independent Work",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Central .git repo
    git_rect = FancyBboxPatch(
        (4.0, 3.5),
        4.0,
        2.0,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent4"],
        linewidth=3,
    )
    ax.add_patch(git_rect)
    ax.text(
        6.0,
        4.8,
        ".git/ (shared)",
        color=STYLE["accent4"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        6.0,
        4.1,
        "commits, branches,\nobjects, refs",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
    )

    # Worktree boxes
    wt_data = [
        (0.2, 0.2, "forge-gpu/", "main", STYLE["accent1"], "You work here"),
        (
            4.2,
            0.2,
            "forge-gpu-bloom/",
            "feature-bloom",
            STYLE["accent2"],
            "Agent works here",
        ),
        (8.2, 0.2, "forge-gpu-fog/", "feature-fog", STYLE["accent3"], "Another agent"),
    ]

    for wx, wy, label, branch, color, note in wt_data:
        wt_rect = FancyBboxPatch(
            (wx, wy),
            3.6,
            2.5,
            boxstyle="round,pad=0.12",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
        )
        ax.add_patch(wt_rect)
        ax.text(
            wx + 1.8,
            wy + 2.0,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            wx + 1.8,
            wy + 1.3,
            f"branch: {branch}",
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            va="center",
            family="monospace",
        )
        ax.text(
            wx + 1.8,
            wy + 0.7,
            "build/ (independent)",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="center",
        )
        ax.text(
            wx + 1.8,
            wy + 0.2,
            note,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="center",
            style="italic",
        )

        # Connection line to .git
        ax.plot(
            [wx + 1.8, 6.0],
            [wy + 2.7, 3.5],
            color=color,
            linewidth=1.5,
            linestyle="--",
            alpha=0.6,
        )

    save(fig, "engine/11-git-version-control", "worktree_architecture.png")


# ---------------------------------------------------------------------------
# engine/11-git-version-control — submodule_vs_fetchcontent.png
# ---------------------------------------------------------------------------
def diagram_submodule_vs_fetchcontent():
    """Side-by-side comparison of submodules vs FetchContent."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 6), facecolor=STYLE["bg"])

    for ax in (ax1, ax2):
        ax.set_facecolor(STYLE["bg"])
        ax.set_xlim(-0.5, 6.5)
        ax.set_ylim(-0.5, 7.0)
        ax.set_aspect("equal")
        ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Left: Submodules ---
    ax1.text(
        3.0,
        6.5,
        "Git Submodules",
        color=STYLE["accent1"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Project repo box
    proj_rect = FancyBboxPatch(
        (0.3, 0.5),
        5.4,
        5.2,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
    )
    ax1.add_patch(proj_rect)
    ax1.text(
        3.0,
        5.3,
        "my-project/",
        color=STYLE["accent1"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    items_left = [
        ("src/", STYLE["text"]),
        ("CMakeLists.txt", STYLE["text"]),
        (".gitmodules", STYLE["warn"]),
        ("third_party/stb/  (commit: a1b2c3)", STYLE["accent3"]),
        ("third_party/SDL/  (commit: d4e5f6)", STYLE["accent3"]),
    ]
    for i, (txt, color) in enumerate(items_left):
        ax1.text(
            1.0,
            4.5 - i * 0.8,
            txt,
            color=color,
            fontsize=9,
            ha="left",
            va="center",
            family="monospace",
        )

    ax1.text(
        3.0,
        0.2,
        "Source visible in repo tree",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
    )

    # --- Right: FetchContent ---
    ax2.text(
        3.0,
        6.5,
        "FetchContent",
        color=STYLE["accent2"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Project repo box
    proj_rect2 = FancyBboxPatch(
        (0.3, 2.8),
        5.4,
        2.9,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
    )
    ax2.add_patch(proj_rect2)
    ax2.text(
        3.0,
        5.3,
        "my-project/",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    items_right_proj = [
        ("src/", STYLE["text"]),
        ("CMakeLists.txt  (FetchContent_Declare)", STYLE["warn"]),
    ]
    for i, (txt, color) in enumerate(items_right_proj):
        ax2.text(
            1.0,
            4.5 - i * 0.8,
            txt,
            color=color,
            fontsize=9,
            ha="left",
            va="center",
            family="monospace",
        )

    # Build dir box
    build_rect = FancyBboxPatch(
        (0.3, 0.5),
        5.4,
        1.8,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
        linestyle="--",
    )
    ax2.add_patch(build_rect)
    ax2.text(
        3.0,
        2.0,
        "build/_deps/",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
        family="monospace",
    )
    ax2.text(
        3.0,
        1.3,
        "SDL3-src/    SDL3-build/",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        family="monospace",
    )
    ax2.text(
        3.0,
        0.2,
        "Downloaded at configure time",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
    )

    # Arrow from project to build
    ax2.annotate(
        "",
        xy=(3.0, 2.5),
        xytext=(3.0, 2.8),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.15",
            "color": STYLE["text_dim"],
            "lw": 1.5,
        },
    )

    fig.tight_layout()
    save(fig, "engine/11-git-version-control", "submodule_vs_fetchcontent.png")
