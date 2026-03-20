"""Diagrams for gpu/46 — Particle Animations."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import Circle, FancyBboxPatch, Polygon

from .._common import STYLE, save, setup_axes

LESSON_PATH = "gpu/46-particle-animations"


# ---------------------------------------------------------------------------
# gpu/46-particle-animations — particle_lifecycle.png
# ---------------------------------------------------------------------------


def diagram_particle_lifecycle():
    """Particle state machine: dead -> spawn (atomic) -> alive -> simulate -> dead."""
    fig = plt.figure(figsize=(12, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1, 13), ylim=(-2, 7), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # ── State boxes ─────────────────────────────────────────────────────
    def state_box(x, y, w, h, label, color, sublabel=None):
        box = FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.2",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.5,
            zorder=3,
        )
        ax.add_patch(box)
        ax.text(
            x + w / 2,
            y + h / 2 + (0.15 if sublabel else 0),
            label,
            color=color,
            fontsize=13,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )
        if sublabel:
            ax.text(
                x + w / 2,
                y + h / 2 - 0.35,
                sublabel,
                color=STYLE["text_dim"],
                fontsize=9,
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )

    # Dead state (left)
    state_box(0, 2.5, 2.5, 1.8, "DEAD", STYLE["text_dim"], "lifetime \u2264 0")

    # Atomic check (center diamond-ish box)
    state_box(
        3.8, 2.5, 3.5, 1.8, "SPAWN?", STYLE["warn"], "InterlockedAdd(counter, -1)"
    )

    # Alive state (right)
    state_box(8.5, 2.5, 3.0, 1.8, "ALIVE", STYLE["accent3"], "lifetime > 0")

    # Simulate box (below alive)
    state_box(
        8.0,
        -0.5,
        4.0,
        2.0,
        "SIMULATE",
        STYLE["accent1"],
        "gravity + drag + lifetime\u2212\u2212",
    )

    # CPU budget upload (above spawn check)
    state_box(
        3.3,
        5.3,
        4.5,
        1.2,
        "CPU: Upload Budget",
        STYLE["accent2"],
        "spawn_count = rate \u00d7 dt",
    )

    # ── Arrows ──────────────────────────────────────────────────────────
    def arrow(x1, y1, x2, y2, color, label=None, label_x=None, label_y=None):
        ax.annotate(
            "",
            xy=(x2, y2),
            xytext=(x1, y1),
            arrowprops=dict(
                arrowstyle="->,head_width=0.25,head_length=0.12",
                color=color,
                lw=2,
            ),
            zorder=4,
        )
        if label:
            lx = label_x if label_x else (x1 + x2) / 2
            ly = label_y if label_y else (y1 + y2) / 2
            ax.text(
                lx,
                ly,
                label,
                color=color,
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )

    # Dead -> Spawn check
    arrow(2.5, 3.4, 3.8, 3.4, STYLE["text_dim"])

    # Spawn check -> Alive (budget > 0)
    arrow(7.3, 3.4, 8.5, 3.4, STYLE["accent3"], "prev > 0", label_y=3.8)

    # Spawn check -> Dead (no budget)
    arrow(
        5.5, 2.5, 2.2, 1.5, STYLE["text_dim"], "prev \u2264 0", label_x=3.6, label_y=1.6
    )
    # Connect back up to dead
    arrow(2.2, 1.5, 1.25, 2.5, STYLE["text_dim"])

    # Alive -> Simulate
    arrow(10.0, 2.5, 10.0, 1.5, STYLE["accent1"])

    # Simulate -> Dead (lifetime expired)
    arrow(
        8.0,
        0.5,
        1.25,
        2.5,
        STYLE["accent2"],
        "lifetime \u2264 0",
        label_x=4.5,
        label_y=1.0,
    )

    # Simulate -> Alive (still alive, loop back)
    arrow(
        12.0, 0.5, 12.0, 3.4, STYLE["accent3"], "next frame", label_x=12.7, label_y=2.0
    )
    arrow(12.0, 3.4, 11.5, 3.4, STYLE["accent3"])

    # CPU budget -> Spawn counter
    arrow(5.5, 5.3, 5.5, 4.3, STYLE["accent2"], "copy pass", label_x=6.8, label_y=4.8)

    ax.set_title(
        "Particle Lifecycle \u2014 Atomic Spawn Recycling",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "particle_lifecycle.png")


# ---------------------------------------------------------------------------
# gpu/46-particle-animations — billboard_expansion.png
# ---------------------------------------------------------------------------


def diagram_billboard_expansion():
    """How a particle point becomes a camera-facing billboard quad."""
    fig = plt.figure(figsize=(12, 6), facecolor=STYLE["bg"])

    # ── Left panel: concept ─────────────────────────────────────────────
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-2.5, 2.5), ylim=(-2.5, 2.5), grid=True, aspect="equal")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Particle center point
    ax1.plot(0, 0, "o", color=STYLE["warn"], markersize=10, zorder=5)
    ax1.text(
        0.15,
        -0.35,
        "particle.pos",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        path_effects=stroke,
        zorder=6,
    )

    # Camera right and up vectors
    ax1.annotate(
        "",
        xy=(1.5, 0),
        xytext=(0, 0),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.1",
            color=STYLE["accent1"],
            lw=2.5,
        ),
        zorder=4,
    )
    ax1.text(
        1.7,
        0.0,
        "cam_right",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    ax1.annotate(
        "",
        xy=(0, 1.5),
        xytext=(0, 0),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.1",
            color=STYLE["accent3"],
            lw=2.5,
        ),
        zorder=4,
    )
    ax1.text(
        0.15,
        1.7,
        "cam_up",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        path_effects=stroke,
        zorder=6,
    )

    # Billboard quad corners (size = 1.0 for illustration)
    s = 1.0
    corners = [
        (-s / 2, -s / 2),  # BL
        (s / 2, -s / 2),  # BR
        (s / 2, s / 2),  # TR
        (-s / 2, s / 2),  # TL
    ]

    # Draw quad as a filled polygon
    quad = Polygon(
        corners,
        closed=True,
        facecolor=STYLE["accent2"] + "30",  # semi-transparent fill
        edgecolor=STYLE["accent2"],
        linewidth=2,
        zorder=3,
    )
    ax1.add_patch(quad)

    # Corner labels
    corner_labels = [
        ((-s / 2 - 0.15, -s / 2 - 0.25), "(-0.5, -0.5)"),
        ((s / 2 + 0.15, -s / 2 - 0.25), "(+0.5, -0.5)"),
        ((s / 2 + 0.15, s / 2 + 0.2), "(+0.5, +0.5)"),
        ((-s / 2 - 0.15, s / 2 + 0.2), "(-0.5, +0.5)"),
    ]
    has = ["right", "left", "left", "right"]
    for (lx, ly), text, ha in zip(
        [c[0] for c in corner_labels],
        [c[1] for c in corner_labels],
        has,
        strict=True,
    ):
        ax1.text(
            lx,
            ly,
            text,
            color=STYLE["text_dim"],
            fontsize=8,
            ha=ha,
            va="center",
            path_effects=stroke,
            zorder=6,
        )

    # Corner dots
    for cx, cy in corners:
        ax1.plot(cx, cy, "s", color=STYLE["accent2"], markersize=6, zorder=5)

    # Triangle split (diagonal)
    ax1.plot(
        [-s / 2, s / 2],
        [-s / 2, s / 2],
        "--",
        color=STYLE["text_dim"],
        linewidth=1,
        alpha=0.6,
        zorder=3,
    )

    # Formula
    ax1.text(
        0.0,
        -2.0,
        "world = pos + right \u00d7 cx \u00d7 size + up \u00d7 cy \u00d7 size",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        fontfamily="monospace",
        path_effects=stroke,
        zorder=6,
    )

    ax1.set_title(
        "Billboard Quad Expansion",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    # ── Right panel: atlas UV mapping ───────────────────────────────────
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, xlim=(-0.2, 4.5), ylim=(-0.5, 4.5), grid=False, aspect="equal")

    # Draw 4x4 atlas grid
    for i in range(5):
        ax2.plot(
            [i, i],
            [0, 4],
            "-",
            color=STYLE["grid"],
            linewidth=1,
            zorder=2,
        )
        ax2.plot(
            [0, 4],
            [i, i],
            "-",
            color=STYLE["grid"],
            linewidth=1,
            zorder=2,
        )

    # Frame numbers
    for row in range(4):
        for col in range(4):
            frame = row * 4 + col
            # Intensity decreases with frame number (simulating atlas)
            t = frame / 15.0
            alpha = max(0.15, 1.0 - t * 0.7)
            radius = 0.15 + t * 0.25
            circle = Circle(
                (col + 0.5, 3 - row + 0.5),
                radius,
                color=STYLE["warn"],
                alpha=alpha,
                zorder=3,
            )
            ax2.add_patch(circle)
            ax2.text(
                col + 0.5,
                3 - row + 0.5,
                str(frame),
                color=STYLE["text"],
                fontsize=7,
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )

    # Highlight a sample cell (frame 4)
    highlight = FancyBboxPatch(
        (0, 2),
        1,
        1,
        boxstyle="square,pad=0",
        facecolor="none",
        edgecolor=STYLE["accent1"],
        linewidth=2.5,
        zorder=4,
    )
    ax2.add_patch(highlight)
    ax2.text(
        0.5,
        -0.3,
        "age_ratio \u2192 frame index",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
        zorder=6,
    )

    # Axis labels
    ax2.text(
        2.0,
        4.3,
        "4\u00d74 Atlas (16 frames)",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
        zorder=6,
    )
    ax2.text(
        -0.15,
        2.0,
        "birth",
        color=STYLE["accent3"],
        fontsize=9,
        ha="right",
        va="center",
        rotation=90,
        path_effects=stroke,
        zorder=6,
    )
    ax2.text(
        4.35,
        2.0,
        "death",
        color=STYLE["accent2"],
        fontsize=9,
        ha="left",
        va="center",
        rotation=90,
        path_effects=stroke,
        zorder=6,
    )

    ax2.set_xticks([])
    ax2.set_yticks([])

    ax2.set_title(
        "Atlas UV Indexing by Age",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, LESSON_PATH, "billboard_expansion.png")


# ---------------------------------------------------------------------------
# gpu/46-particle-animations — gpu_data_flow.png
# ---------------------------------------------------------------------------


def diagram_gpu_data_flow():
    """Per-frame GPU pipeline: CPU -> copy pass -> compute -> render -> output."""
    fig = plt.figure(figsize=(14, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 14.5), ylim=(-1.5, 7), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # ── Pipeline stage boxes ────────────────────────────────────────────
    def stage_box(x, y, w, h, label, color, sublabel=None):
        box = FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.5,
            zorder=3,
        )
        ax.add_patch(box)
        ty = y + h / 2 + (0.2 if sublabel else 0)
        ax.text(
            x + w / 2,
            ty,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )
        if sublabel:
            ax.text(
                x + w / 2,
                y + h / 2 - 0.3,
                sublabel,
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )

    # ── Data buffer boxes (shorter, distinct style) ─────────────────────
    def buffer_box(x, y, w, h, label, color, flags=None):
        box = FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["bg"],
            edgecolor=color,
            linewidth=1.5,
            linestyle="--",
            zorder=3,
        )
        ax.add_patch(box)
        ax.text(
            x + w / 2,
            y + h / 2 + (0.12 if flags else 0),
            label,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )
        if flags:
            ax.text(
                x + w / 2,
                y + h / 2 - 0.2,
                flags,
                color=STYLE["text_dim"],
                fontsize=7,
                fontfamily="monospace",
                ha="center",
                va="center",
                path_effects=stroke,
                zorder=5,
            )

    # Row 1: Pipeline stages (top row, y=4.5)
    stage_box(0, 4.5, 2.5, 1.8, "CPU", STYLE["accent2"], "Upload spawn budget")
    stage_box(
        3.5, 4.5, 2.5, 1.8, "Copy Pass", STYLE["accent2"], "4 bytes \u2192 counter"
    )
    stage_box(7.0, 4.5, 3.0, 1.8, "Compute Pass", STYLE["accent1"], "simulate + emit")
    stage_box(11.0, 4.5, 3.0, 1.8, "Render Pass", STYLE["accent3"], "billboard quads")

    # Row 2: Data buffers (middle row, y=1.5)
    buffer_box(
        0.5, 1.5, 2.5, 1.2, "Transfer Buffer", STYLE["accent2"], "UPLOAD (4 bytes)"
    )
    buffer_box(4.0, 1.5, 2.0, 1.2, "Counter", STYLE["warn"], "COMPUTE_STORAGE_WRITE")
    buffer_box(
        7.0,
        1.5,
        3.0,
        1.2,
        "Particle Buffer",
        STYLE["accent1"],
        "COMPUTE_WRITE | GFX_READ",
    )
    buffer_box(11.0, 1.5, 2.5, 1.2, "Atlas Texture", STYLE["accent4"], "SAMPLER")

    # Row 3: Output
    stage_box(11.5, -0.8, 2.0, 1.2, "Swapchain", STYLE["accent3"], "blended output")

    # ── Arrows: stages left-to-right ────────────────────────────────────
    def arrow(x1, y1, x2, y2, color):
        ax.annotate(
            "",
            xy=(x2, y2),
            xytext=(x1, y1),
            arrowprops=dict(
                arrowstyle="->,head_width=0.2,head_length=0.1",
                color=color,
                lw=2,
            ),
            zorder=4,
        )

    # CPU -> Copy Pass
    arrow(2.5, 5.4, 3.5, 5.4, STYLE["accent2"])
    # Copy Pass -> Compute
    arrow(6.0, 5.4, 7.0, 5.4, STYLE["accent1"])
    # Compute -> Render
    arrow(10.0, 5.4, 11.0, 5.4, STYLE["accent3"])

    # ── Arrows: stages to buffers (vertical) ────────────────────────────
    # CPU -> Transfer buffer
    arrow(1.75, 4.5, 1.75, 2.7, STYLE["accent2"])
    # Copy Pass -> Counter
    arrow(4.75, 4.5, 5.0, 2.7, STYLE["accent2"])
    # Transfer -> Counter (horizontal)
    arrow(3.0, 2.1, 4.0, 2.1, STYLE["accent2"])

    # Compute <-> Particle buffer (bidirectional: read-write)
    arrow(8.5, 4.5, 8.5, 2.7, STYLE["accent1"])
    ax.text(
        8.9,
        3.6,
        "R/W",
        color=STYLE["accent1"],
        fontsize=8,
        fontweight="bold",
        path_effects=stroke,
        zorder=6,
    )

    # Compute <- Counter (reads counter)
    arrow(6.0, 2.1, 7.0, 4.8, STYLE["warn"])
    ax.text(
        6.2,
        3.6,
        "atomic",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        path_effects=stroke,
        zorder=6,
    )

    # Render <- Particle buffer (vertex pulling, read-only)
    arrow(10.0, 2.1, 11.5, 4.5, STYLE["accent3"])
    ax.text(
        10.4,
        3.6,
        "vertex\npulling",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
        zorder=6,
    )

    # Render <- Atlas texture
    arrow(12.25, 2.7, 12.25, 4.5, STYLE["accent4"])
    ax.text(
        12.8,
        3.6,
        "sample",
        color=STYLE["accent4"],
        fontsize=8,
        fontweight="bold",
        path_effects=stroke,
        zorder=6,
    )

    # Render -> Swapchain
    arrow(12.5, 4.5, 12.5, 0.4, STYLE["accent3"])
    ax.text(
        13.2,
        2.5,
        "blend",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        path_effects=stroke,
        zorder=6,
    )

    # ── Frame label ─────────────────────────────────────────────────────
    ax.text(
        7.0,
        -1.2,
        "One frame: copy \u2192 compute \u2192 render (all on same command buffer)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        fontfamily="monospace",
        path_effects=stroke,
        zorder=6,
    )

    ax.set_title(
        "GPU Data Flow \u2014 Per-Frame Particle Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "gpu_data_flow.png")
