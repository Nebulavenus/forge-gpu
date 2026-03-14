"""Diagrams for math/17 — Implicit 2D Curves."""

import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon

from .._common import STYLE, save, setup_axes

_LESSON = "math/17-implicit-curves"


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def _sdf_circle(px, py, r=1.0):
    """Circle SDF centered at origin."""
    return np.sqrt(px**2 + py**2) - r


# ---------------------------------------------------------------------------
# Diagram: SDF distance field with isolines
# ---------------------------------------------------------------------------


def diagram_sdf_distance_field():
    """Visualize a circle SDF as a color-mapped distance field with isolines."""
    res = 400
    extent = 2.0
    x = np.linspace(-extent, extent, res)
    y = np.linspace(-extent, extent, res)
    X, Y = np.meshgrid(x, y)
    D = _sdf_circle(X, Y, 1.0)

    fig = plt.figure(figsize=(7, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-extent, extent), ylim=(-extent, extent), grid=False)

    # Distance field as filled contour
    levels = np.linspace(-1.5, 1.5, 60)
    cf = ax.contourf(X, Y, D, levels=levels, cmap="RdBu_r", extend="both")

    # Isolines at key values
    cs = ax.contour(
        X,
        Y,
        D,
        levels=[-0.5, 0.0, 0.5],
        colors=[
            STYLE["accent3"],
            STYLE["warn"],
            STYLE["accent2"],
        ],
        linewidths=[1.5, 2.5, 1.5],
    )
    ax.clabel(
        cs,
        fmt={-0.5: "f = -0.5", 0.0: "f = 0", 0.5: "f = +0.5"},
        fontsize=9,
        colors=STYLE["text"],
    )

    cbar = fig.colorbar(cf, ax=ax, shrink=0.8, pad=0.02)
    cbar.set_label("Signed distance", color=STYLE["text"], fontsize=10)
    cbar.ax.tick_params(colors=STYLE["axis"], labelsize=8)

    ax.set_title(
        "Circle SDF — distance field and isolines",
        color=STYLE["text"],
        fontsize=12,
        pad=10,
    )
    ax.set_xlabel("x", color=STYLE["text"], fontsize=10)
    ax.set_ylabel("y", color=STYLE["text"], fontsize=10)

    fig.tight_layout()
    save(fig, _LESSON, "sdf_distance_field.png")


# ---------------------------------------------------------------------------
# Diagram: CSG operations
# ---------------------------------------------------------------------------


def diagram_csg_operations():
    """Show union, intersection, and subtraction of two circles."""
    res = 300
    extent = 2.0
    x = np.linspace(-extent, extent, res)
    y = np.linspace(-extent, extent, res)
    X, Y = np.meshgrid(x, y)

    da = _sdf_circle(X + 0.5, Y, 1.0)
    db = _sdf_circle(X - 0.5, Y, 1.0)

    ops = [
        ("Union: min(A, B)", np.minimum(da, db)),
        ("Intersection: max(A, B)", np.maximum(da, db)),
        ("Subtraction: max(A, −B)", np.maximum(da, -db)),
    ]

    fig, axes = plt.subplots(1, 3, figsize=(14, 4.5), facecolor=STYLE["bg"])

    for ax, (title, D) in zip(axes, ops, strict=True):
        setup_axes(ax, xlim=(-extent, extent), ylim=(-extent, extent), grid=False)

        levels = np.linspace(-1.5, 1.5, 50)
        ax.contourf(X, Y, D, levels=levels, cmap="RdBu_r", extend="both")
        ax.contour(X, Y, D, levels=[0.0], colors=[STYLE["warn"]], linewidths=2.0)

        # Show original circle outlines as dashed
        ax.contour(
            X,
            Y,
            da,
            levels=[0.0],
            colors=[STYLE["accent1"]],
            linewidths=1.0,
            linestyles="dashed",
        )
        ax.contour(
            X,
            Y,
            db,
            levels=[0.0],
            colors=[STYLE["accent2"]],
            linewidths=1.0,
            linestyles="dashed",
        )

        ax.set_title(title, color=STYLE["text"], fontsize=11, pad=8)
        ax.set_xlabel("x", color=STYLE["text"], fontsize=9)
        ax.set_ylabel("y", color=STYLE["text"], fontsize=9)

    fig.tight_layout(w_pad=1.5)
    save(fig, _LESSON, "csg_operations.png")


# ---------------------------------------------------------------------------
# Diagram: Smooth blending comparison
# ---------------------------------------------------------------------------


def _smooth_union(d1, d2, k):
    """Polynomial smooth-min."""
    if k <= 0:
        return np.minimum(d1, d2)
    h = np.clip(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0)
    return d2 + (d1 - d2) * h - k * h * (1.0 - h)


def diagram_smooth_blend():
    """Compare hard union vs smooth unions with increasing k."""
    res = 300
    extent = 2.0
    x = np.linspace(-extent, extent, res)
    y = np.linspace(-extent, extent, res)
    X, Y = np.meshgrid(x, y)

    da = _sdf_circle(X + 0.6, Y, 0.8)
    db = _sdf_circle(X - 0.6, Y, 0.8)

    k_vals = [0.0, 0.3, 0.8]
    titles = ["k = 0 (hard union)", "k = 0.3", "k = 0.8"]

    fig, axes = plt.subplots(1, 3, figsize=(14, 4.5), facecolor=STYLE["bg"])

    for ax, k, title in zip(axes, k_vals, titles, strict=True):
        setup_axes(ax, xlim=(-extent, extent), ylim=(-extent, extent), grid=False)
        D = _smooth_union(da, db, k)
        levels = np.linspace(-1.5, 1.5, 50)
        ax.contourf(X, Y, D, levels=levels, cmap="RdBu_r", extend="both")
        ax.contour(X, Y, D, levels=[0.0], colors=[STYLE["warn"]], linewidths=2.0)

        ax.set_title(title, color=STYLE["text"], fontsize=11, pad=8)
        ax.set_xlabel("x", color=STYLE["text"], fontsize=9)
        ax.set_ylabel("y", color=STYLE["text"], fontsize=9)

    fig.tight_layout(w_pad=1.5)
    save(fig, _LESSON, "smooth_blend.png")


