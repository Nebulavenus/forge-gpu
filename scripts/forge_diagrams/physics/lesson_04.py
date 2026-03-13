"""Diagrams for physics/04 — Rigid Body State and Orientation."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401 — registers 3D projection

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]


def _style_3d_panes(ax: Axes3D) -> None:
    """Style 3D axis panes — suppress pyright for internal mpl attributes."""
    ax.xaxis.pane.fill = False  # type: ignore[attr-defined]
    ax.yaxis.pane.fill = False  # type: ignore[attr-defined]
    ax.zaxis.pane.fill = False  # type: ignore[attr-defined]
    ax.xaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax.yaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax.zaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]


# ---------------------------------------------------------------------------
# physics/04-rigid-body-state — rigid_body_state.png
# ---------------------------------------------------------------------------


def diagram_rigid_body_state():
    """Box with position vector, body-frame axes, velocity, and angular velocity."""
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax: Axes3D = fig.add_axes([0.0, 0.0, 1.0, 1.0], projection="3d")  # type: ignore[assignment]
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1.5, 3.5)
    ax.set_ylim(-1.5, 3.5)
    ax.set_zlim(-1.5, 3.0)
    ax.tick_params(colors=STYLE["axis"], labelsize=8)
    _style_3d_panes(ax)
    ax.grid(True, color=STYLE["grid"], linewidth=0.4, alpha=0.4)

    # World origin to COM arrow (position vector)
    com = np.array([1.2, 1.2, 1.0])
    ax.quiver(
        0,
        0,
        0,
        com[0],
        com[1],
        com[2],
        color=STYLE["warn"],
        linewidth=2.0,
        arrow_length_ratio=0.12,
    )
    ax.text(
        com[0] * 0.5 - 0.3,
        com[1] * 0.5 - 0.3,
        com[2] * 0.5 + 0.15,
        "p (position)",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Draw a tilted box around COM using line segments
    # Box half-extents in body frame
    he = np.array([0.55, 0.35, 0.25])
    # Body-frame rotation: slight tilt
    angle = np.radians(20)
    Ry = np.array(
        [
            [np.cos(angle), 0, np.sin(angle)],
            [0, 1, 0],
            [-np.sin(angle), 0, np.cos(angle)],
        ]
    )
    angle2 = np.radians(15)
    Rz = np.array(
        [
            [np.cos(angle2), -np.sin(angle2), 0],
            [np.sin(angle2), np.cos(angle2), 0],
            [0, 0, 1],
        ]
    )
    R = Rz @ Ry

    corners_local = np.array(
        [
            [-he[0], -he[1], -he[2]],
            [he[0], -he[1], -he[2]],
            [he[0], he[1], -he[2]],
            [-he[0], he[1], -he[2]],
            [-he[0], -he[1], he[2]],
            [he[0], -he[1], he[2]],
            [he[0], he[1], he[2]],
            [-he[0], he[1], he[2]],
        ]
    )
    corners = (R @ corners_local.T).T + com

    edges = [
        (0, 1),
        (1, 2),
        (2, 3),
        (3, 0),  # bottom face
        (4, 5),
        (5, 6),
        (6, 7),
        (7, 4),  # top face
        (0, 4),
        (1, 5),
        (2, 6),
        (3, 7),  # verticals
    ]
    for i, j in edges:
        xs = [corners[i, 0], corners[j, 0]]
        ys = [corners[i, 1], corners[j, 1]]
        zs = [corners[i, 2], corners[j, 2]]
        ax.plot(xs, ys, zs, color=STYLE["accent4"], lw=1.5, alpha=0.8)

    # Body-frame axes
    axis_scale = 0.8
    body_x = R @ np.array([1, 0, 0]) * axis_scale
    body_y = R @ np.array([0, 1, 0]) * axis_scale
    body_z = R @ np.array([0, 0, 1]) * axis_scale

    for vec, color, label in [
        (body_x, STYLE["accent1"], "x\u1d47"),
        (body_y, STYLE["accent2"], "y\u1d47"),
        (body_z, STYLE["accent3"], "z\u1d47"),
    ]:
        ax.quiver(
            com[0],
            com[1],
            com[2],
            vec[0],
            vec[1],
            vec[2],
            color=color,
            linewidth=2.2,
            arrow_length_ratio=0.18,
        )
        tip = com + vec * 1.05
        ax.text(
            tip[0],
            tip[1],
            tip[2],
            label,
            color=color,
            fontsize=10,
            fontweight="bold",
            path_effects=_STROKE,
        )

    # COM dot
    ax.scatter(*com, color=STYLE["warn"], s=60, zorder=5)
    ax.text(
        com[0] + 0.1,
        com[1] + 0.1,
        com[2] - 0.25,
        "COM",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Velocity arrow
    vel = np.array([0.7, 0.4, 0.5])
    ax.quiver(
        com[0],
        com[1],
        com[2],
        vel[0],
        vel[1],
        vel[2],
        color=STYLE["accent1"],
        linewidth=2.2,
        arrow_length_ratio=0.18,
        linestyle="dashed",
    )
    ax.text(
        com[0] + vel[0] + 0.05,
        com[1] + vel[1],
        com[2] + vel[2] + 0.1,
        "v (velocity)",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Angular velocity arrow
    omega = np.array([-0.2, 0.8, 0.9])
    omega_norm = omega / np.linalg.norm(omega)
    ax.quiver(
        com[0],
        com[1],
        com[2],
        omega_norm[0] * 0.9,
        omega_norm[1] * 0.9,
        omega_norm[2] * 0.9,
        color=STYLE["accent2"],
        linewidth=2.2,
        arrow_length_ratio=0.18,
    )
    ax.text(
        com[0] + omega_norm[0] * 1.0,
        com[1] + omega_norm[1] * 1.05,
        com[2] + omega_norm[2] * 1.0,
        "\u03c9 (angular vel.)",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )

    ax.set_title(
        "Rigid Body State",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )
    ax.set_xlabel("X", color=STYLE["axis"], fontsize=9)
    ax.set_ylabel("Y", color=STYLE["axis"], fontsize=9)
    ax.set_zlabel("Z", color=STYLE["axis"], fontsize=9)

    fig.patch.set_facecolor(STYLE["bg"])
    save(fig, "physics/04-rigid-body-state", "rigid_body_state.png")


# ---------------------------------------------------------------------------
# physics/04-rigid-body-state — inertia_tensor.png
# ---------------------------------------------------------------------------


def diagram_inertia_tensor():
    """Box with principal axes showing relative magnitudes of Ixx, Iyy, Izz."""
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax: Axes3D = fig.add_axes([0.05, 0.05, 0.90, 0.88], projection="3d")  # type: ignore[assignment]
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-2.5, 2.5)
    ax.set_ylim(-2.5, 2.5)
    ax.set_zlim(-2.5, 2.5)
    _style_3d_panes(ax)
    ax.grid(True, color=STYLE["grid"], linewidth=0.4, alpha=0.4)
    ax.tick_params(colors=STYLE["axis"], labelsize=8)

    # Draw a box (elongated along Y)
    hx, hy, hz = 0.5, 1.2, 0.35
    corners_local = np.array(
        [
            [-hx, -hy, -hz],
            [hx, -hy, -hz],
            [hx, hy, -hz],
            [-hx, hy, -hz],
            [-hx, -hy, hz],
            [hx, -hy, hz],
            [hx, hy, hz],
            [-hx, hy, hz],
        ]
    )
    edges = [
        (0, 1),
        (1, 2),
        (2, 3),
        (3, 0),
        (4, 5),
        (5, 6),
        (6, 7),
        (7, 4),
        (0, 4),
        (1, 5),
        (2, 6),
        (3, 7),
    ]
    for i, j in edges:
        xs = [corners_local[i, 0], corners_local[j, 0]]
        ys = [corners_local[i, 1], corners_local[j, 1]]
        zs = [corners_local[i, 2], corners_local[j, 2]]
        ax.plot(xs, ys, zs, color=STYLE["surface"], lw=1.5, alpha=0.9)

    # Inertia magnitudes (box: I = m/12*(b²+c²) for each axis)
    m = 1.0
    # Ixx: rotation about x — involves y and z dimensions
    Ixx = m / 12.0 * ((2 * hy) ** 2 + (2 * hz) ** 2)
    # Iyy: rotation about y — involves x and z dimensions
    Iyy = m / 12.0 * ((2 * hx) ** 2 + (2 * hz) ** 2)
    # Izz: rotation about z — involves x and y dimensions
    Izz = m / 12.0 * ((2 * hx) ** 2 + (2 * hy) ** 2)

    max_I = max(Ixx, Iyy, Izz)
    scale = 1.8 / max_I

    axes_data = [
        (np.array([1, 0, 0]), Ixx, STYLE["accent2"], f"I\u2093\u2093 = {Ixx:.2f}"),
        (np.array([0, 1, 0]), Iyy, STYLE["accent3"], f"I\u028f\u028f = {Iyy:.2f}"),
        (np.array([0, 0, 1]), Izz, STYLE["accent1"], f"I\u1d22\u1d22 = {Izz:.2f}"),
    ]

    for direction, I_val, color, label in axes_data:
        length = I_val * scale
        # Draw both directions (principal axis goes through COM in both directions)
        for sign in [-1, 1]:
            ax.quiver(
                0,
                0,
                0,
                direction[0] * sign * length,
                direction[1] * sign * length,
                direction[2] * sign * length,
                color=color,
                linewidth=2.5,
                arrow_length_ratio=0.1 if sign == 1 else 0.0,
            )
        tip = direction * length * 1.15
        ax.text(
            tip[0],
            tip[1],
            tip[2],
            label,
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="center",
            path_effects=_STROKE,
        )

    ax.scatter(0, 0, 0, color=STYLE["warn"], s=50, zorder=5)
    ax.text(
        0.1,
        0.1,
        -0.3,
        "COM",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )

    ax.set_title(
        "Inertia Tensor — Principal Axes\n"
        "Longer arrow = greater resistance to angular acceleration",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    ax.set_xlabel("X", color=STYLE["axis"], fontsize=9)
    ax.set_ylabel("Y", color=STYLE["axis"], fontsize=9)
    ax.set_zlabel("Z", color=STYLE["axis"], fontsize=9)

    fig.patch.set_facecolor(STYLE["bg"])
    save(fig, "physics/04-rigid-body-state", "inertia_tensor.png")


# ---------------------------------------------------------------------------
# physics/04-rigid-body-state — inertia_shapes.png
# ---------------------------------------------------------------------------


def diagram_inertia_shapes():
    """Sphere, box, and cylinder side-by-side with inertia formulas."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 6), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])

    shape_data = [
        {
            "title": "Solid Sphere",
            "color": STYLE["accent1"],
            "formulas": [
                r"$I_{xx} = I_{yy} = I_{zz} = \frac{2}{5} m r^2$",
                "",
                r"All three axes equal",
                r"(fully isotropic)",
            ],
        },
        {
            "title": "Box  (w \u00d7 h \u00d7 d)",
            "color": STYLE["accent2"],
            "formulas": [
                r"$I_{xx} = \frac{m}{12}(h^2 + d^2)$",
                r"$I_{yy} = \frac{m}{12}(w^2 + d^2)$",
                r"$I_{zz} = \frac{m}{12}(w^2 + h^2)$",
                "",
                r"Elongated boxes have low",
                r"$I$ along the long axis",
            ],
        },
        {
            "title": "Cylinder  (r, h)",
            "color": STYLE["accent3"],
            "formulas": [
                r"$I_{xx} = I_{zz} = \frac{m}{12}(3r^2 + 4h^2)$",
                r"$I_{yy} = \frac{1}{2} m r^2$" + "  (spin axis)",
                "",
                r"Spin axis (Y) $<$ tipping axes",
                r"when $h > r/\sqrt{3}$ (tall cylinder)",
            ],
        },
    ]

    for ax, data in zip(axes, shape_data, strict=True):
        ax.set_facecolor(STYLE["bg"])
        ax.set_xlim(-1.5, 1.5)
        ax.set_ylim(-1.5, 2.8)
        ax.set_aspect("equal")
        ax.axis("off")

        color = data["color"]

        # Draw shape icon
        if data["title"].startswith("Solid Sphere"):
            circle = mpatches.Circle(
                (0, 0.6),
                0.7,
                facecolor=color,
                edgecolor=STYLE["text"],
                lw=2,
                alpha=0.25,
            )
            ax.add_patch(circle)
            ax.add_patch(
                mpatches.Circle(
                    (0, 0.6),
                    0.7,
                    fill=False,
                    edgecolor=color,
                    lw=2.5,
                )
            )
            # Draw equator ellipse
            ell = mpatches.Ellipse(
                (0, 0.6),
                1.4,
                0.35,
                fill=False,
                edgecolor=color,
                lw=1.5,
                alpha=0.6,
                linestyle="--",
            )
            ax.add_patch(ell)

        elif data["title"].startswith("Box"):
            rect = mpatches.FancyBboxPatch(
                (-0.65, -0.1),
                1.3,
                1.4,
                boxstyle="round,pad=0.05",
                facecolor=color,
                edgecolor=STYLE["text"],
                lw=2,
                alpha=0.20,
            )
            ax.add_patch(rect)
            ax.add_patch(
                mpatches.FancyBboxPatch(
                    (-0.65, -0.1),
                    1.3,
                    1.4,
                    boxstyle="round,pad=0.05",
                    fill=False,
                    edgecolor=color,
                    lw=2.5,
                )
            )
            # Faux 3D lines
            for ox, oy in [(0.15, 0.20)]:
                for x0, y0, x1, y1 in [
                    (-0.65, 1.30, -0.65 + ox, 1.30 + oy),
                    (0.65, 1.30, 0.65 + ox, 1.30 + oy),
                    (-0.65 + ox, -0.10 + oy, 0.65 + ox, -0.10 + oy),
                    (-0.65 + ox, 1.30 + oy, 0.65 + ox, 1.30 + oy),
                    (0.65, -0.10, 0.65 + ox, -0.10 + oy),
                    (0.65, 1.30, 0.65 + ox, 1.30 + oy),
                ]:
                    ax.plot([x0, x1], [y0, y1], color=color, lw=1.2, alpha=0.5)

        else:  # Cylinder
            # Cylinder body
            rect = mpatches.Rectangle(
                (-0.45, -0.05),
                0.9,
                1.35,
                facecolor=color,
                edgecolor=STYLE["text"],
                lw=2,
                alpha=0.20,
            )
            ax.add_patch(rect)
            ax.add_patch(
                mpatches.Rectangle(
                    (-0.45, -0.05),
                    0.9,
                    1.35,
                    fill=False,
                    edgecolor=color,
                    lw=2.5,
                )
            )
            # Top ellipse
            ax.add_patch(
                mpatches.Ellipse(
                    (0, 1.30),
                    0.9,
                    0.28,
                    facecolor=color,
                    edgecolor=color,
                    lw=2,
                    alpha=0.35,
                )
            )
            # Bottom ellipse
            ax.add_patch(
                mpatches.Ellipse(
                    (0, -0.05),
                    0.9,
                    0.28,
                    facecolor=STYLE["bg"],
                    edgecolor=color,
                    lw=2,
                    alpha=0.8,
                )
            )

        # Principal axis arrows
        ax.annotate(
            "",
            xy=(0, -0.65),
            xytext=(0, -0.95),
            arrowprops={
                "arrowstyle": "<->, head_width=0.12, head_length=0.08",
                "color": STYLE["warn"],
                "lw": 1.5,
            },
        )
        ax.annotate(
            "",
            xy=(1.0, 0.6),
            xytext=(-1.0, 0.6),
            arrowprops={
                "arrowstyle": "<->, head_width=0.12, head_length=0.08",
                "color": STYLE["warn"],
                "lw": 1.5,
            },
        )

        # Title
        ax.text(
            0,
            2.65,
            data["title"],
            color=color,
            fontsize=12,
            fontweight="bold",
            ha="center",
            path_effects=_STROKE,
        )

        # Formulas
        y_start = 2.20
        for line in data["formulas"]:
            if line:
                ax.text(
                    0,
                    y_start,
                    line,
                    color=STYLE["text"],
                    fontsize=9,
                    ha="center",
                    va="top",
                    path_effects=_STROKE,
                )
            y_start -= 0.32

    fig.suptitle(
        "Inertia Tensors for Common Shapes",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    save(fig, "physics/04-rigid-body-state", "inertia_shapes.png")


# ---------------------------------------------------------------------------
# physics/04-rigid-body-state — quaternion_rotation.png
# ---------------------------------------------------------------------------


def diagram_quaternion_rotation():
    """Unit sphere with rotation axis, angle arc, and quaternion component labels."""
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax: Axes3D = fig.add_axes([0.05, 0.02, 0.90, 0.92], projection="3d")  # type: ignore[assignment]
    ax.set_facecolor(STYLE["bg"])

    # Draw unit sphere wireframe
    u = np.linspace(0, 2 * np.pi, 36)
    v = np.linspace(0, np.pi, 18)
    xs = np.outer(np.cos(u), np.sin(v))
    ys = np.outer(np.sin(u), np.sin(v))
    zs = np.outer(np.ones_like(u), np.cos(v))
    ax.plot_wireframe(xs, ys, zs, color=STYLE["grid"], linewidth=0.4, alpha=0.25)

    # Rotation axis (unit vector)
    axis = np.array([1, 1, 1]) / np.sqrt(3)
    ax.quiver(
        0,
        0,
        0,
        axis[0] * 1.5,
        axis[1] * 1.5,
        axis[2] * 1.5,
        color=STYLE["accent3"],
        linewidth=2.5,
        arrow_length_ratio=0.12,
    )
    ax.text(
        axis[0] * 1.65,
        axis[1] * 1.65,
        axis[2] * 1.65,
        "axis\n(n)",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Rotation angle theta/2 arc from north pole toward axis
    theta = np.radians(55)  # half-angle
    # Orthogonalize axis against pole so arc stays on the unit sphere
    pole = np.array([0.0, 0.0, 1.0])
    axis_tangent = axis - np.dot(axis, pole) * pole
    axis_tangent /= np.linalg.norm(axis_tangent)
    arc_t = np.linspace(0, theta, 30)
    arc_pts = np.outer(np.cos(arc_t), pole) + np.outer(np.sin(arc_t), axis_tangent)
    ax.plot(
        arc_pts[:, 0],
        arc_pts[:, 1],
        arc_pts[:, 2],
        color=STYLE["warn"],
        lw=2.5,
        zorder=5,
    )

    # Angle label
    mid_idx = len(arc_t) // 2
    mid_pt = arc_pts[mid_idx] * 1.25
    ax.text(
        mid_pt[0],
        mid_pt[1],
        mid_pt[2],
        "\u03b8/2",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Two points on sphere: q tip and reference (north pole)
    np_pole = pole
    q_tip = np.cos(theta) * np_pole + np.sin(theta) * axis_tangent

    ax.scatter(*np_pole, color=STYLE["accent1"], s=60, zorder=6)
    ax.scatter(*q_tip, color=STYLE["accent2"], s=60, zorder=6)
    ax.text(
        np_pole[0] + 0.05,
        np_pole[1] + 0.05,
        np_pole[2] + 0.1,
        "w=cos(\u03b8/2)",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )
    ax.text(
        q_tip[0] - 0.6,
        q_tip[1] - 0.1,
        q_tip[2] + 0.05,
        "xyz=n\u00b7sin(\u03b8/2)",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Unit sphere label
    u_circ = np.linspace(0, 2 * np.pi, 100)
    ax.plot(
        np.cos(u_circ),
        np.sin(u_circ),
        np.zeros(100),
        color=STYLE["axis"],
        lw=1.0,
        alpha=0.4,
    )
    ax.plot(
        np.cos(u_circ),
        np.zeros(100),
        np.sin(u_circ),
        color=STYLE["axis"],
        lw=1.0,
        alpha=0.4,
    )
    ax.plot(
        np.zeros(100),
        np.cos(u_circ),
        np.sin(u_circ),
        color=STYLE["axis"],
        lw=1.0,
        alpha=0.4,
    )

    # Formula box
    ax.text2D(
        0.50,
        0.04,
        "q = w + xi + yj + zk       |q| = 1",
        transform=ax.transAxes,
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        fontfamily="monospace",
        path_effects=_STROKE,
    )

    ax.set_xlim(-1.6, 1.6)
    ax.set_ylim(-1.6, 1.6)
    ax.set_zlim(-1.6, 1.6)
    _style_3d_panes(ax)
    ax.tick_params(colors=STYLE["axis"], labelsize=8)
    ax.set_xlabel("X", color=STYLE["axis"], fontsize=9)
    ax.set_ylabel("Y", color=STYLE["axis"], fontsize=9)
    ax.set_zlabel("Z", color=STYLE["axis"], fontsize=9)

    ax.set_title(
        "Quaternion as Axis-Angle on the Unit Sphere",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )

    fig.patch.set_facecolor(STYLE["bg"])
    save(fig, "physics/04-rigid-body-state", "quaternion_rotation.png")


# ---------------------------------------------------------------------------
# physics/04-rigid-body-state — angular_velocity.png
# ---------------------------------------------------------------------------


def diagram_angular_velocity():
    """Spinning body with omega vector and curved arrows showing rotation direction."""
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax: Axes3D = fig.add_axes([0.05, 0.02, 0.90, 0.92], projection="3d")  # type: ignore[assignment]
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-2.5, 2.5)
    ax.set_ylim(-2.5, 2.5)
    ax.set_zlim(-1.5, 2.8)
    _style_3d_panes(ax)
    ax.grid(True, color=STYLE["grid"], linewidth=0.4, alpha=0.3)
    ax.tick_params(colors=STYLE["axis"], labelsize=8)

    # Disc / cylinder body
    theta_vals = np.linspace(0, 2 * np.pi, 48)
    r_disc = 1.1
    # Bottom circle
    xb = r_disc * np.cos(theta_vals)
    yb = r_disc * np.sin(theta_vals)
    zb = np.zeros_like(theta_vals)
    # Top circle
    xt = xb.copy()
    yt = yb.copy()
    zt = np.full_like(theta_vals, 0.4)

    ax.plot(xb, yb, zb, color=STYLE["accent4"], lw=1.8, alpha=0.7)
    ax.plot(xt, yt, zt, color=STYLE["accent4"], lw=1.8, alpha=0.7)

    # Fill disc surface (top face as polygon)
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    verts_top = [list(zip(xt, yt, zt, strict=True))]
    poly_top = Poly3DCollection(verts_top, alpha=0.18, facecolor=STYLE["accent4"])
    ax.add_collection3d(poly_top)

    # Vertical lines on cylinder edge (every 60 deg)
    for angle in np.linspace(0, 2 * np.pi, 7)[:-1]:
        xv = r_disc * np.cos(angle)
        yv = r_disc * np.sin(angle)
        ax.plot([xv, xv], [yv, yv], [0, 0.4], color=STYLE["accent4"], lw=1.2, alpha=0.5)

    # Omega vector (spin axis — along Z)
    omega_len = 2.0
    ax.quiver(
        0,
        0,
        0,
        0,
        0,
        omega_len,
        color=STYLE["accent2"],
        linewidth=3.0,
        arrow_length_ratio=0.12,
    )
    ax.text(
        0.1,
        0.1,
        omega_len + 0.2,
        "\u03c9",
        color=STYLE["accent2"],
        fontsize=16,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Curved rotation arrows (drawn as arc segments on a circle around the axis)
    n_arcs = 4
    r_arc = 1.45
    for i in range(n_arcs):
        t_start = 2 * np.pi * i / n_arcs
        t_end = t_start + 1.1
        t_arc = np.linspace(t_start, t_end, 20)
        xa = r_arc * np.cos(t_arc)
        ya = r_arc * np.sin(t_arc)
        za = np.full_like(t_arc, 0.2)
        ax.plot(xa, ya, za, color=STYLE["accent1"], lw=2.2, alpha=0.85)
        # Arrowhead at end
        dx = xa[-1] - xa[-2]
        dy = ya[-1] - ya[-2]
        ax.quiver(
            xa[-2],
            ya[-2],
            za[-1],
            dx * 2.5,
            dy * 2.5,
            0,
            color=STYLE["accent1"],
            linewidth=0.1,
            arrow_length_ratio=0.8,
            length=0.25,
        )

    # Label
    ax.text(
        1.6,
        1.0,
        0.22,
        "rotation direction",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Formula
    ax.text2D(
        0.5,
        0.04,
        "v = \u03c9 \u00d7 r        |\u03c9| = angular speed (rad/s)",
        transform=ax.transAxes,
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        fontfamily="monospace",
        path_effects=_STROKE,
    )

    ax.set_title(
        "Angular Velocity Vector",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )
    ax.set_xlabel("X", color=STYLE["axis"], fontsize=9)
    ax.set_ylabel("Y", color=STYLE["axis"], fontsize=9)
    ax.set_zlabel("Z", color=STYLE["axis"], fontsize=9)

    fig.patch.set_facecolor(STYLE["bg"])
    save(fig, "physics/04-rigid-body-state", "angular_velocity.png")


# ---------------------------------------------------------------------------
# physics/04-rigid-body-state — torque_force_at_point.png
# ---------------------------------------------------------------------------


def diagram_torque_force_at_point():
    """Box with COM, force at offset point, lever arm r, and torque = r x F."""
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.5, 9.5), ylim=(-1.5, 7.5), grid=False)

    arrow_kw = {"arrowstyle": "->,head_width=0.28,head_length=0.18"}

    # Draw box
    box = mpatches.FancyBboxPatch(
        (1.0, 1.5),
        5.0,
        3.0,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent4"],
        lw=2.0,
    )
    ax.add_patch(box)

    # COM
    com = np.array([3.5, 3.0])
    ax.plot(*com, "o", color=STYLE["warn"], markersize=10, zorder=6)
    ax.text(
        com[0] - 0.1,
        com[1] - 0.45,
        "COM",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Application point
    pt = np.array([5.5, 4.5])
    ax.plot(*pt, "D", color=STYLE["accent2"], markersize=8, zorder=6)
    ax.text(
        pt[0] + 0.2,
        pt[1] + 0.2,
        "P",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Lever arm r = P - COM
    r_vec = pt - com
    ax.annotate(
        "",
        xy=pt,
        xytext=com,
        arrowprops={**arrow_kw, "color": STYLE["accent3"], "lw": 2.5},
    )
    mid_r = (com + pt) / 2
    ax.text(
        mid_r[0] - 0.55,
        mid_r[1] + 0.15,
        "r",
        color=STYLE["accent3"],
        fontsize=14,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Force F applied at P
    F = np.array([0.6, 2.2])
    F_end = pt + F
    ax.annotate(
        "",
        xy=F_end,
        xytext=pt,
        arrowprops={**arrow_kw, "color": STYLE["accent1"], "lw": 2.5},
    )
    ax.text(
        F_end[0] + 0.15,
        F_end[1],
        "F",
        color=STYLE["accent1"],
        fontsize=14,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Torque arc (curved arrow around COM to show rotation direction)
    # tau = r x F = rz component (2D cross product)
    tau_z = r_vec[0] * F[1] - r_vec[1] * F[0]
    arc_r = 0.8
    t_arc = np.linspace(np.pi * 0.1, np.pi * 1.15, 50)
    if tau_z > 0:
        xa = com[0] + arc_r * np.cos(t_arc)
        ya = com[1] + arc_r * np.sin(t_arc)
    else:
        xa = com[0] + arc_r * np.cos(-t_arc)
        ya = com[1] + arc_r * np.sin(-t_arc)
    ax.plot(xa, ya, color=STYLE["accent2"], lw=2.5, zorder=5)
    # Arrowhead
    dx = xa[-1] - xa[-2]
    dy = ya[-1] - ya[-2]
    scale = 0.22 / (dx**2 + dy**2) ** 0.5
    ax.annotate(
        "",
        xy=(xa[-1] + dx * scale, ya[-1] + dy * scale),
        xytext=(xa[-1], ya[-1]),
        arrowprops={
            "arrowstyle": "->,head_width=0.22,head_length=0.15",
            "color": STYLE["accent2"],
            "lw": 2.0,
        },
    )
    ax.text(
        com[0] - arc_r - 0.5,
        com[1] + 0.1,
        "\u03c4 = r \u00d7 F",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Perpendicular distance annotation
    # Project F onto perpendicular of r
    r_unit = r_vec / np.linalg.norm(r_vec)
    r_perp = np.array([-r_unit[1], r_unit[0]])
    ax.annotate(
        "",
        xy=(pt[0] - r_perp[0] * 0.5, pt[1] - r_perp[1] * 0.5),
        xytext=(pt[0] + F[0] * 0.5, pt[1] + F[1] * 0.5),
        arrowprops={
            "arrowstyle": "<->, head_width=0.15, head_length=0.1",
            "color": STYLE["text_dim"],
            "lw": 1.5,
        },
    )

    # Magnitude text
    ax.text(
        7.0,
        2.5,
        f"|\u03c4| = |r||F|sin\u03b8\n  = {np.linalg.norm(r_vec):.2f} \u00d7"
        f" {np.linalg.norm(F):.2f} \u00d7 sin\u03b8",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        path_effects=_STROKE,
    )

    ax.set_title(
        "Torque: \u03c4 = r \u00d7 F",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "physics/04-rigid-body-state", "torque_force_at_point.png")


# ---------------------------------------------------------------------------
# physics/04-rigid-body-state — world_space_inertia.png
# ---------------------------------------------------------------------------


def diagram_world_space_inertia():
    """Body-space vs world-space inertia with rotation matrix R connecting them."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 6), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])

    arrow_kw = {"arrowstyle": "->,head_width=0.25,head_length=0.15"}

    for ax in (ax1, ax2):
        ax.set_facecolor(STYLE["bg"])
        ax.set_xlim(-2.5, 2.5)
        ax.set_ylim(-2.5, 2.5)
        ax.set_aspect("equal")
        ax.axis("off")

    # ── Left: body space ──
    # Axis-aligned box
    rect_b = mpatches.FancyBboxPatch(
        (-0.9, -0.55),
        1.8,
        1.1,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent4"],
        lw=2.0,
    )
    ax1.add_patch(rect_b)

    # Body-space principal axes (aligned with box)
    ax1.annotate(
        "",
        xy=(1.6, 0),
        xytext=(-1.6, 0),
        arrowprops={**arrow_kw, "color": STYLE["accent2"], "lw": 2.0},
    )
    ax1.annotate(
        "",
        xy=(0, 1.6),
        xytext=(0, -1.6),
        arrowprops={**arrow_kw, "color": STYLE["accent3"], "lw": 2.0},
    )
    ax1.text(
        1.7,
        0.1,
        "x\u1d47",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )
    ax1.text(
        0.1,
        1.7,
        "y\u1d47",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )

    ax1.text(
        0,
        -0.1,
        "I_local",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
    )

    # Diagonal inertia matrix hint
    ax1.text(
        0,
        -1.8,
        "I_local is diagonal\nwhen aligned to\nprincipal axes",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
    )

    ax1.set_title(
        "Body Space", color=STYLE["text"], fontsize=13, fontweight="bold", pad=12
    )

    # ── Right: world space (rotated box) ──
    angle = np.radians(35)
    Rmat = np.array([[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]])
    corners = np.array(
        [[-0.9, -0.55], [0.9, -0.55], [0.9, 0.55], [-0.9, 0.55], [-0.9, -0.55]]
    )
    rot_corners = (Rmat @ corners.T).T
    ax2.plot(rot_corners[:, 0], rot_corners[:, 1], color=STYLE["accent4"], lw=2.0)
    ax2.fill(rot_corners[:, 0], rot_corners[:, 1], color=STYLE["surface"], alpha=0.6)

    # World axes (fixed)
    ax2.annotate(
        "",
        xy=(1.6, 0),
        xytext=(-1.6, 0),
        arrowprops={**arrow_kw, "color": STYLE["axis"], "lw": 1.5, "alpha": 0.5},
    )
    ax2.annotate(
        "",
        xy=(0, 1.6),
        xytext=(0, -1.6),
        arrowprops={**arrow_kw, "color": STYLE["axis"], "lw": 1.5, "alpha": 0.5},
    )
    ax2.text(
        1.7, 0.1, "X", color=STYLE["axis"], fontsize=11, alpha=0.6, path_effects=_STROKE
    )
    ax2.text(
        0.1, 1.7, "Y", color=STYLE["axis"], fontsize=11, alpha=0.6, path_effects=_STROKE
    )

    # Rotated body axes
    body_x = Rmat @ np.array([1.3, 0])
    body_y = Rmat @ np.array([0, 1.3])
    ax2.annotate(
        "",
        xy=body_x,
        xytext=-body_x,
        arrowprops={**arrow_kw, "color": STYLE["accent2"], "lw": 2.0},
    )
    ax2.annotate(
        "",
        xy=body_y,
        xytext=-body_y,
        arrowprops={**arrow_kw, "color": STYLE["accent3"], "lw": 2.0},
    )

    ax2.text(
        0,
        -0.1,
        "I_world",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
    )

    ax2.text(
        0,
        -1.8,
        "I_world = R \u00b7 I_local \u00b7 R\u1d40\nfull 3\u00d73 matrix",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
    )

    ax2.set_title(
        "World Space", color=STYLE["text"], fontsize=13, fontweight="bold", pad=12
    )

    # Central arrow with label R
    fig.text(
        0.50,
        0.52,
        "R",
        color=STYLE["accent1"],
        fontsize=22,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=4, foreground=STYLE["bg"])],
    )
    fig.text(
        0.50,
        0.46,
        "\u27f6",
        color=STYLE["accent1"],
        fontsize=24,
        ha="center",
        va="center",
    )

    fig.suptitle(
        "World-Space Inertia:  I_world = R \u00b7 I_local \u00b7 R\u1d40",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    save(fig, "physics/04-rigid-body-state", "world_space_inertia.png")


# ---------------------------------------------------------------------------
# physics/04-rigid-body-state — integration_flowchart.png
# ---------------------------------------------------------------------------


def diagram_integration_flowchart():
    """Vertical flowchart: Forces → Acceleration → Velocities → Position → Orientation → I_world."""
    fig = plt.figure(figsize=(8, 10), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1.0, 7.0)
    ax.set_ylim(-0.5, 12.5)
    ax.set_aspect("equal")
    ax.axis("off")

    steps = [
        (
            "Accumulate\nForces & Torques",
            STYLE["accent1"],
            "F = \u03a3F_ext + gravity\n\u03c4 = \u03a3\u03c4_ext",
        ),
        ("Linear\nAcceleration", STYLE["accent2"], "a = F / m"),
        (
            "Angular\nAcceleration",
            STYLE["accent3"],
            "\u03b1 = I_world\u207b\u00b9 \u00b7 (\u03c4 \u2212 \u03c9\u00d7(I\u00b7\u03c9))",
        ),
        (
            "Integrate\nVelocities",
            STYLE["accent4"],
            "v += a \u00b7 dt\n\u03c9 += \u03b1 \u00b7 dt",
        ),
        ("Integrate\nPosition", STYLE["warn"], "p += v \u00b7 dt"),
        (
            "Integrate\nOrientation",
            STYLE["accent1"],
            "q += \u00bd(0,\u03c9)\u00b7q \u00b7 dt\nnormalize(q)",
        ),
        (
            "Recompute\nI_world",
            STYLE["accent2"],
            "R = quat_to_mat(q)\nI_world = R I_local R\u1d40",
        ),
    ]

    box_w = 3.2
    box_h = 1.1
    x_center = 3.0
    y_start = 11.8
    y_step = 1.75

    for i, (label, color, formula) in enumerate(steps):
        y = y_start - i * y_step
        x = x_center - box_w / 2

        rect = mpatches.FancyBboxPatch(
            (x, y - box_h),
            box_w,
            box_h,
            boxstyle="round,pad=0.12",
            facecolor=color,
            edgecolor=STYLE["text"],
            lw=1.8,
            alpha=0.22,
        )
        ax.add_patch(rect)
        border = mpatches.FancyBboxPatch(
            (x, y - box_h),
            box_w,
            box_h,
            boxstyle="round,pad=0.12",
            fill=False,
            edgecolor=color,
            lw=1.8,
        )
        ax.add_patch(border)

        ax.text(
            x_center,
            y - box_h / 2 + 0.08,
            label,
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
        )

        # Formula to the right
        ax.text(
            x_center + box_w / 2 + 0.22,
            y - box_h / 2,
            formula,
            color=color,
            fontsize=8,
            ha="left",
            va="center",
            fontfamily="monospace",
            path_effects=_STROKE,
        )

        # Arrow to next
        if i < len(steps) - 1:
            ax.annotate(
                "",
                xy=(x_center, y - box_h - 0.05),
                xytext=(x_center, y - box_h - 0.58),
                arrowprops={
                    "arrowstyle": "->,head_width=0.2,head_length=0.12",
                    "color": STYLE["text_dim"],
                    "lw": 1.8,
                },
            )

    ax.set_title(
        "Rigid Body Integration Loop",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=12,
    )
    fig.tight_layout()
    save(fig, "physics/04-rigid-body-state", "integration_flowchart.png")


# ---------------------------------------------------------------------------
# physics/04-rigid-body-state — precession.png
# ---------------------------------------------------------------------------


def diagram_precession():
    """Spinning top with gravity, torque, and precession cone."""
    fig = plt.figure(figsize=(10, 8), facecolor=STYLE["bg"])
    ax: Axes3D = fig.add_axes([0.05, 0.05, 0.90, 0.88], projection="3d")  # type: ignore[assignment]
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-2.5, 2.5)
    ax.set_ylim(-2.5, 2.5)
    ax.set_zlim(-0.5, 3.8)
    _style_3d_panes(ax)
    ax.grid(True, color=STYLE["grid"], linewidth=0.4, alpha=0.3)
    ax.tick_params(colors=STYLE["axis"], labelsize=8)

    # Ground plane
    gx, gy = np.meshgrid(np.linspace(-2, 2, 5), np.linspace(-2, 2, 5))
    gz = np.zeros_like(gx)
    ax.plot_surface(gx, gy, gz, alpha=0.08, color=STYLE["grid"])

    # Spin axis (tilted from vertical)
    tilt = np.radians(25)
    spin_axis = np.array([np.sin(tilt), 0, np.cos(tilt)])
    pivot = np.array([0, 0, 0])
    top_tip = pivot + spin_axis * 2.8

    # Top body (line from pivot to tip)
    ax.plot(
        [pivot[0], top_tip[0]],
        [pivot[1], top_tip[1]],
        [pivot[2], top_tip[2]],
        color=STYLE["accent4"],
        lw=4,
        alpha=0.9,
    )

    # Disc at mid-point of top
    mid = pivot + spin_axis * 1.6
    perp1 = np.cross(spin_axis, np.array([0, 1, 0]))
    perp1 /= np.linalg.norm(perp1)
    perp2 = np.cross(spin_axis, perp1)
    disc_t = np.linspace(0, 2 * np.pi, 48)
    r_disc = 0.55
    disc_pts = mid[:, None] + r_disc * (
        np.outer(perp1, np.cos(disc_t)) + np.outer(perp2, np.sin(disc_t))
    )
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    verts = [list(zip(disc_pts[0], disc_pts[1], disc_pts[2], strict=True))]
    poly = Poly3DCollection(verts, alpha=0.30, facecolor=STYLE["accent4"])
    ax.add_collection3d(poly)
    ax.plot(disc_pts[0], disc_pts[1], disc_pts[2], color=STYLE["accent4"], lw=1.8)

    # Spin angular momentum L along spin_axis
    ax.quiver(
        mid[0],
        mid[1],
        mid[2],
        spin_axis[0] * 1.1,
        spin_axis[1] * 1.1,
        spin_axis[2] * 1.1,
        color=STYLE["accent2"],
        linewidth=2.5,
        arrow_length_ratio=0.15,
    )
    ax.text(
        mid[0] + spin_axis[0] * 1.25 + 0.05,
        mid[1] + spin_axis[1] * 1.25,
        mid[2] + spin_axis[2] * 1.25 + 0.1,
        "L (spin)",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Gravity arrow (down at COM = mid)
    g_len = 1.0
    ax.quiver(
        mid[0],
        mid[1],
        mid[2] + 0.3,
        0,
        0,
        -g_len,
        color=STYLE["accent1"],
        linewidth=2.5,
        arrow_length_ratio=0.18,
    )
    ax.text(
        mid[0] + 0.2,
        mid[1] + 0.1,
        mid[2] - 0.4,
        "g",
        color=STYLE["accent1"],
        fontsize=12,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Torque vector (horizontal, perpendicular to spin axis and gravity)
    torque_dir = np.cross(spin_axis, np.array([0, 0, -1]))
    torque_dir /= np.linalg.norm(torque_dir)
    ax.quiver(
        pivot[0],
        pivot[1],
        pivot[2],
        torque_dir[0] * 1.1,
        torque_dir[1] * 1.1,
        torque_dir[2] * 1.1,
        color=STYLE["accent3"],
        linewidth=2.5,
        arrow_length_ratio=0.15,
    )
    ax.text(
        torque_dir[0] * 1.3,
        torque_dir[1] * 1.3,
        torque_dir[2] * 1.3 + 0.15,
        "\u03c4 = r \u00d7 mg",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # Precession cone (circle traced by spin axis tip)
    cone_r = top_tip[0]  # horizontal radius
    cone_z = top_tip[2]
    prec_t = np.linspace(0, 2 * np.pi, 72)
    prec_x = cone_r * np.cos(prec_t)
    prec_y = cone_r * np.sin(prec_t)
    prec_z = np.full_like(prec_t, cone_z)
    ax.plot(
        prec_x, prec_y, prec_z, color=STYLE["warn"], lw=1.8, linestyle="--", alpha=0.7
    )

    # Lines from pivot to cone circle (show cone shape)
    for angle in np.linspace(0, 2 * np.pi, 9)[:-1]:
        ax.plot(
            [pivot[0], cone_r * np.cos(angle)],
            [pivot[1], cone_r * np.sin(angle)],
            [pivot[2], cone_z],
            color=STYLE["warn"],
            lw=0.7,
            alpha=0.25,
        )

    ax.text(
        1.8,
        0.3,
        cone_z + 0.15,
        "precession\ncone",
        color=STYLE["warn"],
        fontsize=9,
        path_effects=_STROKE,
    )

    ax.set_title(
        "Gyroscopic Precession\n\u03c9_prec = \u03c4 / (I\u00b7\u03c9_spin)",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    ax.set_xlabel("X", color=STYLE["axis"], fontsize=9)
    ax.set_ylabel("Y", color=STYLE["axis"], fontsize=9)
    ax.set_zlabel("Z", color=STYLE["axis"], fontsize=9)

    fig.patch.set_facecolor(STYLE["bg"])
    save(fig, "physics/04-rigid-body-state", "precession.png")


# ---------------------------------------------------------------------------
# physics/04-rigid-body-state — kinetic_energy.png
# ---------------------------------------------------------------------------


def diagram_kinetic_energy():
    """Stacked bars showing KE_linear and KE_rotational for four configurations."""
    fig = plt.figure(figsize=(12, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])

    # Four scenarios
    labels = [
        "Cube\n(slow spin)",
        "Cube\n(fast spin)",
        "Sphere\n(same \u03c9)",
        "Cylinder\n(spin axis)",
    ]

    # All share the same translational velocity
    m = 1.0
    v = 2.0
    KE_lin = 0.5 * m * v**2  # same for all

    # Inertia and spin values
    side = 1.0
    r = 0.5

    I_cube = m / 6.0 * side**2  # about any axis through COM
    I_sphere = 2.0 / 5.0 * m * r**2
    I_cyl_spin = 0.5 * m * r**2  # about cylinder axis

    omega_slow = 2.0
    omega_fast = 6.0
    omega_sphere = omega_fast
    omega_cyl = omega_fast

    KE_rot_values = [
        0.5 * I_cube * omega_slow**2,
        0.5 * I_cube * omega_fast**2,
        0.5 * I_sphere * omega_sphere**2,
        0.5 * I_cyl_spin * omega_cyl**2,
    ]

    x = np.arange(len(labels))
    bar_w = 0.5

    bars_lin = ax.bar(
        x,
        [KE_lin] * len(labels),
        bar_w,
        color=STYLE["accent1"],
        edgecolor=STYLE["text"],
        lw=1.5,
        alpha=0.85,
        label="KE_linear = \u00bdmv\u00b2",
    )
    bars_rot = ax.bar(
        x,
        KE_rot_values,
        bar_w,
        bottom=[KE_lin] * len(labels),
        color=STYLE["accent2"],
        edgecolor=STYLE["text"],
        lw=1.5,
        alpha=0.85,
        label="KE_rotation = \u00bdI\u03c9\u00b2",
    )

    # Value labels
    for _i, (bar_l, bar_r, ke_r) in enumerate(
        zip(bars_lin, bars_rot, KE_rot_values, strict=True)
    ):
        ax.text(
            bar_l.get_x() + bar_l.get_width() / 2,
            KE_lin / 2,
            f"{KE_lin:.2f}",
            color=STYLE["bg"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
        )
        ax.text(
            bar_r.get_x() + bar_r.get_width() / 2,
            KE_lin + ke_r / 2,
            f"{ke_r:.2f}",
            color=STYLE["bg"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
        )
        total = KE_lin + ke_r
        ax.text(
            bar_r.get_x() + bar_r.get_width() / 2,
            total + 0.15,
            f"total={total:.2f}",
            color=STYLE["text"],
            fontsize=8,
            ha="center",
            path_effects=_STROKE,
        )

    ax.set_xticks(x)
    ax.set_xticklabels(labels, color=STYLE["text"], fontsize=10)
    ax.set_ylabel("Kinetic Energy (J)", color=STYLE["axis"], fontsize=11)
    ax.tick_params(colors=STYLE["axis"], labelsize=9)
    for sp in ax.spines.values():
        sp.set_color(STYLE["grid"])
    ax.grid(True, axis="y", color=STYLE["grid"], linewidth=0.5, alpha=0.5)
    ax.set_axisbelow(True)

    ax.legend(
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    ax.set_title(
        "Kinetic Energy: KE = \u00bdmv\u00b2 + \u00bdI\u03c9\u00b2",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    # Notes
    ax.text(
        0.5,
        -0.16,
        f"m={m:.0f} kg   v={v:.0f} m/s   \u03c9_slow={omega_slow:.0f} rad/s   "
        f"\u03c9_fast={omega_fast:.0f} rad/s",
        transform=ax.transAxes,
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        fontfamily="monospace",
    )

    fig.tight_layout(rect=[0, 0.04, 1, 1])
    save(fig, "physics/04-rigid-body-state", "kinetic_energy.png")
