# ---------------------------------------------------------------------------
# physics/13-constraint-solver — diagram functions
# ---------------------------------------------------------------------------

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
_LESSON = "physics/13-constraint-solver"


# ---------------------------------------------------------------------------
# physics/13-constraint-solver — ball_socket_joint.png
# ---------------------------------------------------------------------------


def diagram_ball_socket_joint():
    """Ball-socket joint: anchor points, lever arms, and constraint equation.

    Shows two rigid bodies connected at a shared anchor point. The lever
    arms r_a and r_b extend from each body's center of mass to the anchor.
    A summary box states the DOF trade-off.
    """
    fig, ax = plt.subplots(1, 1, figsize=(9, 7), facecolor=STYLE["bg"])
    fig.suptitle(
        "Ball-Socket Joint",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=0.96,
    )
    fig.subplots_adjust(top=0.89, bottom=0.08, left=0.06, right=0.94)
    setup_axes(ax, xlim=(-4, 4), ylim=(-2, 4.5), grid=False, aspect="equal")
    ax.axis("off")

    # Body A (left) — filled ellipse
    body_a_center = np.array([-2.2, 2.8])
    ellipse_a = mpatches.Ellipse(
        body_a_center,
        1.8,
        1.1,
        angle=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(ellipse_a)
    ax.text(
        body_a_center[0],
        body_a_center[1] + 0.05,
        "Body A",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )
    ax.plot(
        *body_a_center,
        "o",
        color=STYLE["accent1"],
        markersize=5,
        zorder=4,
    )

    # Body B (right) — filled rectangle
    body_b_center = np.array([2.2, 1.0])
    rect_b = mpatches.FancyBboxPatch(
        (body_b_center[0] - 1.0, body_b_center[1] - 0.55),
        2.0,
        1.1,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(rect_b)
    ax.text(
        body_b_center[0],
        body_b_center[1] + 0.05,
        "Body B",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )
    ax.plot(
        *body_b_center,
        "o",
        color=STYLE["accent2"],
        markersize=5,
        zorder=4,
    )

    # Anchor point (shared constraint point)
    anchor = np.array([0.0, 2.0])
    ax.plot(
        *anchor,
        "D",
        color=STYLE["warn"],
        markersize=12,
        zorder=6,
        markeredgecolor=STYLE["bg"],
        markeredgewidth=1.5,
    )
    ax.text(
        anchor[0],
        anchor[1] + 0.45,
        "Anchor",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
        zorder=5,
    )

    # Lever arm r_a: COM_A -> anchor
    ax.annotate(
        "",
        xy=anchor,
        xytext=body_a_center,
        arrowprops=dict(
            arrowstyle="-|>",
            color=STYLE["accent1"],
            lw=2,
            linestyle="--",
        ),
        zorder=3,
    )
    ra_mid = (body_a_center + anchor) / 2
    ax.text(
        ra_mid[0] - 0.05,
        ra_mid[1] + 0.35,
        "$\\mathbf{r}_A$",
        color=STYLE["accent1"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
        zorder=5,
    )

    # Lever arm r_b: COM_B -> anchor
    ax.annotate(
        "",
        xy=anchor,
        xytext=body_b_center,
        arrowprops=dict(
            arrowstyle="-|>",
            color=STYLE["accent2"],
            lw=2,
            linestyle="--",
        ),
        zorder=3,
    )
    rb_mid = (body_b_center + anchor) / 2
    ax.text(
        rb_mid[0] + 0.45,
        rb_mid[1] + 0.05,
        "$\\mathbf{r}_B$",
        color=STYLE["accent2"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
        zorder=5,
    )

    # Rotation arcs around the anchor to show free rotation
    for angle_start, angle_end in [(200, 280), (30, 110), (310, 370)]:
        arc = mpatches.Arc(
            anchor,
            1.2,
            1.2,
            angle=0,
            theta1=angle_start,
            theta2=angle_end,
            color=STYLE["accent3"],
            lw=1.5,
            linestyle=":",
            zorder=3,
        )
        ax.add_patch(arc)

    # DOF summary box
    ax.text(
        0.0,
        -0.8,
        "3 translational DOF removed\n3 rotational DOF free",
        color=STYLE["text_dim"],
        fontsize=11,
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
        bbox=dict(
            facecolor=STYLE["surface"],
            edgecolor=STYLE["grid"],
            boxstyle="round,pad=0.5",
        ),
    )

    # Constraint equation
    ax.text(
        0.0,
        -1.7,
        "$C = \\mathbf{p}_A + R_A \\, \\mathbf{r}_A^{\\,local}"
        " - \\mathbf{p}_B - R_B \\, \\mathbf{r}_B^{\\,local} = 0$",
        color=STYLE["text"],
        fontsize=11,
        ha="center",
        path_effects=_STROKE,
        zorder=5,
    )

    save(fig, _LESSON, "ball_socket_joint.png")
    plt.close(fig)


# ---------------------------------------------------------------------------
# physics/13-constraint-solver — hinge_joint.png
# ---------------------------------------------------------------------------


def diagram_hinge_joint():
    """Hinge joint: rotation axis, perpendicular constraint axes, 5 DOF.

    Shows a hinge axis (allowed rotation direction) with two perpendicular
    axes that represent the angular constraints. The point constraint is
    also shown, totaling 5 DOF removed.
    """
    fig, ax = plt.subplots(1, 1, figsize=(9, 7), facecolor=STYLE["bg"])
    fig.suptitle(
        "Hinge (Revolute) Joint",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=0.96,
    )
    fig.subplots_adjust(top=0.89, bottom=0.06, left=0.06, right=0.94)
    setup_axes(ax, xlim=(-4, 4), ylim=(-2.5, 4.5), grid=False, aspect="equal")
    ax.axis("off")

    # Hinge pivot point
    pivot = np.array([0.0, 1.5])

    # Body A (attached to world via hinge)
    body_center = np.array([1.5, 0.5])
    rect = mpatches.FancyBboxPatch(
        (body_center[0] - 1.0, body_center[1] - 0.6),
        2.0,
        1.2,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(rect)
    ax.text(
        body_center[0],
        body_center[1],
        "Body",
        color=STYLE["accent1"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )

    # World anchor symbol (triangle)
    wx, wy = -0.3, 2.8
    tri = mpatches.Polygon(
        [(wx - 0.25, wy + 0.35), (wx + 0.25, wy + 0.35), (wx, wy)],
        facecolor=STYLE["grid"],
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
        zorder=3,
    )
    ax.add_patch(tri)
    # Hatching lines for wall
    for i in range(5):
        yy = wy + 0.35 + i * 0.12
        ax.plot(
            [wx - 0.3, wx + 0.3],
            [yy, yy + 0.1],
            color=STYLE["text_dim"],
            lw=1,
            zorder=3,
        )
    ax.text(
        wx,
        wy + 1.0,
        "World",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
        zorder=5,
    )

    # Pivot marker
    ax.plot(
        *pivot,
        "D",
        color=STYLE["warn"],
        markersize=10,
        zorder=6,
        markeredgecolor=STYLE["bg"],
        markeredgewidth=1.5,
    )

    # Connection line from wall to pivot to body
    ax.plot(
        [wx, pivot[0], body_center[0] - 1.0],
        [wy, pivot[1], body_center[1] + 0.6],
        color=STYLE["text_dim"],
        lw=1.5,
        linestyle="-",
        zorder=1,
    )

    # Hinge axis (Y direction, going into/out of screen shown as vertical)
    axis_len = 1.8
    ax.annotate(
        "",
        xy=(pivot[0], pivot[1] + axis_len),
        xytext=(pivot[0], pivot[1] - axis_len * 0.3),
        arrowprops=dict(
            arrowstyle="-|>",
            color=STYLE["accent3"],
            lw=3,
        ),
        zorder=4,
    )
    ax.text(
        pivot[0] + 0.3,
        pivot[1] + axis_len - 0.2,
        "Hinge axis\n(free rotation)",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        path_effects=_STROKE,
        zorder=5,
    )

    # Rotation arc around hinge axis
    theta = np.linspace(-0.6, 2.2, 40)
    arc_r = 1.2
    arc_x = pivot[0] + arc_r * np.cos(theta)
    arc_y = pivot[1] - 0.3 + arc_r * 0.4 * np.sin(theta)
    ax.plot(arc_x, arc_y, color=STYLE["accent3"], lw=1.5, linestyle="--", zorder=3)
    # Arrow tip on arc
    ax.annotate(
        "",
        xy=(arc_x[-1], arc_y[-1]),
        xytext=(arc_x[-3], arc_y[-3]),
        arrowprops=dict(arrowstyle="-|>", color=STYLE["accent3"], lw=1.5),
        zorder=4,
    )

    # Perpendicular constraint axes (shown as blocked rotation)
    perp_color = STYLE["accent2"]

    # Perp 1 (horizontal)
    p1_start = pivot + np.array([-2.2, -0.3])
    p1_end = pivot + np.array([2.5, -0.3])
    ax.annotate(
        "",
        xy=p1_end,
        xytext=p1_start,
        arrowprops=dict(arrowstyle="<->", color=perp_color, lw=1.5, linestyle=":"),
        zorder=3,
    )
    ax.text(
        p1_end[0] + 0.15,
        p1_end[1],
        "perp$_1$",
        color=perp_color,
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )
    # X through perp1
    cx1 = (p1_start[0] + p1_end[0]) / 2 - 0.8
    cy1 = p1_start[1]
    ax.plot(
        [cx1 - 0.15, cx1 + 0.15],
        [cy1 - 0.15, cy1 + 0.15],
        color=perp_color,
        lw=2.5,
        zorder=4,
    )
    ax.plot(
        [cx1 - 0.15, cx1 + 0.15],
        [cy1 + 0.15, cy1 - 0.15],
        color=perp_color,
        lw=2.5,
        zorder=4,
    )

    # Perp 2 (depth direction, shown as diagonal)
    p2_start = pivot + np.array([-1.2, -1.5])
    p2_end = pivot + np.array([1.5, 0.8])
    ax.annotate(
        "",
        xy=p2_end,
        xytext=p2_start,
        arrowprops=dict(arrowstyle="<->", color=perp_color, lw=1.5, linestyle=":"),
        zorder=3,
    )
    ax.text(
        p2_end[0] + 0.15,
        p2_end[1] + 0.15,
        "perp$_2$",
        color=perp_color,
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="bottom",
        path_effects=_STROKE,
        zorder=5,
    )

    # DOF summary box
    ax.text(
        -3.5,
        -2.0,
        "3 translational + 2 angular DOF removed\n1 rotational DOF free (hinge axis)",
        color=STYLE["text_dim"],
        fontsize=10,
        va="center",
        path_effects=_STROKE,
        zorder=5,
        bbox=dict(
            facecolor=STYLE["surface"],
            edgecolor=STYLE["grid"],
            boxstyle="round,pad=0.4",
        ),
    )

    save(fig, _LESSON, "hinge_joint.png")
    plt.close(fig)


# ---------------------------------------------------------------------------
# physics/13-constraint-solver — slider_joint.png
# ---------------------------------------------------------------------------


def diagram_slider_joint():
    """Slider joint: slide axis, locked rotation, perpendicular constraints.

    Shows a body constrained to slide along a single axis, with rotation
    fully locked and perpendicular translation removed.
    """
    fig, ax = plt.subplots(1, 1, figsize=(10, 6), facecolor=STYLE["bg"])
    fig.suptitle(
        "Slider (Prismatic) Joint",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=0.96,
    )
    fig.subplots_adjust(top=0.89, bottom=0.08, left=0.06, right=0.94)
    setup_axes(ax, xlim=(-5, 5), ylim=(-2.5, 3.5), grid=False, aspect="equal")
    ax.axis("off")

    # Slide axis (horizontal rail)
    rail_y = 1.0
    ax.plot(
        [-4.5, 4.5],
        [rail_y, rail_y],
        color=STYLE["accent3"],
        lw=3,
        zorder=2,
    )
    # Rail tick marks
    for x in np.arange(-4, 4.5, 0.8):
        ax.plot(
            [x, x],
            [rail_y - 0.15, rail_y + 0.15],
            color=STYLE["accent3"],
            lw=1.5,
            zorder=2,
        )

    # Slide axis arrow and label
    ax.annotate(
        "",
        xy=(4.5, rail_y + 0.5),
        xytext=(-4.5, rail_y + 0.5),
        arrowprops=dict(arrowstyle="<->", color=STYLE["accent3"], lw=2.5),
        zorder=3,
    )
    ax.text(
        0,
        rail_y + 0.85,
        "Slide axis (free translation)",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
        zorder=5,
    )

    # Sliding body
    body_x = 1.0
    body_w, body_h = 1.6, 1.0
    rect = mpatches.FancyBboxPatch(
        (body_x - body_w / 2, rail_y - body_h / 2),
        body_w,
        body_h,
        boxstyle="round,pad=0.08",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2.5,
        zorder=4,
    )
    ax.add_patch(rect)
    ax.text(
        body_x,
        rail_y,
        "Body",
        color=STYLE["accent1"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )

    # Ghost positions showing sliding motion
    for gx in [-2.0, -0.5, 2.5]:
        ghost = mpatches.FancyBboxPatch(
            (gx - body_w / 2, rail_y - body_h / 2),
            body_w,
            body_h,
            boxstyle="round,pad=0.08",
            facecolor="none",
            edgecolor=STYLE["accent1"],
            linewidth=1,
            linestyle="--",
            alpha=0.3,
            zorder=2,
        )
        ax.add_patch(ghost)

    # Perpendicular constraints (vertical — blocked)
    perp_color = STYLE["accent2"]
    # Vertical blocked arrow
    ax.annotate(
        "",
        xy=(body_x + 1.5, rail_y + 1.3),
        xytext=(body_x + 1.5, rail_y - 1.3),
        arrowprops=dict(arrowstyle="<->", color=perp_color, lw=1.5, linestyle=":"),
        zorder=3,
    )
    # X through it
    cx = body_x + 1.5
    ax.plot(
        [cx - 0.15, cx + 0.15],
        [rail_y - 0.15, rail_y + 0.15],
        color=perp_color,
        lw=2.5,
        zorder=4,
    )
    ax.plot(
        [cx - 0.15, cx + 0.15],
        [rail_y + 0.15, rail_y - 0.15],
        color=perp_color,
        lw=2.5,
        zorder=4,
    )
    ax.text(
        cx + 0.3,
        rail_y + 1.0,
        "perp$_1$",
        color=perp_color,
        fontsize=10,
        fontweight="bold",
        ha="left",
        path_effects=_STROKE,
        zorder=5,
    )

    # Rotation lock symbol
    rot_x = body_x - 2.5
    rot_y = rail_y - 1.5
    theta_arc = np.linspace(0.3, 2.5, 30)
    arc_r = 0.5
    ax.plot(
        rot_x + arc_r * np.cos(theta_arc),
        rot_y + arc_r * np.sin(theta_arc),
        color=STYLE["accent4"],
        lw=2,
        zorder=3,
    )
    ax.annotate(
        "",
        xy=(
            rot_x + arc_r * np.cos(theta_arc[-1]),
            rot_y + arc_r * np.sin(theta_arc[-1]),
        ),
        xytext=(
            rot_x + arc_r * np.cos(theta_arc[-3]),
            rot_y + arc_r * np.sin(theta_arc[-3]),
        ),
        arrowprops=dict(arrowstyle="-|>", color=STYLE["accent4"], lw=2),
        zorder=4,
    )
    # X through rotation
    ax.plot(
        [rot_x - 0.2, rot_x + 0.2],
        [rot_y - 0.2, rot_y + 0.2],
        color=STYLE["accent2"],
        lw=2.5,
        zorder=4,
    )
    ax.plot(
        [rot_x - 0.2, rot_x + 0.2],
        [rot_y + 0.2, rot_y - 0.2],
        color=STYLE["accent2"],
        lw=2.5,
        zorder=4,
    )
    ax.text(
        rot_x,
        rot_y - 0.7,
        "All rotation\nlocked (3 DOF)",
        color=STYLE["accent4"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
        zorder=5,
    )

    # DOF summary
    ax.text(
        -4.5,
        -2.0,
        "3 angular + 2 linear DOF removed\n(perp + depth axes blocked)\n1 translational DOF free (slide axis)",
        color=STYLE["text_dim"],
        fontsize=10,
        va="center",
        path_effects=_STROKE,
        zorder=5,
        bbox=dict(
            facecolor=STYLE["surface"],
            edgecolor=STYLE["grid"],
            boxstyle="round,pad=0.4",
        ),
    )

    save(fig, _LESSON, "slider_joint.png")
    plt.close(fig)


# ---------------------------------------------------------------------------
# physics/13-constraint-solver — effective_mass.png
# ---------------------------------------------------------------------------


def diagram_effective_mass():
    """K matrix construction: mass + inertia contributions combine.

    Shows how the 3x3 effective mass matrix K is built from the scalar
    mass terms and the angular inertia terms via skew-symmetric matrices.
    """
    fig, ax = plt.subplots(1, 1, figsize=(11, 6), facecolor=STYLE["bg"])
    fig.suptitle(
        "Effective Mass Matrix Construction",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=0.97,
    )
    fig.subplots_adjust(top=0.88, bottom=0.05, left=0.03, right=0.97)
    ax.set_xlim(-0.5, 11)
    ax.set_ylim(-1, 5.5)
    ax.set_aspect("equal")
    ax.axis("off")

    box_w = 2.6
    box_h = 1.4

    def draw_box(x, y, text, color, sub=None):
        """Draw a rounded box with text."""
        rect = mpatches.FancyBboxPatch(
            (x - box_w / 2, y - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
            zorder=3,
        )
        ax.add_patch(rect)
        ax.text(
            x,
            y + (0.15 if sub else 0),
            text,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
            zorder=5,
        )
        if sub:
            ax.text(
                x,
                y - 0.35,
                sub,
                color=STYLE["text_dim"],
                fontsize=9,
                ha="center",
                va="center",
                path_effects=_STROKE,
                zorder=5,
            )

    # Top row: two contributions
    draw_box(
        2.0,
        4.0,
        "$(\\frac{1}{m_A} + \\frac{1}{m_B}) \\cdot I_{3\\times 3}$",
        STYLE["accent1"],
        "Mass term",
    )
    draw_box(
        5.5,
        4.0,
        "$[r_A]_\\times^T \\, I_A^{-1} \\, [r_A]_\\times$",
        STYLE["accent2"],
        "Inertia A",
    )
    draw_box(
        9.0,
        4.0,
        "$[r_B]_\\times^T \\, I_B^{-1} \\, [r_B]_\\times$",
        STYLE["accent3"],
        "Inertia B",
    )

    # Plus signs
    ax.text(
        3.75,
        4.0,
        "+",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )
    ax.text(
        7.25,
        4.0,
        "+",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )

    # Arrows down to result
    for x in [2.0, 5.5, 9.0]:
        ax.annotate(
            "",
            xy=(5.5, 2.3),
            xytext=(x, 4.0 - box_h / 2),
            arrowprops=dict(
                arrowstyle="-|>",
                color=STYLE["text_dim"],
                lw=1.5,
                connectionstyle="arc3,rad=0",
            ),
            zorder=2,
        )

    # Result box
    result_y = 1.5
    result_w = 4.0
    result_rect = mpatches.FancyBboxPatch(
        (5.5 - result_w / 2, result_y - box_h / 2),
        result_w,
        box_h,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["warn"],
        linewidth=2.5,
        zorder=3,
    )
    ax.add_patch(result_rect)
    ax.text(
        5.5,
        result_y + 0.15,
        "$K$ ($3 \\times 3$ effective mass)",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )
    ax.text(
        5.5,
        result_y - 0.35,
        "$\\lambda = K^{-1} \\cdot (- \\dot{C} - b)$",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )

    # Annotation: skew-symmetric matrix (plain text — mathtext lacks bmatrix)
    skew_lines = [
        "$[r]_\\times$ produces the cross-product matrix:",
        "  skew(r) * u  =  cross(r, u)",
    ]
    for k, line in enumerate(skew_lines):
        ax.text(
            0.5,
            0.2 - k * 0.45,
            line,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="left",
            va="center",
            path_effects=_STROKE,
            zorder=5,
        )

    save(fig, _LESSON, "effective_mass.png")
    plt.close(fig)


# ---------------------------------------------------------------------------
# physics/13-constraint-solver — solver_pipeline.png
# ---------------------------------------------------------------------------


def diagram_joint_contact_pipeline():
    """Combined solver pipeline: joints and contacts in same iteration loop.

    Flowchart showing the full physics step with joint and contact
    constraints solved together in a unified iteration loop.
    """
    fig, ax = plt.subplots(1, 1, figsize=(15, 5.5), facecolor=STYLE["bg"])
    fig.suptitle(
        "Joint + Contact Solver Pipeline",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=0.97,
    )
    fig.subplots_adjust(top=0.87, bottom=0.05, left=0.02, right=0.98)
    ax.set_xlim(-0.5, 15)
    ax.set_ylim(-1.5, 3.5)
    ax.set_aspect("equal")
    ax.axis("off")

    box_w = 1.7
    box_h = 0.9

    def draw_step(x, y, text, color, w=box_w):
        """Draw a pipeline step box."""
        rect = mpatches.FancyBboxPatch(
            (x - w / 2, y - box_h / 2),
            w,
            box_h,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
            zorder=3,
        )
        ax.add_patch(rect)
        ax.text(
            x,
            y,
            text,
            color=color,
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
            zorder=5,
        )

    def draw_arrow(x1, x2, y, color=STYLE["text_dim"]):
        """Draw a horizontal arrow between boxes."""
        ax.annotate(
            "",
            xy=(x2 - box_w / 2, y),
            xytext=(x1 + box_w / 2, y),
            arrowprops=dict(arrowstyle="-|>", color=color, lw=1.5),
            zorder=2,
        )

    # Top row: main pipeline
    steps = [
        (0.8, "Integrate\nVelocities", STYLE["text_dim"]),
        (2.8, "Detect\nContacts", STYLE["accent1"]),
        (4.8, "Prepare\nConstraints", STYLE["accent3"]),
        (6.8, "Warm\nStart", STYLE["warn"]),
        (9.2, "Store\nImpulses", STYLE["accent4"]),
        (11.2, "Position\nCorrection", STYLE["accent4"]),
        (13.2, "Integrate\nPositions", STYLE["text_dim"]),
    ]

    for x, text, color in steps:
        draw_step(x, 2.2, text, color)

    # Arrows between top row
    for i in range(len(steps) - 1):
        x1 = steps[i][0]
        x2 = steps[i + 1][0]
        if i == 3:  # Skip the iteration loop gap
            continue
        draw_arrow(x1, x2, 2.2)

    # Iteration loop box (larger)
    iter_x = 8.0
    iter_w = 2.0
    iter_rect = mpatches.FancyBboxPatch(
        (iter_x - iter_w / 2, 2.2 - box_h / 2),
        iter_w,
        box_h,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=2.5,
        zorder=3,
    )
    ax.add_patch(iter_rect)
    ax.text(
        iter_x,
        2.4,
        "N iterations",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )
    ax.text(
        iter_x,
        2.0,
        "(Gauss-Seidel)",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )

    # Arrows around iteration box (explicit endpoints to avoid reversed arrows
    # when the iteration box overlaps with adjacent step boxes)
    ax.annotate(
        "",
        xy=(iter_x - iter_w / 2, 2.2),
        xytext=(6.8 + box_w / 2, 2.2),
        arrowprops=dict(arrowstyle="-|>", color=STYLE["accent2"], lw=1.5),
        zorder=2,
    )
    ax.annotate(
        "",
        xy=(9.2 - box_w / 2, 2.2),
        xytext=(iter_x + iter_w / 2, 2.2),
        arrowprops=dict(arrowstyle="-|>", color=STYLE["accent2"], lw=1.5),
        zorder=2,
    )

    # Iteration loop detail (bottom row)
    joint_x = 7.3
    contact_x = 8.7
    detail_y = 0.5

    draw_step(joint_x, detail_y, "Joint\nSolve", STYLE["accent3"], w=1.3)
    draw_step(contact_x, detail_y, "Contact\nSolve", STYLE["accent1"], w=1.3)

    # Arrow between joint and contact
    ax.annotate(
        "",
        xy=(contact_x - 0.65, detail_y),
        xytext=(joint_x + 0.65, detail_y),
        arrowprops=dict(arrowstyle="-|>", color=STYLE["text_dim"], lw=1.5),
        zorder=2,
    )

    # Arrows from iteration box to detail
    ax.annotate(
        "",
        xy=(iter_x, 2.2 - box_h / 2),
        xytext=(iter_x, detail_y + box_h / 2 + 0.15),
        arrowprops=dict(
            arrowstyle="<->",
            color=STYLE["accent2"],
            lw=1.5,
            linestyle="--",
        ),
        zorder=2,
    )

    # Loop arrow (curved, showing iteration)
    loop_theta = np.linspace(0, np.pi, 30)
    loop_r = 0.5
    loop_cx = (joint_x + contact_x) / 2
    loop_cy = detail_y - box_h / 2 - 0.15
    loop_x = loop_cx + loop_r * 1.8 * np.cos(loop_theta)
    loop_y = loop_cy - loop_r * np.sin(loop_theta)
    ax.plot(
        loop_x,
        loop_y,
        color=STYLE["accent2"],
        lw=1.5,
        linestyle="--",
        zorder=2,
    )
    ax.annotate(
        "",
        xy=(loop_x[0] + 0.05, loop_y[0]),
        xytext=(loop_x[1], loop_y[1]),
        arrowprops=dict(arrowstyle="-|>", color=STYLE["accent2"], lw=1.5),
        zorder=3,
    )

    save(fig, _LESSON, "solver_pipeline.png")
    plt.close(fig)
