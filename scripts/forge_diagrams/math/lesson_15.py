"""Diagrams for math/15."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon

from .._common import STYLE, save, setup_axes


def _bezier_quadratic(p0, p1, p2, t):
    """Evaluate a quadratic Bézier curve at parameter t."""
    return (1 - t) ** 2 * p0 + 2 * (1 - t) * t * p1 + t**2 * p2


def _bezier_cubic_tangent(p0, p1, p2, p3, t):
    """First derivative of a cubic Bézier at parameter t."""
    u = 1 - t
    return 3 * u**2 * (p1 - p0) + 6 * u * t * (p2 - p1) + 3 * t**2 * (p3 - p2)


def _bezier_cubic(p0, p1, p2, p3, t):
    """Evaluate a cubic Bézier curve at parameter t."""
    u = 1 - t
    return u**3 * p0 + 3 * u**2 * t * p1 + 3 * u * t**2 * p2 + t**3 * p3


# ---------------------------------------------------------------------------
# math/15-bezier-curves
# ---------------------------------------------------------------------------

_BEZIER_LESSON = "math/15-bezier-curves"


def diagram_lerp_foundation():
    """Show lerp as the foundation — degree-1 Bézier."""
    fig = plt.figure(figsize=(8, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.3, 4.5), ylim=(-0.5, 2.5))

    p0 = np.array([0.0, 0.0])
    p1 = np.array([4.0, 2.0])

    # The line
    ax.plot([p0[0], p1[0]], [p0[1], p1[1]], color=STYLE["accent1"], lw=2.5, zorder=2)

    # Control points
    ax.plot(*p0, "o", color=STYLE["accent2"], markersize=10, zorder=5)
    ax.plot(*p1, "o", color=STYLE["accent2"], markersize=10, zorder=5)
    ax.text(
        p0[0] - 0.2,
        p0[1] - 0.25,
        "$P_0$",
        color=STYLE["accent2"],
        fontsize=13,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        p1[0] + 0.1,
        p1[1] + 0.15,
        "$P_1$",
        color=STYLE["accent2"],
        fontsize=13,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Sample points at various t
    ts = [0.0, 0.25, 0.5, 0.75, 1.0]
    for t in ts:
        pt = (1 - t) * p0 + t * p1
        ax.plot(*pt, "o", color=STYLE["warn"], markersize=7, zorder=4)
        label_y = 0.2 if t < 0.6 else -0.25
        ax.text(
            pt[0],
            pt[1] + label_y,
            f"t={t:.2f}",
            color=STYLE["warn"],
            fontsize=9,
            ha="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "Linear Interpolation (Lerp) — Degree-1 Bézier",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, _BEZIER_LESSON, "lerp_foundation.png")


def diagram_quadratic_vs_cubic():
    """Side-by-side comparison of quadratic and cubic Bézier curves."""
    fig, axes = plt.subplots(1, 2, figsize=(14, 5.5), facecolor=STYLE["bg"])

    ts = np.linspace(0, 1, 200)

    # ── Quadratic ──
    ax = axes[0]
    setup_axes(ax, xlim=(-0.5, 4.8), ylim=(-0.8, 4.8))

    p0 = np.array([0.0, 0.0])
    p1 = np.array([2.0, 4.0])
    p2 = np.array([4.0, 0.0])

    # Control polygon
    ax.plot(
        [p0[0], p1[0], p2[0]],
        [p0[1], p1[1], p2[1]],
        "--",
        color=STYLE["text_dim"],
        lw=1.2,
        alpha=0.6,
        zorder=1,
    )

    # Curve
    curve = np.array([_bezier_quadratic(p0, p1, p2, t) for t in ts])
    ax.plot(curve[:, 0], curve[:, 1], color=STYLE["accent1"], lw=3, zorder=3)

    # Control points
    for p, lbl, off in zip(
        [p0, p1, p2],
        ["$P_0$", "$P_1$", "$P_2$"],
        [(-0.3, -0.35), (0.0, 0.25), (0.15, -0.35)],
        strict=True,
    ):
        ax.plot(*p, "o", color=STYLE["accent2"], markersize=10, zorder=5)
        ax.text(
            p[0] + off[0],
            p[1] + off[1],
            lbl,
            color=STYLE["accent2"],
            fontsize=13,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "Quadratic (3 control points, 1 guide)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # ── Cubic ──
    ax = axes[1]
    setup_axes(ax, xlim=(-0.5, 4.8), ylim=(-0.8, 3.8))

    cp0 = np.array([0.0, 0.0])
    cp1 = np.array([1.0, 3.0])
    cp2 = np.array([3.0, 3.0])
    cp3 = np.array([4.0, 0.0])

    # Control polygon
    ax.plot(
        [cp0[0], cp1[0], cp2[0], cp3[0]],
        [cp0[1], cp1[1], cp2[1], cp3[1]],
        "--",
        color=STYLE["text_dim"],
        lw=1.2,
        alpha=0.6,
        zorder=1,
    )

    # Curve
    curve = np.array([_bezier_cubic(cp0, cp1, cp2, cp3, t) for t in ts])
    ax.plot(curve[:, 0], curve[:, 1], color=STYLE["accent1"], lw=3, zorder=3)

    # Control points
    for p, lbl, off in zip(
        [cp0, cp1, cp2, cp3],
        ["$P_0$", "$P_1$", "$P_2$", "$P_3$"],
        [(-0.3, -0.3), (-0.35, 0.2), (0.15, 0.2), (0.15, -0.3)],
        strict=True,
    ):
        ax.plot(*p, "o", color=STYLE["accent2"], markersize=10, zorder=5)
        ax.text(
            p[0] + off[0],
            p[1] + off[1],
            lbl,
            color=STYLE["accent2"],
            fontsize=13,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "Cubic (4 control points, 2 guides)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    fig.suptitle(
        "Quadratic vs Cubic Bézier Curves",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, _BEZIER_LESSON, "quadratic_vs_cubic.png")


def diagram_de_casteljau_quadratic():
    """Visualize De Casteljau's algorithm for a quadratic Bézier."""
    fig, axes = plt.subplots(1, 3, figsize=(16, 5.5), facecolor=STYLE["bg"])

    p0 = np.array([0.0, 0.0])
    p1 = np.array([2.0, 4.0])
    p2 = np.array([4.0, 0.0])
    ctrl = [p0, p1, p2]

    t_vals = [0.25, 0.5, 0.75]

    for ax, t_val in zip(axes, t_vals, strict=True):
        setup_axes(ax, xlim=(-0.5, 4.8), ylim=(-0.8, 4.8))

        # Full curve (faint)
        ts = np.linspace(0, 1, 100)
        curve = np.array([_bezier_quadratic(p0, p1, p2, t) for t in ts])
        ax.plot(
            curve[:, 0],
            curve[:, 1],
            color=STYLE["accent1"],
            lw=1.5,
            alpha=0.4,
            zorder=1,
        )

        # Control polygon
        poly_x = [p[0] for p in ctrl]
        poly_y = [p[1] for p in ctrl]
        ax.plot(
            poly_x, poly_y, "--", color=STYLE["text_dim"], lw=1.2, alpha=0.6, zorder=1
        )

        # Control points
        labels = ["$P_0$", "$P_1$", "$P_2$"]
        offsets = [(-0.3, -0.35), (0.0, 0.25), (0.15, -0.35)]
        for p, lbl, off in zip(ctrl, labels, offsets, strict=True):
            ax.plot(*p, "o", color=STYLE["accent2"], markersize=9, zorder=5)
            ax.text(
                p[0] + off[0],
                p[1] + off[1],
                lbl,
                color=STYLE["accent2"],
                fontsize=12,
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

        # De Casteljau round 1
        q0 = (1 - t_val) * p0 + t_val * p1
        q1 = (1 - t_val) * p1 + t_val * p2

        ax.plot(
            [q0[0], q1[0]],
            [q0[1], q1[1]],
            "-",
            color=STYLE["accent3"],
            lw=1.5,
            alpha=0.7,
            zorder=2,
        )
        ax.plot(*q0, "s", color=STYLE["accent3"], markersize=8, zorder=5)
        ax.plot(*q1, "s", color=STYLE["accent3"], markersize=8, zorder=5)
        ax.text(
            q0[0] - 0.4,
            q0[1] + 0.15,
            "$Q_0$",
            color=STYLE["accent3"],
            fontsize=10,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )
        ax.text(
            q1[0] + 0.15,
            q1[1] + 0.15,
            "$Q_1$",
            color=STYLE["accent3"],
            fontsize=10,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

        # De Casteljau round 2 — the curve point
        r = (1 - t_val) * q0 + t_val * q1
        ax.plot(*r, "D", color=STYLE["warn"], markersize=10, zorder=6)
        ax.text(
            r[0] + 0.15,
            r[1] + 0.25,
            f"B({t_val:.2f})",
            color=STYLE["warn"],
            fontsize=11,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

        # Highlight the curve up to t
        curve_partial = np.array(
            [_bezier_quadratic(p0, p1, p2, t) for t in np.linspace(0, t_val, 60)]
        )
        ax.plot(
            curve_partial[:, 0],
            curve_partial[:, 1],
            color=STYLE["accent1"],
            lw=3,
            zorder=3,
        )

        ax.set_title(
            f"t = {t_val:.2f}",
            color=STYLE["text"],
            fontsize=13,
            fontweight="bold",
            pad=10,
        )
        ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)

    axes[0].set_ylabel("y", color=STYLE["axis"], fontsize=10)

    fig.suptitle(
        "De Casteljau's Algorithm — Quadratic Bézier",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, _BEZIER_LESSON, "de_casteljau_quadratic.png")


def diagram_de_casteljau_cubic():
    """Visualize De Casteljau's algorithm for a cubic Bézier."""
    fig, axes = plt.subplots(1, 3, figsize=(16, 5.5), facecolor=STYLE["bg"])

    p0 = np.array([0.0, 0.0])
    p1 = np.array([1.0, 3.0])
    p2 = np.array([3.0, 3.0])
    p3 = np.array([4.0, 0.0])
    ctrl = [p0, p1, p2, p3]

    t_vals = [0.25, 0.5, 0.75]

    for ax, t_val in zip(axes, t_vals, strict=True):
        setup_axes(ax, xlim=(-0.5, 4.8), ylim=(-0.8, 3.8))

        # Full curve (faint)
        ts = np.linspace(0, 1, 100)
        curve = np.array([_bezier_cubic(p0, p1, p2, p3, t) for t in ts])
        ax.plot(
            curve[:, 0],
            curve[:, 1],
            color=STYLE["accent1"],
            lw=1.5,
            alpha=0.4,
            zorder=1,
        )

        # Control polygon
        poly_x = [p[0] for p in ctrl]
        poly_y = [p[1] for p in ctrl]
        ax.plot(
            poly_x, poly_y, "--", color=STYLE["text_dim"], lw=1.2, alpha=0.6, zorder=1
        )

        # Control points
        labels = ["$P_0$", "$P_1$", "$P_2$", "$P_3$"]
        offsets = [(-0.3, -0.3), (-0.35, 0.2), (0.15, 0.2), (0.15, -0.3)]
        for p, lbl, off in zip(ctrl, labels, offsets, strict=True):
            ax.plot(*p, "o", color=STYLE["accent2"], markersize=9, zorder=5)
            ax.text(
                p[0] + off[0],
                p[1] + off[1],
                lbl,
                color=STYLE["accent2"],
                fontsize=12,
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )

        # Round 1: 3 intermediate points
        q0 = (1 - t_val) * p0 + t_val * p1
        q1 = (1 - t_val) * p1 + t_val * p2
        q2 = (1 - t_val) * p2 + t_val * p3

        for qi in [q0, q1, q2]:
            ax.plot(*qi, "s", color=STYLE["accent3"], markersize=7, zorder=5)
        ax.plot(
            [q0[0], q1[0], q2[0]],
            [q0[1], q1[1], q2[1]],
            "-",
            color=STYLE["accent3"],
            lw=1.2,
            alpha=0.6,
            zorder=2,
        )

        # Round 2: 2 intermediate points
        r0 = (1 - t_val) * q0 + t_val * q1
        r1 = (1 - t_val) * q1 + t_val * q2

        for ri in [r0, r1]:
            ax.plot(*ri, "^", color=STYLE["accent4"], markersize=8, zorder=5)
        ax.plot(
            [r0[0], r1[0]],
            [r0[1], r1[1]],
            "-",
            color=STYLE["accent4"],
            lw=1.2,
            alpha=0.6,
            zorder=2,
        )

        # Round 3: the curve point
        s = (1 - t_val) * r0 + t_val * r1
        ax.plot(*s, "D", color=STYLE["warn"], markersize=10, zorder=6)
        ax.text(
            s[0] + 0.15,
            s[1] + 0.25,
            f"B({t_val:.2f})",
            color=STYLE["warn"],
            fontsize=11,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

        # Highlight curve up to t
        curve_partial = np.array(
            [_bezier_cubic(p0, p1, p2, p3, t) for t in np.linspace(0, t_val, 60)]
        )
        ax.plot(
            curve_partial[:, 0],
            curve_partial[:, 1],
            color=STYLE["accent1"],
            lw=3,
            zorder=3,
        )

        ax.set_title(
            f"t = {t_val:.2f}",
            color=STYLE["text"],
            fontsize=13,
            fontweight="bold",
            pad=10,
        )
        ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)

    axes[0].set_ylabel("y", color=STYLE["axis"], fontsize=10)

    fig.suptitle(
        "De Casteljau's Algorithm — Cubic Bézier",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, _BEZIER_LESSON, "de_casteljau_cubic.png")


def diagram_control_point_influence():
    """Show how moving a guide point reshapes a quadratic Bézier."""
    fig = plt.figure(figsize=(10, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 4.8), ylim=(-1.5, 5.5))

    p0 = np.array([0.0, 0.0])
    p2 = np.array([4.0, 0.0])

    # Several guide-point positions
    guides = [
        (np.array([2.0, 1.0]), STYLE["text_dim"], "$P_1$ = (2, 1)"),
        (np.array([2.0, 2.5]), STYLE["accent3"], "$P_1$ = (2, 2.5)"),
        (np.array([2.0, 4.0]), STYLE["accent1"], "$P_1$ = (2, 4)"),
        (np.array([0.5, 4.0]), STYLE["accent4"], "$P_1$ = (0.5, 4)"),
        (np.array([3.5, 4.0]), STYLE["accent2"], "$P_1$ = (3.5, 4)"),
    ]

    ts = np.linspace(0, 1, 100)

    for p1, color, label in guides:
        curve = np.array([_bezier_quadratic(p0, p1, p2, t) for t in ts])
        ax.plot(curve[:, 0], curve[:, 1], color=color, lw=2.5, label=label, zorder=2)
        # Dashed line to guide point
        ax.plot(
            [p0[0], p1[0], p2[0]],
            [p0[1], p1[1], p2[1]],
            "--",
            color=color,
            lw=0.8,
            alpha=0.5,
            zorder=1,
        )
        ax.plot(*p1, "o", color=color, markersize=7, zorder=4)

    # Fixed endpoints
    ax.plot(*p0, "o", color=STYLE["warn"], markersize=11, zorder=5)
    ax.plot(*p2, "o", color=STYLE["warn"], markersize=11, zorder=5)
    ax.text(
        p0[0] - 0.35,
        p0[1] - 0.35,
        "$P_0$",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        p2[0] + 0.1,
        p2[1] - 0.35,
        "$P_2$",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.legend(
        loc="lower right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "Control-Point Influence — Quadratic Bézier",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, _BEZIER_LESSON, "control_point_influence.png")


def diagram_cubic_tangent_vectors():
    """Show tangent vectors along a cubic Bézier curve."""
    fig = plt.figure(figsize=(9, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.8, 5.0), ylim=(-1.5, 4.5))

    p0 = np.array([0.0, 0.0])
    p1 = np.array([1.0, 3.0])
    p2 = np.array([3.0, 3.0])
    p3 = np.array([4.0, 0.0])

    # Full curve
    ts = np.linspace(0, 1, 200)
    curve = np.array([_bezier_cubic(p0, p1, p2, p3, t) for t in ts])
    ax.plot(curve[:, 0], curve[:, 1], color=STYLE["accent1"], lw=3, zorder=2)

    # Control polygon
    ax.plot(
        [p0[0], p1[0], p2[0], p3[0]],
        [p0[1], p1[1], p2[1], p3[1]],
        "--",
        color=STYLE["text_dim"],
        lw=1.2,
        alpha=0.5,
        zorder=1,
    )

    # Control points
    labels = ["$P_0$", "$P_1$", "$P_2$", "$P_3$"]
    offsets = [(-0.3, -0.35), (-0.4, 0.2), (0.15, 0.2), (0.15, -0.35)]
    for p, lbl, off in zip([p0, p1, p2, p3], labels, offsets, strict=True):
        ax.plot(*p, "o", color=STYLE["accent2"], markersize=9, zorder=5)
        ax.text(
            p[0] + off[0],
            p[1] + off[1],
            lbl,
            color=STYLE["accent2"],
            fontsize=12,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Tangent vectors at several t values
    t_samples = [0.0, 0.2, 0.4, 0.6, 0.8, 1.0]
    colors = [
        STYLE["accent3"],
        STYLE["accent4"],
        STYLE["warn"],
        STYLE["warn"],
        STYLE["accent4"],
        STYLE["accent3"],
    ]

    for t_val, col in zip(t_samples, colors, strict=True):
        pt = _bezier_cubic(p0, p1, p2, p3, t_val)
        tan = _bezier_cubic_tangent(p0, p1, p2, p3, t_val)
        # Scale tangent for visualization (normalize to fixed visual length)
        tan_len = np.sqrt(tan[0] ** 2 + tan[1] ** 2)
        tan_vis = tan / tan_len * 0.8 if tan_len > 0 else tan

        ax.annotate(
            "",
            xy=(pt[0] + tan_vis[0], pt[1] + tan_vis[1]),
            xytext=(pt[0], pt[1]),
            arrowprops={
                "arrowstyle": "->,head_width=0.2,head_length=0.1",
                "color": col,
                "lw": 2,
            },
            zorder=4,
        )
        ax.plot(*pt, "o", color=col, markersize=6, zorder=5)
        ax.text(
            pt[0] + tan_vis[0] * 0.5 + 0.1,
            pt[1] + tan_vis[1] * 0.5 + 0.2,
            f"t={t_val:.1f}",
            color=col,
            fontsize=9,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "Tangent Vectors Along a Cubic Bézier",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.tight_layout()
    save(fig, _BEZIER_LESSON, "cubic_tangent_vectors.png")


def diagram_convex_hull():
    """Show that a Bézier curve lies inside the convex hull of its control points."""
    fig, axes = plt.subplots(1, 2, figsize=(14, 5.5), facecolor=STYLE["bg"])

    # Quadratic
    ax = axes[0]
    setup_axes(ax, xlim=(-0.5, 4.8), ylim=(-0.8, 4.8))

    p0 = np.array([0.0, 0.0])
    p1 = np.array([2.0, 4.0])
    p2 = np.array([4.0, 0.0])

    # Convex hull (triangle for quadratic)
    hull = Polygon(
        [p0, p1, p2],
        closed=True,
        facecolor=STYLE["accent1"],
        alpha=0.1,
        edgecolor=STYLE["accent1"],
        lw=1.5,
        linestyle="--",
        zorder=1,
    )
    ax.add_patch(hull)

    # Curve
    ts = np.linspace(0, 1, 100)
    curve = np.array([_bezier_quadratic(p0, p1, p2, t) for t in ts])
    ax.plot(curve[:, 0], curve[:, 1], color=STYLE["accent1"], lw=3, zorder=3)

    # Control points
    for p in [p0, p1, p2]:
        ax.plot(*p, "o", color=STYLE["accent2"], markersize=9, zorder=5)
    ax.text(
        -0.3,
        -0.35,
        "$P_0$",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        2.0,
        4.25,
        "$P_1$",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        4.15,
        -0.35,
        "$P_2$",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "Quadratic — Convex Hull",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=10,
    )

    # Cubic
    ax = axes[1]
    setup_axes(ax, xlim=(-0.5, 4.8), ylim=(-0.8, 3.8))

    cp0 = np.array([0.0, 0.0])
    cp1 = np.array([1.0, 3.0])
    cp2 = np.array([3.0, 3.0])
    cp3 = np.array([4.0, 0.0])

    # Convex hull (quadrilateral)
    hull = Polygon(
        [cp0, cp1, cp2, cp3],
        closed=True,
        facecolor=STYLE["accent1"],
        alpha=0.1,
        edgecolor=STYLE["accent1"],
        lw=1.5,
        linestyle="--",
        zorder=1,
    )
    ax.add_patch(hull)

    # Curve
    curve = np.array([_bezier_cubic(cp0, cp1, cp2, cp3, t) for t in ts])
    ax.plot(curve[:, 0], curve[:, 1], color=STYLE["accent1"], lw=3, zorder=3)

    # Control points
    clabels = ["$P_0$", "$P_1$", "$P_2$", "$P_3$"]
    coffsets = [(-0.3, -0.3), (-0.35, 0.2), (0.15, 0.2), (0.15, -0.3)]
    for p, lbl, off in zip([cp0, cp1, cp2, cp3], clabels, coffsets, strict=True):
        ax.plot(*p, "o", color=STYLE["accent2"], markersize=9, zorder=5)
        ax.text(
            p[0] + off[0],
            p[1] + off[1],
            lbl,
            color=STYLE["accent2"],
            fontsize=12,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "Cubic — Convex Hull",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=10,
    )

    fig.suptitle(
        "Convex Hull Property",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, _BEZIER_LESSON, "convex_hull.png")


def diagram_continuity():
    """Show C0 and C1 continuity when joining Bézier segments."""
    fig, axes = plt.subplots(1, 2, figsize=(14, 5.5), facecolor=STYLE["bg"])

    # ── C0 continuity (only endpoints match) ──
    ax = axes[0]
    setup_axes(ax, xlim=(-0.5, 7), ylim=(-3, 3.5))

    # Segment 1
    s1 = [np.array([0, 0]), np.array([1, 2]), np.array([2, 2]), np.array([3, 0])]
    # Segment 2 — C0 only (different tangent direction at junction)
    s2 = [np.array([3, 0]), np.array([4, 1]), np.array([5, -2]), np.array([6, 0])]

    ts = np.linspace(0, 1, 100)

    # Draw curves
    c1 = np.array([_bezier_cubic(*s1, t) for t in ts])
    c2 = np.array([_bezier_cubic(*s2, t) for t in ts])
    ax.plot(
        c1[:, 0], c1[:, 1], color=STYLE["accent1"], lw=3, zorder=3, label="Segment 1"
    )
    ax.plot(
        c2[:, 0], c2[:, 1], color=STYLE["accent3"], lw=3, zorder=3, label="Segment 2"
    )

    # Control polygons
    for seg, col in [(s1, STYLE["accent1"]), (s2, STYLE["accent3"])]:
        xs = [p[0] for p in seg]
        ys = [p[1] for p in seg]
        ax.plot(xs, ys, "--", color=col, lw=0.8, alpha=0.5)
        for p in seg:
            ax.plot(*p, "o", color=col, markersize=6, zorder=4, alpha=0.7)

    # Highlight junction
    ax.plot(3, 0, "D", color=STYLE["warn"], markersize=12, zorder=6)
    ax.text(
        3.0,
        0.35,
        "junction",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Show tangent mismatch
    tan1 = _bezier_cubic_tangent(*s1, 1.0)
    tan2 = _bezier_cubic_tangent(*s2, 0.0)
    scale = 0.15
    ax.annotate(
        "",
        xy=(3 + tan1[0] * scale, tan1[1] * scale),
        xytext=(3, 0),
        arrowprops={
            "arrowstyle": "->,head_width=0.15",
            "color": STYLE["accent1"],
            "lw": 2,
        },
        zorder=5,
    )
    ax.annotate(
        "",
        xy=(3 + tan2[0] * scale, tan2[1] * scale),
        xytext=(3, 0),
        arrowprops={
            "arrowstyle": "->,head_width=0.15",
            "color": STYLE["accent3"],
            "lw": 2,
        },
        zorder=5,
    )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "C0 Continuity — Shared Endpoint (Corner)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax.legend(
        loc="upper right",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    # ── C1 continuity (tangent also matches) ──
    ax = axes[1]
    setup_axes(ax, xlim=(-0.5, 7), ylim=(-3, 3.5))

    # Segment 1 same
    # Segment 2 — C1: first guide is reflection of s1_p2 across junction
    s2_c1 = [
        np.array([3, 0]),
        np.array([3 + (3 - 2), 0 + (0 - 2)]),  # reflection: (4, -2)
        np.array([5, -2]),
        np.array([6, 0]),
    ]

    c1 = np.array([_bezier_cubic(*s1, t) for t in ts])
    c2 = np.array([_bezier_cubic(*s2_c1, t) for t in ts])
    ax.plot(
        c1[:, 0], c1[:, 1], color=STYLE["accent1"], lw=3, zorder=3, label="Segment 1"
    )
    ax.plot(
        c2[:, 0], c2[:, 1], color=STYLE["accent3"], lw=3, zorder=3, label="Segment 2"
    )

    # Control polygons
    for seg, col in [(s1, STYLE["accent1"]), (s2_c1, STYLE["accent3"])]:
        xs = [p[0] for p in seg]
        ys = [p[1] for p in seg]
        ax.plot(xs, ys, "--", color=col, lw=0.8, alpha=0.5)
        for p in seg:
            ax.plot(*p, "o", color=col, markersize=6, zorder=4, alpha=0.7)

    # Highlight junction
    ax.plot(3, 0, "D", color=STYLE["warn"], markersize=12, zorder=6)
    ax.text(
        3.0,
        0.35,
        "junction",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Show tangent match
    tan1 = _bezier_cubic_tangent(*s1, 1.0)
    tan2 = _bezier_cubic_tangent(*s2_c1, 0.0)
    scale = 0.15
    ax.annotate(
        "",
        xy=(3 + tan1[0] * scale, tan1[1] * scale),
        xytext=(3, 0),
        arrowprops={
            "arrowstyle": "->,head_width=0.15",
            "color": STYLE["warn"],
            "lw": 2.5,
        },
        zorder=5,
    )

    # Show mirror relationship
    ax.plot([2, 4], [2, -2], ":", color=STYLE["accent4"], lw=1.5, alpha=0.7)
    ax.text(
        2.0,
        2.2,
        "$P_2$",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        4.0,
        -2.3,
        "$P_1'$",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        3.3,
        -0.9,
        "mirror",
        color=STYLE["accent4"],
        fontsize=9,
        fontweight="bold",
        rotation=-55,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "C1 Continuity — Tangent Matches (Smooth)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax.legend(
        loc="upper right",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    fig.suptitle(
        "Joining Bézier Curves — Continuity",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, _BEZIER_LESSON, "continuity.png")


def diagram_arc_length():
    """Show arc-length approximation by summing line segments."""
    fig, axes = plt.subplots(1, 3, figsize=(16, 5.5), facecolor=STYLE["bg"])

    p0 = np.array([0.0, 0.0])
    p1 = np.array([1.0, 3.0])
    p2 = np.array([3.0, 3.0])
    p3 = np.array([4.0, 0.0])

    ts_smooth = np.linspace(0, 1, 200)
    curve = np.array([_bezier_cubic(p0, p1, p2, p3, t) for t in ts_smooth])

    segment_counts = [4, 8, 32]

    for ax, n_seg in zip(axes, segment_counts, strict=True):
        setup_axes(ax, xlim=(-0.3, 4.5), ylim=(-0.5, 3.5))

        # Faint full curve
        ax.plot(
            curve[:, 0],
            curve[:, 1],
            color=STYLE["accent1"],
            lw=1.5,
            alpha=0.3,
            zorder=1,
        )

        # Line segments
        seg_t = np.linspace(0, 1, n_seg + 1)
        seg_pts = np.array([_bezier_cubic(p0, p1, p2, p3, t) for t in seg_t])
        ax.plot(
            seg_pts[:, 0],
            seg_pts[:, 1],
            "o-",
            color=STYLE["accent3"],
            lw=2,
            markersize=5,
            zorder=3,
        )

        # Compute approx length
        length = 0
        for i in range(len(seg_pts) - 1):
            d = seg_pts[i + 1] - seg_pts[i]
            length += np.sqrt(d[0] ** 2 + d[1] ** 2)

        ax.set_xlabel("x", color=STYLE["axis"], fontsize=10)
        ax.set_title(
            f"{n_seg} segments  (length ≈ {length:.3f})",
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            pad=10,
        )

    axes[0].set_ylabel("y", color=STYLE["axis"], fontsize=10)

    fig.suptitle(
        "Arc-Length Approximation",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, _BEZIER_LESSON, "arc_length.png")


def diagram_bernstein_basis():
    """Plot the Bernstein basis polynomials for quadratic and cubic Bézier."""
    fig, axes = plt.subplots(1, 2, figsize=(14, 5.5), facecolor=STYLE["bg"])

    ts = np.linspace(0, 1, 200)

    # ── Quadratic (n=2) ──
    ax = axes[0]
    setup_axes(ax, xlim=(-0.05, 1.05), ylim=(-0.05, 1.1), aspect=None)

    b20 = (1 - ts) ** 2
    b21 = 2 * (1 - ts) * ts
    b22 = ts**2

    ax.plot(ts, b20, color=STYLE["accent1"], lw=2.5, label="$B_{0,2} = (1-t)^2$")
    ax.plot(ts, b21, color=STYLE["accent2"], lw=2.5, label="$B_{1,2} = 2(1-t)t$")
    ax.plot(ts, b22, color=STYLE["accent3"], lw=2.5, label="$B_{2,2} = t^2$")

    # Sum line
    ax.plot(
        ts,
        b20 + b21 + b22,
        "--",
        color=STYLE["text_dim"],
        lw=1.2,
        alpha=0.6,
        label="sum = 1",
    )

    ax.set_xlabel("t", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("weight", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Quadratic Bernstein Basis (n = 2)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax.legend(
        loc="center right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    # ── Cubic (n=3) ──
    ax = axes[1]
    setup_axes(ax, xlim=(-0.05, 1.05), ylim=(-0.05, 1.1), aspect=None)

    b30 = (1 - ts) ** 3
    b31 = 3 * (1 - ts) ** 2 * ts
    b32 = 3 * (1 - ts) * ts**2
    b33 = ts**3

    ax.plot(ts, b30, color=STYLE["accent1"], lw=2.5, label="$B_{0,3} = (1-t)^3$")
    ax.plot(ts, b31, color=STYLE["accent2"], lw=2.5, label="$B_{1,3} = 3(1-t)^2 t$")
    ax.plot(ts, b32, color=STYLE["accent3"], lw=2.5, label="$B_{2,3} = 3(1-t)t^2$")
    ax.plot(ts, b33, color=STYLE["accent4"], lw=2.5, label="$B_{3,3} = t^3$")

    # Sum line
    ax.plot(
        ts,
        b30 + b31 + b32 + b33,
        "--",
        color=STYLE["text_dim"],
        lw=1.2,
        alpha=0.6,
        label="sum = 1",
    )

    ax.set_xlabel("t", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Cubic Bernstein Basis (n = 3)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax.legend(
        loc="center right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    fig.suptitle(
        "Bernstein Basis Polynomials — Blending Weights",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, _BEZIER_LESSON, "bernstein_basis.png")
