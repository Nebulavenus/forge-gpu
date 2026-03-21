"""Diagrams for physics/12 — Impulse-Based Resolution."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
_LESSON = "physics/12-impulse-based-resolution"


# ---------------------------------------------------------------------------
# physics/12 — accumulated_vs_per_iteration.png
# ---------------------------------------------------------------------------


def diagram_accumulated_vs_per_iteration():
    """Compare accumulated impulse clamping vs per-iteration clamping.

    Shows how accumulated clamping converges to the correct total impulse
    while per-iteration clamping over-applies because it cannot back off.
    """
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 5.5), facecolor=STYLE["bg"])
    fig.suptitle(
        "Accumulated vs Per-Iteration Impulse Clamping",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.subplots_adjust(top=0.88, wspace=0.3, left=0.08, right=0.95)

    iterations = np.arange(1, 11)
    target_impulse = 5.0

    # ── Per-iteration clamping (left panel) ──
    # Each iteration computes a delta and clamps it independently.
    # When a neighbor changes, it can only add more — never subtract.
    per_iter_accum = np.zeros(len(iterations))
    deltas_per = []
    running = 0.0
    for i in range(len(iterations)):
        # Simulate: each iteration sees a different error because neighbors changed
        error = target_impulse - running + np.sin(i * 0.8) * 1.5
        delta = max(error * 0.4, 0.0)  # per-iteration clamp: >= 0
        deltas_per.append(delta)
        running += delta
        per_iter_accum[i] = running

    setup_axes(ax1, xlim=(0.5, 10.5), ylim=(0, 10), grid=True, aspect=None)
    ax1.set_xlabel("Iteration", color=STYLE["axis"], fontsize=10)
    ax1.set_ylabel("Total applied impulse (N·s)", color=STYLE["axis"], fontsize=10)
    ax1.set_title(
        "Per-Iteration Clamping", color=STYLE["accent2"], fontsize=12, fontweight="bold"
    )

    ax1.axhline(
        y=target_impulse,
        color=STYLE["accent3"],
        ls="--",
        lw=1.5,
        alpha=0.7,
        label="Correct solution",
    )
    ax1.bar(
        iterations,
        deltas_per,
        bottom=[sum(deltas_per[:i]) for i in range(len(deltas_per))],
        color=STYLE["accent2"],
        alpha=0.4,
        width=0.6,
        label="Delta (clamped ≥ 0)",
    )
    ax1.step(
        iterations,
        per_iter_accum,
        where="mid",
        color=STYLE["accent2"],
        lw=2.5,
        label="Accumulated total",
    )
    ax1.legend(
        loc="lower right",
        fontsize=8,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    # Annotate overshoot
    ax1.annotate(
        "Cannot back off\n→ overshoots",
        xy=(8, per_iter_accum[7]),
        xytext=(6, 8.5),
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        arrowprops={"arrowstyle": "->", "color": STYLE["warn"], "lw": 1.5},
        path_effects=_STROKE,
    )

    # ── Accumulated clamping (right panel) ──
    # Clamp the total, apply only the change. Can reduce previous impulse.
    accum_total = np.zeros(len(iterations))
    deltas_accum = []
    j_accum = 0.0
    for i in range(len(iterations)):
        error = target_impulse - j_accum + np.sin(i * 0.8) * 1.5
        # Compute delta, then clamp total
        delta_raw = error * 0.4
        old_j = j_accum
        j_accum = max(j_accum + delta_raw, 0.0)
        applied = j_accum - old_j
        deltas_accum.append(applied)
        accum_total[i] = j_accum

    setup_axes(ax2, xlim=(0.5, 10.5), ylim=(0, 10), grid=True, aspect=None)
    ax2.set_xlabel("Iteration", color=STYLE["axis"], fontsize=10)
    ax2.set_ylabel("Total applied impulse (N·s)", color=STYLE["axis"], fontsize=10)
    ax2.set_title(
        "Accumulated Clamping (Catto)",
        color=STYLE["accent1"],
        fontsize=12,
        fontweight="bold",
    )

    ax2.axhline(
        y=target_impulse,
        color=STYLE["accent3"],
        ls="--",
        lw=1.5,
        alpha=0.7,
        label="Correct solution",
    )

    ax2.step(
        iterations,
        accum_total,
        where="mid",
        color=STYLE["accent1"],
        lw=2.5,
        label="Accumulated total",
    )
    ax2.legend(
        loc="lower right",
        fontsize=8,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    # Annotate convergence
    ax2.annotate(
        "Converges to\ncorrect solution",
        xy=(9, accum_total[8]),
        xytext=(6, 7.5),
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        arrowprops={"arrowstyle": "->", "color": STYLE["accent3"], "lw": 1.5},
        path_effects=_STROKE,
    )

    save(fig, _LESSON, "accumulated_vs_per_iteration.png")
    plt.close(fig)


# ---------------------------------------------------------------------------
# physics/12 — friction_cone.png
# ---------------------------------------------------------------------------


def diagram_friction_cone():
    """Coulomb friction cone: j_tangent constrained by mu * j_normal.

    Shows the 2D cross-section of the cone in (j_normal, j_tangent) space,
    with the feasible region shaded.
    """
    fig, ax = plt.subplots(1, 1, figsize=(7, 6), facecolor=STYLE["bg"])
    fig.suptitle(
        "Coulomb Friction Cone",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.96,
    )
    fig.subplots_adjust(top=0.90, bottom=0.12, left=0.12, right=0.95)

    setup_axes(ax, xlim=(-0.5, 6), ylim=(-4, 4), grid=True, aspect=None)
    ax.set_xlabel("Normal impulse  $j_n$", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Tangential impulse  $j_t$", color=STYLE["axis"], fontsize=11)

    mu = 0.6
    jn_max = 5.5
    jn = np.linspace(0, jn_max, 100)

    # Cone boundaries: j_t = ±mu * j_n
    upper = mu * jn
    lower = -mu * jn

    # Shaded feasible region
    ax.fill_between(
        jn, lower, upper, color=STYLE["accent1"], alpha=0.15, label="Feasible region"
    )

    # Cone boundary lines
    ax.plot(
        jn,
        upper,
        color=STYLE["accent1"],
        lw=2,
        ls="-",
        label=f"$j_t = \\mu \\cdot j_n$  ($\\mu = {mu}$)",
    )
    ax.plot(jn, lower, color=STYLE["accent1"], lw=2, ls="-")

    # j_n = 0 boundary (no tension)
    ax.axvline(x=0, color=STYLE["accent2"], lw=1.5, ls="--", alpha=0.7)

    # Example points
    # Inside cone (valid)
    ax.plot(3.5, 1.0, "o", color=STYLE["accent3"], ms=10, zorder=5)
    ax.text(
        3.7,
        1.3,
        "Valid",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # On cone boundary (sliding)
    jn_slide = 4.0
    jt_slide = mu * jn_slide
    ax.plot(jn_slide, jt_slide, "s", color=STYLE["warn"], ms=10, zorder=5)
    ax.text(
        4.2,
        jt_slide + 0.3,
        "Sliding",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Outside cone (clamped)
    ax.plot(2.0, 2.8, "X", color=STYLE["accent2"], ms=10, zorder=5)
    ax.text(
        2.2,
        3.1,
        "Clamped",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )
    # Arrow from clamped to cone boundary
    clamped_target_jt = mu * 2.0
    ax.annotate(
        "",
        xy=(2.0, clamped_target_jt),
        xytext=(2.0, 2.8),
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent2"],
            "lw": 1.5,
            "ls": "--",
        },
    )

    # Labels for cone edges
    ax.text(
        5.0,
        mu * 5.0 + 0.4,
        "$+\\mu \\cdot j_n$",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )
    ax.text(
        5.0,
        -mu * 5.0 - 0.6,
        "$-\\mu \\cdot j_n$",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )

    ax.text(
        0.3,
        -3.5,
        "$j_n \\geq 0$\n(push only)",
        color=STYLE["accent2"],
        fontsize=9,
        fontstyle="italic",
        path_effects=_STROKE,
    )

    ax.legend(
        loc="upper left",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    save(fig, _LESSON, "friction_cone.png")
    plt.close(fig)


# ---------------------------------------------------------------------------
# physics/12 — solver_pipeline.png
# ---------------------------------------------------------------------------


def diagram_solver_pipeline():
    """Sequential impulse solver pipeline: prepare → warm-start → iterate → store.

    Shows the data flow from manifold cache through the solver phases
    and back to the cache for next-frame warm-starting.
    """
    fig, ax = plt.subplots(1, 1, figsize=(12, 5), facecolor=STYLE["bg"])
    fig.suptitle(
        "Sequential Impulse Solver Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.96,
    )
    fig.subplots_adjust(top=0.88, bottom=0.08, left=0.04, right=0.96)

    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 11.5)
    ax.set_ylim(-1.5, 3)
    ax.set_aspect("equal")
    ax.axis("off")

    # Box positions (x, y) and labels
    boxes = [
        (0, 1, "Manifold\nCache", STYLE["accent4"]),
        (2.5, 1, "Prepare", STYLE["accent1"]),
        (5, 1, "Warm-\nStart", STYLE["accent3"]),
        (7.5, 1, "Solve\nVelocities", STYLE["accent2"]),
        (10, 1, "Store\nImpulses", STYLE["warn"]),
    ]

    box_w, box_h = 1.8, 1.4

    for bx, by, label, color in boxes:
        rect = mpatches.FancyBboxPatch(
            (bx - box_w / 2, by - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
        )
        ax.add_patch(rect)
        ax.text(
            bx,
            by,
            label,
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
        )

    # Arrows between boxes
    arrow_y = 1.0
    arrow_kw = {
        "arrowstyle": "->,head_width=0.2,head_length=0.12",
        "color": STYLE["text"],
        "lw": 2,
    }
    for i in range(len(boxes) - 1):
        x1 = boxes[i][0] + box_w / 2
        x2 = boxes[i + 1][0] - box_w / 2
        ax.annotate("", xy=(x2, arrow_y), xytext=(x1, arrow_y), arrowprops=arrow_kw)

    # Iteration loop arrow (solve velocities loops back to itself)
    loop_x = 7.5
    loop_top = 1 + box_h / 2 + 0.15
    ax.annotate(
        "",
        xy=(loop_x - 0.4, loop_top),
        xytext=(loop_x + 0.4, loop_top),
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent2"],
            "lw": 2,
            "connectionstyle": "arc3,rad=-0.6",
        },
    )
    ax.text(
        loop_x,
        loop_top + 0.45,
        "N iterations",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=_STROKE,
    )

    # Feedback arrow: Store → Cache (warm-start for next frame)
    store_x = 10
    cache_x = 0
    fb_y = 1 - box_h / 2 - 0.3
    ax.annotate(
        "",
        xy=(cache_x, fb_y),
        xytext=(store_x, fb_y),
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent4"],
            "lw": 2,
            "ls": "--",
            "connectionstyle": "arc3,rad=0.15",
        },
    )
    ax.text(
        5,
        fb_y - 0.35,
        "Next frame: warm-start impulses",
        color=STYLE["accent4"],
        fontsize=9,
        fontstyle="italic",
        ha="center",
        va="top",
        path_effects=_STROKE,
    )

    # Annotations for data flow
    ax.text(
        1.25,
        1.7,
        "cached\nimpulses",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontstyle="italic",
    )
    ax.text(
        3.75,
        1.7,
        "eff. mass\nbias",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontstyle="italic",
    )
    ax.text(
        6.25,
        1.7,
        "velocities\nupdated",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontstyle="italic",
    )
    ax.text(
        8.75,
        1.7,
        "converged\nimpulses",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="bottom",
        fontstyle="italic",
    )

    save(fig, _LESSON, "solver_pipeline.png")
    plt.close(fig)
