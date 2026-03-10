"""Diagrams for physics/01 — Point Particles."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]


# ---------------------------------------------------------------------------
# physics/01-point-particles — euler_comparison.png
# ---------------------------------------------------------------------------


def diagram_symplectic_vs_explicit_euler():
    """Compare energy behaviour of symplectic vs explicit Euler over 500 steps."""
    fig = plt.figure(figsize=(9, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, grid=True, aspect=None)

    # Simulation parameters
    g = -9.81
    dt = 1.0 / 60.0
    steps = 500

    # --- Symplectic Euler (update v then x) ---
    sy_x, sy_v = 10.0, 0.0
    sy_energy = np.empty(steps)
    for i in range(steps):
        ke = 0.5 * sy_v * sy_v
        pe_val = -g * sy_x  # potential = -g*x (g is negative, so -g > 0)
        sy_energy[i] = ke + pe_val
        sy_v += g * dt
        sy_x += sy_v * dt
        # Bounce off ground
        if sy_x < 0.0:
            sy_x = -sy_x
            sy_v = -sy_v

    # --- Explicit Euler (update x then v) ---
    ex_x, ex_v = 10.0, 0.0
    ex_energy = np.empty(steps)
    for i in range(steps):
        ke = 0.5 * ex_v * ex_v
        pe_val = -g * ex_x
        ex_energy[i] = ke + pe_val
        ex_x += ex_v * dt
        ex_v += g * dt
        if ex_x < 0.0:
            ex_x = -ex_x
            ex_v = -ex_v

    t = np.arange(steps)
    ax.plot(t, sy_energy, color=STYLE["accent1"], lw=2, label="Symplectic Euler")
    ax.plot(t, ex_energy, color=STYLE["accent2"], lw=2, label="Explicit Euler")

    ax.set_xlabel("Time step", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Total energy (J)", color=STYLE["axis"], fontsize=11)
    ax.set_title(
        "Energy Conservation: Symplectic vs Explicit Euler",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    legend = ax.legend(
        loc="upper left",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    legend.get_frame().set_alpha(0.9)

    # Annotation
    ax.text(
        steps * 0.65,
        sy_energy[steps // 2],
        "Bounded",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )
    ax.text(
        steps * 0.65,
        ex_energy[-1] * 0.85,
        "Drifts upward",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )

    fig.tight_layout()
    save(fig, "physics/01-point-particles", "euler_comparison.png")


# ---------------------------------------------------------------------------
# physics/01-point-particles — sphere_plane_collision.png
# ---------------------------------------------------------------------------


def diagram_sphere_plane_collision():
    """Sphere-plane collision geometry: normal, distance, penetration, reflection."""
    fig = plt.figure(figsize=(9, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1, 9), ylim=(-1.5, 8), grid=False)

    # Ground plane
    ax.plot([-1, 9], [0, 0], color=STYLE["text"], lw=3, solid_capstyle="round")
    ax.fill_between([-1, 9], -1.5, 0, color=STYLE["surface"], alpha=0.4)
    ax.text(
        8.5,
        -0.7,
        "Ground plane",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="right",
        path_effects=_STROKE,
    )

    # Sphere
    center_x, center_y = 4.0, 3.0
    radius = 1.5
    circle = mpatches.Circle(
        (center_x, center_y),
        radius,
        fill=False,
        edgecolor=STYLE["accent1"],
        lw=2.5,
    )
    ax.add_patch(circle)
    ax.plot(center_x, center_y, "o", color=STYLE["accent1"], markersize=5, zorder=5)
    ax.text(
        center_x + 0.2,
        center_y + 0.3,
        "Center",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Plane normal arrow (from plane surface upward)
    normal_x = 1.5
    ax.annotate(
        "",
        xy=(normal_x, 2.5),
        xytext=(normal_x, 0),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.15",
            "color": STYLE["accent3"],
            "lw": 2.5,
        },
    )
    ax.text(
        normal_x - 0.5,
        1.3,
        "n",
        color=STYLE["accent3"],
        fontsize=14,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    # Signed distance (center to plane, vertical dashed line)
    ax.plot(
        [center_x, center_x],
        [0, center_y],
        "--",
        color=STYLE["warn"],
        lw=1.5,
    )
    ax.annotate(
        "",
        xy=(center_x + 0.3, center_y),
        xytext=(center_x + 0.3, 0),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
    )
    ax.text(
        center_x + 0.7,
        center_y / 2,
        "dist",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Radius line (center to sphere surface at bottom)
    ax.plot(
        [center_x, center_x],
        [center_y, center_y - radius],
        color=STYLE["accent4"],
        lw=2,
    )
    ax.text(
        center_x - 0.6,
        center_y - radius / 2,
        "r",
        color=STYLE["accent4"],
        fontsize=13,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    # Penetration depth annotation — show a second sphere that IS penetrating
    pen_cx, pen_cy = 7.0, 0.8
    pen_r = 1.5
    pen_circle = mpatches.Circle(
        (pen_cx, pen_cy),
        pen_r,
        fill=False,
        edgecolor=STYLE["accent2"],
        lw=2.5,
        linestyle="--",
    )
    ax.add_patch(pen_circle)
    ax.plot(pen_cx, pen_cy, "o", color=STYLE["accent2"], markersize=5, zorder=5)

    actual_pen = pen_r - pen_cy
    ax.annotate(
        "",
        xy=(pen_cx + 0.3, 0),
        xytext=(pen_cx + 0.3, -(actual_pen)),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["accent2"],
            "lw": 1.5,
        },
    )
    ax.text(
        pen_cx + 0.7,
        -actual_pen / 2,
        "penetration\n= r \u2212 dist",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Velocity vectors — incoming and reflected on the left sphere
    v_in = (1.5, -2.5)
    v_start = (center_x - 2.5, center_y + 3.5)
    ax.annotate(
        "",
        xy=(v_start[0] + v_in[0], v_start[1] + v_in[1]),
        xytext=v_start,
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.12",
            "color": STYLE["accent2"],
            "lw": 2,
        },
    )
    ax.text(
        v_start[0] - 0.2,
        v_start[1] + 0.3,
        "v (incoming)",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Reflected velocity
    v_ref_start = (v_start[0] + v_in[0], v_start[1] + v_in[1])
    v_ref = (1.5, 2.0)  # reflected: tangential kept, normal reversed * e
    ax.annotate(
        "",
        xy=(v_ref_start[0] + v_ref[0], v_ref_start[1] + v_ref[1]),
        xytext=v_ref_start,
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.12",
            "color": STYLE["accent3"],
            "lw": 2,
        },
    )
    ax.text(
        v_ref_start[0] + v_ref[0] + 0.2,
        v_ref_start[1] + v_ref[1] + 0.3,
        "v\u2032 (reflected)",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Decomposition labels
    # v_normal component (vertical part of incoming)
    vn_start = v_ref_start
    ax.annotate(
        "",
        xy=(vn_start[0], vn_start[1] + 1.8),
        xytext=vn_start,
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.1",
            "color": STYLE["warn"],
            "lw": 1.8,
            "linestyle": "--",
        },
    )
    ax.text(
        vn_start[0] - 0.9,
        vn_start[1] + 1.0,
        "v_n",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    # v_tangential component (horizontal part)
    ax.annotate(
        "",
        xy=(vn_start[0] + 1.5, vn_start[1]),
        xytext=vn_start,
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.1",
            "color": STYLE["accent4"],
            "lw": 1.8,
            "linestyle": "--",
        },
    )
    ax.text(
        vn_start[0] + 0.7,
        vn_start[1] - 0.5,
        "v_t",
        color=STYLE["accent4"],
        fontsize=10,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    # Restitution label
    ax.text(
        0.5,
        7.2,
        "v\u2032 = v_t \u2212 e \u00b7 v_n",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        path_effects=_STROKE,
    )
    ax.text(
        0.5,
        6.5,
        "e = coefficient of restitution",
        color=STYLE["text_dim"],
        fontsize=10,
        path_effects=_STROKE,
    )

    ax.set_title(
        "Sphere\u2013Plane Collision Detection and Response",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "physics/01-point-particles", "sphere_plane_collision.png")


# ---------------------------------------------------------------------------
# physics/01-point-particles — force_accumulator.png
# ---------------------------------------------------------------------------


def diagram_force_accumulator_pattern():
    """Pipeline diagram: clear forces -> accumulate -> integrate."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 10.5)
    ax.set_ylim(-3.5, 3.5)
    ax.set_aspect("equal")
    ax.axis("off")

    # Box positions (x_center, y_center, width, height)
    boxes = [
        (1.5, 1.5, 2.2, 1.4, "Clear\nforces", STYLE["accent4"]),
        (5.0, 1.5, 2.2, 1.4, "Accumulate", STYLE["accent1"]),
        (8.5, 1.5, 2.2, 1.4, "Integrate", STYLE["accent3"]),
    ]

    for bx, by, bw, bh, label, color in boxes:
        rect = mpatches.FancyBboxPatch(
            (bx - bw / 2, by - bh / 2),
            bw,
            bh,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=color,
            lw=2.5,
        )
        ax.add_patch(rect)
        ax.text(
            bx,
            by,
            label,
            color=color,
            fontsize=13,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
        )

    # Arrows between boxes
    arrow_props = {
        "arrowstyle": "->,head_width=0.3,head_length=0.15",
        "color": STYLE["text"],
        "lw": 2,
    }
    ax.annotate("", xy=(3.8, 1.5), xytext=(2.7, 1.5), arrowprops=arrow_props)
    ax.annotate("", xy=(7.3, 1.5), xytext=(6.2, 1.5), arrowprops=arrow_props)

    # Sub-items under "Accumulate"
    sub_items = [
        ("Gravity:  F = mg", STYLE["accent2"]),
        ("Drag:  F = \u2212kv", STYLE["accent2"]),
        ("Custom forces", STYLE["accent2"]),
    ]
    sub_x = 5.0
    for j, (text, color) in enumerate(sub_items):
        sub_y = 0.3 - j * 0.6
        ax.text(
            sub_x,
            sub_y,
            text,
            color=color,
            fontsize=9,
            ha="center",
            va="center",
            path_effects=_STROKE,
        )
        # Small connecting line from accumulate box down
        if j == 0:
            ax.plot(
                [sub_x, sub_x],
                [0.8, 0.55],
                color=STYLE["grid"],
                lw=1,
                alpha=0.7,
            )

    # Equations below the pipeline
    equations = [
        "F_total = \u03a3 forces",
        "a = F_total / m",
        "v += a \u00b7 dt",
        "x += v \u00b7 dt",
    ]
    eq_y_start = -1.8
    for k, eq in enumerate(equations):
        ax.text(
            5.0,
            eq_y_start - k * 0.55,
            eq,
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            fontfamily="monospace",
            path_effects=_STROKE,
        )

    # Dividing line between pipeline and equations
    ax.plot(
        [1.0, 9.0],
        [-1.3, -1.3],
        color=STYLE["grid"],
        lw=1,
        alpha=0.5,
    )

    ax.set_title(
        "Force Accumulator Pattern",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "physics/01-point-particles", "force_accumulator.png")
