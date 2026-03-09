"""Diagrams for gpu/33."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/33-vertex-pulling — traditional_vs_pulled_pipeline.png
# ---------------------------------------------------------------------------


def diagram_traditional_vs_pulled_pipeline():
    """Side-by-side comparison of the traditional vertex pipeline vs vertex pulling."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6), facecolor=STYLE["bg"])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    for ax in (ax1, ax2):
        setup_axes(ax, xlim=(0, 10), ylim=(0, 10), grid=False, aspect=None)
        ax.set_xticks([])
        ax.set_yticks([])

    def draw_box(ax, cx, cy, w, h, label, color, alpha=1.0, facecolor=None):
        """Draw a rounded box with centered label."""
        fc = facecolor if facecolor else STYLE["surface"]
        rect = FancyBboxPatch(
            (cx - w / 2, cy - h / 2),
            w,
            h,
            boxstyle="round,pad=0.15",
            facecolor=fc,
            edgecolor=color,
            linewidth=2,
            alpha=alpha,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            cx,
            cy,
            label,
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    def draw_arrow(ax, x1, y1, x2, y2, color):
        """Draw a downward arrow between boxes."""
        ax.annotate(
            "",
            xy=(x2, y2),
            xytext=(x1, y1),
            arrowprops={
                "arrowstyle": "->,head_width=0.5,head_length=0.25",
                "color": color,
                "lw": 2.5,
            },
            zorder=3,
        )

    # --- LEFT: Traditional Pipeline ---
    ax1.set_title(
        "Traditional Pipeline",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    trad_cx = 5.0
    trad_boxes = [
        (trad_cx, 8.2, 5.0, 1.2, "Vertex Buffer", STYLE["accent1"]),
        (trad_cx, 6.0, 5.0, 1.2, "Input Assembler", STYLE["text_dim"]),
        (trad_cx, 3.8, 5.0, 1.2, "Vertex Shader", STYLE["accent2"]),
        (trad_cx, 1.6, 5.0, 1.2, "Rasterizer", STYLE["accent4"]),
    ]

    for cx, cy, w, h, label, color in trad_boxes:
        fc = "#3a3a4a" if label == "Input Assembler" else STYLE["surface"]
        draw_box(ax1, cx, cy, w, h, label, color, facecolor=fc)

    # Arrows between traditional boxes — pad 0.3 from box edges
    draw_arrow(ax1, trad_cx, 7.3, trad_cx, 6.9, STYLE["text_dim"])
    draw_arrow(ax1, trad_cx, 5.1, trad_cx, 4.7, STYLE["text_dim"])
    draw_arrow(ax1, trad_cx, 2.9, trad_cx, 2.5, STYLE["text_dim"])

    # Annotation: IA decodes vertex attributes
    ax1.text(
        trad_cx + 2.8,
        5.0,
        "Decodes vertex attributes\nfrom buffer descriptions",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="center",
        fontstyle="italic",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- RIGHT: Vertex Pulling Pipeline ---
    ax2.set_title(
        "Vertex Pulling",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )

    pull_cx = 5.0
    pull_boxes = [
        (pull_cx, 8.2, 5.0, 1.2, "Storage Buffer", STYLE["accent1"]),
        (pull_cx, 5.0, 5.0, 1.2, "Vertex Shader\n(manual fetch)", STYLE["accent2"]),
        (pull_cx, 1.6, 5.0, 1.2, "Rasterizer", STYLE["accent4"]),
    ]

    for cx, cy, w, h, label, color in pull_boxes:
        draw_box(ax2, cx, cy, w, h, label, color)

    # Arrows between pulled boxes — pad 0.3 from box edges
    draw_arrow(ax2, pull_cx, 7.3, pull_cx, 5.9, STYLE["accent1"])
    draw_arrow(ax2, pull_cx, 4.1, pull_cx, 2.5, STYLE["text_dim"])

    # Ghost box where Input Assembler would be — centered between
    # Storage Buffer (bottom=7.6) and Vertex Shader (top=5.6), offset
    # right so the arrow doesn't obstruct the label.
    ghost_cy = 6.6
    ghost_rect = FancyBboxPatch(
        (pull_cx - 2.5, ghost_cy - 0.5),
        5.0,
        1.0,
        boxstyle="round,pad=0.15",
        facecolor="none",
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
        linestyle="--",
        alpha=0.3,
        zorder=2,
    )
    ax2.add_patch(ghost_rect)
    ax2.text(
        pull_cx + 1.8,
        ghost_cy,
        "No Input\nAssembler",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        fontstyle="italic",
        alpha=0.5,
        path_effects=stroke_thin,
        zorder=5,
    )

    # Annotation: shader fetches directly
    ax2.text(
        pull_cx,
        3.9,
        "Shader reads vertex data\nvia SV_VertexID index",
        color=STYLE["accent3"],
        fontsize=7.5,
        ha="center",
        va="top",
        fontstyle="italic",
        path_effects=stroke_thin,
        zorder=5,
    )

    fig.tight_layout()
    save(fig, "gpu/33-vertex-pulling", "traditional_vs_pulled_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/33-vertex-pulling — storage_buffer_layout.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/33-vertex-pulling — storage_buffer_layout.png
# ---------------------------------------------------------------------------


def diagram_storage_buffer_layout():
    """Memory layout of a PulledVertex struct in a storage buffer."""
    fig = plt.figure(figsize=(14, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.5, 14.5), ylim=(-2.5, 5.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # Field definitions: (name, byte_size, color)
    fields = [
        ("position\n(vec3)", 12, STYLE["accent1"]),
        ("normal\n(vec3)", 12, STYLE["accent3"]),
        ("uv\n(vec2)", 8, STYLE["accent2"]),
    ]
    total_bytes = 32
    struct_width = 3.2  # visual width per vertex struct
    field_height = 1.6
    base_y = 1.5

    num_vertices = 4
    start_x = 0.0

    for vi in range(num_vertices):
        vx = start_x + vi * (struct_width + 0.3)
        byte_offset_start = vi * total_bytes

        # Vertex label above
        label = f"Vertex {vi}" if vi < 3 else "..."
        alpha = 1.0 if vi < 3 else 0.5
        ax.text(
            vx + struct_width / 2,
            base_y + field_height + 0.5,
            label,
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            alpha=alpha,
            path_effects=stroke,
            zorder=5,
        )

        if vi == 3:
            # Draw ellipsis vertex as faded
            rect = FancyBboxPatch(
                (vx, base_y),
                struct_width,
                field_height,
                boxstyle="round,pad=0.05",
                facecolor=STYLE["surface"],
                edgecolor=STYLE["text_dim"],
                linewidth=1.5,
                alpha=0.3,
                zorder=2,
            )
            ax.add_patch(rect)
            ax.text(
                vx + struct_width / 2,
                base_y + field_height / 2,
                "...",
                color=STYLE["text_dim"],
                fontsize=14,
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )
            continue

        # Draw fields within this vertex
        x_cursor = vx
        accumulated_bytes = 0
        for fname, fbytes, fcolor in fields:
            fw = struct_width * (fbytes / total_bytes)
            rect = FancyBboxPatch(
                (x_cursor, base_y),
                fw,
                field_height,
                boxstyle="round,pad=0.02",
                facecolor=STYLE["surface"],
                edgecolor=fcolor,
                linewidth=2,
                zorder=2,
            )
            ax.add_patch(rect)
            ax.text(
                x_cursor + fw / 2,
                base_y + field_height / 2,
                fname,
                color=fcolor,
                fontsize=7,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

            # Byte offset below each field
            byte_off = byte_offset_start + accumulated_bytes
            ax.text(
                x_cursor,
                base_y - 0.3,
                str(byte_off),
                color=STYLE["text_dim"],
                fontsize=6.5,
                ha="center",
                va="top",
                path_effects=stroke_thin,
                zorder=5,
            )
            accumulated_bytes += fbytes
            x_cursor += fw

        # End byte offset for last field
        ax.text(
            vx + struct_width,
            base_y - 0.3,
            str(byte_offset_start + total_bytes),
            color=STYLE["text_dim"],
            fontsize=6.5,
            ha="center",
            va="top",
            path_effects=stroke_thin,
            zorder=5,
        )

    # --- SV_VertexID annotation ---
    # Short arrow from label down to the top of Vertex 1's struct box.
    # "Vertex 1" label sits at base_y + field_height + 0.5 = 3.6.
    # Arrow starts above the label and tip stops at the struct top edge.
    target_vx = start_x + 1 * (struct_width + 0.3) + struct_width / 2
    label_y = base_y + field_height + 0.5  # 3.6 — "Vertex 1" text
    ax.text(
        target_vx,
        label_y + 1.0,
        "SV_VertexID = 1",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.annotate(
        "",
        xy=(target_vx, label_y + 0.15),
        xytext=(target_vx, label_y + 0.65),
        arrowprops={
            "arrowstyle": "->,head_width=0.5,head_length=0.15",
            "color": STYLE["warn"],
            "lw": 2.5,
        },
        zorder=4,
    )

    # Formula at bottom
    ax.text(
        6.5,
        -1.5,
        "vertex = storage_buffer[SV_VertexID]",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        fontfamily="monospace",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        6.5,
        -2.1,
        "Each vertex is 32 bytes: position (12) + normal (12) + uv (8)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    ax.set_title(
        "Storage Buffer Layout — PulledVertex Struct",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/33-vertex-pulling", "storage_buffer_layout.png")


# ---------------------------------------------------------------------------
# gpu/33-vertex-pulling — pipeline_state_comparison.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/33-vertex-pulling — pipeline_state_comparison.png
# ---------------------------------------------------------------------------


def diagram_pipeline_state_comparison():
    """Table-style comparison of pipeline state: traditional vs vertex pulling."""
    fig = plt.figure(figsize=(12, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 12.5), ylim=(0, 8), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # Column positions
    label_x = 2.0
    trad_x = 6.0
    pull_x = 10.0
    col_w = 3.2
    header_y = 7.0
    row_h = 1.2

    # Column headers
    headers = [
        (label_x, "Property", STYLE["text_dim"]),
        (trad_x, "Traditional", STYLE["accent2"]),
        (pull_x, "Vertex Pulling", STYLE["accent1"]),
    ]
    for hx, hlabel, hcolor in headers:
        rect = FancyBboxPatch(
            (hx - col_w / 2, header_y - 0.4),
            col_w,
            0.8,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["surface"],
            edgecolor=hcolor,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            hx,
            header_y,
            hlabel,
            color=hcolor,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Row data: (label, traditional_value, pulled_value, highlight_pulled)
    rows = [
        ("vertex_buffer_\ndescriptions", "1", "0", True),
        ("vertex_attributes", "3", "0", True),
        ("buffer_usage", "VERTEX", "GRAPHICS_\nSTORAGE_READ", False),
        (
            "Attribute decode",
            "Hardware\n(Input Assembler)",
            "Shader\n(manual fetch)",
            False,
        ),
    ]

    for i, (label, trad_val, pull_val, highlight) in enumerate(rows):
        ry = header_y - (i + 1) * row_h - 0.3

        # Alternating row background
        if i % 2 == 0:
            row_bg = Rectangle(
                (0.0, ry - 0.4),
                12.5,
                row_h,
                facecolor=STYLE["surface"],
                alpha=0.3,
                zorder=1,
            )
            ax.add_patch(row_bg)

        # Label
        ax.text(
            label_x,
            ry + 0.1,
            label,
            color=STYLE["text_dim"],
            fontsize=9,
            fontweight="bold",
            fontfamily="monospace",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Traditional value
        ax.text(
            trad_x,
            ry + 0.1,
            trad_val,
            color=STYLE["accent2"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # Pulled value
        pcolor = STYLE["accent3"] if highlight else STYLE["accent1"]
        ax.text(
            pull_x,
            ry + 0.1,
            pull_val,
            color=pcolor,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Bottom annotation
    ax.text(
        6.5,
        0.5,
        "Vertex pulling eliminates vertex input state from the pipeline,\n"
        "making pipeline creation simpler and more flexible.",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        fontstyle="italic",
        path_effects=stroke_thin,
        zorder=5,
    )

    ax.set_title(
        "Pipeline State Comparison",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/33-vertex-pulling", "pipeline_state_comparison.png")


# ---------------------------------------------------------------------------
# gpu/33-vertex-pulling — vertex_pulling_use_cases.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/33-vertex-pulling — vertex_pulling_use_cases.png
# ---------------------------------------------------------------------------


def diagram_vertex_pulling_use_cases():
    """Four-quadrant diagram showing key use cases for vertex pulling."""
    fig, axes = plt.subplots(2, 2, figsize=(12, 10), facecolor=STYLE["bg"])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    quadrants = [
        {
            "title": "Flexible Formats",
            "desc": "Different vertex layouts\nsharing one pipeline",
            "color": STYLE["accent1"],
            "ax": axes[0, 0],
            "icon": "formats",
        },
        {
            "title": "Mesh Compression",
            "desc": "Quantized data decoded\nin shader",
            "color": STYLE["accent2"],
            "ax": axes[0, 1],
            "icon": "compress",
        },
        {
            "title": "Compute to Vertex",
            "desc": "Compute shader writes,\nvertex shader reads",
            "color": STYLE["accent3"],
            "ax": axes[1, 0],
            "icon": "compute",
        },
        {
            "title": "Multi-Draw",
            "desc": "One pipeline, many\ndifferent mesh formats",
            "color": STYLE["accent4"],
            "ax": axes[1, 1],
            "icon": "multidraw",
        },
    ]

    for q in quadrants:
        ax = q["ax"]
        setup_axes(ax, xlim=(0, 10), ylim=(0, 10), grid=False, aspect=None)
        ax.set_xticks([])
        ax.set_yticks([])

        color = q["color"]

        # Background border
        border = FancyBboxPatch(
            (0.3, 0.3),
            9.4,
            9.4,
            boxstyle="round,pad=0.2",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
            alpha=0.5,
            zorder=1,
        )
        ax.add_patch(border)

        # Title
        ax.text(
            5.0,
            8.5,
            q["title"],
            color=color,
            fontsize=14,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # Description
        ax.text(
            5.0,
            1.5,
            q["desc"],
            color=STYLE["text_dim"],
            fontsize=10,
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Icon shapes per quadrant
        if q["icon"] == "formats":
            # Three different-shaped boxes representing different layouts
            for i, (bx, bw, bc) in enumerate(
                [
                    (1.5, 2.0, STYLE["accent1"]),
                    (4.0, 3.0, STYLE["accent3"]),
                    (7.5, 1.5, STYLE["accent2"]),
                ]
            ):
                rect = FancyBboxPatch(
                    (bx, 4.5),
                    bw,
                    1.5,
                    boxstyle="round,pad=0.08",
                    facecolor=STYLE["surface"],
                    edgecolor=bc,
                    linewidth=1.8,
                    zorder=3,
                )
                ax.add_patch(rect)
                ax.text(
                    bx + bw / 2,
                    5.25,
                    f"Layout {i}",
                    color=bc,
                    fontsize=7.5,
                    fontweight="bold",
                    ha="center",
                    va="center",
                    path_effects=stroke_thin,
                    zorder=5,
                )
            # Single pipeline box below
            pipe_rect = FancyBboxPatch(
                (2.5, 3.0),
                5.0,
                1.0,
                boxstyle="round,pad=0.08",
                facecolor=STYLE["surface"],
                edgecolor=STYLE["warn"],
                linewidth=1.8,
                zorder=3,
            )
            ax.add_patch(pipe_rect)
            ax.text(
                5.0,
                3.5,
                "1 Pipeline",
                color=STYLE["warn"],
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

        elif q["icon"] == "compress":
            # Large box (full data) shrinking to small box (quantized)
            big = FancyBboxPatch(
                (1.5, 4.0),
                3.0,
                2.5,
                boxstyle="round,pad=0.08",
                facecolor=STYLE["surface"],
                edgecolor=STYLE["text_dim"],
                linewidth=1.8,
                zorder=3,
            )
            ax.add_patch(big)
            ax.text(
                3.0,
                5.25,
                "float32\n96 bytes",
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )
            # Arrow — pad 0.3 from each box edge so head doesn't collide
            ax.annotate(
                "",
                xy=(6.2, 5.25),
                xytext=(4.8, 5.25),
                arrowprops={
                    "arrowstyle": "->,head_width=0.5,head_length=0.25",
                    "color": color,
                    "lw": 2.5,
                },
                zorder=4,
            )
            small = FancyBboxPatch(
                (6.5, 4.5),
                2.0,
                1.5,
                boxstyle="round,pad=0.08",
                facecolor=STYLE["surface"],
                edgecolor=color,
                linewidth=2,
                zorder=3,
            )
            ax.add_patch(small)
            ax.text(
                7.5,
                5.25,
                "int16\n48 bytes",
                color=color,
                fontsize=8,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

        elif q["icon"] == "compute":
            # Compute box -> arrow -> Storage -> arrow -> Vertex box
            # Spaced out so arrows have room to breathe
            boxes_data = [
                (0.6, 4.5, 2.2, 1.5, "Compute\nShader", STYLE["accent4"]),
                (4.0, 4.5, 2.0, 1.5, "Storage\nBuffer", color),
                (7.2, 4.5, 2.2, 1.5, "Vertex\nShader", STYLE["accent1"]),
            ]
            for bx, by, bw, bh, blabel, bc in boxes_data:
                rect = FancyBboxPatch(
                    (bx, by),
                    bw,
                    bh,
                    boxstyle="round,pad=0.08",
                    facecolor=STYLE["surface"],
                    edgecolor=bc,
                    linewidth=1.8,
                    zorder=3,
                )
                ax.add_patch(rect)
                ax.text(
                    bx + bw / 2,
                    by + bh / 2,
                    blabel,
                    color=bc,
                    fontsize=8,
                    fontweight="bold",
                    ha="center",
                    va="center",
                    path_effects=stroke_thin,
                    zorder=5,
                )
            # Arrows — pad 0.3 from box edges so heads don't collide
            ax.annotate(
                "",
                xy=(3.7, 5.25),
                xytext=(3.1, 5.25),
                arrowprops={
                    "arrowstyle": "->,head_width=0.4,head_length=0.2",
                    "color": STYLE["text_dim"],
                    "lw": 2.5,
                },
                zorder=4,
            )
            ax.annotate(
                "",
                xy=(6.9, 5.25),
                xytext=(6.3, 5.25),
                arrowprops={
                    "arrowstyle": "->,head_width=0.4,head_length=0.2",
                    "color": STYLE["text_dim"],
                    "lw": 2.5,
                },
                zorder=4,
            )

        elif q["icon"] == "multidraw":
            # Multiple small mesh boxes feeding into one draw call
            # Mesh boxes at y=6.0, draw call at y=3.2 — gap for arrows
            mesh_colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]
            for i, mc in enumerate(mesh_colors):
                mx = 1.5 + i * 2.8
                rect = FancyBboxPatch(
                    (mx, 6.0),
                    2.2,
                    1.2,
                    boxstyle="round,pad=0.08",
                    facecolor=STYLE["surface"],
                    edgecolor=mc,
                    linewidth=1.8,
                    zorder=3,
                )
                ax.add_patch(rect)
                ax.text(
                    mx + 1.1,
                    6.6,
                    f"Mesh {i}",
                    color=mc,
                    fontsize=8,
                    fontweight="bold",
                    ha="center",
                    va="center",
                    path_effects=stroke_thin,
                    zorder=5,
                )
                # Arrow down to draw call — pad 0.3 from each box edge
                ax.annotate(
                    "",
                    xy=(mx + 1.1, 4.7),
                    xytext=(mx + 1.1, 5.7),
                    arrowprops={
                        "arrowstyle": "->,head_width=0.4,head_length=0.2",
                        "color": STYLE["text_dim"],
                        "lw": 2,
                    },
                    zorder=4,
                )
            # Single draw call box — wide enough to span all 3 mesh centers
            draw_rect = FancyBboxPatch(
                (1.5, 3.2),
                7.2,
                1.2,
                boxstyle="round,pad=0.08",
                facecolor=STYLE["surface"],
                edgecolor=color,
                linewidth=2,
                zorder=3,
            )
            ax.add_patch(draw_rect)
            ax.text(
                5.1,
                3.8,
                "1 Draw Call, 1 Pipeline",
                color=color,
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

    fig.suptitle(
        "Vertex Pulling Use Cases",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=0.98,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "gpu/33-vertex-pulling", "vertex_pulling_use_cases.png")


# ---------------------------------------------------------------------------
# gpu/33-vertex-pulling — binding_comparison.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/33-vertex-pulling — binding_comparison.png
# ---------------------------------------------------------------------------


def diagram_binding_comparison():
    """Comparison of SDL GPU binding calls: traditional vertex buffers vs storage buffers."""
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(13, 7), facecolor=STYLE["bg"])

    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    for ax in (ax1, ax2):
        setup_axes(ax, xlim=(0, 14), ylim=(0, 5), grid=False, aspect=None)
        ax.set_xticks([])
        ax.set_yticks([])

    def draw_box(ax, cx, cy, w, h, label, color, fontsize=9, fc=None):
        """Draw a rounded box with centered label."""
        facecolor = fc if fc else STYLE["surface"]
        rect = FancyBboxPatch(
            (cx - w / 2, cy - h / 2),
            w,
            h,
            boxstyle="round,pad=0.1",
            facecolor=facecolor,
            edgecolor=color,
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            cx,
            cy,
            label,
            color=color,
            fontsize=fontsize,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

    def draw_arrow_h(ax, x1, x2, y, color):
        ax.annotate(
            "",
            xy=(x2, y),
            xytext=(x1, y),
            arrowprops={
                "arrowstyle": "->,head_width=0.4,head_length=0.2",
                "color": color,
                "lw": 2,
            },
            zorder=3,
        )

    # --- TOP: Traditional binding ---
    ax1.set_title(
        "Traditional: SDL_BindGPUVertexBuffers()",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # API call box
    draw_box(ax1, 3.0, 2.5, 5.0, 1.4, "SDL_BindGPU\nVertexBuffers()", STYLE["accent2"])

    # Arrow to attribute locations — pad from box edges
    draw_arrow_h(ax1, 5.8, 7.8, 2.5, STYLE["text_dim"])

    # Attribute location boxes
    attrs = [
        (9.5, 3.8, "location 0\nposition", STYLE["accent1"]),
        (9.5, 2.5, "location 1\nnormal", STYLE["accent3"]),
        (9.5, 1.2, "location 2\nuv", STYLE["accent2"]),
    ]
    for ax_x, ay, alabel, acolor in attrs:
        draw_box(ax1, ax_x, ay, 2.8, 1.0, alabel, acolor, fontsize=8)

    # Brace or label for HLSL side
    ax1.text(
        12.5,
        2.5,
        "HLSL\nTEXCOORD0\nTEXCOORD1\nTEXCOORD2",
        color=STYLE["text_dim"],
        fontsize=7.5,
        fontfamily="monospace",
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- BOTTOM: Vertex pulling binding ---
    ax2.set_title(
        "Vertex Pulling: SDL_BindGPUVertexStorageBuffers()",
        color=STYLE["accent1"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # API call box
    draw_box(
        ax2, 3.0, 2.5, 5.0, 1.4, "SDL_BindGPUVertex\nStorageBuffers()", STYLE["accent1"]
    )

    # Arrow to structured buffer — pad from box edges
    draw_arrow_h(ax2, 5.8, 7.7, 2.5, STYLE["text_dim"])

    # Structured buffer box
    draw_box(
        ax2,
        10.0,
        2.5,
        4.0,
        1.6,
        "StructuredBuffer\n<PulledVertex>",
        STYLE["accent1"],
        fontsize=9,
    )

    # Register annotation
    ax2.text(
        10.0,
        1.2,
        "register(t0, space0)",
        color=STYLE["text_dim"],
        fontsize=8.5,
        fontfamily="monospace",
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Annotation: no vertex attributes needed
    ax2.text(
        3.0,
        1.0,
        "No vertex_buffer_descriptions\nNo vertex_attributes",
        color=STYLE["accent3"],
        fontsize=8,
        ha="center",
        va="center",
        fontstyle="italic",
        path_effects=stroke_thin,
        zorder=5,
    )

    fig.tight_layout()
    save(fig, "gpu/33-vertex-pulling", "binding_comparison.png")


# ---------------------------------------------------------------------------
# gpu/34-stencil-testing — stencil_buffer_concept.png
# ---------------------------------------------------------------------------
