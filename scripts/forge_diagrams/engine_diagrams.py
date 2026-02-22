"""Diagram functions for engine lessons (lessons/engine/)."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle

from ._common import STYLE, save


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
