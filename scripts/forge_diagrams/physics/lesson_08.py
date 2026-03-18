"""Diagrams for physics/08 — Sweep-and-Prune Broadphase."""

import matplotlib.lines as mlines
import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
_LESSON = "physics/08-sweep-and-prune"


# ---------------------------------------------------------------------------
# physics/08-sweep-and-prune — sap_algorithm.png
# ---------------------------------------------------------------------------


def diagram_sap_algorithm():
    """Sweep-and-prune algorithm: AABBs projected onto X axis with sweep."""
    fig = plt.figure(figsize=(12, 8), facecolor=STYLE["bg"])

    # Two panels: top = 2D scene with AABBs, bottom = 1D number line sweep
    ax_scene = fig.add_axes((0.06, 0.48, 0.90, 0.44))
    ax_sweep = fig.add_axes((0.06, 0.06, 0.90, 0.32))

    # --- Define 5 AABBs (some overlapping, some not) ---
    # Format: (x_min, y_min, x_max, y_max, label)
    aabbs = [
        (0.5, 1.0, 2.5, 3.0, "A"),
        (2.0, 0.5, 4.0, 2.5, "B"),
        (3.5, 2.0, 5.5, 4.0, "C"),
        (6.5, 1.0, 8.0, 3.5, "D"),
        (7.5, 0.5, 9.5, 2.5, "E"),
    ]

    colors = [
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent3"],
        STYLE["accent4"],
        STYLE["warn"],
    ]

    # --- Top panel: 2D scene with AABBs ---
    setup_axes(ax_scene, xlim=(-0.2, 10.5), ylim=(-0.2, 5.0), aspect="auto")
    ax_scene.set_title(
        "2D Scene — AABB Overlap Testing",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=14,
    )

    for i, (xmin, ymin, xmax, ymax, label) in enumerate(aabbs):
        rect = mpatches.FancyBboxPatch(
            (xmin, ymin),
            xmax - xmin,
            ymax - ymin,
            boxstyle="round,pad=0.02",
            facecolor=colors[i],
            edgecolor=colors[i],
            alpha=0.2,
            lw=2.0,
            zorder=2,
        )
        ax_scene.add_patch(rect)
        # Border
        border = mpatches.Rectangle(
            (xmin, ymin),
            xmax - xmin,
            ymax - ymin,
            fill=False,
            edgecolor=colors[i],
            lw=2.0,
            linestyle="--",
            zorder=3,
        )
        ax_scene.add_patch(border)
        # Label
        cx = (xmin + xmax) / 2
        cy = (ymin + ymax) / 2
        ax_scene.text(
            cx,
            cy,
            label,
            color=colors[i],
            fontsize=16,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
            zorder=5,
        )

    # Mark overlapping pairs with connecting lines
    # A-B overlap, B-C overlap, D-E overlap
    overlap_pairs = [(0, 1), (1, 2), (3, 4)]
    for ai, bi in overlap_pairs:
        ax1 = (aabbs[ai][0] + aabbs[ai][2]) / 2
        ay1 = (aabbs[ai][1] + aabbs[ai][3]) / 2
        bx1 = (aabbs[bi][0] + aabbs[bi][2]) / 2
        by1 = (aabbs[bi][1] + aabbs[bi][3]) / 2
        ax_scene.annotate(
            "",
            xy=(bx1, by1),
            xytext=(ax1, ay1),
            arrowprops={
                "arrowstyle": "<->",
                "color": STYLE["text_dim"],
                "lw": 1.5,
                "linestyle": ":",
            },
            zorder=1,
        )

    # Projection lines from AABBs down to sweep panel
    for i, (xmin, _ymin, xmax, _ymax, _label) in enumerate(aabbs):
        for x in [xmin, xmax]:
            ax_scene.plot(
                [x, x],
                [-0.2, 0.0],
                color=colors[i],
                lw=0.8,
                alpha=0.4,
                linestyle=":",
                clip_on=False,
            )

    ax_scene.set_xlabel("X", color=STYLE["axis"], fontsize=10)
    ax_scene.set_ylabel("Y", color=STYLE["axis"], fontsize=10)

    # --- Bottom panel: 1D number line with intervals ---
    setup_axes(ax_sweep, xlim=(-0.2, 10.5), ylim=(-0.8, 4.0), aspect="auto")
    ax_sweep.set_title(
        "X-Axis Projection — Sorted Endpoints and Sweep",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=14,
    )

    # Number line
    ax_sweep.axhline(y=0, color=STYLE["grid"], lw=2.0, zorder=1)
    for x in range(11):
        ax_sweep.plot([x, x], [-0.15, 0.15], color=STYLE["grid"], lw=1.5, zorder=2)
        ax_sweep.text(
            x,
            -0.45,
            str(x),
            color=STYLE["axis"],
            fontsize=9,
            ha="center",
            va="top",
        )

    # Draw intervals as horizontal bars at staggered heights
    bar_y = [0.7, 1.3, 1.9, 2.5, 3.1]
    bar_h = 0.35

    for i, (xmin, _ymin, xmax, _ymax, label) in enumerate(aabbs):
        y = bar_y[i]
        # Interval bar
        rect = mpatches.FancyBboxPatch(
            (xmin, y - bar_h / 2),
            xmax - xmin,
            bar_h,
            boxstyle="round,pad=0.03",
            facecolor=colors[i],
            edgecolor=colors[i],
            alpha=0.35,
            lw=1.5,
            zorder=3,
        )
        ax_sweep.add_patch(rect)

        # Endpoint markers on number line
        ax_sweep.plot(
            xmin,
            0,
            "^",
            color=colors[i],
            markersize=8,
            zorder=5,
            clip_on=False,
        )
        ax_sweep.plot(
            xmax,
            0,
            "v",
            color=colors[i],
            markersize=8,
            zorder=5,
            clip_on=False,
        )

        # Label
        ax_sweep.text(
            (xmin + xmax) / 2,
            y,
            label,
            color=colors[i],
            fontsize=13,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
            zorder=6,
        )

    # Highlight overlap regions on number line
    overlap_regions = [
        (2.0, 2.5, "A\u2229B"),  # A and B overlap on X: [2.0, 2.5]
        (3.5, 4.0, "B\u2229C"),  # B and C overlap on X: [3.5, 4.0]
        (7.5, 8.0, "D\u2229E"),  # D and E overlap on X: [7.5, 8.0]
    ]
    for xstart, xend, olabel in overlap_regions:
        ax_sweep.axvspan(
            xstart, xend, ymin=0.0, ymax=0.12, color=STYLE["warn"], alpha=0.3
        )
        ax_sweep.text(
            (xstart + xend) / 2,
            -0.7,
            olabel,
            color=STYLE["warn"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=_STROKE,
            zorder=6,
        )

    ax_sweep.set_xlabel("X axis (sweep direction)", color=STYLE["axis"], fontsize=10)
    ax_sweep.set_yticks([])

    # Legend
    legend_elements = [
        mlines.Line2D(
            [0],
            [0],
            marker="^",
            color=STYLE["text_dim"],
            lw=0,
            markersize=8,
            label="Min endpoint",
        ),
        mlines.Line2D(
            [0],
            [0],
            marker="v",
            color=STYLE["text_dim"],
            lw=0,
            markersize=8,
            label="Max endpoint",
        ),
        mpatches.Patch(facecolor=STYLE["warn"], alpha=0.3, label="Overlap region"),
    ]
    leg = ax_sweep.legend(
        handles=legend_elements,
        loc="upper right",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    for text in leg.get_texts():
        text.set_color(STYLE["text"])

    save(fig, _LESSON, "sap_algorithm.png")
    plt.close(fig)


# ---------------------------------------------------------------------------
# physics/08-sweep-and-prune — axis_selection.png
# ---------------------------------------------------------------------------


def diagram_axis_selection():
    """Axis selection: same AABBs viewed from X-spread and Y-spread perspectives."""
    fig, (ax_bad, ax_good) = plt.subplots(1, 2, figsize=(12, 5), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])

    # Bodies spread horizontally (high X variance, low Y variance)
    bodies_x = [
        (-4.0, 0.0),
        (-2.5, 0.3),
        (-1.0, -0.2),
        (0.5, 0.1),
        (2.0, -0.3),
        (3.5, 0.2),
        (5.0, 0.0),
    ]
    radius = 0.7
    colors_cycle = [
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent3"],
        STYLE["accent4"],
        STYLE["warn"],
        STYLE["accent1"],
        STYLE["accent2"],
    ]

    # --- Left panel: sweep on Y axis (poor choice) ---
    setup_axes(ax_bad, xlim=(-5.5, 6.5), ylim=(-2.5, 2.5), aspect="equal")
    ax_bad.set_title(
        "Sweep on Y axis (poor)",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        pad=14,
    )

    for i, (bx, by) in enumerate(bodies_x):
        circle = mpatches.Circle(
            (bx, by),
            radius,
            facecolor=colors_cycle[i],
            edgecolor=colors_cycle[i],
            alpha=0.25,
            lw=1.5,
            zorder=2,
        )
        ax_bad.add_patch(circle)
        # Y projection bar on right side
        ax_bad.plot(
            [6.0, 6.0],
            [by - radius, by + radius],
            color=colors_cycle[i],
            lw=3.0,
            alpha=0.7,
            solid_capstyle="round",
            zorder=3,
        )

    # Y axis sweep line
    ax_bad.axvline(x=6.0, color=STYLE["grid"], lw=1.0, linestyle=":")
    ax_bad.text(
        6.3,
        2.0,
        "Y sweep",
        color=STYLE["text_dim"],
        fontsize=9,
        rotation=90,
        va="top",
        path_effects=_STROKE,
    )
    # Overlap annotation
    ax_bad.text(
        -4.5,
        -2.0,
        "All intervals\noverlap on Y",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="bottom",
        path_effects=_STROKE,
    )

    ax_bad.set_xlabel("X", color=STYLE["axis"], fontsize=10)
    ax_bad.set_ylabel("Y", color=STYLE["axis"], fontsize=10)

    # --- Right panel: sweep on X axis (good choice) ---
    setup_axes(ax_good, xlim=(-5.5, 6.5), ylim=(-2.5, 2.5), aspect="equal")
    ax_good.set_title(
        "Sweep on X axis (optimal)",
        color=STYLE["accent3"],
        fontsize=12,
        fontweight="bold",
        pad=14,
    )

    for i, (bx, by) in enumerate(bodies_x):
        circle = mpatches.Circle(
            (bx, by),
            radius,
            facecolor=colors_cycle[i],
            edgecolor=colors_cycle[i],
            alpha=0.25,
            lw=1.5,
            zorder=2,
        )
        ax_good.add_patch(circle)
        # X projection bar on bottom
        ax_good.plot(
            [bx - radius, bx + radius],
            [-2.0, -2.0],
            color=colors_cycle[i],
            lw=3.0,
            alpha=0.7,
            solid_capstyle="round",
            zorder=3,
        )

    # X axis sweep line
    ax_good.axhline(y=-2.0, color=STYLE["grid"], lw=1.0, linestyle=":")
    ax_good.text(
        5.5,
        -1.6,
        "X sweep",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="right",
        path_effects=_STROKE,
    )
    # Separation annotation
    ax_good.text(
        -4.5,
        -2.3,
        "Intervals spread out,\nmost pairs pruned",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="top",
        path_effects=_STROKE,
    )

    ax_good.set_xlabel("X", color=STYLE["axis"], fontsize=10)
    ax_good.set_ylabel("Y", color=STYLE["axis"], fontsize=10)

    fig.suptitle(
        "Axis Selection — Choose the Axis with Greatest Variance",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )

    plt.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, _LESSON, "axis_selection.png")
    plt.close(fig)