# ---------------------------------------------------------------------------
# Diagram: Gradient field
# ---------------------------------------------------------------------------


def diagram_gradient_field():
    """Show gradient vectors of a circle SDF as a quiver plot."""
    fig = plt.figure(figsize=(7, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    extent = 2.0
    setup_axes(ax, xlim=(-extent, extent), ylim=(-extent, extent), grid=False)

    # Background distance field
    res = 300
    xf = np.linspace(-extent, extent, res)
    yf = np.linspace(-extent, extent, res)
    Xf, Yf = np.meshgrid(xf, yf)
    Df = _sdf_circle(Xf, Yf, 1.0)
    levels = np.linspace(-1.5, 1.5, 50)
    ax.contourf(Xf, Yf, Df, levels=levels, cmap="RdBu_r", extend="both", alpha=0.4)
    ax.contour(Xf, Yf, Df, levels=[0.0], colors=[STYLE["warn"]], linewidths=2.0)

    # Quiver grid (sparse)
    n = 12
    xq = np.linspace(-extent * 0.9, extent * 0.9, n)
    yq = np.linspace(-extent * 0.9, extent * 0.9, n)
    Xq, Yq = np.meshgrid(xq, yq)
    R = np.sqrt(Xq**2 + Yq**2)
    R[R < 1e-6] = 1e-6  # avoid division by zero at origin
    Gx = Xq / R
    Gy = Yq / R

    ax.quiver(
        Xq,
        Yq,
        Gx,
        Gy,
        color=STYLE["accent1"],
        scale=20,
        width=0.004,
        headwidth=4,
        headlength=5,
        alpha=0.9,
    )

    ax.set_title(
        "Gradient field of circle SDF — unit vectors away from origin",
        color=STYLE["text"],
        fontsize=11,
        pad=10,
    )
    ax.set_xlabel("x", color=STYLE["text"], fontsize=10)
    ax.set_ylabel("y", color=STYLE["text"], fontsize=10)

    fig.tight_layout()
    save(fig, _LESSON, "gradient_field.png")


# ---------------------------------------------------------------------------
# Diagram: Marching squares cases
# ---------------------------------------------------------------------------


def diagram_marching_squares():
    """Show the 16 marching squares cases."""
    fig, axes = plt.subplots(2, 8, figsize=(14, 4), facecolor=STYLE["bg"])

    # Edge midpoints: bottom, right, top, left
    edge_mid = {
        0: (0.5, 0.0),  # bottom
        1: (1.0, 0.5),  # right
        2: (0.5, 1.0),  # top
        3: (0.0, 0.5),  # left
    }

    # Cases: each is a list of (edge_from, edge_to) pairs
    cases = [
        [],  # 0: all outside
        [(0, 3)],  # 1: BL
        [(1, 0)],  # 2: BR
        [(1, 3)],  # 3: bottom
        [(2, 1)],  # 4: TR
        [(0, 3), (2, 1)],  # 5: saddle BL+TR
        [(2, 0)],  # 6: right
        [(2, 3)],  # 7: all but TL
        [(3, 2)],  # 8: TL
        [(0, 2)],  # 9: left
        [(3, 0), (1, 2)],  # 10: saddle TL+BR
        [(1, 2)],  # 11: all but TR
        [(3, 1)],  # 12: top
        [(0, 1)],  # 13: all but BR
        [(3, 0)],  # 14: all but BL
        [],  # 15: all inside
    ]

    # Corner positions: BL, BR, TR, TL
    corners = [(0, 0), (1, 0), (1, 1), (0, 1)]
    # Bit index for each corner
    bits = [1, 2, 4, 8]

    for idx in range(16):
        row = idx // 8
        col = idx % 8
        ax = axes[row][col]
        ax.set_xlim(-0.2, 1.2)
        ax.set_ylim(-0.2, 1.2)
        ax.set_aspect("equal")
        ax.set_facecolor(STYLE["bg"])
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

        # Draw cell outline
        cell = Polygon(corners, fill=False, edgecolor=STYLE["grid"], linewidth=1.0)
        ax.add_patch(cell)

        # Draw corners
        for ci, (cx, cy) in enumerate(corners):
            inside = bool(idx & bits[ci])
            color = STYLE["accent1"] if inside else STYLE["accent2"]
            ax.plot(
                cx,
                cy,
                "o",
                color=color,
                markersize=6,
                markeredgecolor=STYLE["bg"],
                markeredgewidth=0.5,
            )

        # Draw segments
        for e_from, e_to in cases[idx]:
            x0, y0 = edge_mid[e_from]
            x1, y1 = edge_mid[e_to]
            ax.plot(
                [x0, x1],
                [y0, y1],
                color=STYLE["warn"],
                linewidth=2.5,
                solid_capstyle="round",
            )

        # Mark saddle cases with an asterisk to indicate ambiguity
        label = f"{idx}*" if idx in (5, 10) else str(idx)
        ax.set_title(label, color=STYLE["text"], fontsize=9, pad=2)

    fig.suptitle(
        "Marching squares — 16 cases", color=STYLE["text"], fontsize=12, y=1.02
    )
    fig.tight_layout(h_pad=0.5, w_pad=0.3)
    save(fig, _LESSON, "marching_squares.png")
