"""Diagrams for physics/06 — Resting Contacts and Friction."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]


# ---------------------------------------------------------------------------
# physics/06-resting-contacts-and-friction — coulomb_friction_cone.png
# ---------------------------------------------------------------------------


def diagram_coulomb_friction_cone():
    """Coulomb friction cone showing static and dynamic cones around the normal force."""
    fig, ax = plt.subplots(figsize=(10, 8), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-3.5, 3.5), ylim=(-0.5, 5.5), aspect="equal")
    ax.axis("off")

    # --- Contact surface ---
    ax.axhline(0.0, color=STYLE["grid"], lw=2.0, xmin=0.05, xmax=0.95)
    for hx in np.arange(-3.2, 3.5, 0.4):
        ax.plot([hx, hx + 0.28], [0.0, -0.28], color=STYLE["grid"], lw=1.0, alpha=0.5)
    ax.fill_between(
        [-3.5, 3.5], [-0.5, -0.5], [0.0, 0.0], color=STYLE["surface"], alpha=0.5
    )

    # Cone half-angles
    mu_s = 0.6  # static friction coefficient  → tan(theta_s) = mu_s
    mu_d = 0.35  # dynamic friction coefficient → tan(theta_d) = mu_d
    cone_height = 4.0
    half_w_s = mu_s * cone_height  # static cone half-width at top
    half_w_d = mu_d * cone_height  # dynamic cone half-width at top

    # --- Static friction cone (outer, filled) ---
    cone_s_pts = np.array(
        [
            [0.0, 0.0],
            [-half_w_s, cone_height],
            [half_w_s, cone_height],
        ]
    )
    ax.add_patch(
        mpatches.Polygon(
            cone_s_pts,
            closed=True,
            facecolor=STYLE["accent3"],
            edgecolor=STYLE["accent3"],
            lw=2.0,
            alpha=0.12,
            zorder=2,
        )
    )
    ax.add_patch(
        mpatches.Polygon(
            cone_s_pts,
            closed=True,
            fill=False,
            edgecolor=STYLE["accent3"],
            lw=2.0,
            linestyle="--",
            zorder=3,
        )
    )

    # --- Dynamic friction boundary (sliding friction lies ON this boundary) ---
    cone_d_pts = np.array(
        [
            [0.0, 0.0],
            [-half_w_d, cone_height],
            [half_w_d, cone_height],
        ]
    )
    ax.add_patch(
        mpatches.Polygon(
            cone_d_pts,
            closed=True,
            fill=False,
            edgecolor=STYLE["accent2"],
            lw=2.0,
            zorder=4,
        )
    )

    # --- Normal force N (vertical, upward) ---
    ax.annotate(
        "",
        xy=(0.0, cone_height + 0.4),
        xytext=(0.0, 0.0),
        arrowprops={
            "arrowstyle": "->,head_width=0.28,head_length=0.15",
            "color": STYLE["accent1"],
            "lw": 2.8,
        },
        zorder=6,
    )
    ax.text(
        0.15,
        cone_height + 0.5,
        "N  (normal force)",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # --- Example resultant contact force (inside static cone) ---
    f_angle_deg = 22
    f_angle_rad = np.radians(f_angle_deg)
    f_mag = 3.2
    f_x = np.sin(f_angle_rad) * f_mag
    f_y = np.cos(f_angle_rad) * f_mag
    ax.annotate(
        "",
        xy=(f_x, f_y),
        xytext=(0.0, 0.0),
        arrowprops={
            "arrowstyle": "->,head_width=0.26,head_length=0.14",
            "color": STYLE["warn"],
            "lw": 2.5,
        },
        zorder=6,
    )
    ax.text(
        f_x + 0.15,
        f_y * 0.5,
        r"$F_{contact}$",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # --- Decomposition: normal component F_n (vertical, dashed) ---
    ax.annotate(
        "",
        xy=(0.0, f_y),
        xytext=(0.0, 0.0),
        arrowprops={
            "arrowstyle": "->,head_width=0.20,head_length=0.12",
            "color": STYLE["accent1"],
            "lw": 1.8,
            "linestyle": "dashed",
        },
        zorder=5,
    )
    ax.text(
        -0.55,
        f_y * 0.5,
        r"$F_n$",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # --- Decomposition: tangential component F_t (horizontal, dashed) ---
    ax.annotate(
        "",
        xy=(f_x, f_y),
        xytext=(0.0, f_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.20,head_length=0.12",
            "color": STYLE["accent2"],
            "lw": 1.8,
            "linestyle": "dashed",
        },
        zorder=5,
    )
    ax.text(
        f_x * 0.5,
        f_y + 0.25,
        r"$F_t$",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # --- Static cone angle annotation ---
    arc_r_s = 1.5
    theta_s = np.arctan(mu_s)
    theta_arc_s = np.linspace(np.pi / 2 - theta_s, np.pi / 2, 60)
    ax.plot(
        arc_r_s * np.cos(theta_arc_s),
        arc_r_s * np.sin(theta_arc_s),
        color=STYLE["accent3"],
        lw=1.8,
        zorder=5,
    )
    label_ang_s = np.pi / 2 - theta_s / 2
    ax.text(
        arc_r_s * np.cos(label_ang_s) + 0.08,
        arc_r_s * np.sin(label_ang_s) + 0.15,
        r"$\theta_s = \arctan(\mu_s)$",
        color=STYLE["accent3"],
        fontsize=9,
        path_effects=_STROKE,
    )

    # --- Dynamic cone angle annotation (left side) ---
    arc_r_d = 1.0
    theta_d = np.arctan(mu_d)
    theta_arc_d = np.linspace(np.pi / 2, np.pi / 2 + theta_d, 60)
    ax.plot(
        arc_r_d * np.cos(theta_arc_d),
        arc_r_d * np.sin(theta_arc_d),
        color=STYLE["accent2"],
        lw=1.8,
        zorder=5,
    )
    label_ang_d = np.pi / 2 + theta_d / 2
    ax.text(
        arc_r_d * np.cos(label_ang_d) - 2.0,
        arc_r_d * np.sin(label_ang_d) + 0.15,
        r"$\theta_d = \arctan(\mu_d)$",
        color=STYLE["accent2"],
        fontsize=9,
        path_effects=_STROKE,
    )

    # --- Legend labels for cones ---
    ax.text(
        half_w_s + 0.15,
        cone_height * 0.75,
        r"Static cone  $|F_t| \leq \mu_s N$",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )
    ax.text(
        half_w_d + 0.15,
        cone_height * 0.45,
        r"Dynamic cone  $|F_t| = \mu_d N$",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # --- Contact point dot ---
    ax.scatter([0.0], [0.0], color=STYLE["warn"], s=60, zorder=7)
    ax.text(
        0.12,
        -0.30,
        "contact point",
        color=STYLE["warn"],
        fontsize=9,
        path_effects=_STROKE,
    )

    ax.set_title(
        "Coulomb Friction Cone\n"
        r"Static: $|F_t| \leq \mu_s N$ — Dynamic: $|F_t| = \mu_d N$",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    fig.tight_layout()
    save(fig, "physics/06-resting-contacts-and-friction", "coulomb_friction_cone.png")


# ---------------------------------------------------------------------------
# physics/06-resting-contacts-and-friction — contact_normal_tangent.png
# ---------------------------------------------------------------------------


def diagram_contact_normal_tangent():
    """Sphere resting on a surface with contact point, normal, tangent plane, and velocity decomposition."""
    fig, ax = plt.subplots(figsize=(10, 8), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-3.0, 5.0), ylim=(-1.2, 6.0), aspect="equal")
    ax.axis("off")

    # --- Ground surface ---
    ax.axhline(0.0, color=STYLE["grid"], lw=2.0, xmin=0.0, xmax=1.0)
    ax.fill_between(
        [-3.0, 5.0], [-1.2, -1.2], [0.0, 0.0], color=STYLE["surface"], alpha=0.5
    )
    for hx in np.arange(-2.8, 5.0, 0.45):
        ax.plot([hx, hx + 0.30], [0.0, -0.30], color=STYLE["grid"], lw=1.0, alpha=0.5)

    # --- Sphere ---
    sphere_r = 1.8
    sphere_cx, sphere_cy = 1.0, sphere_r  # resting on the surface
    sphere = mpatches.Circle(
        (sphere_cx, sphere_cy),
        sphere_r,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        lw=2.0,
        alpha=0.18,
        zorder=3,
    )
    ax.add_patch(sphere)
    ax.add_patch(
        mpatches.Circle(
            (sphere_cx, sphere_cy),
            sphere_r,
            fill=False,
            edgecolor=STYLE["accent1"],
            lw=2.0,
            zorder=4,
        )
    )

    # Contact point at bottom of sphere
    contact = np.array([sphere_cx, 0.0])
    ax.scatter([contact[0]], [contact[1]], color=STYLE["warn"], s=70, zorder=7)
    ax.text(
        contact[0] + 0.12,
        contact[1] - 0.38,
        "contact point",
        color=STYLE["warn"],
        fontsize=9,
        path_effects=_STROKE,
    )

    # --- Normal vector n (pointing up from surface) ---
    n_end = contact + np.array([0.0, 2.2])
    ax.annotate(
        "",
        xy=tuple(n_end),
        xytext=tuple(contact),
        arrowprops={
            "arrowstyle": "->,head_width=0.28,head_length=0.15",
            "color": STYLE["accent3"],
            "lw": 2.5,
        },
        zorder=6,
    )
    ax.text(
        n_end[0] - 0.55,
        n_end[1] + 0.12,
        r"$\hat{n}$  (surface normal)",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # --- Tangent plane indicator (horizontal line through contact) ---
    ax.plot(
        [contact[0] - 2.2, contact[0] + 2.5],
        [contact[1], contact[1]],
        color=STYLE["accent4"],
        lw=2.0,
        linestyle="--",
        alpha=0.85,
        zorder=4,
    )
    ax.text(
        contact[0] + 2.55,
        contact[1],
        "tangent\nplane",
        color=STYLE["accent4"],
        fontsize=9,
        fontweight="bold",
        va="center",
        path_effects=_STROKE,
    )

    # --- Velocity vector v (angled, from contact point) ---
    v = np.array([2.0, 1.2])
    v_end = contact + v
    ax.annotate(
        "",
        xy=tuple(v_end),
        xytext=tuple(contact),
        arrowprops={
            "arrowstyle": "->,head_width=0.28,head_length=0.15",
            "color": STYLE["accent1"],
            "lw": 2.5,
        },
        zorder=6,
    )
    ax.text(
        v_end[0] + 0.12,
        v_end[1] + 0.12,
        r"$v$",
        color=STYLE["accent1"],
        fontsize=13,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    # --- v_n: normal component (vertical projection) ---
    n_hat = np.array([0.0, 1.0])
    v_n_mag = np.dot(v, n_hat)
    v_n = v_n_mag * n_hat
    v_n_end = contact + v_n
    ax.annotate(
        "",
        xy=tuple(v_n_end),
        xytext=tuple(contact),
        arrowprops={
            "arrowstyle": "->,head_width=0.24,head_length=0.13",
            "color": STYLE["accent3"],
            "lw": 2.0,
            "linestyle": "dashed",
        },
        zorder=5,
    )
    ax.text(
        v_n_end[0] - 0.70,
        v_n_end[1] * 0.5 + 0.12,
        r"$v_n \hat{n}$   $(v_n = v \cdot \hat{n})$",
        color=STYLE["accent3"],
        fontsize=9,
        path_effects=_STROKE,
    )

    # --- v_t: tangential component ---
    v_t = v - v_n
    v_t_end = contact + v_t
    ax.annotate(
        "",
        xy=tuple(v_t_end),
        xytext=tuple(contact),
        arrowprops={
            "arrowstyle": "->,head_width=0.24,head_length=0.13",
            "color": STYLE["accent2"],
            "lw": 2.0,
            "linestyle": "dashed",
        },
        zorder=5,
    )
    ax.text(
        v_t_end[0] * 0.55,
        v_t_end[1] - 0.35,
        r"$v_t = v - v_n \hat{n}$",
        color=STYLE["accent2"],
        fontsize=9,
        path_effects=_STROKE,
    )

    # Dashed completion lines for parallelogram decomposition
    ax.plot(
        [v_n_end[0], v_end[0]],
        [v_n_end[1], v_end[1]],
        color=STYLE["text_dim"],
        lw=1.2,
        linestyle=":",
        zorder=3,
    )
    ax.plot(
        [v_t_end[0], v_end[0]],
        [v_t_end[1], v_end[1]],
        color=STYLE["text_dim"],
        lw=1.2,
        linestyle=":",
        zorder=3,
    )

    # --- Centre of sphere dot ---
    ax.scatter([sphere_cx], [sphere_cy], color=STYLE["warn"], s=40, zorder=6)
    ax.text(
        sphere_cx + 0.12,
        sphere_cy - 0.22,
        "COM",
        color=STYLE["warn"],
        fontsize=9,
        path_effects=_STROKE,
    )

    ax.set_title(
        "Contact Geometry — Normal and Tangent Decomposition",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    fig.tight_layout()
    save(fig, "physics/06-resting-contacts-and-friction", "contact_normal_tangent.png")


# ---------------------------------------------------------------------------
# physics/06-resting-contacts-and-friction — box_plane_contacts.png
# ---------------------------------------------------------------------------


def diagram_box_plane_contacts():
    """Box resting on a plane in 3 orientations: face-down (4 contacts), edge (2 contacts), corner (1 contact)."""
    fig, axes = plt.subplots(1, 3, figsize=(14, 6), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])

    configs = [
        {
            "title": "Face Down\n4 contact points",
            "angle": 0.0,
            "color": STYLE["accent1"],
        },
        {
            "title": "Edge Contact\n2 contact points (3D)",
            "angle": np.radians(45),
            "color": STYLE["accent3"],
        },
        {
            "title": "Corner Contact\n1 contact point",
            "angle": np.radians(35.26),  # arctan(1/sqrt(2)) — true corner
            "color": STYLE["accent2"],
        },
    ]

    for ax, cfg in zip(axes, configs, strict=True):
        ax.set_facecolor(STYLE["bg"])
        ax.set_xlim(-3.0, 3.0)
        ax.set_ylim(-0.8, 4.0)
        ax.set_aspect("equal")
        ax.axis("off")

        angle = cfg["angle"]
        color = cfg["color"]
        bw, bh = 1.6, 1.6  # box half-extents

        # --- Ground surface ---
        ax.axhline(0.0, color=STYLE["grid"], lw=2.0, xmin=0.0, xmax=1.0)
        ax.fill_between(
            [-3.0, 3.0], [-0.8, -0.8], [0.0, 0.0], color=STYLE["surface"], alpha=0.4
        )
        for hx in np.arange(-2.8, 3.0, 0.45):
            ax.plot(
                [hx, hx + 0.28], [0.0, -0.28], color=STYLE["grid"], lw=1.0, alpha=0.5
            )

        # Box corners in local space (centred at origin)
        local_corners = np.array(
            [
                [-bw, -bh],
                [bw, -bh],
                [bw, bh],
                [-bw, bh],
            ]
        )

        # Rotate
        cos_a, sin_a = np.cos(angle), np.sin(angle)
        R = np.array([[cos_a, -sin_a], [sin_a, cos_a]])
        rotated = (R @ local_corners.T).T

        # Find the lowest point and shift so it sits on y=0
        min_y = rotated[:, 1].min()
        offset_y = -min_y
        rotated[:, 1] += offset_y

        # Draw box
        box_poly = mpatches.Polygon(
            rotated,
            closed=True,
            facecolor=color,
            edgecolor=STYLE["text"],
            lw=1.8,
            alpha=0.18,
            zorder=3,
        )
        ax.add_patch(box_poly)
        ax.add_patch(
            mpatches.Polygon(
                rotated,
                closed=True,
                fill=False,
                edgecolor=color,
                lw=2.0,
                zorder=4,
            )
        )

        # Identify contact points (corners at y == 0, within tolerance)
        tol = 1e-6
        contact_pts = rotated[np.abs(rotated[:, 1]) < tol]

        # For face-down (angle=0), force all 4 bottom corners to be contacts
        if abs(angle) < 0.01:
            contact_pts = rotated[
                :2
            ]  # bottom two corners ARE the contacts (left/right)
            # Actually for a square face down we get 2 bottom corners in 2D;
            # label it as 4-point contact (in 3D there are 4 corners)
            # Draw both bottom corners as contact points
            for cp in contact_pts:
                ax.scatter([cp[0]], [cp[1]], color=STYLE["accent2"], s=80, zorder=7)
            # Add a note that in 3D this is 4 contacts
            ax.text(
                0.0,
                -0.55,
                "4 contacts (3D corners)",
                color=STYLE["warn"],
                fontsize=8,
                ha="center",
                path_effects=_STROKE,
            )
        else:
            for cp in contact_pts:
                ax.scatter([cp[0]], [cp[1]], color=STYLE["accent2"], s=80, zorder=7)
            # For edge contact (45°), note the 3D count
            if abs(angle - np.radians(45)) < 0.01:
                ax.text(
                    0.0,
                    -0.55,
                    "2 contacts (3D edge)",
                    color=STYLE["warn"],
                    fontsize=8,
                    ha="center",
                    path_effects=_STROKE,
                )

        # Contact count annotation
        n_contacts = len(contact_pts)
        ax.text(
            0.0,
            3.5,
            f"{n_contacts} contact{'s' if n_contacts != 1 else ''} (2D)",
            color=color,
            fontsize=9,
            ha="center",
            fontweight="bold",
            path_effects=_STROKE,
        )

        ax.set_title(
            cfg["title"],
            color=color,
            fontsize=10,
            fontweight="bold",
            pad=12,
        )

    # Shared legend for contact dots
    fig.text(
        0.5,
        0.02,
        "Red dots = contact points",
        color=STYLE["accent2"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=_STROKE,
    )
    fig.suptitle(
        "Box–Plane Contact Points by Orientation",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0.06, 1, 0.94])
    save(fig, "physics/06-resting-contacts-and-friction", "box_plane_contacts.png")


# ---------------------------------------------------------------------------
# physics/06-resting-contacts-and-friction — iterative_solver_convergence.png
# ---------------------------------------------------------------------------


def diagram_iterative_solver_convergence():
    """Residual error vs solver iterations showing exponential decay for different iteration counts."""
    fig, ax = plt.subplots(figsize=(10, 7), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(1, 20), ylim=(0.0, 1.05), aspect=None)

    iterations = np.arange(1, 21)

    # Three convergence profiles: fast, medium, slow
    profiles = [
        (0.45, STYLE["accent3"], "High compliance  (fast convergence)"),
        (0.68, STYLE["accent1"], "Medium stiffness (moderate convergence)"),
        (0.82, STYLE["accent2"], "High stiffness   (slow convergence)"),
    ]

    for decay, color, label in profiles:
        # Residual: r(i) = decay^(i-1) * initial_error
        residual = decay ** (iterations - 1)
        ax.plot(
            iterations,
            residual,
            color=color,
            lw=2.5,
            label=label,
            marker="o",
            markersize=4,
        )

    # Annotate convergence threshold
    threshold = 0.01
    ax.axhline(
        threshold,
        color=STYLE["warn"],
        lw=1.5,
        linestyle="--",
        alpha=0.8,
        zorder=2,
    )
    ax.text(
        20.3,
        threshold,
        f"threshold\n({threshold})",
        color=STYLE["warn"],
        fontsize=8,
        va="center",
        path_effects=_STROKE,
    )

    # Mark where fast profile crosses threshold
    for decay, color, _ in profiles:
        residual = decay ** (iterations - 1)
        cross = np.where(residual <= threshold)[0]
        if len(cross) > 0:
            i_cross = iterations[cross[0]]
            ax.axvline(i_cross, color=color, lw=1.0, linestyle=":", alpha=0.5)

    # Formula annotation
    ax.text(
        10.5,
        0.82,
        r"$r_k \approx r_0 \cdot \rho^{k-1}$" + "\n(spectral radius ρ < 1)",
        color=STYLE["text"],
        fontsize=11,
        ha="center",
        path_effects=_STROKE,
    )

    ax.set_xlabel("Solver iterations", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("Residual error (normalized)", color=STYLE["text"], fontsize=11)
    ax.tick_params(colors=STYLE["axis"])
    ax.set_xticks(np.arange(1, 21, 2))
    ax.legend(
        framealpha=0.2,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        fontsize=9,
        loc="upper right",
    )
    ax.set_title(
        "Iterative Constraint Solver — Convergence Rate\n"
        "More iterations reduce residual error exponentially",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    fig.subplots_adjust(left=0.10, right=0.88, top=0.88, bottom=0.10)
    save(
        fig,
        "physics/06-resting-contacts-and-friction",
        "iterative_solver_convergence.png",
    )


# ---------------------------------------------------------------------------
# physics/06-resting-contacts-and-friction — impulse_resolution.png
# ---------------------------------------------------------------------------


def diagram_impulse_resolution():
    """Before/after diagram showing two bodies with velocity arrows and impulse at contact."""
    fig, axes = plt.subplots(1, 2, figsize=(13, 7), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])

    for ax in axes:
        ax.set_facecolor(STYLE["bg"])
        ax.set_xlim(-4.5, 4.5)
        ax.set_ylim(-2.5, 4.0)
        ax.set_aspect("equal")
        ax.axis("off")

    # --- LEFT PANEL: before (approaching) ---
    ax_before = axes[0]

    # Body A (circle, left side, moving right)
    body_a_cx = -0.9
    body_a_cy = 1.0
    body_r = 0.9
    ax_before.add_patch(
        mpatches.Circle(
            (body_a_cx, body_a_cy),
            body_r,
            facecolor=STYLE["accent1"],
            edgecolor=STYLE["text"],
            lw=2.0,
            alpha=0.22,
            zorder=3,
        )
    )
    ax_before.add_patch(
        mpatches.Circle(
            (body_a_cx, body_a_cy),
            body_r,
            fill=False,
            edgecolor=STYLE["accent1"],
            lw=2.0,
            zorder=4,
        )
    )
    ax_before.text(
        body_a_cx,
        body_a_cy,
        "A",
        color=STYLE["accent1"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )
    # Velocity arrow for A (rightward, drawn above sphere)
    ax_before.annotate(
        "",
        xy=(body_a_cx + 1.6, body_a_cy + 1.2),
        xytext=(body_a_cx, body_a_cy + 1.2),
        arrowprops={
            "arrowstyle": "->,head_width=0.28,head_length=0.15",
            "color": STYLE["accent1"],
            "lw": 2.5,
        },
        zorder=6,
    )
    ax_before.text(
        body_a_cx + 0.6,
        body_a_cy + 1.45,
        r"$v_A$",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    # Body B (circle, right side, moving left)
    body_b_cx = 0.9
    body_b_cy = 1.0
    ax_before.add_patch(
        mpatches.Circle(
            (body_b_cx, body_b_cy),
            body_r,
            facecolor=STYLE["accent2"],
            edgecolor=STYLE["text"],
            lw=2.0,
            alpha=0.22,
            zorder=3,
        )
    )
    ax_before.add_patch(
        mpatches.Circle(
            (body_b_cx, body_b_cy),
            body_r,
            fill=False,
            edgecolor=STYLE["accent2"],
            lw=2.0,
            zorder=4,
        )
    )
    ax_before.text(
        body_b_cx,
        body_b_cy,
        "B",
        color=STYLE["accent2"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )
    # Velocity arrow for B (leftward, drawn above sphere)
    ax_before.annotate(
        "",
        xy=(body_b_cx - 1.6, body_b_cy + 1.2),
        xytext=(body_b_cx, body_b_cy + 1.2),
        arrowprops={
            "arrowstyle": "->,head_width=0.28,head_length=0.15",
            "color": STYLE["accent2"],
            "lw": 2.5,
        },
        zorder=6,
    )
    ax_before.text(
        body_b_cx - 1.0,
        body_b_cy + 1.45,
        r"$v_B$",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    # Contact point (midway between bodies)
    contact_x = (body_a_cx + body_b_cx) / 2
    contact_y = body_a_cy
    ax_before.scatter([contact_x], [contact_y], color=STYLE["warn"], s=70, zorder=7)
    ax_before.text(
        contact_x,
        contact_y - 0.38,
        "contact",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
    )

    # Normal vector n (pointing from B to A, i.e. leftward, drawn below)
    ax_before.annotate(
        "",
        xy=(contact_x - 1.2, contact_y - 0.8),
        xytext=(contact_x, contact_y - 0.8),
        arrowprops={
            "arrowstyle": "->,head_width=0.22,head_length=0.13",
            "color": STYLE["accent3"],
            "lw": 2.0,
        },
        zorder=6,
    )
    ax_before.text(
        contact_x - 1.5,
        contact_y - 0.65,
        r"$\hat{n}$",
        color=STYLE["accent3"],
        fontsize=12,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    ax_before.text(
        0.0,
        -1.8,
        r"Before: $v_{rel} \cdot \hat{n} < 0$  (approaching)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
    )
    ax_before.set_title(
        "Before — Approaching",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=12,
    )

    # --- RIGHT PANEL: after (separating) ---
    ax_after = axes[1]

    # Body A (moved slightly left)
    ax_after.add_patch(
        mpatches.Circle(
            (body_a_cx, body_a_cy),
            body_r,
            facecolor=STYLE["accent1"],
            edgecolor=STYLE["text"],
            lw=2.0,
            alpha=0.22,
            zorder=3,
        )
    )
    ax_after.add_patch(
        mpatches.Circle(
            (body_a_cx, body_a_cy),
            body_r,
            fill=False,
            edgecolor=STYLE["accent1"],
            lw=2.0,
            zorder=4,
        )
    )
    ax_after.text(
        body_a_cx,
        body_a_cy,
        "A",
        color=STYLE["accent1"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )
    # Post-impulse velocity for A (leftward / reversed, drawn above)
    ax_after.annotate(
        "",
        xy=(body_a_cx - 1.6, body_a_cy + 1.2),
        xytext=(body_a_cx, body_a_cy + 1.2),
        arrowprops={
            "arrowstyle": "->,head_width=0.28,head_length=0.15",
            "color": STYLE["accent1"],
            "lw": 2.5,
        },
        zorder=6,
    )
    ax_after.text(
        body_a_cx - 1.0,
        body_a_cy + 1.45,
        r"$v_A'$",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    # Body B (same position)
    ax_after.add_patch(
        mpatches.Circle(
            (body_b_cx, body_b_cy),
            body_r,
            facecolor=STYLE["accent2"],
            edgecolor=STYLE["text"],
            lw=2.0,
            alpha=0.22,
            zorder=3,
        )
    )
    ax_after.add_patch(
        mpatches.Circle(
            (body_b_cx, body_b_cy),
            body_r,
            fill=False,
            edgecolor=STYLE["accent2"],
            lw=2.0,
            zorder=4,
        )
    )
    ax_after.text(
        body_b_cx,
        body_b_cy,
        "B",
        color=STYLE["accent2"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
        zorder=5,
    )
    # Post-impulse velocity for B (rightward / reversed, drawn above)
    ax_after.annotate(
        "",
        xy=(body_b_cx + 1.6, body_b_cy + 1.2),
        xytext=(body_b_cx, body_b_cy + 1.2),
        arrowprops={
            "arrowstyle": "->,head_width=0.28,head_length=0.15",
            "color": STYLE["accent2"],
            "lw": 2.5,
        },
        zorder=6,
    )
    ax_after.text(
        body_b_cx + 0.6,
        body_b_cy + 1.45,
        r"$v_B'$",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )

    # Impulse vector j*n at contact point
    ax_after.scatter([contact_x], [contact_y], color=STYLE["warn"], s=70, zorder=7)
    # Impulse arrow (downward/into contact, toward A)
    ax_after.annotate(
        "",
        xy=(contact_x - 0.75, contact_y),
        xytext=(contact_x, contact_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.30,head_length=0.16",
            "color": STYLE["warn"],
            "lw": 3.0,
        },
        zorder=7,
    )
    ax_after.annotate(
        "",
        xy=(contact_x + 0.75, contact_y),
        xytext=(contact_x, contact_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.30,head_length=0.16",
            "color": STYLE["warn"],
            "lw": 3.0,
        },
        zorder=7,
    )
    ax_after.text(
        contact_x,
        contact_y + 0.38,
        r"$j \cdot \hat{n}$  (impulse)",
        color=STYLE["warn"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=_STROKE,
    )

    ax_after.text(
        0.0,
        -1.8,
        r"After: $v_{rel}' \cdot \hat{n} \geq 0$  (separating)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
    )
    ax_after.set_title(
        "After — Separating",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=12,
    )

    # Impulse formula below
    fig.text(
        0.5,
        0.04,
        r"$j = \frac{-(1+e)(v_{rel} \cdot \hat{n})}{1/m_A + 1/m_B + \text{angular terms}}$"
        + "      (e = coefficient of restitution)",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    fig.suptitle(
        "Impulse Resolution — Velocity Correction at Contact",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0.08, 1, 0.94])
    save(fig, "physics/06-resting-contacts-and-friction", "impulse_resolution.png")
