"""Diagrams for gpu/10."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, draw_vector, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/10-basic-lighting — blinn_phong_vectors.png
# ---------------------------------------------------------------------------


def diagram_blinn_phong_vectors():
    """Classic lighting vector diagram: surface point with N, L, V, H, R vectors.

    This is the canonical diagram found in every graphics textbook.  A flat
    surface segment is shown from the side, with the five key vectors radiating
    from the shading point: normal (N), light direction (L), view direction (V),
    Blinn half-vector (H), and Phong reflection (R).  Angle arcs show the
    relationships that drive each shading term.
    """
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.grid(False)

    # --- Surface ---
    surf_y = 0.0
    surf_left = -3.5
    surf_right = 3.5
    ax.fill_between(
        [surf_left, surf_right],
        [surf_y, surf_y],
        [surf_y - 0.6, surf_y - 0.6],
        color=STYLE["surface"],
        alpha=0.8,
        zorder=1,
    )
    ax.plot(
        [surf_left, surf_right],
        [surf_y, surf_y],
        "-",
        color=STYLE["axis"],
        lw=2,
        zorder=2,
    )
    ax.text(
        surf_right - 0.1,
        surf_y - 0.35,
        "surface",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="right",
        style="italic",
    )

    # --- Shading point (P) ---
    P = np.array([0.0, surf_y])
    ax.plot(P[0], P[1], "o", color=STYLE["text"], markersize=7, zorder=10)
    ax.text(
        P[0] - 0.25,
        P[1] - 0.35,
        "P",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
    )

    # --- Define vector directions (all unit length, displayed as arrows) ---
    # Angles from vertical (the normal direction)
    light_angle = np.radians(40)  # L is 40° from N
    view_angle = np.radians(55)  # V is 55° on the other side

    arrow_len = 2.8

    # Normal N — straight up from surface
    N_dir = np.array([0.0, 1.0])
    N_end = N_dir * arrow_len

    # Light direction L — upper-left (40° from N toward the left)
    L_dir = np.array([-np.sin(light_angle), np.cos(light_angle)])
    L_end = L_dir * arrow_len

    # View direction V — upper-right (55° from N toward the right)
    V_dir = np.array([np.sin(view_angle), np.cos(view_angle)])
    V_end = V_dir * arrow_len

    # Reflection R — reflect L about N:  R = 2(N·L)N - L
    NdotL = np.dot(N_dir, L_dir)
    R_dir = 2.0 * NdotL * N_dir - L_dir
    R_dir = R_dir / np.linalg.norm(R_dir)
    R_end = R_dir * arrow_len

    # Half-vector H = normalize(L + V)
    H_raw = L_dir + V_dir
    H_dir = H_raw / np.linalg.norm(H_raw)
    H_end = H_dir * (arrow_len * 0.85)  # slightly shorter to distinguish

    # --- Draw vectors ---
    def draw_arrow(start, end, color, lw=2.5):
        ax.annotate(
            "",
            xy=(start[0] + end[0], start[1] + end[1]),
            xytext=(start[0], start[1]),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.12",
                "color": color,
                "lw": lw,
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

    # N — normal (green)
    draw_arrow(P, N_end, STYLE["accent3"], lw=3)
    label_vec(N_end, "N", STYLE["accent3"], (0.3, 0.15))

    # L — light direction (cyan)
    draw_arrow(P, L_end, STYLE["accent1"], lw=2.5)
    label_vec(L_end, "L", STYLE["accent1"], (-0.3, 0.15))

    # V — view direction (orange)
    draw_arrow(P, V_end, STYLE["accent2"], lw=2.5)
    label_vec(V_end, "V", STYLE["accent2"], (0.35, 0.1))

    # H — half-vector (yellow/warn)
    draw_arrow(P, H_end, STYLE["warn"], lw=2)
    label_vec(H_end, "H", STYLE["warn"], (0.3, 0.2))

    # R — reflection (purple, dashed)
    ax.annotate(
        "",
        xy=(P[0] + R_end[0], P[1] + R_end[1]),
        xytext=(P[0], P[1]),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.1",
            "color": STYLE["accent4"],
            "lw": 2,
            "linestyle": "dashed",
        },
        zorder=4,
    )
    label_vec(R_end, "R", STYLE["accent4"], (0.35, 0.0))

    # --- Angle arcs ---
    def draw_arc(from_angle, to_angle, radius, color, label, label_r=None):
        """Draw an arc from from_angle to to_angle (in radians from +Y axis,
        clockwise positive) with a label."""
        # Convert from "angle from +Y" to standard polar (from +X axis)
        # +Y axis = 90° in polar.  "from +Y clockwise" = 90° - angle in polar
        a1 = np.pi / 2 - from_angle
        a2 = np.pi / 2 - to_angle
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
        # Label at midpoint
        mid = (a1 + a2) / 2
        lr = label_r if label_r else radius + 0.2
        ax.text(
            P[0] + lr * np.cos(mid),
            P[1] + lr * np.sin(mid),
            label,
            color=color,
            fontsize=11,
            ha="center",
            va="center",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=7,
        )

    # Angle between N and L (theta_i — angle of incidence)
    # N is at 0° from Y, L is at -light_angle from Y
    draw_arc(0, -light_angle, 1.0, STYLE["accent1"], "\u03b8\u1d62", 1.3)

    # Angle between N and H (alpha — for Blinn specular)
    # H direction angle from Y
    H_angle_from_y = np.arctan2(H_dir[0], H_dir[1])
    draw_arc(0, H_angle_from_y, 0.7, STYLE["warn"], "\u03b1", 0.95)

    # Angle between N and R (to show reflection is symmetric with L)
    R_angle_from_y = np.arctan2(R_dir[0], R_dir[1])
    draw_arc(0, R_angle_from_y, 1.5, STYLE["accent4"], "\u03b8\u1d63", 1.8)

    # --- Text annotations below surface ---
    anno_y = -1.3
    annotations = [
        (
            STYLE["accent3"],
            "N = surface normal",
        ),
        (
            STYLE["accent1"],
            "L = direction toward light",
        ),
        (
            STYLE["accent2"],
            "V = direction toward viewer",
        ),
        (
            STYLE["warn"],
            "H = normalize(L + V)  \u2190 Blinn half-vector",
        ),
        (
            STYLE["accent4"],
            "R = reflect(\u2212L, N)  \u2190 Phong reflection",
        ),
    ]
    for i, (color, text) in enumerate(annotations):
        ax.text(
            -3.3,
            anno_y - i * 0.42,
            text,
            color=color,
            fontsize=9,
            fontweight="bold",
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # --- Equation summary at bottom ---
    eq_y = anno_y - len(annotations) * 0.42 - 0.3
    eqs = [
        ("Diffuse:", "max(dot(N, L), 0) \u00d7 color", STYLE["accent1"]),
        ("Specular (Blinn):", "pow(max(dot(N, H), 0), shininess)", STYLE["warn"]),
        ("Specular (Phong):", "pow(max(dot(R, V), 0), shininess)", STYLE["accent4"]),
    ]
    for i, (label, eq, color) in enumerate(eqs):
        ax.text(
            -3.3,
            eq_y - i * 0.42,
            f"{label}  {eq}",
            color=color,
            fontsize=9,
            va="top",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # --- Final layout ---
    ax.set_xlim(-3.8, 4.0)
    ax.set_ylim(eq_y - 0.8, 3.8)
    ax.set_xticks([])
    ax.set_yticks([])
    for spine in ax.spines.values():
        spine.set_visible(False)

    ax.set_title(
        "Blinn-Phong Lighting Vectors",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/10-basic-lighting", "blinn_phong_vectors.png")


# ---------------------------------------------------------------------------
# gpu/10-basic-lighting — specular_comparison.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/10-basic-lighting — specular_comparison.png
# ---------------------------------------------------------------------------


def diagram_specular_comparison():
    """Three-panel showing how shininess affects the specular highlight.

    Plots the Blinn specular function pow(max(cos(alpha), 0), shininess)
    for three different exponents, showing how higher values produce
    tighter highlights.
    """
    fig = plt.figure(figsize=(10, 4.5), facecolor=STYLE["bg"])

    shininess_values = [
        (8, "shininess = 8\n(rough)", STYLE["accent2"]),
        (64, "shininess = 64\n(plastic)", STYLE["accent1"]),
        (256, "shininess = 256\n(polished)", STYLE["accent3"]),
    ]

    alpha = np.linspace(-np.pi / 2, np.pi / 2, 300)
    cos_alpha = np.maximum(np.cos(alpha), 0.0)

    for i, (s, label, color) in enumerate(shininess_values):
        ax = fig.add_subplot(1, 3, i + 1)
        setup_axes(ax, xlim=(-90, 90), ylim=(-0.05, 1.15), grid=False, aspect="auto")

        intensity = cos_alpha**s

        # Fill under curve
        ax.fill_between(
            np.degrees(alpha),
            intensity,
            0,
            color=color,
            alpha=0.15,
            zorder=1,
        )
        # Curve
        ax.plot(np.degrees(alpha), intensity, "-", color=color, lw=2.5, zorder=2)

        # Reference lines
        ax.axhline(0, color=STYLE["grid"], lw=0.5, alpha=0.5)
        ax.axvline(0, color=STYLE["grid"], lw=0.5, alpha=0.3)

        ax.set_title(label, color=color, fontsize=11, fontweight="bold")
        ax.set_xlabel("\u03b1 (degrees from N)", color=STYLE["axis"], fontsize=9)
        if i == 0:
            ax.set_ylabel("specular intensity", color=STYLE["axis"], fontsize=9)
        ax.tick_params(colors=STYLE["axis"], labelsize=8)
        ax.set_xticks([-90, -45, 0, 45, 90])

        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)

    fig.suptitle(
        "Specular Highlight vs Shininess Exponent",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "gpu/10-basic-lighting", "specular_comparison.png")


# ---------------------------------------------------------------------------
# gpu/10-basic-lighting — normal_transformation.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/10-basic-lighting — normal_transformation.png
# ---------------------------------------------------------------------------


def diagram_normal_transformation():
    """Two-panel diagram showing why normals need special transformation.

    Left:  Object space — a circle with a tangent and a perpendicular normal.
    Right: After non-uniform scale (2x horizontal) — shows the WRONG result
           (plain model matrix) and the CORRECT result (adjugate transpose).
    The wrong normal is visibly not perpendicular to the surface; the correct
    one is.
    """
    fig = plt.figure(figsize=(12, 5.5), facecolor=STYLE["bg"])

    # --- Shared geometry ---
    theta = np.linspace(0, 2 * np.pi, 200)

    # Point on the circle at ~60 degrees (nice visual angle)
    t_param = np.radians(60)
    px, py = np.cos(t_param), np.sin(t_param)

    # Tangent at that point (derivative of (cos t, sin t) = (-sin t, cos t))
    tx, ty = -np.sin(t_param), np.cos(t_param)
    tangent_len = 0.9

    # Normal at that point (perpendicular to tangent, pointing outward)
    nx, ny = np.cos(t_param), np.sin(t_param)
    normal_len = 1.1

    # Non-uniform scale: stretch X by 2, keep Y
    sx, sy = 2.0, 1.0

    # --- Left panel: Object space ---
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-2.0, 2.5), ylim=(-1.8, 2.5), grid=False)
    ax1.set_title(
        "Object Space (circle)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    # Draw circle
    ax1.plot(
        np.cos(theta),
        np.sin(theta),
        "-",
        color=STYLE["text_dim"],
        lw=2,
        alpha=0.8,
    )
    ax1.fill(np.cos(theta), np.sin(theta), color=STYLE["surface"], alpha=0.4)

    # Draw point
    ax1.plot(px, py, "o", color=STYLE["text"], markersize=6, zorder=5)
    ax1.text(
        px + 0.15,
        py + 0.2,
        "P",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Draw tangent
    draw_vector(
        ax1,
        (px, py),
        (tx * tangent_len, ty * tangent_len),
        STYLE["accent1"],
        "T",
        label_offset=(0.2, 0.1),
    )

    # Draw normal
    draw_vector(
        ax1,
        (px, py),
        (nx * normal_len, ny * normal_len),
        STYLE["accent3"],
        "N",
        label_offset=(0.15, 0.15),
    )

    # Perpendicularity indicator (small square)
    sq_size = 0.12
    sq_t = np.array([tx, ty]) * sq_size
    sq_n = np.array([nx, ny]) * sq_size
    sq_pts = np.array(
        [
            [px + sq_t[0], py + sq_t[1]],
            [px + sq_t[0] + sq_n[0], py + sq_t[1] + sq_n[1]],
            [px + sq_n[0], py + sq_n[1]],
        ]
    )
    ax1.plot(
        [sq_pts[0, 0], sq_pts[1, 0], sq_pts[2, 0]],
        [sq_pts[0, 1], sq_pts[1, 1], sq_pts[2, 1]],
        "-",
        color=STYLE["text"],
        lw=1.0,
        alpha=0.6,
    )

    ax1.text(
        0.0,
        -1.5,
        "N \u22a5 T  (perpendicular)",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Right panel: After non-uniform scale ---
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, xlim=(-3.5, 4.5), ylim=(-2.0, 3.8), grid=False)
    ax2.set_title(
        "World Space  (scale 2\u00d71)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    # Draw ellipse (scaled circle)
    ax2.plot(
        sx * np.cos(theta),
        sy * np.sin(theta),
        "-",
        color=STYLE["text_dim"],
        lw=2,
        alpha=0.8,
    )
    ax2.fill(
        sx * np.cos(theta),
        sy * np.sin(theta),
        color=STYLE["surface"],
        alpha=0.4,
    )

    # Transformed point
    wpx, wpy = sx * px, sy * py
    ax2.plot(wpx, wpy, "o", color=STYLE["text"], markersize=6, zorder=5)
    ax2.text(
        wpx + 0.2,
        wpy - 0.25,
        "P'",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Transformed tangent (correct — tangents transform by M)
    wtx, wty = sx * tx, sy * ty
    wt_len = np.sqrt(wtx**2 + wty**2)
    wtx_n, wty_n = wtx / wt_len * tangent_len, wty / wt_len * tangent_len
    t_scale = 1.5
    draw_vector(
        ax2,
        (wpx, wpy),
        (wtx_n * t_scale, wty_n * t_scale),
        STYLE["accent1"],
        label=None,
    )
    # Label at arrow tip
    t_end = (wpx + wtx_n * t_scale, wpy + wty_n * t_scale)
    ax2.text(
        t_end[0] - 0.35,
        t_end[1] + 0.2,
        "M\u00b7T",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # WRONG normal: transform by M (same as tangent) — not perpendicular
    wnx_bad, wny_bad = sx * nx, sy * ny
    wn_bad_len = np.sqrt(wnx_bad**2 + wny_bad**2)
    wnx_bad_n = wnx_bad / wn_bad_len * normal_len
    wny_bad_n = wny_bad / wn_bad_len * normal_len
    n_scale = 1.4
    draw_vector(
        ax2,
        (wpx, wpy),
        (wnx_bad_n * n_scale, wny_bad_n * n_scale),
        STYLE["accent2"],
        label=None,
        lw=2.0,
    )
    # Label at arrow tip — offset to the right
    bad_end = (wpx + wnx_bad_n * n_scale, wpy + wny_bad_n * n_scale)
    ax2.text(
        bad_end[0] + 0.55,
        bad_end[1] + 0.1,
        "M\u00b7N  \u2717",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # CORRECT normal: adjugate transpose = (sy, sx) for diagonal 2D scale
    # For scale(sx, sy), adj^T = diag(sy, sx) (the cofactor matrix)
    cnx, cny = sy * nx, sx * ny
    cn_len = np.sqrt(cnx**2 + cny**2)
    cnx_n = cnx / cn_len * normal_len
    cny_n = cny / cn_len * normal_len
    draw_vector(
        ax2,
        (wpx, wpy),
        (cnx_n * n_scale, cny_n * n_scale),
        STYLE["accent3"],
        label=None,
    )
    # Label at arrow tip — offset to the left
    good_end = (wpx + cnx_n * n_scale, wpy + cny_n * n_scale)
    ax2.text(
        good_end[0] - 0.55,
        good_end[1] + 0.1,
        "\u2713  adj\u1d40\u00b7N",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="right",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Perpendicularity marker between correct normal and tangent
    cdir = np.array([cnx_n, cny_n])
    cdir = cdir / np.linalg.norm(cdir)
    tdir = np.array([wtx_n, wty_n])
    tdir = tdir / np.linalg.norm(tdir)
    sq_size2 = 0.15
    sq_t2 = tdir * sq_size2
    sq_n2 = cdir * sq_size2
    sq_pts2 = np.array(
        [
            [wpx + sq_t2[0], wpy + sq_t2[1]],
            [wpx + sq_t2[0] + sq_n2[0], wpy + sq_t2[1] + sq_n2[1]],
            [wpx + sq_n2[0], wpy + sq_n2[1]],
        ]
    )
    ax2.plot(
        [sq_pts2[0, 0], sq_pts2[1, 0], sq_pts2[2, 0]],
        [sq_pts2[0, 1], sq_pts2[1, 1], sq_pts2[2, 1]],
        "-",
        color=STYLE["accent3"],
        lw=1.0,
        alpha=0.8,
    )

    # Legend — two separate colored text elements, no overlapping markers
    ax2.text(
        -1.4,
        -1.7,
        "\u2717  M\u00b7N is wrong",
        color=STYLE["accent2"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.text(
        2.4,
        -1.7,
        "\u2713  adj(M)\u1d40\u00b7N is correct",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "gpu/10-basic-lighting", "normal_transformation.png")


# ---------------------------------------------------------------------------
# gpu/11-compute-shaders — fullscreen_triangle.png
# ---------------------------------------------------------------------------
