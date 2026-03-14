"""Diagrams for physics/05 — Forces and Torques."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]


# ---------------------------------------------------------------------------
# physics/05-forces-and-torques — force_at_point.png
# ---------------------------------------------------------------------------


def diagram_force_at_point():
    """Box side-view showing force at corner, offset vector r, and torque τ = r × F."""
    fig, ax = plt.subplots(figsize=(10, 7), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-1.2, 5.5), ylim=(-1.2, 5.5), aspect="equal")
    ax.axis("off")

    # --- Box ---
    box_cx, box_cy = 2.0, 2.0
    box_w, box_h = 2.0, 1.4
    box = mpatches.FancyBboxPatch(
        (box_cx - box_w / 2, box_cy - box_h / 2),
        box_w,
        box_h,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        lw=2.0,
        alpha=0.18,
    )
    ax.add_patch(box)
    ax.add_patch(
        mpatches.FancyBboxPatch(
            (box_cx - box_w / 2, box_cy - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.05",
            fill=False,
            edgecolor=STYLE["accent1"],
            lw=2.0,
        )
    )

    # --- Centre of mass dot and label ---
    ax.scatter([box_cx], [box_cy], color=STYLE["warn"], s=60, zorder=6)
    ax.text(
        box_cx + 0.12,
        box_cy - 0.22,
        "COM",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # --- Application point (top-right corner) ---
    app_x = box_cx + box_w / 2
    app_y = box_cy + box_h / 2
    ax.scatter([app_x], [app_y], color=STYLE["accent2"], s=50, zorder=6)

    # --- r vector: COM → application point ---
    r_dx = app_x - box_cx
    r_dy = app_y - box_cy
    ax.annotate(
        "",
        xy=(app_x, app_y),
        xytext=(box_cx, box_cy),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.13",
            "color": STYLE["accent3"],
            "lw": 2.2,
        },
        zorder=5,
    )
    mid_rx = box_cx + r_dx * 0.5
    mid_ry = box_cy + r_dy * 0.5
    ax.text(
        mid_rx - 0.28,
        mid_ry + 0.15,
        "r",
        color=STYLE["accent3"],
        fontsize=14,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )
    ax.text(
        mid_rx - 0.15,
        mid_ry - 0.22,
        "(offset from COM)",
        color=STYLE["accent3"],
        fontsize=9,
        path_effects=_STROKE,
    )

    # --- F vector: applied force upward-right from the corner ---
    f_dx, f_dy = 1.1, 1.3
    ax.annotate(
        "",
        xy=(app_x + f_dx, app_y + f_dy),
        xytext=(app_x, app_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.28,head_length=0.14",
            "color": STYLE["accent2"],
            "lw": 2.5,
        },
        zorder=5,
    )
    ax.text(
        app_x + f_dx * 0.55 + 0.15,
        app_y + f_dy * 0.55 + 0.12,
        "F",
        color=STYLE["accent2"],
        fontsize=14,
        fontweight="bold",
        fontstyle="italic",
        path_effects=_STROKE,
    )
    ax.text(
        app_x + f_dx * 0.55 + 0.15,
        app_y + f_dy * 0.55 - 0.15,
        "(applied force)",
        color=STYLE["accent2"],
        fontsize=9,
        path_effects=_STROKE,
    )

    # --- Torque arc indicator (curved arrow around COM) ---
    # τ = r × F.  For these 2D vectors the magnitude is r_x*f_y − r_y*f_x > 0 → CCW
    tau_r = 0.55
    theta_start = -20
    theta_end = 200
    theta = np.linspace(np.radians(theta_start), np.radians(theta_end), 120)
    arc_x = box_cx + tau_r * np.cos(theta)
    arc_y = box_cy + tau_r * np.sin(theta)
    ax.plot(arc_x, arc_y, color=STYLE["accent4"], lw=2.5, zorder=4)
    # Arrowhead at end of arc
    dt = 0.001
    theta_tip = np.radians(theta_end)
    tip_x = box_cx + tau_r * np.cos(theta_tip)
    tip_y = box_cy + tau_r * np.sin(theta_tip)
    dtip_x = -tau_r * np.sin(theta_tip) * dt
    dtip_y = tau_r * np.cos(theta_tip) * dt
    ax.annotate(
        "",
        xy=(tip_x + dtip_x * 800, tip_y + dtip_y * 800),
        xytext=(tip_x, tip_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.22,head_length=0.12",
            "color": STYLE["accent4"],
            "lw": 2.5,
        },
        zorder=5,
    )
    ax.text(
        box_cx + tau_r + 0.12,
        box_cy + tau_r + 0.10,
        r"$\tau = r \times F$",
        color=STYLE["accent4"],
        fontsize=12,
        fontweight="bold",
        path_effects=_STROKE,
    )
    ax.text(
        box_cx + tau_r + 0.12,
        box_cy + tau_r - 0.18,
        "(torque, CCW)",
        color=STYLE["accent4"],
        fontsize=9,
        path_effects=_STROKE,
    )

    # --- Cross-product note ---
    ax.text(
        0.05,
        0.04,
        r"$|\tau| = |r| \, |F| \sin\theta$  — only the perpendicular component of F creates rotation",
        color=STYLE["text_dim"],
        fontsize=9,
        transform=ax.transAxes,
        path_effects=_STROKE,
    )

    ax.set_title(
        "Force at a Point — Torque from Off-Center Forces",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    fig.tight_layout()
    save(fig, "physics/05-forces-and-torques", "force_at_point.png")


# ---------------------------------------------------------------------------
# physics/05-forces-and-torques — force_accumulator_lifecycle.png
# ---------------------------------------------------------------------------


def diagram_force_accumulator_lifecycle():
    """Horizontal flowchart: Clear → Gravity → Drag → Friction → Integrate → Render → loop."""
    fig, ax = plt.subplots(figsize=(13, 5), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 13.5)
    ax.set_ylim(-1.5, 3.0)
    ax.set_aspect("equal")
    ax.axis("off")

    stages = [
        ("Clear\nAccumulators", STYLE["accent4"]),
        ("Apply\nGravity", STYLE["accent1"]),
        ("Apply\nLinear Drag", STYLE["accent1"]),
        ("Apply\nAngular Drag", STYLE["accent1"]),
        ("Apply\nFriction", STYLE["accent1"]),
        ("Integrate", STYLE["accent2"]),
        ("Render", STYLE["accent3"]),
    ]

    box_w = 1.35
    box_h = 0.95
    gap = 0.45
    start_x = 0.3
    cy = 1.1

    centers = []
    for i, (label, color) in enumerate(stages):
        cx = start_x + i * (box_w + gap)
        centers.append(cx)
        box = mpatches.FancyBboxPatch(
            (cx - box_w / 2, cy - box_h / 2),
            box_w,
            box_h,
            boxstyle="round,pad=0.08",
            facecolor=color,
            edgecolor=STYLE["text"],
            lw=1.5,
            alpha=0.22,
        )
        ax.add_patch(box)
        ax.add_patch(
            mpatches.FancyBboxPatch(
                (cx - box_w / 2, cy - box_h / 2),
                box_w,
                box_h,
                boxstyle="round,pad=0.08",
                fill=False,
                edgecolor=color,
                lw=2.0,
            )
        )
        ax.text(
            cx,
            cy,
            label,
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
        )

    # Forward arrows between boxes
    for i in range(len(stages) - 1):
        x0 = centers[i] + box_w / 2
        x1 = centers[i + 1] - box_w / 2
        ax.annotate(
            "",
            xy=(x1, cy),
            xytext=(x0, cy),
            arrowprops={
                "arrowstyle": "->,head_width=0.22,head_length=0.13",
                "color": STYLE["warn"],
                "lw": 1.8,
            },
        )

    # Loop-back arrow from Render back to Clear
    last_cx = centers[-1]
    first_cx = centers[0]
    loop_y = cy - box_h / 2 - 0.55
    # Down from Render
    ax.annotate(
        "",
        xy=(last_cx, loop_y),
        xytext=(last_cx, cy - box_h / 2),
        arrowprops={
            "arrowstyle": "-",
            "color": STYLE["warn"],
            "lw": 1.8,
        },
    )
    # Left along the bottom
    ax.annotate(
        "",
        xy=(first_cx, loop_y),
        xytext=(last_cx, loop_y),
        arrowprops={
            "arrowstyle": "-",
            "color": STYLE["warn"],
            "lw": 1.8,
        },
    )
    # Up to Clear box
    ax.annotate(
        "",
        xy=(first_cx, cy - box_h / 2),
        xytext=(first_cx, loop_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.22,head_length=0.13",
            "color": STYLE["warn"],
            "lw": 1.8,
        },
    )
    mid_loop_x = (first_cx + last_cx) / 2
    ax.text(
        mid_loop_x,
        loop_y - 0.22,
        "next frame",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
    )

    # Sub-labels: force application phase bracket
    brace_x0 = centers[1] - box_w / 2 - 0.05
    brace_x1 = centers[4] + box_w / 2 + 0.05
    brace_y = cy + box_h / 2 + 0.25
    ax.annotate(
        "",
        xy=(brace_x1, brace_y),
        xytext=(brace_x0, brace_y),
        arrowprops={
            "arrowstyle": "|-|,widthA=0.15,widthB=0.15",
            "color": STYLE["accent1"],
            "lw": 1.5,
        },
    )
    ax.text(
        (brace_x0 + brace_x1) / 2,
        brace_y + 0.18,
        "accumulate forces & torques",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
    )

    ax.set_title(
        "Force Accumulator Lifecycle",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    fig.tight_layout(pad=0.5)
    save(fig, "physics/05-forces-and-torques", "force_accumulator_lifecycle.png")


# ---------------------------------------------------------------------------
# physics/05-forces-and-torques — drag_terminal_velocity.png
# ---------------------------------------------------------------------------


def diagram_drag_terminal_velocity():
    """Velocity vs. time curves for three drag coefficients with terminal velocity lines."""
    fig, ax = plt.subplots(figsize=(10, 7), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(0, 10), ylim=(0, 110), aspect=None)

    m = 5.0  # kg
    g = 9.81  # m/s²
    t = np.linspace(0, 10, 500)

    drag_cases = [
        (0.5, STYLE["accent1"], "k = 0.5"),
        (2.0, STYLE["accent2"], "k = 2.0"),
        (5.0, STYLE["accent3"], "k = 5.0"),
    ]

    for k, color, label in drag_cases:
        # Analytic solution: v(t) = (mg/k)(1 − e^(−kt/m))
        v_term = m * g / k
        v = v_term * (1.0 - np.exp(-k * t / m))
        ax.plot(t, v, color=color, lw=2.5, label=label)

        # Terminal velocity dashed line
        ax.axhline(
            v_term,
            color=color,
            lw=1.2,
            linestyle="--",
            alpha=0.65,
        )
        ax.text(
            10.08,
            v_term,
            f"$v_t = {v_term:.1f}$ m/s",
            color=color,
            fontsize=8.5,
            va="center",
            path_effects=_STROKE,
        )

    # Formula annotation
    ax.text(
        5.0,
        103.0,
        r"$v(t) = \frac{mg}{k}\left(1 - e^{-kt/m}\right)$",
        color=STYLE["text"],
        fontsize=11,
        ha="center",
        path_effects=_STROKE,
    )
    ax.text(
        5.0,
        93.0,
        r"$F_{drag} = -k\,v \quad$ (linear drag),  $\quad v_t = mg/k$",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
    )

    ax.set_xlabel("Time (s)", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("Speed (m/s)", color=STYLE["text"], fontsize=11)
    ax.tick_params(colors=STYLE["axis"])
    ax.legend(
        framealpha=0.2,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        fontsize=10,
        loc="lower right",
    )
    ax.set_title(
        "Drag Force and Terminal Velocity\n"
        r"m = 5 kg,  g = 9.81 m/s²",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    fig.subplots_adjust(left=0.09, right=0.88, top=0.88, bottom=0.10)
    save(fig, "physics/05-forces-and-torques", "drag_terminal_velocity.png")


# ---------------------------------------------------------------------------
# physics/05-forces-and-torques — gyroscopic_stability.png
# ---------------------------------------------------------------------------


def diagram_gyroscopic_stability():
    """Side-by-side: fast-spinning top (stable) vs. slow-spinning top (falling)."""
    fig, axes = plt.subplots(1, 2, figsize=(12, 7), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])

    configs = [
        {
            "title": "Fast Spin — Gyroscopically Stable",
            "tilt": 0.08,  # radians — nearly upright
            "stable": True,
            "color": STYLE["accent3"],
        },
        {
            "title": "Slow Spin — Falling Over",
            "tilt": 0.70,  # radians — noticeably tilted
            "stable": False,
            "color": STYLE["accent2"],
        },
    ]

    for ax, cfg in zip(axes, configs, strict=True):
        ax.set_facecolor(STYLE["bg"])
        ax.set_xlim(-2.2, 2.2)
        ax.set_ylim(-0.5, 4.5)
        ax.set_aspect("equal")
        ax.axis("off")

        tilt = cfg["tilt"]
        color = cfg["color"]

        # --- Ground surface ---
        ax.axhline(0.0, color=STYLE["grid"], lw=1.5, xmin=0.05, xmax=0.95)

        # --- Top body: a tapered cone-like shape ---
        # Pivot at origin, tip of top at (sin(tilt)*2.5, cos(tilt)*2.5)
        pivot = np.array([0.0, 0.0])
        top_len = 2.5
        spin_dir = np.array([np.sin(tilt), np.cos(tilt)])
        tip = pivot + spin_dir * top_len

        # Body as a fat line with varying width using a filled polygon
        perp = np.array([-spin_dir[1], spin_dir[0]])
        body_half_w = 0.28
        tip_half_w = 0.04
        body_pts = np.array(
            [
                pivot + perp * tip_half_w,
                tip + perp * body_half_w,
                tip - perp * body_half_w,
                pivot - perp * tip_half_w,
            ]
        )
        body_patch = mpatches.Polygon(
            body_pts,
            closed=True,
            facecolor=color,
            edgecolor=STYLE["text"],
            lw=1.5,
            alpha=0.35,
            zorder=3,
        )
        ax.add_patch(body_patch)
        ax.add_patch(
            mpatches.Polygon(
                body_pts,
                closed=True,
                fill=False,
                edgecolor=color,
                lw=2.0,
                zorder=4,
            )
        )

        # --- Angular momentum vector L (along spin axis, upward) ---
        l_start = tip
        l_end = tip + spin_dir * 0.9
        ax.annotate(
            "",
            xy=tuple(l_end),
            xytext=tuple(l_start),
            arrowprops={
                "arrowstyle": "->,head_width=0.22,head_length=0.13",
                "color": STYLE["accent1"],
                "lw": 2.2,
            },
            zorder=5,
        )
        ax.text(
            l_end[0] + 0.14,
            l_end[1] + 0.05,
            "L",
            color=STYLE["accent1"],
            fontsize=13,
            fontweight="bold",
            fontstyle="italic",
            path_effects=_STROKE,
        )

        # --- Gravity torque τ (into the page for this side view) ---
        # τ = r × mg: with r and mg both in the x-y plane, torque is along ±z.
        # For a rightward tilt with downward gravity, τ points into the page (⊗).
        tau_pos = tip + np.array([0.5, 0.0])
        circle_r = 0.18
        theta_c = np.linspace(0, 2 * np.pi, 100)
        ax.plot(
            tau_pos[0] + circle_r * np.cos(theta_c),
            tau_pos[1] + circle_r * np.sin(theta_c),
            color=STYLE["warn"],
            lw=2.0,
            zorder=5,
        )
        # ⊗ cross for "into page"
        d = circle_r * 0.65
        ax.plot(
            [tau_pos[0] - d, tau_pos[0] + d],
            [tau_pos[1] - d, tau_pos[1] + d],
            color=STYLE["warn"],
            lw=1.8,
            zorder=5,
        )
        ax.plot(
            [tau_pos[0] - d, tau_pos[0] + d],
            [tau_pos[1] + d, tau_pos[1] - d],
            color=STYLE["warn"],
            lw=1.8,
            zorder=5,
        )
        ax.text(
            tau_pos[0] + circle_r + 0.12,
            tau_pos[1],
            r"$\tau_g$" + " (into page)",
            color=STYLE["warn"],
            fontsize=10,
            fontweight="bold",
            path_effects=_STROKE,
        )

        # --- Precession circle (fast spin only) ---
        if cfg["stable"]:
            prec_r = 0.50
            theta_prec = np.linspace(0, 2 * np.pi, 200)
            prec_x = prec_r * np.cos(theta_prec)
            prec_y = tip[1] + prec_r * 0.20 * np.sin(theta_prec)
            ax.plot(
                prec_x,
                prec_y,
                color=STYLE["accent4"],
                lw=1.6,
                linestyle="--",
                alpha=0.8,
                zorder=2,
            )
            # Small precession arrow
            p_idx = 80
            ax.annotate(
                "",
                xy=(prec_x[p_idx + 2], prec_y[p_idx + 2]),
                xytext=(prec_x[p_idx], prec_y[p_idx]),
                arrowprops={
                    "arrowstyle": "->,head_width=0.15,head_length=0.10",
                    "color": STYLE["accent4"],
                    "lw": 1.6,
                },
                zorder=5,
            )
            ax.text(
                prec_r + 0.12,
                tip[1] - 0.05,
                "precession",
                color=STYLE["accent4"],
                fontsize=9,
                path_effects=_STROKE,
            )

        # --- ω spin arrow (small, at top of body) ---
        omega_center = pivot + spin_dir * top_len * 0.55
        omega_r = 0.32
        t_arc = np.linspace(0, 1.7 * np.pi, 120)
        ox = (
            omega_center[0]
            + omega_r * np.cos(t_arc) * np.abs(np.cos(tilt))
            - omega_r * np.sin(t_arc) * np.sin(tilt)
        )
        oy = (
            omega_center[1]
            + omega_r * np.cos(t_arc) * np.sin(tilt)
            + omega_r * np.sin(t_arc) * np.cos(tilt) * 0.35
        )
        ax.plot(ox, oy, color=color, lw=1.5, alpha=0.75, zorder=3)
        ax.annotate(
            "",
            xy=(ox[-1] + 0.01, oy[-1] + 0.03),
            xytext=(ox[-1], oy[-1]),
            arrowprops={
                "arrowstyle": "->,head_width=0.15,head_length=0.10",
                "color": color,
                "lw": 1.5,
            },
            zorder=4,
        )
        ax.text(
            omega_center[0] - omega_r - 0.25,
            omega_center[1],
            r"$\omega$",
            color=color,
            fontsize=12,
            fontweight="bold",
            fontstyle="italic",
            ha="right",
            path_effects=_STROKE,
        )

        # --- Pivot dot ---
        ax.scatter([0], [0], color=STYLE["warn"], s=55, zorder=6)

        # --- Gravity arrow (downward from COM) ---
        com_pt = pivot + spin_dir * top_len * 0.5
        ax.annotate(
            "",
            xy=(com_pt[0], com_pt[1] - 0.60),
            xytext=tuple(com_pt),
            arrowprops={
                "arrowstyle": "->,head_width=0.18,head_length=0.11",
                "color": STYLE["text_dim"],
                "lw": 1.8,
            },
            zorder=4,
        )
        ax.text(
            com_pt[0] + 0.12,
            com_pt[1] - 0.30,
            "g",
            color=STYLE["text_dim"],
            fontsize=10,
            fontstyle="italic",
            path_effects=_STROKE,
        )

        ax.set_title(
            cfg["title"],
            color=color,
            fontsize=11,
            fontweight="bold",
            pad=12,
        )

    fig.suptitle(
        "Gyroscopic Stability — Fast vs. Slow Spin",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.94])
    save(fig, "physics/05-forces-and-torques", "gyroscopic_stability.png")


# ---------------------------------------------------------------------------
# physics/05-forces-and-torques — friction_decomposition.png
# ---------------------------------------------------------------------------


def diagram_friction_decomposition():
    """Block on surface: velocity, normal, v_tangent projection, and friction force."""
    fig, ax = plt.subplots(figsize=(10, 7), facecolor=STYLE["bg"])
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-1.5, 6.5), ylim=(-1.2, 5.5), aspect="equal")
    ax.axis("off")

    # --- Surface ---
    ax.axhline(0.0, color=STYLE["grid"], lw=2.0, xmin=0.0, xmax=1.0)
    ax.fill_between(
        [-1.5, 6.5], [-1.2, -1.2], [0.0, 0.0], color=STYLE["surface"], alpha=0.5
    )

    # Hatching to indicate ground
    for hx in np.arange(-1.2, 6.5, 0.45):
        ax.plot([hx, hx + 0.3], [0.0, -0.3], color=STYLE["grid"], lw=1.0, alpha=0.6)

    # --- Block ---
    block_cx, block_cy = 2.5, 0.65
    bw, bh = 1.2, 0.9
    block = mpatches.FancyBboxPatch(
        (block_cx - bw / 2, block_cy - bh / 2),
        bw,
        bh,
        boxstyle="round,pad=0.04",
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        lw=1.8,
        alpha=0.22,
    )
    ax.add_patch(block)
    ax.add_patch(
        mpatches.FancyBboxPatch(
            (block_cx - bw / 2, block_cy - bh / 2),
            bw,
            bh,
            boxstyle="round,pad=0.04",
            fill=False,
            edgecolor=STYLE["accent1"],
            lw=2.0,
        )
    )

    # --- Contact point at bottom-centre of block ---
    contact = np.array([block_cx, block_cy - bh / 2])
    ax.scatter([contact[0]], [contact[1]], color=STYLE["warn"], s=50, zorder=6)

    # --- Contact-point velocity vector (from the contact point) ---
    v = np.array([2.2, 0.6])
    v_start = contact
    v_end = v_start + v
    ax.annotate(
        "",
        xy=tuple(v_end),
        xytext=tuple(v_start),
        arrowprops={
            "arrowstyle": "->,head_width=0.28,head_length=0.15",
            "color": STYLE["accent1"],
            "lw": 2.5,
        },
        zorder=5,
    )
    ax.text(
        v_end[0] + 0.10,
        v_end[1] + 0.12,
        r"$v_p = v + \omega \times r$",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # --- Surface normal vector (pointing up) ---
    n = np.array([0.0, 1.0])
    n_start = contact
    n_end = n_start + n * 1.4
    ax.annotate(
        "",
        xy=tuple(n_end),
        xytext=tuple(n_start),
        arrowprops={
            "arrowstyle": "->,head_width=0.24,head_length=0.13",
            "color": STYLE["accent3"],
            "lw": 2.2,
        },
        zorder=5,
    )
    ax.text(
        n_end[0] - 0.40,
        n_end[1] + 0.12,
        "n̂  (surface normal)",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        path_effects=_STROKE,
    )

    # --- v_tangent: project v onto surface plane (remove normal component) ---
    # v_normal_component = (v · n̂) * n̂
    v_n_mag = np.dot(v, n)
    v_normal = v_n_mag * n
    v_tangent = v - v_normal

    vt_start = v_start
    vt_end = vt_start + v_tangent
    ax.annotate(
        "",
        xy=tuple(vt_end),
        xytext=tuple(vt_start),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.14",
            "color": STYLE["accent4"],
            "lw": 2.2,
            "linestyle": "dashed",
        },
        zorder=5,
    )
    ax.text(
        vt_start[0] + v_tangent[0] * 0.5,
        vt_start[1] + v_tangent[1] * 0.5 - 0.35,
        r"$v_t = v_p - (v_p \cdot \hat{n})\hat{n}$",
        color=STYLE["accent4"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    # Dashed decomposition line (v_normal component, vertical)
    ax.plot(
        [vt_end[0], v_end[0]],
        [vt_end[1], v_end[1]],
        color=STYLE["text_dim"],
        lw=1.3,
        linestyle=":",
        zorder=3,
    )
    ax.text(
        (vt_end[0] + v_end[0]) / 2 + 0.14,
        (vt_end[1] + v_end[1]) / 2,
        r"$v_n = (v_p \cdot \hat{n})\hat{n}$",
        color=STYLE["text_dim"],
        fontsize=9,
        path_effects=_STROKE,
    )

    # --- Friction force: opposes v_tangent ---
    vt_norm = np.linalg.norm(v_tangent)
    vt_hat = v_tangent / vt_norm if vt_norm > 1e-6 else np.array([1.0, 0.0])
    friction_mag = 0.85
    f_fric = -vt_hat * friction_mag
    ff_start = contact
    ff_end = ff_start + f_fric
    ax.annotate(
        "",
        xy=tuple(ff_end),
        xytext=tuple(ff_start),
        arrowprops={
            "arrowstyle": "->,head_width=0.26,head_length=0.14",
            "color": STYLE["accent2"],
            "lw": 2.5,
        },
        zorder=5,
    )
    ax.text(
        ff_end[0] - 0.08,
        ff_end[1] - 0.28,
        r"$F_{friction} = -k_f \, |v_t| \, \hat{v}_{tangent}$",
        color=STYLE["accent2"],
        fontsize=10,
        ha="center",
        path_effects=_STROKE,
    )

    ax.set_title(
        "Contact Friction — Velocity Decomposition",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=12,
    )
    fig.tight_layout()
    save(fig, "physics/05-forces-and-torques", "friction_decomposition.png")
