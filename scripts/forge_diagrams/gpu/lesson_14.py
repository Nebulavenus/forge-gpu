"""Diagrams for gpu/14."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save

# ---------------------------------------------------------------------------
# gpu/14-environment-mapping — reflection_mapping.png
# ---------------------------------------------------------------------------


def diagram_reflection_mapping():
    """Environment reflection mapping: incident view ray reflects off a surface
    and samples a cube map.

    Shows the key vectors for environment mapping: V (view direction from surface
    to camera), N (surface normal), I = -V (incident direction), and R (the
    reflected direction that samples the cube map).  The formula
    R = I - 2(I . N)N is annotated.  A small cube map indicator shows how R
    is used to sample the environment.
    """
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.grid(False)

    # --- Surface (slightly curved to suggest a hull) ---
    surf_y = 0.0
    surf_x = np.linspace(-4.0, 4.0, 100)
    surf_curve = surf_y + 0.04 * (surf_x**2)  # gentle upward curve
    ax.fill_between(
        surf_x,
        surf_curve,
        surf_curve - 0.5,
        color=STYLE["surface"],
        alpha=0.8,
        zorder=1,
    )
    ax.plot(surf_x, surf_curve, "-", color=STYLE["axis"], lw=2, zorder=2)
    ax.text(
        3.6,
        surf_y - 0.25,
        "surface",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="right",
        style="italic",
    )

    # --- Shading point P ---
    P = np.array([0.0, surf_y])
    ax.plot(P[0], P[1], "o", color=STYLE["text"], markersize=7, zorder=10)
    ax.text(
        P[0] - 0.25,
        P[1] - 0.4,
        "P",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
    )

    # --- Vector definitions ---
    arrow_len = 2.8
    view_angle = np.radians(50)  # V is 50 degrees from N toward the right

    # Normal N — straight up from surface
    N_dir = np.array([0.0, 1.0])
    N_end = N_dir * arrow_len

    # View direction V — upper-right (from surface toward camera)
    V_dir = np.array([np.sin(view_angle), np.cos(view_angle)])
    V_end = V_dir * arrow_len

    # Incident direction I = -V (from camera toward surface)
    I_dir = -V_dir

    # Reflected direction R = I - 2(I . N)N
    IdotN = np.dot(I_dir, N_dir)
    R_dir = I_dir - 2.0 * IdotN * N_dir
    R_dir = R_dir / np.linalg.norm(R_dir)
    R_end = R_dir * arrow_len

    # --- Draw the incoming ray (dashed, from upper-right toward P) ---
    # Show a ray coming from the camera toward the surface
    incoming_start = P + V_dir * 3.2
    ax.annotate(
        "",
        xy=(P[0], P[1]),
        xytext=(incoming_start[0], incoming_start[1]),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.1",
            "color": STYLE["text_dim"],
            "lw": 1.5,
            "linestyle": "dashed",
        },
        zorder=3,
    )
    # "eye" label
    ax.text(
        incoming_start[0] + 0.2,
        incoming_start[1] + 0.15,
        "eye",
        color=STYLE["text_dim"],
        fontsize=10,
        style="italic",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Draw vectors from P ---
    def draw_arrow(start, end, color, lw=2.5, ls="-"):
        ax.annotate(
            "",
            xy=(start[0] + end[0], start[1] + end[1]),
            xytext=(start[0], start[1]),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.12",
                "color": color,
                "lw": lw,
                "linestyle": ls,
            },
            zorder=5,
        )

    def label_vec(end, text, color, offset):
        pos = P + end
        ax.text(
            pos[0] + offset[0],
            pos[1] + offset[1],
            text,
            color=color,
            fontsize=14,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=4, foreground=STYLE["bg"])],
            zorder=8,
        )

    # N — surface normal (green)
    draw_arrow(P, N_end, STYLE["accent3"], lw=3)
    label_vec(N_end, "N", STYLE["accent3"], (0.3, 0.15))

    # V — view direction (orange)
    draw_arrow(P, V_end, STYLE["accent2"], lw=2.5)
    label_vec(V_end, "V", STYLE["accent2"], (0.35, 0.1))

    # R — reflected direction (cyan, prominent)
    draw_arrow(P, R_end, STYLE["accent1"], lw=3)
    label_vec(R_end, "R", STYLE["accent1"], (-0.35, 0.15))

    # I = -V — incident direction (dim, dashed, shorter)
    I_end_short = I_dir * (arrow_len * 0.6)
    draw_arrow(P, I_end_short, STYLE["text_dim"], lw=1.5, ls="dashed")
    label_vec(I_end_short, "I = \u2212V", STYLE["text_dim"], (-0.1, -0.35))

    # --- Symmetry guides (purple) showing I and R are mirror images about N ---
    I_tip = P + I_dir * arrow_len * 0.6
    R_tip = P + R_dir * arrow_len * 0.6

    # Thin dotted lines from I tip and R tip through the normal axis
    ax.plot(
        [I_tip[0], I_tip[0], R_tip[0]],
        [I_tip[1], N_dir[1] * np.dot(I_tip - P, N_dir) + P[1], R_tip[1]],
        "--",
        color=STYLE["accent4"],
        lw=1,
        alpha=0.5,
        zorder=3,
    )

    # --- Angle arcs ---
    def draw_arc(angle_from, angle_to, radius, color, label, label_r=None):
        # Angles in radians from +Y axis (clockwise positive)
        a1 = np.pi / 2 - angle_from
        a2 = np.pi / 2 - angle_to
        if a1 > a2:
            a1, a2 = a2, a1
        t = np.linspace(a1, a2, 40)
        ax.plot(
            P[0] + radius * np.cos(t),
            P[1] + radius * np.sin(t),
            "-",
            color=color,
            lw=1.5,
            alpha=0.8,
            zorder=6,
        )
        mid_t = (a1 + a2) / 2
        lr = label_r if label_r else radius + 0.2
        ax.text(
            P[0] + lr * np.cos(mid_t),
            P[1] + lr * np.sin(mid_t),
            label,
            color=color,
            fontsize=11,
            ha="center",
            va="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=7,
        )

    # Angle between N and V (theta)
    V_angle_from_y = np.arctan2(V_dir[0], V_dir[1])
    draw_arc(0, V_angle_from_y, 1.0, STYLE["accent2"], "\u03b8", 1.3)

    # Angle between N and R (also theta — reflection is symmetric)
    R_angle_from_y = np.arctan2(R_dir[0], R_dir[1])
    draw_arc(0, R_angle_from_y, 1.4, STYLE["accent1"], "\u03b8", 1.7)

    # --- Cube map indicator (small box in upper-left corner) ---
    cube_cx, cube_cy = -3.0, 3.0
    cube_size = 0.5
    # Draw a small cube outline
    corners = [
        (-1, -1),
        (1, -1),
        (1, 1),
        (-1, 1),
        (-1, -1),
    ]
    for i in range(len(corners) - 1):
        x1 = cube_cx + corners[i][0] * cube_size
        y1 = cube_cy + corners[i][1] * cube_size
        x2 = cube_cx + corners[i + 1][0] * cube_size
        y2 = cube_cy + corners[i + 1][1] * cube_size
        ax.plot([x1, x2], [y1, y2], "-", color=STYLE["accent1"], lw=1.5, zorder=5)
    # 3D effect: offset back face
    off = 0.3
    for i in range(len(corners) - 1):
        x1 = cube_cx + corners[i][0] * cube_size + off
        y1 = cube_cy + corners[i][1] * cube_size + off
        x2 = cube_cx + corners[i + 1][0] * cube_size + off
        y2 = cube_cy + corners[i + 1][1] * cube_size + off
        ax.plot(
            [x1, x2], [y1, y2], "-", color=STYLE["accent1"], lw=0.8, alpha=0.4, zorder=4
        )
    # Connect front to back corners
    for cx, cy in [(1, 1), (1, -1), (-1, 1)]:
        ax.plot(
            [cube_cx + cx * cube_size, cube_cx + cx * cube_size + off],
            [cube_cy + cy * cube_size, cube_cy + cy * cube_size + off],
            "-",
            color=STYLE["accent1"],
            lw=0.8,
            alpha=0.4,
            zorder=4,
        )
    ax.text(
        cube_cx,
        cube_cy - cube_size - 0.3,
        "cube map",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    # Arrow from R toward cube map
    R_tip_full = P + R_end
    ax.annotate(
        "",
        xy=(cube_cx + 0.4, cube_cy - cube_size + 0.1),
        xytext=(R_tip_full[0], R_tip_full[1]),
        arrowprops={
            "arrowstyle": "->,head_width=0.15,head_length=0.08",
            "color": STYLE["accent1"],
            "lw": 1.2,
            "linestyle": "dotted",
            "connectionstyle": "arc3,rad=-0.2",
        },
        zorder=4,
    )
    ax.text(
        -1.3,
        2.8,
        "sample(R)",
        color=STYLE["accent1"],
        fontsize=9,
        style="italic",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Annotations below surface ---
    anno_y = -1.2
    annotations = [
        (STYLE["accent3"], "N = surface normal"),
        (STYLE["accent2"], "V = direction from surface toward camera"),
        (STYLE["text_dim"], "I = \u2212V = incident direction (toward surface)"),
        (STYLE["accent1"], "R = reflect(\u2212V, N) = I \u2212 2(I \u00b7 N)N"),
    ]
    for i, (color, text) in enumerate(annotations):
        ax.text(
            -3.8,
            anno_y - i * 0.42,
            text,
            color=color,
            fontsize=9,
            fontweight="bold",
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # --- HLSL code snippet ---
    code_y = anno_y - len(annotations) * 0.42 - 0.15
    code_lines = [
        "float3 V = normalize(eye_pos - world_pos);",
        "float3 R = reflect(-V, N);",
        "float3 env = env_tex.Sample(smp, R).rgb;",
        "float3 blended = lerp(diffuse, env, reflectivity);",
    ]
    for i, line in enumerate(code_lines):
        ax.text(
            -3.8,
            code_y - i * 0.38,
            line,
            color=STYLE["warn"],
            fontsize=8,
            family="monospace",
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # --- Layout ---
    ax.set_xlim(-4.2, 4.5)
    ax.set_ylim(code_y - 0.8, 4.2)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    ax.set_title(
        "Environment Reflection Mapping",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/14-environment-mapping", "reflection_mapping.png")