# ---------------------------------------------------------------------------
# physics/08-sweep-and-prune — temporal_coherence.png
# ---------------------------------------------------------------------------


def diagram_temporal_coherence():
    """Insertion sort exploits temporal coherence: nearly-sorted vs random."""
    fig, (ax_rand, ax_coher) = plt.subplots(
        1, 2, figsize=(12, 5), facecolor=STYLE["bg"]
    )
    fig.patch.set_facecolor(STYLE["bg"])

    rng = np.random.default_rng(42)

    n = 30
    # Frame 1: random initial positions
    frame1 = np.sort(rng.uniform(0, 10, n))
    # Frame 2: each endpoint moves by a small random offset (coherent)
    frame2_coherent = np.clip(frame1 + rng.normal(0, 0.15, n), 0, 10)
    # Frame 2: completely reshuffled (no coherence)
    frame2_random = rng.uniform(0, 10, n)

    def count_inversions(arr):
        """Count number of insertion-sort swaps needed."""
        count = 0
        a = arr.copy()
        for i in range(1, len(a)):
            key = a[i]
            j = i - 1
            while j >= 0 and a[j] > key:
                a[j + 1] = a[j]
                j -= 1
                count += 1
            a[j + 1] = key
        return count

    swaps_random = count_inversions(frame2_random)
    swaps_coherent = count_inversions(frame2_coherent)

    # --- Left panel: no coherence (random reorder) ---
    setup_axes(ax_rand, xlim=(-0.5, 10.5), ylim=(-0.3, 2.5), aspect="auto")
    ax_rand.set_title(
        f"Random — {swaps_random} swaps",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        pad=14,
    )

    # Frame 1 endpoints
    ax_rand.scatter(
        frame1,
        np.ones(n) * 1.5,
        color=STYLE["accent1"],
        s=20,
        zorder=4,
        label="Frame N (sorted)",
    )
    # Frame 2 endpoints (random)
    ax_rand.scatter(
        frame2_random,
        np.ones(n) * 0.5,
        color=STYLE["accent2"],
        s=20,
        zorder=4,
        label="Frame N+1 (random)",
    )
    # Connecting lines showing displacement
    for i in range(n):
        ax_rand.plot(
            [frame1[i], frame2_random[i]],
            [1.5, 0.5],
            color=STYLE["text_dim"],
            lw=0.5,
            alpha=0.3,
        )

    ax_rand.set_xlabel("Endpoint value", color=STYLE["axis"], fontsize=10)
    ax_rand.set_yticks([0.5, 1.5])
    ax_rand.set_yticklabels(
        ["Frame N+1", "Frame N"],
        color=STYLE["axis"],
        fontsize=9,
    )

    leg1 = ax_rand.legend(
        loc="upper right",
        fontsize=8,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
    )
    for text in leg1.get_texts():
        text.set_color(STYLE["text"])

    # --- Right panel: temporal coherence (small perturbation) ---
    setup_axes(ax_coher, xlim=(-0.5, 10.5), ylim=(-0.3, 2.5), aspect="auto")
    ax_coher.set_title(
        f"Coherent — {swaps_coherent} swaps",
        color=STYLE["accent3"],
        fontsize=12,
        fontweight="bold",
        pad=14,
    )

    ax_coher.scatter(
        frame1,
        np.ones(n) * 1.5,
        color=STYLE["accent1"],
        s=20,
        zorder=4,
        label="Frame N (sorted)",
    )
    ax_coher.scatter(
        frame2_coherent,
        np.ones(n) * 0.5,
        color=STYLE["accent3"],
        s=20,
        zorder=4,
        label="Frame N+1 (coherent)",
    )
    for i in range(n):
        ax_coher.plot(
            [frame1[i], frame2_coherent[i]],
            [1.5, 0.5],
            color=STYLE["text_dim"],
            lw=0.5,
            alpha=0.3,
        )

    ax_coher.set_xlabel("Endpoint value", color=STYLE["axis"], fontsize=10)
    ax_coher.set_yticks([0.5, 1.5])
    ax_coher.set_yticklabels(
        ["Frame N+1", "Frame N"],
        color=STYLE["axis"],
        fontsize=9,
    )

    leg2 = ax_coher.legend(
        loc="upper right",
        fontsize=8,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
    )
    for text in leg2.get_texts():
        text.set_color(STYLE["text"])

    fig.suptitle(
        "Temporal Coherence — Insertion Sort Is O(n) for Nearly-Sorted Data",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )

    plt.tight_layout(rect=[0, 0, 1, 0.93])
    save(fig, _LESSON, "temporal_coherence.png")
    plt.close(fig)


# ---------------------------------------------------------------------------
# Public list of all diagram functions
# ---------------------------------------------------------------------------

ALL_DIAGRAMS = [
    diagram_sap_algorithm,
    diagram_axis_selection,
    diagram_temporal_coherence,
]
