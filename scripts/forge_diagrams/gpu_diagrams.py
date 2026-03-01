"""Diagram functions for GPU lessons (lessons/gpu/)."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle, Polygon, Rectangle

from ._common import FORGE_CMAP, STYLE, draw_vector, save, setup_axes

# ACES filmic tone mapping coefficients (Narkowicz approximation).
# Shared across diagram functions to keep the curve definition in one place.
ACES_COEFFS = (2.51, 0.03, 2.43, 0.59, 0.14)

# ---------------------------------------------------------------------------
# gpu/03-uniforms-and-motion — unit_circle.png
# ---------------------------------------------------------------------------


def diagram_unit_circle():
    """Unit circle showing cos(t) and sin(t) as coordinates of a point.

    Shows the unit circle centered at the origin with a point at angle t,
    projections onto the x and y axes demonstrating cos and sin, and a
    dashed radius line from origin to the point.
    """
    fig = plt.figure(figsize=(7, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.65, 1.65), ylim=(-1.65, 1.65))

    # --- Unit circle ---
    theta = np.linspace(0, 2 * np.pi, 200)
    ax.plot(np.cos(theta), np.sin(theta), "-", color=STYLE["grid"], lw=2, zorder=2)

    # --- Axes through origin ---
    ax.axhline(0, color=STYLE["axis"], lw=0.8, zorder=1)
    ax.axvline(0, color=STYLE["axis"], lw=0.8, zorder=1)

    # --- Axis labels ---
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    ax.text(
        1.5,
        -0.15,
        "x",
        color=STYLE["axis"],
        fontsize=11,
        ha="center",
        va="top",
        path_effects=stroke,
    )
    ax.text(
        -0.15,
        1.5,
        "y",
        color=STYLE["axis"],
        fontsize=11,
        ha="right",
        va="center",
        path_effects=stroke,
    )

    # --- Cardinal labels on the circle ---
    cardinal_style = dict(
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(1.0, -0.18, "(1, 0)", **cardinal_style)
    ax.text(-1.0, -0.18, "(\u22121, 0)", **cardinal_style)
    ax.text(0.22, 1.12, "(0, 1)", **cardinal_style)
    ax.text(0.22, -1.12, "(0, \u22121)", **cardinal_style)

    # --- Point on circle at angle t ---
    t = np.radians(40)  # 40° for a clear visual
    px, py = np.cos(t), np.sin(t)

    # Radius line from origin to point
    ax.plot([0, px], [0, py], "-", color=STYLE["accent1"], lw=2.5, zorder=4)
    ax.plot(px, py, "o", color=STYLE["accent1"], markersize=10, zorder=6)

    # Point label
    ax.text(
        px + 0.12,
        py + 0.12,
        "(cos t, sin t)",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        va="bottom",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=7,
    )

    # --- Projection lines (dashed) ---
    # Vertical drop from point to x-axis (shows cos t)
    ax.plot(
        [px, px],
        [0, py],
        "--",
        color=STYLE["accent2"],
        lw=1.5,
        alpha=0.8,
        zorder=3,
    )
    # Horizontal line from point to y-axis (shows sin t)
    ax.plot(
        [0, px],
        [py, py],
        "--",
        color=STYLE["accent3"],
        lw=1.5,
        alpha=0.8,
        zorder=3,
    )

    # --- cos(t) label on x-axis ---
    ax.annotate(
        "",
        xy=(px, -0.08),
        xytext=(0, -0.08),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["accent2"],
            "lw": 2,
        },
        zorder=5,
    )
    ax.text(
        px / 2,
        -0.22,
        "cos t",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=7,
    )

    # --- sin(t) label on y-axis ---
    ax.annotate(
        "",
        xy=(-0.08, py),
        xytext=(-0.08, 0),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["accent3"],
            "lw": 2,
        },
        zorder=5,
    )
    ax.text(
        -0.25,
        py / 2,
        "sin t",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="right",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=7,
    )

    # --- Angle arc from +x axis to the radius ---
    arc_r = 0.3
    arc_t = np.linspace(0, t, 40)
    ax.plot(
        arc_r * np.cos(arc_t),
        arc_r * np.sin(arc_t),
        "-",
        color=STYLE["warn"],
        lw=2,
        zorder=5,
    )
    # Angle label
    arc_mid = t / 2
    ax.text(
        0.45 * np.cos(arc_mid),
        0.45 * np.sin(arc_mid),
        "t",
        color=STYLE["warn"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=7,
    )

    # --- Hide ticks, keep minimal appearance ---
    ax.set_xticks([-1, 0, 1])
    ax.set_yticks([-1, 0, 1])
    ax.tick_params(labelsize=9, colors=STYLE["axis"])

    ax.set_title(
        "The Unit Circle: cos and sin",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/03-uniforms-and-motion", "unit_circle.png")


# ---------------------------------------------------------------------------
# gpu/03-uniforms-and-motion — aspect_ratio.png
# ---------------------------------------------------------------------------


def diagram_aspect_ratio():
    """Side-by-side comparison of NDC square vs. stretched window.

    Left panel shows a circle in the square NDC space (-1 to +1 on both axes).
    Right panel shows how the same NDC circle appears stretched into an ellipse
    on a 16:9 window.  Demonstrates why aspect ratio correction is needed.
    """
    fig = plt.figure(figsize=(11, 5), facecolor=STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    theta = np.linspace(0, 2 * np.pi, 100)
    r = 0.6
    aspect = 16.0 / 9.0  # 1280 / 720

    # --- Left panel: NDC space (square, equal aspect) ---
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-1.55, 1.55), ylim=(-1.55, 1.55))

    # NDC boundary
    ndc_box = Rectangle(
        (-1, -1),
        2,
        2,
        linewidth=1.5,
        edgecolor=STYLE["axis"],
        facecolor=STYLE["surface"],
        alpha=0.3,
        zorder=1,
    )
    ax1.add_patch(ndc_box)

    # Circle
    ax1.fill(
        r * np.cos(theta),
        r * np.sin(theta),
        color=STYLE["accent1"],
        alpha=0.25,
        zorder=2,
    )
    ax1.plot(
        r * np.cos(theta),
        r * np.sin(theta),
        "-",
        color=STYLE["accent1"],
        lw=2.5,
        zorder=4,
    )
    ax1.text(
        0,
        0,
        "circle",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Corner labels
    ax1.text(
        1.0,
        -1.2,
        "+1",
        color=STYLE["axis"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )
    ax1.text(
        -1.0,
        -1.2,
        "\u22121",
        color=STYLE["axis"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )
    ax1.text(
        -1.25,
        1.0,
        "+1",
        color=STYLE["axis"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )
    ax1.text(
        -1.25,
        -1.0,
        "\u22121",
        color=STYLE["axis"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    ax1.set_xticks([])
    ax1.set_yticks([])
    ax1.set_title(
        "NDC Space (square \u22121 to +1)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=14,
    )

    # --- Arrow between panels ---
    fig.text(
        0.50,
        0.5,
        "\u2192",
        color=STYLE["warn"],
        fontsize=28,
        ha="center",
        va="center",
        fontweight="bold",
    )
    fig.text(
        0.50,
        0.40,
        "maps to\nwindow",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
    )

    # --- Right panel: Window (16:9) ---
    # Use a non-equal aspect so the axes are wider than tall, showing stretch.
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, xlim=(-aspect - 0.3, aspect + 0.3), ylim=(-1.55, 1.55), aspect=None)

    # Window boundary (wider rectangle)
    win_box = Rectangle(
        (-aspect, -1),
        2 * aspect,
        2,
        linewidth=1.5,
        edgecolor=STYLE["axis"],
        facecolor=STYLE["surface"],
        alpha=0.3,
        zorder=1,
    )
    ax2.add_patch(win_box)

    # Same NDC circle mapped to the window — stretched horizontally
    ax2.fill(
        r * np.cos(theta) * aspect,
        r * np.sin(theta),
        color=STYLE["accent2"],
        alpha=0.25,
        zorder=2,
    )
    ax2.plot(
        r * np.cos(theta) * aspect,
        r * np.sin(theta),
        "-",
        color=STYLE["accent2"],
        lw=2.5,
        zorder=4,
    )
    ax2.text(
        0,
        0,
        "ellipse",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Dimension labels
    ax2.text(
        0,
        -1.3,
        "1280 px",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )
    ax2.text(
        aspect + 0.15,
        0,
        "720\npx",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    ax2.set_xticks([])
    ax2.set_yticks([])
    ax2.set_title(
        "Window (1280\u00d7720, stretched)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/03-uniforms-and-motion", "aspect_ratio.png")


# ---------------------------------------------------------------------------
# gpu/04-textures-and-samplers — uv_mapping.png
# ---------------------------------------------------------------------------


def diagram_uv_mapping():
    """Position space to UV space mapping."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    # Left: Position space (quad)
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-1, 1), ylim=(-1, 1), grid=False)

    quad_x = [-0.6, 0.6, 0.6, -0.6, -0.6]
    quad_y = [-0.6, -0.6, 0.6, 0.6, -0.6]
    ax1.fill(quad_x, quad_y, color=STYLE["surface"], alpha=0.5)
    ax1.plot(quad_x, quad_y, "-", color=STYLE["accent1"], lw=2)

    verts = [
        (-0.6, -0.6, "v0\n(-0.6, -0.6)"),
        (0.6, -0.6, "v1\n(0.6, -0.6)"),
        (0.6, 0.6, "v2\n(0.6, 0.6)"),
        (-0.6, 0.6, "v3\n(-0.6, 0.6)"),
    ]
    vert_colors = [
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent3"],
        STYLE["accent4"],
    ]
    for (vx, vy, label), vc in zip(verts, vert_colors):
        ax1.plot(vx, vy, "o", color=vc, markersize=8, zorder=5)
        ox = -0.32 if vx < 0 else 0.08
        oy = -0.18 if vy < 0 else 0.06
        ax1.text(
            vx + ox,
            vy + oy,
            label,
            color=vc,
            fontsize=8,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Diagonal line (two triangles)
    ax1.plot([-0.6, 0.6], [-0.6, 0.6], "--", color=STYLE["text_dim"], lw=1)

    ax1.set_title("Position Space", color=STYLE["text"], fontsize=12, fontweight="bold")
    ax1.axhline(0, color=STYLE["grid"], lw=0.5, alpha=0.3)
    ax1.axvline(0, color=STYLE["grid"], lw=0.5, alpha=0.3)

    # Arrow between subplots
    fig.text(
        0.50,
        0.5,
        "\u2192",
        color=STYLE["warn"],
        fontsize=28,
        ha="center",
        va="center",
        fontweight="bold",
    )
    fig.text(
        0.50,
        0.42,
        "UV mapping",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
    )

    # Right: UV space (texture)
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, xlim=(-0.15, 1.15), ylim=(-0.15, 1.15), grid=False)

    # Checkerboard texture preview
    for ci in range(4):
        for cj in range(4):
            shade = STYLE["surface"] if (ci + cj) % 2 == 0 else STYLE["grid"]
            r = Rectangle(
                (ci / 4, cj / 4),
                0.25,
                0.25,
                facecolor=shade,
                edgecolor=STYLE["grid"],
                linewidth=0.5,
                zorder=0,
            )
            ax2.add_patch(r)

    # UV quad outline
    uv_x = [0, 1, 1, 0, 0]
    uv_y = [1, 1, 0, 0, 1]
    ax2.plot(uv_x, uv_y, "-", color=STYLE["accent1"], lw=2, alpha=0.7)

    uv_verts = [
        (0, 1, "v0 (0, 1)", STYLE["accent1"]),
        (1, 1, "v1 (1, 1)", STYLE["accent2"]),
        (1, 0, "v2 (1, 0)", STYLE["accent3"]),
        (0, 0, "v3 (0, 0)", STYLE["accent4"]),
    ]
    for ux, uy, label, uc in uv_verts:
        ax2.plot(ux, uy, "o", color=uc, markersize=8, zorder=5)
        ox = -0.12 if ux == 0 else 0.04
        oy = 0.05 if uy == 0 else -0.1
        ax2.text(
            ux + ox,
            uy + oy,
            label,
            color=uc,
            fontsize=8,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax2.set_title(
        "UV / Texture Space", color=STYLE["text"], fontsize=12, fontweight="bold"
    )
    ax2.set_xlabel("U \u2192", color=STYLE["axis"], fontsize=10)
    ax2.set_ylabel("V \u2193 (shown flipped)", color=STYLE["axis"], fontsize=10)

    fig.suptitle(
        "UV Mapping: Position \u2192 Texture Coordinates",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "gpu/04-textures-and-samplers", "uv_mapping.png")


# ---------------------------------------------------------------------------
# gpu/04-textures-and-samplers — filtering_comparison.png
# ---------------------------------------------------------------------------


def diagram_filtering_comparison():
    """NEAREST vs LINEAR filtering comparison."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    # Create a small 4x4 "texture" with distinct values
    texture = np.array(
        [
            [0.2, 0.4, 0.6, 0.8],
            [0.3, 0.5, 0.7, 0.9],
            [0.4, 0.6, 0.8, 1.0],
            [0.1, 0.3, 0.5, 0.7],
        ]
    )

    # Left: NEAREST (blocky)
    ax1 = fig.add_subplot(121)
    ax1.set_facecolor(STYLE["bg"])
    nearest_up = np.repeat(np.repeat(texture, 8, axis=0), 8, axis=1)
    ax1.imshow(
        nearest_up,
        cmap=FORGE_CMAP,
        interpolation="nearest",
        extent=(0, 4, 0, 4),
        origin="lower",
    )
    for i in range(5):
        ax1.axhline(i, color=STYLE["grid"], lw=0.5, alpha=0.6)
        ax1.axvline(i, color=STYLE["grid"], lw=0.5, alpha=0.6)

    # Sample point + selected texel highlight
    ax1.plot(1.3, 2.7, "x", color=STYLE["warn"], markersize=12, mew=2.5, zorder=5)
    ax1.text(1.5, 2.85, "sample", color=STYLE["warn"], fontsize=9, fontweight="bold")
    selected = Rectangle(
        (1, 2),
        1,
        1,
        fill=False,
        edgecolor=STYLE["warn"],
        linewidth=2.5,
        linestyle="--",
        zorder=4,
    )
    ax1.add_patch(selected)

    ax1.set_title(
        "NEAREST \u2014 picks closest texel",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
    )
    ax1.tick_params(colors=STYLE["axis"], labelsize=8)

    # Right: LINEAR (smooth)
    ax2 = fig.add_subplot(122)
    ax2.set_facecolor(STYLE["bg"])
    ax2.imshow(
        texture,
        cmap=FORGE_CMAP,
        interpolation="bilinear",
        extent=(0, 4, 0, 4),
        origin="lower",
    )
    for i in range(5):
        ax2.axhline(i, color=STYLE["grid"], lw=0.5, alpha=0.6)
        ax2.axvline(i, color=STYLE["grid"], lw=0.5, alpha=0.6)

    # Sample point + contributing texels
    ax2.plot(1.3, 2.7, "x", color=STYLE["warn"], markersize=12, mew=2.5, zorder=5)
    ax2.text(1.5, 2.85, "sample", color=STYLE["warn"], fontsize=9, fontweight="bold")

    for tx, ty in [(1, 2), (2, 2), (1, 3), (2, 3)]:
        ax2.plot(
            tx + 0.5,
            ty + 0.5,
            "o",
            color=STYLE["accent3"],
            markersize=6,
            zorder=5,
            alpha=0.8,
        )
        ax2.plot(
            [1.3, tx + 0.5],
            [2.7, ty + 0.5],
            "--",
            color=STYLE["accent3"],
            lw=0.8,
            alpha=0.5,
        )

    ax2.set_title(
        "LINEAR \u2014 blends 4 nearest texels",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
    )
    ax2.tick_params(colors=STYLE["axis"], labelsize=8)

    fig.suptitle(
        "Texture Filtering: NEAREST vs LINEAR",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "gpu/04-textures-and-samplers", "filtering_comparison.png")


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


def diagram_fullscreen_triangle():
    """Two-panel comparison: fullscreen quad vs fullscreen triangle.

    Left:  A quad made of two triangles with a diagonal seam and redundant
           fragment processing along the diagonal.
    Right: A single oversized triangle that covers the viewport, with the
           clipped region shown.
    """
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(11, 5), facecolor=STYLE["bg"])

    for ax in (ax1, ax2):
        setup_axes(ax, xlim=(-2.0, 4.0), ylim=(-2.0, 4.0), grid=False)
        ax.set_xticks([-1, 0, 1, 2, 3])
        ax.set_yticks([-1, 0, 1, 2, 3])
        ax.tick_params(colors=STYLE["axis"], labelsize=8)

    # --- Left panel: fullscreen quad (two triangles) ---
    ax1.set_title(
        "Fullscreen Quad (2 triangles)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # The viewport rectangle [-1,1] x [-1,1]
    viewport = Rectangle(
        (-1, -1),
        2,
        2,
        linewidth=2,
        edgecolor=STYLE["text"],
        facecolor="none",
        linestyle="--",
        zorder=5,
    )
    ax1.add_patch(viewport)

    # Triangle 1 (lower-left)
    tri1 = Polygon(
        [(-1, -1), (1, -1), (-1, 1)],
        closed=True,
        facecolor=STYLE["accent1"],
        alpha=0.3,
        edgecolor=STYLE["accent1"],
        linewidth=2,
        zorder=3,
    )
    ax1.add_patch(tri1)

    # Triangle 2 (upper-right)
    tri2 = Polygon(
        [(1, 1), (-1, 1), (1, -1)],
        closed=True,
        facecolor=STYLE["accent2"],
        alpha=0.3,
        edgecolor=STYLE["accent2"],
        linewidth=2,
        zorder=3,
    )
    ax1.add_patch(tri2)

    # Diagonal seam
    ax1.plot(
        [-1, 1],
        [1, -1],
        color=STYLE["warn"],
        linewidth=2.5,
        linestyle="-",
        zorder=6,
    )
    ax1.text(
        0.15,
        0.15,
        "diagonal\nseam",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        rotation=-45,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=7,
    )

    # Vertex labels — position each individually to avoid overlaps
    for (x, y), label, (ox, oy), va in [
        ((-1, -1), "(-1,-1)", (0, -0.3), "top"),
        ((1, -1), "(1,-1)", (0, -0.3), "top"),
        ((-1, 1), "(-1, 1)", (-0.35, 0), "center"),
        ((1, 1), "(1, 1)", (0.35, 0), "center"),
    ]:
        ax1.plot(x, y, "o", color=STYLE["text"], markersize=6, zorder=8)
        ax1.text(
            x + ox,
            y + oy,
            label,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va=va,
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax1.text(
        -0.6,
        -0.55,
        "Tri 1",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax1.text(
        0.55,
        0.65,
        "Tri 2",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Annotations
    ax1.text(
        0.0,
        -1.7,
        "4 vertices, 2 triangles, 6 indices\nfragments on diagonal processed twice",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
    )

    # --- Right panel: fullscreen triangle (single oversized) ---
    ax2.set_title(
        "Fullscreen Triangle (1 triangle)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # The viewport rectangle
    viewport2 = Rectangle(
        (-1, -1),
        2,
        2,
        linewidth=2,
        edgecolor=STYLE["text"],
        facecolor="none",
        linestyle="--",
        zorder=5,
    )
    ax2.add_patch(viewport2)

    # The oversized triangle: (-1,-1), (3,-1), (-1,3)
    tri_full = Polygon(
        [(-1, -1), (3, -1), (-1, 3)],
        closed=True,
        facecolor=STYLE["accent3"],
        alpha=0.15,
        edgecolor=STYLE["accent3"],
        linewidth=2,
        linestyle=":",
        zorder=2,
    )
    ax2.add_patch(tri_full)

    # The visible (clipped) portion
    tri_visible = Polygon(
        [(-1, -1), (1, -1), (1, 1), (-1, 1)],
        closed=True,
        facecolor=STYLE["accent3"],
        alpha=0.35,
        edgecolor=STYLE["accent3"],
        linewidth=2,
        zorder=3,
    )
    ax2.add_patch(tri_visible)

    # Vertex labels for the triangle — positioned to avoid edge clipping
    for (x, y), label, (ox, oy), ha in [
        ((-1, -1), "(-1,-1)\nid=0", (-0.1, -0.35), "center"),
        ((3, -1), "(3,-1)\nid=1", (-0.35, 0.35), "center"),
        ((-1, 3), "(-1, 3)\nid=2", (0.35, 0.0), "left"),
    ]:
        ax2.plot(x, y, "o", color=STYLE["accent3"], markersize=8, zorder=8)
        ax2.text(
            x + ox,
            y + oy,
            label,
            color=STYLE["accent3"],
            fontsize=9,
            fontweight="bold",
            ha=ha,
            va="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    # Label the clipped region
    ax2.text(
        0.0,
        0.0,
        "visible\nregion",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=6,
    )

    # Label the clipped-away region
    ax2.text(
        1.6,
        0.5,
        "clipped\naway",
        color=STYLE["text_dim"],
        fontsize=9,
        style="italic",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # "Viewport" label
    ax2.text(
        1.0,
        1.15,
        "viewport",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax2.text(
        0.0,
        -1.7,
        "3 vertices, 1 triangle, 0 indices\nno seam, no redundant fragments",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
    )

    fig.tight_layout()
    save(fig, "gpu/11-compute-shaders", "fullscreen_triangle.png")


# ---------------------------------------------------------------------------
# GPU Lesson 12 — Shader Grid: undersampling / Nyquist aliasing
# ---------------------------------------------------------------------------


def diagram_undersampling():
    """Three-panel diagram showing adequate sampling vs undersampling.

    Demonstrates the Nyquist-Shannon sampling theorem visually:
    - Left:   High-frequency signal sampled above the Nyquist rate (correct)
    - Centre: Same signal sampled BELOW the Nyquist rate (aliased)
    - Right:  Grid analogy — adequate vs inadequate pixel density

    This connects the mathematical concept (sampling theorem) to the practical
    problem (moire on a procedural grid) that fwidth()/smoothstep() solves.
    """
    fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), facecolor=STYLE["bg"])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # -- Shared signal: a sine wave representing a grid-like periodic pattern --
    t_fine = np.linspace(0, 2, 1000)
    freq = 5.0  # 5 Hz signal
    signal = np.sin(2 * np.pi * freq * t_fine)

    # ── Left panel: adequate sampling (above Nyquist rate) ────────────────
    ax = axes[0]
    setup_axes(ax, xlim=(-0.05, 2.05), ylim=(-1.6, 1.6), grid=False, aspect="auto")

    ax.set_title(
        "Adequate Sampling (fs > 2f)",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=10,
    )

    # Continuous signal (faint)
    ax.plot(t_fine, signal, color=STYLE["accent1"], alpha=0.35, linewidth=1.5)

    # Sample points: 12 samples/sec >> 2*5 = 10 (Nyquist rate)
    fs_good = 25
    t_good = np.linspace(0, 2, fs_good * 2 + 1)
    s_good = np.sin(2 * np.pi * freq * t_good)
    ax.plot(
        t_good,
        s_good,
        "o",
        color=STYLE["accent1"],
        markersize=5,
        zorder=5,
    )

    # Reconstructed signal from samples
    ax.plot(t_good, s_good, color=STYLE["accent1"], linewidth=2.0, zorder=4)

    ax.set_xlabel(
        "Position (world space)",
        color=STYLE["axis"],
        fontsize=9,
    )
    ax.set_ylabel(
        "Grid pattern",
        color=STYLE["axis"],
        fontsize=9,
    )
    ax.text(
        1.0,
        -1.35,
        "Samples capture the signal faithfully",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # ── Centre panel: undersampling (below Nyquist rate → alias) ──────────
    ax = axes[1]
    setup_axes(ax, xlim=(-0.05, 2.05), ylim=(-1.6, 1.6), grid=False, aspect="auto")

    ax.set_title(
        "Undersampling (fs < 2f) → Aliasing",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        pad=10,
    )

    # Original signal (faint)
    ax.plot(t_fine, signal, color=STYLE["accent1"], alpha=0.25, linewidth=1.5)

    # Too few samples: 3 samples/sec < 2*5 = 10 (below Nyquist)
    fs_bad = 3
    t_bad = np.linspace(0, 2, fs_bad * 2 + 1)
    s_bad = np.sin(2 * np.pi * freq * t_bad)
    ax.plot(
        t_bad,
        s_bad,
        "o",
        color=STYLE["accent2"],
        markersize=7,
        zorder=5,
    )

    # The alias: connecting the sparse samples shows a WRONG low-frequency wave.
    # np.interp does piecewise linear — sufficient to show the alias clearly.
    t_interp = np.linspace(0, 2, 500)
    s_interp = np.interp(t_interp, t_bad, s_bad)
    ax.plot(
        t_interp,
        s_interp,
        color=STYLE["accent2"],
        linewidth=2.5,
        linestyle="-",
        zorder=4,
        label="Perceived (alias)",
    )

    ax.set_xlabel(
        "Position (world space)",
        color=STYLE["axis"],
        fontsize=9,
    )
    ax.text(
        1.0,
        -1.35,
        "Sparse samples reconstruct a false signal (moire)",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # ── Right panel: grid analogy (pixels vs grid frequency) ──────────────
    ax = axes[2]
    setup_axes(ax, xlim=(-0.5, 10.5), ylim=(-0.5, 10.5), grid=False, aspect="equal")

    ax.set_title(
        "Grid Cells vs Pixel Density",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=10,
    )

    # Draw grid lines (the signal being sampled)
    for i in range(11):
        ax.axhline(y=i, color=STYLE["accent1"], linewidth=0.8, alpha=0.5, zorder=1)
        ax.axvline(x=i, color=STYLE["accent1"], linewidth=0.8, alpha=0.5, zorder=1)

    # Top half: dense pixel grid (well-sampled) — small dots
    for px in np.arange(0.25, 10.0, 0.5):
        for py in np.arange(5.5, 10.0, 0.5):
            ax.plot(
                px,
                py,
                "s",
                color=STYLE["accent3"],
                markersize=2,
                alpha=0.6,
                zorder=3,
            )

    # Bottom half: sparse pixel grid (undersampled) — large dots
    for px in np.arange(0.75, 10.0, 2.5):
        for py in np.arange(0.75, 5.0, 2.5):
            ax.plot(
                px,
                py,
                "s",
                color=STYLE["accent2"],
                markersize=6,
                alpha=0.8,
                zorder=3,
            )

    # Dividing line
    ax.axhline(y=5.0, color=STYLE["text"], linewidth=1.5, linestyle="--", zorder=4)

    ax.text(
        5.0,
        7.75,
        "Close: many pixels per cell\n(above Nyquist → crisp lines)",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        5.0,
        2.25,
        "Far: few pixels per cell\n(below Nyquist → moire)",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    ax.set_xlabel("World X", color=STYLE["axis"], fontsize=9)
    ax.set_ylabel("World Z", color=STYLE["axis"], fontsize=9)

    fig.tight_layout(pad=1.5)
    save(fig, "gpu/12-shader-grid", "undersampling.png")


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


def diagram_cascaded_shadow_maps():
    """Cascaded shadow maps: how the view frustum is split into cascades.

    Shows a side view of the camera frustum divided into 3 cascades at
    logarithmic-linear split distances.  Each cascade has its own
    orthographic projection from the light, covering a progressively
    larger area with the same shadow map resolution.
    """
    fig = plt.figure(figsize=(12, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.5, 14), ylim=(-4, 4.5), grid=False)

    # Camera position
    cam_x, cam_y = 0.0, 0.0
    ax.plot(cam_x, cam_y, "o", color=STYLE["text"], markersize=8, zorder=10)
    ax.text(
        cam_x - 0.5,
        cam_y - 0.6,
        "Camera",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
    )

    # Frustum parameters
    near = 0.5
    far = 12.0
    half_fov = 0.35  # radians, for visual purposes

    # Cascade splits (logarithmic-linear blend, lambda=0.5)
    splits = [near]
    for i in range(1, 4):
        p = i / 3.0
        log_s = near * (far / near) ** p
        lin_s = near + (far - near) * p
        splits.append(0.5 * log_s + 0.5 * lin_s)

    cascade_colors = [STYLE["accent1"], STYLE["accent3"], STYLE["accent2"]]
    cascade_labels = ["Cascade 0\n(nearest)", "Cascade 1\n(mid)", "Cascade 2\n(far)"]
    cascade_alphas = [0.35, 0.25, 0.18]

    # Draw each cascade as a trapezoid
    for ci in range(3):
        d_near = splits[ci]
        d_far = splits[ci + 1]
        h_near = d_near * np.tan(half_fov)
        h_far = d_far * np.tan(half_fov)

        verts = [
            (d_near, -h_near),
            (d_far, -h_far),
            (d_far, h_far),
            (d_near, h_near),
        ]
        poly = Polygon(
            verts,
            closed=True,
            facecolor=cascade_colors[ci],
            edgecolor=cascade_colors[ci],
            alpha=cascade_alphas[ci],
            linewidth=2,
            zorder=2,
        )
        ax.add_patch(poly)

        # Cascade label at center
        cx = (d_near + d_far) / 2.0
        ax.text(
            cx,
            0.0,
            cascade_labels[ci],
            color=cascade_colors[ci],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
        )

        # Split distance annotation at bottom
        if ci < 2:
            ax.axvline(
                x=d_far,
                color=STYLE["text_dim"],
                linestyle="--",
                linewidth=1,
                alpha=0.6,
                zorder=1,
            )
            ax.text(
                d_far,
                -h_far - 0.5,
                f"split {ci}",
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
            )

    # Draw frustum outline
    h_near_full = near * np.tan(half_fov)
    h_far_full = far * np.tan(half_fov)
    ax.plot([cam_x, far], [0, h_far_full], "-", color=STYLE["axis"], linewidth=1.5)
    ax.plot([cam_x, far], [0, -h_far_full], "-", color=STYLE["axis"], linewidth=1.5)
    ax.plot(
        [far, far], [-h_far_full, h_far_full], "-", color=STYLE["axis"], linewidth=1.5
    )

    # Light direction arrow
    ax.annotate(
        "",
        xy=(9.0, 3.0),
        xytext=(12.0, 4.2),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["warn"],
            "lw": 2.5,
        },
    )
    ax.text(
        12.2,
        4.2,
        "Light\ndirection",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        va="center",
    )

    # Near/far labels
    ax.text(
        near,
        h_near_full + 0.4,
        "near",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )
    ax.text(
        far,
        h_far_full + 0.4,
        "far",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )

    # Resolution note
    ax.text(
        6.0,
        -3.5,
        "Each cascade uses the same shadow map resolution (2048\u00b2)\n"
        "Near cascades cover less area \u2192 higher effective resolution",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        style="italic",
    )

    ax.set_title(
        "Cascaded Shadow Maps \u2014 Frustum Partitioning",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/15-cascaded-shadow-maps", "cascaded_shadow_maps.png")


def diagram_cascade_ortho_projections():
    """Side view of cascade orthographic projections aligned to the light.

    Shows the camera frustum from the side (camera looks right, light shines
    from the upper-left at an angle — matching the lesson's LIGHT_DIR).  Each
    cascade slice gets its own orthographic projection, drawn as a rotated
    rectangle aligned to the light direction (because the AABB is computed in
    light view space, not camera space).  Near cascades produce small, tight
    boxes; far cascades produce larger ones.
    """
    fig = plt.figure(figsize=(12, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-2, 15), ylim=(-6.5, 8.5), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])

    # Camera position
    cam_x, cam_y = 0.0, 0.0
    ax.plot(cam_x, cam_y, "o", color=STYLE["text"], markersize=8, zorder=10)
    ax.text(
        cam_x - 0.5,
        cam_y - 0.6,
        "Camera",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
    )

    # Frustum parameters (visual scale)
    near = 0.5
    far = 12.0
    half_fov = 0.35  # radians

    # Cascade splits — same logarithmic-linear blend as main.c
    splits = [near]
    for i in range(1, 4):
        p = i / 3.0
        log_s = near * (far / near) ** p
        lin_s = near + (far - near) * p
        splits.append(0.5 * log_s + 0.5 * lin_s)

    cascade_colors = [STYLE["accent1"], STYLE["accent3"], STYLE["accent2"]]
    cascade_labels = ["Cascade 0\n(nearest)", "Cascade 1\n(mid)", "Cascade 2\n(far)"]

    # Light direction in the 2D side-view plane.
    # main.c uses LIGHT_DIR (1, 1, 0.5) — this points TOWARD the light.
    # In our side view (x = camera forward, y = up), the light comes from
    # the upper-right and shines toward the lower-left.  We use the direction
    # the light travels (negated): roughly (-1, -1) normalized, but we tilt
    # it to ~60° from horizontal so the angle is visually clear.
    light_angle = np.radians(240)  # 240° from +X = lower-left direction
    light_dx = np.cos(light_angle)  # light travel direction x
    light_dy = np.sin(light_angle)  # light travel direction y

    # Light's local axes (for computing oriented bounding boxes)
    # light_forward = direction light travels (into the scene)
    # light_right = perpendicular (the "width" axis of the ortho box)
    lf = np.array([light_dx, light_dy])
    lr = np.array([-light_dy, light_dx])  # 90° CCW rotation

    # Draw the frustum outline (faint)
    h_near_full = near * np.tan(half_fov)
    h_far_full = far * np.tan(half_fov)
    ax.plot(
        [cam_x, far],
        [0, h_far_full],
        "-",
        color=STYLE["axis"],
        linewidth=1.2,
        alpha=0.5,
    )
    ax.plot(
        [cam_x, far],
        [0, -h_far_full],
        "-",
        color=STYLE["axis"],
        linewidth=1.2,
        alpha=0.5,
    )
    ax.plot(
        [far, far],
        [-h_far_full, h_far_full],
        "-",
        color=STYLE["axis"],
        linewidth=1.2,
        alpha=0.5,
    )

    # Draw each cascade slice and its light-aligned orthographic box
    for ci in range(3):
        d_near = splits[ci]
        d_far = splits[ci + 1]
        h_near_c = d_near * np.tan(half_fov)
        h_far_c = d_far * np.tan(half_fov)

        # The 4 corners of this cascade slice (trapezoid in side view)
        corners = np.array(
            [
                [d_near, h_near_c],  # near-top
                [d_near, -h_near_c],  # near-bottom
                [d_far, h_far_c],  # far-top
                [d_far, -h_far_c],  # far-bottom
            ]
        )

        # Cascade frustum slice (trapezoid, lightly filled)
        trap_verts = [
            (d_near, -h_near_c),
            (d_far, -h_far_c),
            (d_far, h_far_c),
            (d_near, h_near_c),
        ]
        poly = Polygon(
            trap_verts,
            closed=True,
            facecolor=cascade_colors[ci],
            edgecolor=cascade_colors[ci],
            alpha=0.15,
            linewidth=1.5,
            zorder=2,
        )
        ax.add_patch(poly)

        # Project cascade corners onto the light's axes to find the
        # oriented bounding box (OBB) — this is what the code does when
        # it transforms corners into light view space and computes AABB.
        proj_fwd = corners @ lf  # projection onto light forward axis
        proj_rgt = corners @ lr  # projection onto light right axis

        fwd_min, fwd_max = proj_fwd.min(), proj_fwd.max()
        rgt_min, rgt_max = proj_rgt.min(), proj_rgt.max()

        # Reconstruct the 4 OBB corners in world (diagram) space
        obb_corners = np.array(
            [
                lf * fwd_min + lr * rgt_min,
                lf * fwd_max + lr * rgt_min,
                lf * fwd_max + lr * rgt_max,
                lf * fwd_min + lr * rgt_max,
            ]
        )

        obb_poly = Polygon(
            obb_corners,
            closed=True,
            facecolor="none",
            edgecolor=cascade_colors[ci],
            linewidth=2.5,
            linestyle="-",
            zorder=5,
        )
        ax.add_patch(obb_poly)

        # Cascade label at the center of the trapezoid
        cx = (d_near + d_far) / 2.0
        ax.text(
            cx,
            0.0,
            cascade_labels[ci],
            color=cascade_colors[ci],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

        # Resolution annotation along the top edge of each OBB
        top_mid = lf * (fwd_min + fwd_max) / 2 + lr * rgt_max
        # Offset outward along the light-right axis
        label_pos = top_mid + lr * 0.4
        ax.text(
            label_pos[0],
            label_pos[1],
            "2048\u00b2",
            color=cascade_colors[ci],
            fontsize=7,
            ha="center",
            va="bottom",
            style="italic",
            rotation=np.degrees(light_angle),
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Light direction arrow — show the light coming from the upper-left
    arrow_start = np.array([8.0, 7.5])
    arrow_end = arrow_start + np.array([light_dx, light_dy]) * 2.0
    ax.annotate(
        "",
        xy=(arrow_end[0], arrow_end[1]),
        xytext=(arrow_start[0], arrow_start[1]),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.2",
            "color": STYLE["warn"],
            "lw": 2.5,
        },
    )
    ax.text(
        arrow_start[0] + 0.8,
        arrow_start[1] + 0.3,
        "Light\ndirection",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        va="center",
    )

    # Faint light rays through the scene showing the light angle
    for x_anchor in [2.0, 6.0, 10.0]:
        ray_start = np.array([x_anchor, 6.5])
        ray_end = ray_start + np.array([light_dx, light_dy]) * 10.0
        ax.plot(
            [ray_start[0], ray_end[0]],
            [ray_start[1], ray_end[1]],
            ":",
            color=STYLE["warn"],
            linewidth=0.6,
            alpha=0.25,
        )

    # Near/far labels
    ax.text(
        near,
        -h_near_full - 0.7,
        "near",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )
    ax.text(
        far,
        -h_far_full - 0.7,
        "far",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )

    # View direction arrow along the bottom
    ax.annotate(
        "",
        xy=(8.5, -5.0),
        xytext=(4.0, -5.0),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.15",
            "color": STYLE["text_dim"],
            "lw": 1.5,
        },
    )
    ax.text(
        6.25,
        -4.6,
        "Camera view direction",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
    )

    # Explanation note at bottom
    ax.text(
        6.25,
        -6.0,
        "Each rectangle is one cascade\u2019s orthographic projection \u2014 a box aligned to the light direction.\n"
        "The AABB is computed in light view space, so the boxes tilt with the light, not the camera.",
        color=STYLE["text_dim"],
        fontsize=7.5,
        ha="center",
        style="italic",
    )

    ax.set_title(
        "Cascade Orthographic Projections \u2014 Side View",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/15-cascaded-shadow-maps", "cascade_ortho_projections.png")


def diagram_pcf_kernel():
    """PCF (Percentage Closer Filtering) 3x3 kernel visualization.

    Shows a 3x3 grid of shadow map texels centered on the fragment's
    projected position.  Each cell shows its offset, whether it passes
    the depth test (lit) or fails (shadowed), and the final averaged
    shadow factor.  Illustrates how the [unroll] loop iterates over
    the 9 sample positions.
    """
    fig, axes = plt.subplots(
        1,
        2,
        figsize=(10, 5.0),
        facecolor=STYLE["bg"],
        gridspec_kw={"width_ratios": [1, 1], "wspace": 0.08},
    )

    ax_grid = axes[0]
    ax_result = axes[1]

    # --- Left panel: the 3x3 sample grid ---------------------------------
    ax_grid.set_facecolor(STYLE["bg"])
    ax_grid.set_xlim(-2.0, 2.0)
    ax_grid.set_ylim(-2.3, 2.3)
    ax_grid.set_aspect("equal")
    ax_grid.axis("off")

    # Depth test results for this example: an edge case where the shadow
    # boundary cuts diagonally across the kernel
    #   1 = lit (map_depth >= fragment_depth - bias)
    #   0 = shadowed
    results = [
        [1, 1, 0],  # top row    (y = -1)
        [1, 1, 0],  # middle row (y =  0)
        [0, 0, 0],  # bottom row (y = +1)
    ]

    lit_color = STYLE["accent3"]  # green
    shadow_color = STYLE["accent2"]  # orange
    cell_size = 1.1

    for row_idx, row in enumerate(results):
        for col_idx, lit in enumerate(row):
            # Offsets: x from -1 to +1, y from -1 to +1
            ox = col_idx - 1
            oy = row_idx - 1

            cx = ox * cell_size
            cy = -oy * cell_size  # flip so y=-1 is top

            color = lit_color if lit else shadow_color
            fill_alpha = 0.45 if lit else 0.30

            # Cell background
            rect = Rectangle(
                (cx - cell_size / 2, cy - cell_size / 2),
                cell_size,
                cell_size,
                facecolor=color,
                edgecolor=STYLE["text_dim"],
                alpha=fill_alpha,
                linewidth=1.2,
            )
            ax_grid.add_patch(rect)

            # Cell border (drawn separately for full opacity)
            border = Rectangle(
                (cx - cell_size / 2, cy - cell_size / 2),
                cell_size,
                cell_size,
                facecolor="none",
                edgecolor=STYLE["text_dim"],
                linewidth=1.0,
            )
            ax_grid.add_patch(border)

            # Offset label
            label = f"({ox:+d},{oy:+d})"
            ax_grid.text(
                cx,
                cy + 0.15,
                label,
                ha="center",
                va="center",
                fontsize=8,
                fontfamily="monospace",
                color=STYLE["text"],
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

            # Lit/shadow indicator
            indicator = "lit" if lit else "shadow"
            ax_grid.text(
                cx,
                cy - 0.22,
                indicator,
                ha="center",
                va="center",
                fontsize=7,
                fontfamily="monospace",
                color=color,
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

    # Center crosshair marking the fragment's projected UV
    ax_grid.plot(
        0, 0, "+", color=STYLE["warn"], markersize=14, markeredgewidth=2.0, zorder=10
    )

    # Annotation for the center
    ax_grid.text(
        0,
        -2.05,
        "fragment\u2019s shadow UV",
        ha="center",
        va="center",
        fontsize=8,
        color=STYLE["warn"],
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax_grid.text(
        0,
        2.15,
        "3\u00d73 Sample Grid",
        ha="center",
        va="center",
        fontsize=12,
        fontweight="bold",
        color=STYLE["text"],
    )

    # --- Right panel: averaging result ------------------------------------
    ax_result.set_facecolor(STYLE["bg"])
    ax_result.set_xlim(-2.0, 2.0)
    ax_result.set_ylim(-2.3, 2.3)
    ax_result.set_aspect("equal")
    ax_result.axis("off")

    lit_count = sum(cell for row in results for cell in row)
    shadow_count = 9 - lit_count
    factor = lit_count / 9.0

    # Draw a bar/equation summary
    y_top = 1.5

    ax_result.text(
        0,
        y_top,
        "Depth test per sample:",
        ha="center",
        va="center",
        fontsize=10,
        color=STYLE["text"],
        fontweight="bold",
    )

    ax_result.text(
        0,
        y_top - 0.55,
        "map_depth \u2265 frag_depth \u2212 bias  \u2192  1 (lit)\n"
        "map_depth < frag_depth \u2212 bias  \u2192  0 (shadow)",
        ha="center",
        va="center",
        fontsize=8,
        fontfamily="monospace",
        color=STYLE["text_dim"],
        linespacing=1.6,
    )

    # Tally
    ax_result.text(
        0,
        y_top - 1.55,
        f"{lit_count} lit  +  {shadow_count} shadowed  =  9 samples",
        ha="center",
        va="center",
        fontsize=10,
        color=STYLE["text"],
    )

    # Result
    ax_result.text(
        0,
        y_top - 2.25,
        f"shadow_factor = {lit_count} / 9 = {factor:.3f}",
        ha="center",
        va="center",
        fontsize=13,
        fontfamily="monospace",
        fontweight="bold",
        color=STYLE["accent1"],
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Gradient bar showing the result visually
    bar_w = 2.8
    bar_h = 0.35
    bar_y = y_top - 3.1

    # Background bar (full shadow)
    bg_bar = Rectangle(
        (-bar_w / 2, bar_y - bar_h / 2),
        bar_w,
        bar_h,
        facecolor=shadow_color,
        alpha=0.3,
        edgecolor=STYLE["text_dim"],
        linewidth=1.0,
    )
    ax_result.add_patch(bg_bar)

    # Lit portion
    lit_bar = Rectangle(
        (-bar_w / 2, bar_y - bar_h / 2),
        bar_w * factor,
        bar_h,
        facecolor=lit_color,
        alpha=0.6,
        edgecolor="none",
    )
    ax_result.add_patch(lit_bar)

    # Border
    border_bar = Rectangle(
        (-bar_w / 2, bar_y - bar_h / 2),
        bar_w,
        bar_h,
        facecolor="none",
        edgecolor=STYLE["text_dim"],
        linewidth=1.0,
    )
    ax_result.add_patch(border_bar)

    ax_result.text(
        -bar_w / 2 - 0.1,
        bar_y,
        "0",
        ha="right",
        va="center",
        fontsize=8,
        color=STYLE["text_dim"],
    )
    ax_result.text(
        bar_w / 2 + 0.1,
        bar_y,
        "1",
        ha="left",
        va="center",
        fontsize=8,
        color=STYLE["text_dim"],
    )

    # Marker at the factor position
    marker_x = -bar_w / 2 + bar_w * factor
    ax_result.plot(
        marker_x,
        bar_y + bar_h / 2 + 0.08,
        "v",
        color=STYLE["accent1"],
        markersize=8,
    )

    ax_result.text(
        0,
        2.15,
        "Averaging",
        ha="center",
        va="center",
        fontsize=12,
        fontweight="bold",
        color=STYLE["text"],
    )

    # Overall title
    fig.suptitle(
        "PCF (Percentage Closer Filtering) \u2014 3\u00d73 Kernel",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    fig.tight_layout(rect=(0, 0, 1, 0.90))
    save(fig, "gpu/15-cascaded-shadow-maps", "pcf_kernel.png")


def diagram_peter_panning():
    """Peter panning: shadow detachment caused by excessive depth bias.

    Shows a side-view cross-section of an object on a surface, with light
    rays casting a shadow.  The left panel shows correct shadows (touching
    the object base), and the right panel shows peter panning where too
    much depth bias shifts the shadow map surface, creating a visible gap
    between the object and its shadow.
    """
    fig, (ax_good, ax_bad) = plt.subplots(
        1, 2, figsize=(13, 5.5), facecolor=STYLE["bg"]
    )

    for ax, title, show_bias in [
        (ax_good, "Correct shadow", False),
        (ax_bad, "Peter panning (too much bias)", True),
    ]:
        setup_axes(ax, xlim=(-0.5, 10.5), ylim=(-1.5, 7.5), grid=False)
        ax.set_xticks([])
        ax.set_yticks([])

        # -- Ground plane --
        ground_y = 1.0
        ax.fill_between(
            [-0.5, 10.5],
            [-1.5, -1.5],
            [ground_y, ground_y],
            color=STYLE["surface"],
            alpha=0.6,
        )
        ax.plot(
            [-0.5, 10.5],
            [ground_y, ground_y],
            color=STYLE["axis"],
            linewidth=1.5,
        )
        ax.text(
            0.0,
            0.3,
            "Ground",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="left",
        )

        # -- Object (box) --
        box_left = 3.0
        box_right = 5.0
        box_top = 4.5
        box = Rectangle(
            (box_left, ground_y),
            box_right - box_left,
            box_top - ground_y,
            facecolor=STYLE["accent1"],
            edgecolor=STYLE["text"],
            linewidth=1.5,
            alpha=0.7,
            zorder=5,
        )
        ax.add_patch(box)
        ax.text(
            (box_left + box_right) / 2,
            (ground_y + box_top) / 2,
            "Object",
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=6,
        )

        # -- Light rays (coming from upper-left) --
        light_angle = 0.6  # radians from vertical
        for lx_start in [2.0, 4.0, 6.0, 8.0]:
            ly_start = 7.5
            lx_end = lx_start + 3.0 * np.sin(light_angle)
            ly_end = ly_start - 3.0 * np.cos(light_angle)
            ax.annotate(
                "",
                xy=(lx_end, ly_end),
                xytext=(lx_start, ly_start),
                arrowprops={
                    "arrowstyle": "->,head_width=0.15,head_length=0.12",
                    "color": STYLE["warn"],
                    "lw": 1.2,
                    "alpha": 0.5,
                },
                zorder=1,
            )

        # -- Shadow on the ground --
        # The shadow starts where the light ray from the object's base hits ground
        # With bias, the effective shadow surface shifts, pushing the shadow start
        # further from the object
        shadow_start = box_right  # light from left, shadow on right side
        shadow_end = 8.5

        bias_offset = 1.3 if show_bias else 0.0
        actual_shadow_start = shadow_start + bias_offset

        # Shadow region
        ax.fill_between(
            [actual_shadow_start, shadow_end],
            [ground_y, ground_y],
            [ground_y - 0.01, ground_y - 0.01],
            color=STYLE["bg"],
            alpha=1.0,
            zorder=3,
        )
        ax.fill_between(
            [actual_shadow_start, shadow_end],
            [ground_y - 0.25, ground_y - 0.25],
            [ground_y, ground_y],
            color="#0a0a1a",
            alpha=0.85,
            zorder=3,
        )
        ax.text(
            (actual_shadow_start + shadow_end) / 2,
            ground_y - 0.65,
            "Shadow",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            zorder=4,
        )

        if show_bias:
            # -- Highlight the gap --
            gap_color = STYLE["accent2"]
            ax.fill_between(
                [shadow_start, actual_shadow_start],
                [ground_y - 0.25, ground_y - 0.25],
                [ground_y, ground_y],
                color=gap_color,
                alpha=0.35,
                hatch="//",
                zorder=3,
            )
            ax.annotate(
                "Gap!\nShadow\ndetached",
                xy=((shadow_start + actual_shadow_start) / 2, ground_y - 0.12),
                xytext=((shadow_start + actual_shadow_start) / 2, -0.8),
                color=gap_color,
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="top",
                arrowprops={
                    "arrowstyle": "->,head_width=0.2,head_length=0.15",
                    "color": gap_color,
                    "lw": 1.5,
                },
                zorder=7,
            )

            # -- Show the biased depth surface above the actual surface --
            bias_y = ground_y + 0.5
            ax.plot(
                [1.0, 9.5],
                [bias_y, bias_y],
                "--",
                color=STYLE["accent2"],
                linewidth=1.5,
                alpha=0.7,
                zorder=2,
            )
            ax.text(
                9.6,
                bias_y,
                "Biased\ndepth\nsurface",
                color=STYLE["accent2"],
                fontsize=7,
                fontweight="bold",
                va="center",
                ha="left",
                alpha=0.9,
            )

            # Bias offset arrow
            ax.annotate(
                "",
                xy=(1.5, ground_y),
                xytext=(1.5, bias_y),
                arrowprops={
                    "arrowstyle": "<->,head_width=0.15,head_length=0.1",
                    "color": STYLE["accent2"],
                    "lw": 1.2,
                },
                zorder=4,
            )
            ax.text(
                1.1,
                (ground_y + bias_y) / 2,
                "bias",
                color=STYLE["accent2"],
                fontsize=8,
                ha="right",
                va="center",
                fontstyle="italic",
            )
        else:
            # Correct case: shadow touches the object base
            ax.annotate(
                "Shadow meets\nobject base",
                xy=(shadow_start, ground_y - 0.12),
                xytext=(shadow_start + 0.3, -0.7),
                color=STYLE["accent3"],
                fontsize=8,
                fontweight="bold",
                ha="left",
                va="top",
                arrowprops={
                    "arrowstyle": "->,head_width=0.15,head_length=0.1",
                    "color": STYLE["accent3"],
                    "lw": 1.2,
                },
                zorder=7,
            )

        ax.set_title(
            title,
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            pad=10,
        )

    fig.suptitle(
        "Peter Panning \u2014 Shadow Detachment from Excessive Depth Bias",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )

    fig.tight_layout()
    save(fig, "gpu/15-cascaded-shadow-maps", "peter_panning.png")


def diagram_arvo_method():
    """Arvo's method for transforming an AABB through a matrix.

    Two-panel diagram showing:
    - Left: The naive approach — transform all corners, find new min/max
    - Right: Arvo's decomposition — per-axis min/max from matrix columns

    Both produce the same world-space AABB, but Arvo's method avoids
    transforming every corner individually.
    """
    # --- Setup: a 2D AABB rotated 35 degrees with translation -----------
    theta = np.radians(35)
    cos_t, sin_t = np.cos(theta), np.sin(theta)
    # Rotation matrix columns
    col0 = np.array([cos_t, sin_t])
    col1 = np.array([-sin_t, cos_t])
    tx = np.array([3.0, 1.5])  # translation

    # Local AABB
    lmin = np.array([-1.5, -1.0])
    lmax = np.array([1.5, 1.0])

    # Compute the 4 corners of the local AABB
    corners_local = np.array(
        [
            [lmin[0], lmin[1]],
            [lmax[0], lmin[1]],
            [lmax[0], lmax[1]],
            [lmin[0], lmax[1]],
        ]
    )

    # Transform corners: world = M * local + t
    corners_world = np.array([col0 * c[0] + col1 * c[1] + tx for c in corners_local])

    # World AABB from corners (the correct answer both methods produce)
    wmin = corners_world.min(axis=0)
    wmax = corners_world.max(axis=0)

    # --- Figure setup ---------------------------------------------------
    fig, (ax_naive, ax_arvo) = plt.subplots(
        1, 2, figsize=(13, 6.5), facecolor=STYLE["bg"]
    )

    pad = 1.2
    xlim = (wmin[0] - 2.5, wmax[0] + pad)
    ylim = (wmin[1] - 2.0, wmax[1] + pad + 0.5)

    for ax in (ax_naive, ax_arvo):
        setup_axes(ax, xlim=xlim, ylim=ylim, grid=True)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(False)

    # --- Helper: draw the rotated rectangle and world AABB ---------------
    def draw_common(ax, show_corners=False):
        """Draw the rotated box and its world-space AABB."""
        # Rotated rectangle (the actual transformed shape)
        rotated_poly = Polygon(
            corners_world,
            closed=True,
            linewidth=2.0,
            edgecolor=STYLE["accent1"],
            facecolor=STYLE["accent1"],
            alpha=0.18,
            zorder=3,
        )
        ax.add_patch(rotated_poly)

        # World-space AABB (dashed)
        world_rect = Rectangle(
            (wmin[0], wmin[1]),
            wmax[0] - wmin[0],
            wmax[1] - wmin[1],
            linewidth=2.0,
            edgecolor=STYLE["accent3"],
            facecolor="none",
            linestyle="--",
            zorder=4,
        )
        ax.add_patch(world_rect)

        # Label the world AABB
        ax.text(
            wmax[0] + 0.1,
            wmax[1],
            "world\nAABB",
            color=STYLE["accent3"],
            fontsize=9,
            fontweight="bold",
            va="top",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        if show_corners:
            for k, c in enumerate(corners_world):
                ax.plot(
                    c[0],
                    c[1],
                    "o",
                    color=STYLE["accent2"],
                    markersize=6,
                    zorder=8,
                )
                # Label alternating corners to avoid clutter
                if k in (0, 2):
                    ax.text(
                        c[0] - 0.15,
                        c[1] - 0.3 if k == 0 else c[1] + 0.2,
                        f"c{k}",
                        color=STYLE["accent2"],
                        fontsize=8,
                        ha="center",
                        path_effects=[
                            pe.withStroke(linewidth=3, foreground=STYLE["bg"])
                        ],
                        zorder=10,
                    )

        # Translation origin
        ax.plot(
            tx[0],
            tx[1],
            "+",
            color=STYLE["text_dim"],
            markersize=8,
            markeredgewidth=1.5,
            zorder=6,
        )

    # ---- Left panel: naive (transform all corners) ----------------------
    draw_common(ax_naive, show_corners=True)

    # Draw lines from each corner to the AABB edges to show min/max search
    for c in corners_world:
        # Vertical projection to the AABB top/bottom
        ax_naive.plot(
            [c[0], c[0]],
            [wmin[1] - 0.15, wmax[1] + 0.15],
            ":",
            color=STYLE["text_dim"],
            lw=0.7,
            alpha=0.4,
            zorder=1,
        )
        # Horizontal projection
        ax_naive.plot(
            [wmin[0] - 0.15, wmax[0] + 0.15],
            [c[1], c[1]],
            ":",
            color=STYLE["text_dim"],
            lw=0.7,
            alpha=0.4,
            zorder=1,
        )

    # Mark the extreme corners that define the AABB
    # Find which corners define xmin, xmax, ymin, ymax
    ixmin = corners_world[:, 0].argmin()
    ixmax = corners_world[:, 0].argmax()
    iymin = corners_world[:, 1].argmin()
    iymax = corners_world[:, 1].argmax()

    for idx in {ixmin, ixmax, iymin, iymax}:
        ax_naive.plot(
            corners_world[idx, 0],
            corners_world[idx, 1],
            "o",
            color=STYLE["accent3"],
            markersize=9,
            markerfacecolor="none",
            markeredgewidth=2,
            zorder=9,
        )

    # Annotation
    ax_naive.text(
        (xlim[0] + xlim[1]) / 2,
        ylim[0] + 0.3,
        "Transform each corner, then find min/max\n"
        "3D: 8 corners \u00d7 matrix multiply",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # ---- Right panel: Arvo's decomposition ------------------------------
    draw_common(ax_arvo, show_corners=False)

    # Show the decomposition: translation + column contributions
    # Start at translation
    ax_arvo.plot(
        tx[0],
        tx[1],
        "o",
        color=STYLE["warn"],
        markersize=7,
        zorder=8,
    )
    ax_arvo.text(
        tx[0] + 0.15,
        tx[1] + 0.3,
        "start: translation",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # Column 0 contributions (scaled by lmin[0] and lmax[0])
    # These are the two possible contributions from the X extent
    contrib0_lo = col0 * lmin[0]
    contrib0_hi = col0 * lmax[0]

    # Column 1 contributions (scaled by lmin[1] and lmax[1])
    contrib1_lo = col1 * lmin[1]
    contrib1_hi = col1 * lmax[1]

    # Draw column 0 contributions from translation point
    arrow_kw = {
        "arrowstyle": "->,head_width=0.2,head_length=0.12",
        "lw": 2.2,
    }

    # Column 0 — max extent (positive X side)
    ax_arvo.annotate(
        "",
        xy=(tx[0] + contrib0_hi[0], tx[1] + contrib0_hi[1]),
        xytext=(tx[0], tx[1]),
        arrowprops={**arrow_kw, "color": STYLE["accent1"]},
        zorder=7,
    )
    ax_arvo.text(
        tx[0] + contrib0_hi[0] / 2 + 0.05,
        tx[1] + contrib0_hi[1] / 2 + 0.25,
        "col\u2080 \u00d7 max\u2093",
        color=STYLE["accent1"],
        fontsize=8,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # Column 0 — min extent (negative X side)
    ax_arvo.annotate(
        "",
        xy=(tx[0] + contrib0_lo[0], tx[1] + contrib0_lo[1]),
        xytext=(tx[0], tx[1]),
        arrowprops={**arrow_kw, "color": STYLE["accent1"], "linestyle": "--"},
        zorder=7,
    )
    ax_arvo.text(
        tx[0] + contrib0_lo[0] / 2 - 0.05,
        tx[1] + contrib0_lo[1] / 2 - 0.3,
        "col\u2080 \u00d7 min\u2093",
        color=STYLE["accent1"],
        fontsize=8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # Column 1 — max extent (positive Y side)
    ax_arvo.annotate(
        "",
        xy=(tx[0] + contrib1_hi[0], tx[1] + contrib1_hi[1]),
        xytext=(tx[0], tx[1]),
        arrowprops={**arrow_kw, "color": STYLE["accent4"]},
        zorder=7,
    )
    ax_arvo.text(
        tx[0] + contrib1_hi[0] / 2 - 0.45,
        tx[1] + contrib1_hi[1] / 2 + 0.15,
        "col\u2081 \u00d7 max\u1d67",
        color=STYLE["accent4"],
        fontsize=8,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # Column 1 — min extent (negative Y side)
    ax_arvo.annotate(
        "",
        xy=(tx[0] + contrib1_lo[0], tx[1] + contrib1_lo[1]),
        xytext=(tx[0], tx[1]),
        arrowprops={**arrow_kw, "color": STYLE["accent4"], "linestyle": "--"},
        zorder=7,
    )
    ax_arvo.text(
        tx[0] + contrib1_lo[0] / 2 + 0.35,
        tx[1] + contrib1_lo[1] / 2 - 0.2,
        "col\u2081 \u00d7 min\u1d67",
        color=STYLE["accent4"],
        fontsize=8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # Annotation explaining the per-axis logic
    ax_arvo.text(
        (xlim[0] + xlim[1]) / 2,
        ylim[0] + 0.3,
        "Per axis: new_min[i] = t[i] + \u03a3 min(M\u1d62\u2c7c\u00b7lo\u2c7c, M\u1d62\u2c7c\u00b7hi\u2c7c)\n"
        "3D: 18 multiplies + 18 comparisons (no corner transforms)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        zorder=10,
    )

    # --- Titles ----------------------------------------------------------
    ax_naive.set_title(
        "Naive: transform all corners",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax_arvo.set_title(
        "Arvo\u2019s method: per-axis decomposition",
        color=STYLE["accent3"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    fig.suptitle(
        "Transforming an AABB Through a Matrix",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )

    fig.tight_layout()
    save(fig, "gpu/16-blending", "arvo_method.png")


def diagram_aabb_sorting():
    """AABB nearest-point sorting vs center-distance sorting for transparent
    objects.

    Side-by-side top-down view showing why center-distance sorting fails when
    two objects share the same center (a flat alpha-symbol plane inside a glass
    box), and how AABB nearest-point distance produces the correct draw order.
    """
    fig, (ax_bad, ax_good) = plt.subplots(1, 2, figsize=(12, 6), facecolor=STYLE["bg"])

    for ax in (ax_bad, ax_good):
        setup_axes(ax, xlim=(-4.5, 6.5), ylim=(-3.5, 3.5), grid=False)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(False)

    # Shared geometry (top-down 2D: X = right, Y = depth into screen)
    # Both objects are centered at the same point
    center = np.array([0.0, 0.0])

    # Glass box — large AABB
    box_half = np.array([2.0, 2.0])
    box_min = center - box_half
    box_max = center + box_half

    # Alpha-symbol plane — thin AABB (a flat vertical surface)
    plane_half = np.array([1.2, 0.15])
    plane_min = center - plane_half
    plane_max = center + plane_half

    # Camera position — to the right
    cam = np.array([5.0, 0.0])

    # Nearest point on box AABB to camera
    box_nearest = np.array(
        [
            np.clip(cam[0], box_min[0], box_max[0]),
            np.clip(cam[1], box_min[1], box_max[1]),
        ]
    )

    # Nearest point on plane AABB to camera
    plane_nearest = np.array(
        [
            np.clip(cam[0], plane_min[0], plane_max[0]),
            np.clip(cam[1], plane_min[1], plane_max[1]),
        ]
    )

    def draw_scene(ax, mode):
        """Draw the scene in either 'center' or 'aabb' mode."""
        # Glass box (semi-transparent blue rectangle)
        box_rect = Rectangle(
            (box_min[0], box_min[1]),
            box_half[0] * 2,
            box_half[1] * 2,
            linewidth=2,
            edgecolor=STYLE["accent1"],
            facecolor=STYLE["accent1"],
            alpha=0.15,
            zorder=2,
        )
        ax.add_patch(box_rect)
        ax.text(
            box_min[0] + 0.15,
            box_max[1] - 0.3,
            "glass box",
            color=STYLE["accent1"],
            fontsize=9,
            style="italic",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        # Alpha-symbol plane (orange filled thin rectangle)
        plane_rect = Rectangle(
            (plane_min[0], plane_min[1]),
            plane_half[0] * 2,
            plane_half[1] * 2,
            linewidth=2,
            edgecolor=STYLE["accent2"],
            facecolor=STYLE["accent2"],
            alpha=0.5,
            zorder=3,
        )
        ax.add_patch(plane_rect)
        ax.text(
            plane_min[0] + 0.15,
            plane_min[1] - 0.4,
            "\u03b1 plane",
            color=STYLE["accent2"],
            fontsize=9,
            style="italic",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        # Shared center point
        ax.plot(
            center[0],
            center[1],
            "o",
            color=STYLE["text_dim"],
            markersize=5,
            zorder=8,
        )
        ax.text(
            center[0] - 0.1,
            center[1] + 0.35,
            "center",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        # Camera
        ax.plot(cam[0], cam[1], "s", color=STYLE["warn"], markersize=10, zorder=10)
        ax.text(
            cam[0],
            cam[1] - 0.55,
            "camera",
            color=STYLE["warn"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        if mode == "center":
            # Both distance lines go to the same center — ambiguous
            ax.plot(
                [cam[0], center[0]],
                [cam[1], center[1]],
                "--",
                color=STYLE["accent1"],
                lw=1.8,
                alpha=0.7,
                zorder=5,
            )
            ax.plot(
                [cam[0], center[0]],
                [cam[1], center[1] + 0.08],
                "--",
                color=STYLE["accent2"],
                lw=1.8,
                alpha=0.7,
                zorder=5,
            )

            dist = np.linalg.norm(cam - center)
            ax.text(
                cam[0] / 2 + center[0] / 2,
                0.55,
                f"d = {dist:.1f}",
                color=STYLE["accent1"],
                fontsize=10,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
            ax.text(
                cam[0] / 2 + center[0] / 2,
                -0.55,
                f"d = {dist:.1f}",
                color=STYLE["accent2"],
                fontsize=10,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

            # Verdict
            ax.text(
                1.0,
                -2.8,
                "Same distance \u2014 arbitrary order!",
                color=STYLE["accent2"],
                fontsize=11,
                fontweight="bold",
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

        else:
            # AABB nearest-point distances
            # Line to box nearest point
            ax.plot(
                box_nearest[0],
                box_nearest[1],
                "o",
                color=STYLE["accent1"],
                markersize=7,
                zorder=8,
            )
            ax.plot(
                [cam[0], box_nearest[0]],
                [cam[1], box_nearest[1]],
                "-",
                color=STYLE["accent1"],
                lw=2.2,
                alpha=0.8,
                zorder=5,
            )
            dist_box = np.linalg.norm(cam - box_nearest)
            ax.text(
                (cam[0] + box_nearest[0]) / 2,
                0.5,
                f"d = {dist_box:.1f}",
                color=STYLE["accent1"],
                fontsize=10,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

            # Line to plane nearest point
            ax.plot(
                plane_nearest[0],
                plane_nearest[1],
                "o",
                color=STYLE["accent2"],
                markersize=7,
                zorder=8,
            )
            ax.plot(
                [cam[0], plane_nearest[0]],
                [cam[1], plane_nearest[1]],
                "-",
                color=STYLE["accent2"],
                lw=2.2,
                alpha=0.8,
                zorder=5,
            )
            dist_plane = np.linalg.norm(cam - plane_nearest)
            ax.text(
                (cam[0] + plane_nearest[0]) / 2,
                -0.5,
                f"d = {dist_plane:.1f}",
                color=STYLE["accent2"],
                fontsize=10,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

            # Draw order annotation
            ax.text(
                1.0,
                -2.4,
                f"\u03b1 plane: d = {dist_plane:.1f} \u2192 draw first",
                color=STYLE["accent2"],
                fontsize=10,
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
            ax.text(
                1.0,
                -3.0,
                f"glass box: d = {dist_box:.1f} \u2192 draw second",
                color=STYLE["accent1"],
                fontsize=10,
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

    draw_scene(ax_bad, "center")
    draw_scene(ax_good, "aabb")

    ax_bad.set_title(
        "Center-distance sorting (broken)",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax_good.set_title(
        "AABB nearest-point sorting (correct)",
        color=STYLE["accent3"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    fig.suptitle(
        "Sorting Transparent Objects \u2014 Top-Down View",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )

    fig.tight_layout()
    save(fig, "gpu/16-blending", "aabb_sorting.png")


def diagram_blend_modes():
    """Four blend modes compared — Opaque, Alpha Test, Alpha Blend, Additive.

    A 2x2 grid showing how each mode combines a foreground color (src) with a
    background color (dst). Each panel renders overlapping colored rectangles
    and computes the actual blended result in the overlap region.
    """
    fig, axes = plt.subplots(2, 2, figsize=(11, 8), facecolor=STYLE["bg"])

    # Source and destination colors (linear RGB, 0–1)
    src_rgb = np.array([0.2, 0.5, 1.0])  # blue-ish (accent1-like)
    src_alpha = 0.5
    dst_rgb = np.array([1.0, 0.4, 0.2])  # orange-ish (accent2-like)
    dst_alpha = 1.0

    modes = [
        {
            "name": "Opaque",
            "subtitle": "Blend disabled — depth write ON",
            "formula": "result = src",
            "overlap_rgb": src_rgb,
            "overlap_alpha": 1.0,
            "show_discard": False,
            "color": STYLE["text"],
        },
        {
            "name": "Alpha Test (MASK)",
            "subtitle": "Blend disabled — depth write ON",
            "formula": "clip(α − cutoff)",
            "overlap_rgb": src_rgb,
            "overlap_alpha": 1.0,
            "show_discard": True,
            "color": STYLE["accent3"],
        },
        {
            "name": "Alpha Blend",
            "subtitle": "SRC_ALPHA / ONE_MINUS_SRC_ALPHA — depth write OFF",
            "formula": "result = src·α + dst·(1−α)",
            "overlap_rgb": src_rgb * src_alpha + dst_rgb * (1.0 - src_alpha),
            "overlap_alpha": 1.0,
            "show_discard": False,
            "color": STYLE["accent1"],
        },
        {
            "name": "Additive",
            "subtitle": "SRC_ALPHA / ONE — depth write OFF",
            "formula": "result = src·α + dst",
            "overlap_rgb": np.clip(src_rgb * src_alpha + dst_rgb * dst_alpha, 0, 1),
            "overlap_alpha": 1.0,
            "show_discard": False,
            "color": STYLE["accent2"],
        },
    ]

    for ax, mode in zip(axes.flat, modes):
        setup_axes(ax, xlim=(-0.5, 10.5), ylim=(-1.5, 6.5), grid=False)
        ax.set_xticks([])
        ax.set_yticks([])
        for spine in ax.spines.values():
            spine.set_visible(False)

        # --- Draw dst rectangle (background, left side) ---
        dst_rect = Rectangle(
            (0.5, 1.0),
            5.0,
            4.0,
            linewidth=1.5,
            edgecolor=(*dst_rgb, 0.8),
            facecolor=(*dst_rgb, dst_alpha),
            zorder=2,
        )
        ax.add_patch(dst_rect)
        ax.text(
            1.5,
            5.4,
            "dst",
            color=(*dst_rgb, 1.0),
            fontsize=11,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        if mode["show_discard"]:
            # Alpha test: show two cases — above cutoff (passes) and
            # below cutoff (discarded, dst shows through)

            # Top half of src: alpha > cutoff → rendered fully opaque
            pass_rect = Rectangle(
                (4.0, 3.0),
                5.5,
                2.0,
                linewidth=1.5,
                edgecolor=(*src_rgb, 0.8),
                facecolor=(*src_rgb, 1.0),
                zorder=4,
            )
            ax.add_patch(pass_rect)

            # Bottom half of src: alpha < cutoff → discarded
            # Draw with dashed border and no fill to show it's gone
            discard_rect = Rectangle(
                (4.0, 1.0),
                5.5,
                2.0,
                linewidth=1.5,
                edgecolor=(*src_rgb, 0.4),
                facecolor="none",
                linestyle="--",
                zorder=4,
            )
            ax.add_patch(discard_rect)

            # Overlap region — top half: src wins (opaque)
            overlap_pass = Rectangle(
                (4.0, 3.0),
                1.5,
                2.0,
                linewidth=0,
                facecolor=(*src_rgb, 1.0),
                zorder=5,
            )
            ax.add_patch(overlap_pass)

            # Overlap region — bottom half: dst shows through (discarded)
            overlap_discard = Rectangle(
                (4.0, 1.0),
                1.5,
                2.0,
                linewidth=0,
                facecolor=(*dst_rgb, dst_alpha),
                zorder=5,
            )
            ax.add_patch(overlap_discard)

            # Labels
            ax.text(
                6.8,
                3.8,
                "α ≥ 0.5",
                color=STYLE["accent3"],
                fontsize=9,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
            ax.text(
                6.8,
                3.3,
                "PASS",
                color=STYLE["accent3"],
                fontsize=8,
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
            ax.text(
                6.8,
                1.8,
                "α < 0.5",
                color=STYLE["accent2"],
                fontsize=9,
                ha="center",
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
            ax.text(
                6.8,
                1.3,
                "DISCARD",
                color=STYLE["accent2"],
                fontsize=8,
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

            # src label
            ax.text(
                8.5,
                5.4,
                "src",
                color=(*src_rgb, 1.0),
                fontsize=11,
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )
        else:
            # Non-discard modes: draw src rectangle overlapping dst
            src_rect = Rectangle(
                (4.0, 1.0),
                5.5,
                4.0,
                linewidth=1.5,
                edgecolor=(*src_rgb, 0.8),
                facecolor=(*src_rgb, src_alpha if mode["name"] != "Opaque" else 1.0),
                zorder=3,
            )
            ax.add_patch(src_rect)

            # src label
            ax.text(
                8.0,
                5.4,
                f"src (α={src_alpha})" if mode["name"] != "Opaque" else "src",
                color=(*src_rgb, 1.0),
                fontsize=11,
                fontweight="bold",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

            # Overlap region with computed blend result
            overlap_rect = Rectangle(
                (4.0, 1.0),
                1.5,
                4.0,
                linewidth=0,
                facecolor=(*mode["overlap_rgb"], mode["overlap_alpha"]),
                zorder=5,
            )
            ax.add_patch(overlap_rect)

        # --- Formula label ---
        ax.text(
            5.25,
            -0.5,
            mode["formula"],
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            family="monospace",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            zorder=10,
        )

        # --- Overlap bracket label ---
        if not mode["show_discard"]:
            ax.annotate(
                "",
                xy=(4.0, 0.7),
                xytext=(5.5, 0.7),
                arrowprops={
                    "arrowstyle": "<->",
                    "color": STYLE["warn"],
                    "lw": 1.5,
                },
                zorder=10,
            )
            ax.text(
                4.75,
                0.2,
                "overlap",
                color=STYLE["warn"],
                fontsize=8,
                ha="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
                zorder=10,
            )

        # --- Title ---
        ax.set_title(
            mode["name"],
            color=mode["color"],
            fontsize=13,
            fontweight="bold",
            pad=8,
        )

        # --- Subtitle ---
        ax.text(
            5.25,
            6.2,
            mode["subtitle"],
            color=STYLE["text_dim"],
            fontsize=7.5,
            ha="center",
            style="italic",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            zorder=10,
        )

    fig.suptitle(
        "Four Blend Modes Compared",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=1.01,
    )

    fig.tight_layout()
    save(fig, "gpu/16-blending", "blend_modes.png")


# ---------------------------------------------------------------------------
# gpu/17-normal-maps — tangent_space.png
# ---------------------------------------------------------------------------


def diagram_tangent_space():
    """Tangent space: a per-vertex coordinate frame on a surface.

    Two-panel diagram.  Left shows a tilted surface in world space with T, B, N
    basis vectors emanating from a vertex — the local coordinate frame that the
    TBN matrix encodes.  Right shows the same frame axis-aligned in tangent
    space, where the normal map is authored: (0,0,1) is the unperturbed normal,
    and a tilted sample vector shows how bump detail is expressed.
    """
    fig = plt.figure(figsize=(11, 6), facecolor=STYLE["bg"])

    # -- Helpers for pseudo-3D projection (simple oblique) --
    def proj(x, y, z):
        """Project a 3D point to 2D using oblique projection."""
        scale = 0.45
        return (x + z * scale * np.cos(0.7), y + z * scale * np.sin(0.7))

    def draw_vec3d(ax, origin3, vec3, color, label, label_off=(0, 0), lw=2.5):
        """Draw a 3D vector projected to 2D."""
        o2 = proj(*origin3)
        tip3 = (origin3[0] + vec3[0], origin3[1] + vec3[1], origin3[2] + vec3[2])
        t2 = proj(*tip3)
        dx, dy = t2[0] - o2[0], t2[1] - o2[1]
        draw_vector(ax, o2, (dx, dy), color, label, label_offset=label_off, lw=lw)

    # -------------------------------------------------------------------
    # Left panel — World space: tilted surface with TBN frame
    # -------------------------------------------------------------------
    ax1 = fig.add_subplot(121)
    ax1.set_facecolor(STYLE["bg"])
    ax1.set_aspect("equal")
    ax1.grid(False)

    # Surface quad vertices (tilted slightly in 3D)
    quad_3d = [
        (-1.8, -0.3, -1.0),
        (1.8, 0.3, -1.0),
        (2.2, 0.5, 1.0),
        (-1.4, -0.1, 1.0),
    ]
    quad_2d = [proj(*v) for v in quad_3d]

    # Draw filled surface
    surface = Polygon(
        quad_2d,
        closed=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["axis"],
        linewidth=1.5,
        alpha=0.6,
        zorder=1,
    )
    ax1.add_patch(surface)

    # Surface label
    cx = float(np.mean([p[0] for p in quad_2d]))
    cy = float(np.mean([p[1] for p in quad_2d])) - 0.3
    ax1.text(
        cx,
        cy,
        "surface",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
        fontstyle="italic",
    )

    # Vertex where the TBN frame lives
    vert_3d = (0.2, 0.1, 0.0)
    vert_2d = proj(*vert_3d)
    ax1.plot(vert_2d[0], vert_2d[1], "o", color=STYLE["text"], markersize=7, zorder=10)

    # TBN basis vectors (tangent along surface U, bitangent along V, normal up)
    vec_scale = 1.8
    T_vec = (1.0 * vec_scale, 0.15 * vec_scale, 0.0)
    B_vec = (0.05 * vec_scale, 0.05 * vec_scale, 0.5 * vec_scale)
    N_vec = (-0.15 * vec_scale, 0.95 * vec_scale, 0.1 * vec_scale)

    draw_vec3d(ax1, vert_3d, T_vec, STYLE["accent1"], "T", label_off=(0.15, -0.3), lw=3)
    draw_vec3d(ax1, vert_3d, B_vec, STYLE["accent4"], "B", label_off=(-0.45, 0.0), lw=3)
    draw_vec3d(ax1, vert_3d, N_vec, STYLE["accent3"], "N", label_off=(-0.4, 0.1), lw=3)

    # A perturbed normal (slightly tilted from N) — the mapped result
    perturbed_vec = (
        -0.15 * vec_scale + 0.4,
        0.95 * vec_scale - 0.15,
        0.1 * vec_scale + 0.25,
    )
    draw_vec3d(
        ax1,
        vert_3d,
        perturbed_vec,
        STYLE["accent2"],
        "N'",
        label_off=(-0.05, 0.15),
        lw=2,
    )

    # Dashed arc between N and N' to show perturbation
    n_tip = proj(
        vert_3d[0] + N_vec[0],
        vert_3d[1] + N_vec[1],
        vert_3d[2] + N_vec[2],
    )
    np_tip = proj(
        vert_3d[0] + perturbed_vec[0],
        vert_3d[1] + perturbed_vec[1],
        vert_3d[2] + perturbed_vec[2],
    )

    # Short dashed curve between N and N' tips
    for t_val in np.linspace(0.3, 0.7, 8):
        px = n_tip[0] * (1 - t_val) + np_tip[0] * t_val
        py = n_tip[1] * (1 - t_val) + np_tip[1] * t_val
        ax1.plot(px, py, ".", color=STYLE["warn"], markersize=3, alpha=0.7, zorder=5)

    # Annotation: "TBN transforms tangent→world"
    ax1.text(
        0.0,
        -1.2,
        "TBN matrix columns = [T, B, N]",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax1.text(
        0.0,
        -1.6,
        "transforms tangent-space → world-space",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax1.set_xlim(-2.5, 3.5)
    ax1.set_ylim(-2.0, 3.0)
    ax1.set_xticks([])
    ax1.set_yticks([])
    for spine in ax1.spines.values():
        spine.set_visible(False)

    ax1.set_title(
        "World Space",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=10,
    )

    # -------------------------------------------------------------------
    # Right panel — Tangent space: axis-aligned TBN frame
    # -------------------------------------------------------------------
    ax2 = fig.add_subplot(122)
    ax2.set_facecolor(STYLE["bg"])
    ax2.set_aspect("equal")
    ax2.grid(False)

    # Origin
    origin = (0.0, 0.0)
    ax2.plot(origin[0], origin[1], "o", color=STYLE["text"], markersize=7, zorder=10)

    # Basis vectors — T right, B into-page (oblique), N up
    axis_scale = 2.0

    # T along +X (tangent → U direction)
    draw_vector(
        ax2,
        origin,
        (axis_scale, 0),
        STYLE["accent1"],
        "T (along U)",
        label_offset=(0.0, -0.3),
        lw=3,
    )

    # B drawn at ~35° to suggest depth (V direction, going "into" the surface)
    b_angle = np.radians(35)
    b_vec = (axis_scale * np.cos(b_angle) * -0.6, axis_scale * np.sin(b_angle))
    draw_vector(
        ax2,
        origin,
        b_vec,
        STYLE["accent4"],
        "B (along V)",
        label_offset=(-0.95, -0.1),
        lw=3,
    )

    # N along +Y (surface normal — straight up)
    draw_vector(
        ax2,
        origin,
        (0, axis_scale),
        STYLE["accent3"],
        "N = (0, 0, 1)",
        label_offset=(-0.7, 0.05),
        lw=3,
    )

    # Faint flat surface parallelogram at origin
    flat_pts = [
        (-1.0, -0.05),
        (axis_scale + 0.3, -0.05),
        (axis_scale + 0.3 + b_vec[0] * 0.45, b_vec[1] * 0.45 - 0.05),
        (-1.0 + b_vec[0] * 0.45, b_vec[1] * 0.45 - 0.05),
    ]
    flat_surf = Polygon(
        flat_pts,
        closed=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["axis"],
        linewidth=1,
        alpha=0.3,
        zorder=0,
    )
    ax2.add_patch(flat_surf)

    # Perturbed normal (from normal map sample) — tilted toward T
    perturbed_ts = np.array([0.35, 0.1, 0.93])
    perturbed_ts = perturbed_ts / np.linalg.norm(perturbed_ts)
    # Project to 2D: x-component → right, z-component → up
    p2d = (perturbed_ts[0] * axis_scale, perturbed_ts[2] * axis_scale)
    draw_vector(
        ax2,
        origin,
        p2d,
        STYLE["accent2"],
        "sampled N'",
        label_offset=(0.2, 0.15),
        lw=2,
    )

    # Short dotted arc from N to N'
    n_end = (0, axis_scale)
    for t_val in np.linspace(0.25, 0.75, 10):
        px = n_end[0] * (1 - t_val) + p2d[0] * t_val
        py = n_end[1] * (1 - t_val) + p2d[1] * t_val
        ax2.plot(px, py, ".", color=STYLE["warn"], markersize=3, alpha=0.7, zorder=5)

    # Annotation about flat normal
    ax2.text(
        0.5,
        -0.8,
        "Normal map (0.5, 0.5, 1.0) decodes to (0, 0, 1)",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax2.text(
        0.5,
        -1.15,
        "= unperturbed surface normal",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Small annotation for the sampled N'
    ax2.text(
        1.6,
        1.7,
        "bump tilts N\ntoward T",
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        va="center",
        fontstyle="italic",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax2.set_xlim(-2.0, 3.2)
    ax2.set_ylim(-1.5, 2.8)
    ax2.set_xticks([])
    ax2.set_yticks([])
    for spine in ax2.spines.values():
        spine.set_visible(False)

    ax2.set_title(
        "Tangent Space (per vertex)",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=10,
    )

    # -------------------------------------------------------------------
    # Arrow between panels
    # -------------------------------------------------------------------
    fig.text(
        0.50,
        0.52,
        "\u2190 TBN \u2192",
        color=STYLE["warn"],
        fontsize=16,
        ha="center",
        va="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    fig.text(
        0.50,
        0.46,
        "matrix transforms\nbetween spaces",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    fig.suptitle(
        "Tangent Space: Per-Vertex Coordinate Frame for Normal Mapping",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    fig.tight_layout(rect=(0, 0.02, 1, 0.93))
    save(fig, "gpu/17-normal-maps", "tangent_space.png")


# ---------------------------------------------------------------------------
# gpu/17-normal-maps — lengyel_tangent_basis.png
# ---------------------------------------------------------------------------


def diagram_lengyel_tangent_basis():
    """Eric Lengyel's method: computing tangent and bitangent from triangle
    edges and UV coordinates.

    Two-panel diagram.  Left shows a triangle in position space with edge
    vectors e1, e2 and the resulting tangent (T) and bitangent (B) vectors.
    Right shows the same triangle in UV space with the UV deltas that drive
    the computation.  An annotation below presents the matrix equation that
    ties the two spaces together.
    """
    fig = plt.figure(figsize=(11, 7), facecolor=STYLE["bg"])

    # -----------------------------------------------------------------------
    # Triangle geometry (chosen for a clear, readable layout)
    # -----------------------------------------------------------------------
    # Position-space triangle
    P0 = np.array([0.0, 0.0])
    P1 = np.array([3.5, 0.6])
    P2 = np.array([1.0, 3.0])
    e1 = P1 - P0
    e2 = P2 - P0

    # UV coordinates — chosen so T points roughly right, B points roughly up
    uv0 = np.array([0.1, 0.1])
    uv1 = np.array([0.9, 0.2])
    uv2 = np.array([0.2, 0.9])
    du1, dv1 = uv1[0] - uv0[0], uv1[1] - uv0[1]
    du2, dv2 = uv2[0] - uv0[0], uv2[1] - uv0[1]

    # Solve for T and B  (the actual Lengyel computation)
    det = du1 * dv2 - du2 * dv1
    inv_det = 1.0 / det
    T = inv_det * (dv2 * e1 - dv1 * e2)
    B = inv_det * (-du2 * e1 + du1 * e2)
    T_hat = T / np.linalg.norm(T)
    B_hat = B / np.linalg.norm(B)

    # -----------------------------------------------------------------------
    # Left panel — Position space
    # -----------------------------------------------------------------------
    ax1 = fig.add_subplot(121)
    ax1.set_facecolor(STYLE["bg"])
    ax1.set_aspect("equal")
    ax1.grid(False)

    # Filled triangle
    tri = Polygon(
        [P0, P1, P2],
        closed=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["axis"],
        linewidth=1.5,
        alpha=0.6,
        zorder=1,
    )
    ax1.add_patch(tri)

    # Edge vectors e1, e2 from P0
    draw_vector(ax1, P0, e1, STYLE["accent1"], "e\u2081", label_offset=(0.1, -0.4))
    draw_vector(ax1, P0, e2, STYLE["accent2"], "e\u2082", label_offset=(-0.5, 0.1))

    # Resulting T and B vectors (scaled for visibility)
    tb_scale = 1.6
    draw_vector(
        ax1,
        P0,
        T_hat * tb_scale,
        STYLE["accent3"],
        "T",
        label_offset=(0.1, -0.35),
        lw=3,
    )
    draw_vector(
        ax1,
        P0,
        B_hat * tb_scale,
        STYLE["accent4"],
        "B",
        label_offset=(-0.4, 0.1),
        lw=3,
    )

    # Vertex labels
    vert_data = [
        (P0, "P\u2080", (-0.35, -0.35)),
        (P1, "P\u2081", (0.15, -0.3)),
        (P2, "P\u2082", (-0.35, 0.15)),
    ]
    for pt, label, off in vert_data:
        ax1.plot(pt[0], pt[1], "o", color=STYLE["text"], markersize=7, zorder=10)
        ax1.text(
            pt[0] + off[0],
            pt[1] + off[1],
            label,
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    ax1.set_xlim(-1.2, 4.5)
    ax1.set_ylim(-1.0, 4.0)
    ax1.set_xticks([])
    ax1.set_yticks([])
    for spine in ax1.spines.values():
        spine.set_visible(False)

    ax1.set_title(
        "Position Space",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=10,
    )

    # Legend below left panel
    legend_items = [
        (STYLE["accent1"], "e\u2081 = P\u2081 \u2212 P\u2080  (edge 1)"),
        (STYLE["accent2"], "e\u2082 = P\u2082 \u2212 P\u2080  (edge 2)"),
        (STYLE["accent3"], "T = tangent  (aligns with U)"),
        (STYLE["accent4"], "B = bitangent  (aligns with V)"),
    ]
    for i, (color, text) in enumerate(legend_items):
        ax1.text(
            -1.0,
            -0.5 - i * 0.45,
            text,
            color=color,
            fontsize=9,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # -----------------------------------------------------------------------
    # Right panel — UV / Texture space
    # -----------------------------------------------------------------------
    ax2 = fig.add_subplot(122)
    ax2.set_facecolor(STYLE["bg"])
    ax2.set_aspect("equal")
    ax2.grid(False)

    # Faint unit square boundary
    sq = Rectangle(
        (0, 0),
        1,
        1,
        facecolor="none",
        edgecolor=STYLE["grid"],
        linewidth=1,
        linestyle="--",
        alpha=0.5,
        zorder=0,
    )
    ax2.add_patch(sq)

    # Checkerboard inside the unit square (subtle)
    for ci in range(4):
        for cj in range(4):
            shade = STYLE["surface"] if (ci + cj) % 2 == 0 else STYLE["bg"]
            r = Rectangle(
                (ci / 4, cj / 4),
                0.25,
                0.25,
                facecolor=shade,
                edgecolor=STYLE["grid"],
                linewidth=0.3,
                alpha=0.3,
                zorder=0,
            )
            ax2.add_patch(r)

    # Filled UV triangle
    uv_tri = Polygon(
        [uv0, uv1, uv2],
        closed=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["axis"],
        linewidth=1.5,
        alpha=0.6,
        zorder=1,
    )
    ax2.add_patch(uv_tri)

    # UV delta vectors from uv0
    duv1 = uv1 - uv0
    duv2 = uv2 - uv0
    draw_vector(
        ax2, uv0, duv1, STYLE["accent1"], "\u0394uv\u2081", label_offset=(0.0, -0.08)
    )
    draw_vector(
        ax2, uv0, duv2, STYLE["accent2"], "\u0394uv\u2082", label_offset=(-0.12, 0.04)
    )

    # Vertex labels with UV coords
    uv_vert_data = [
        (uv0, "uv\u2080", (-0.02, 0.06)),
        (uv1, "uv\u2081", (0.04, 0.04)),
        (uv2, "uv\u2082", (-0.02, -0.1)),
    ]
    for pt, label, off in uv_vert_data:
        ax2.plot(pt[0], pt[1], "o", color=STYLE["text"], markersize=7, zorder=10)
        ax2.text(
            pt[0] + off[0],
            pt[1] + off[1],
            label,
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            ha="center",
            path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
        )

    ax2.set_xlim(-0.15, 1.15)
    ax2.set_ylim(-0.15, 1.15)
    ax2.set_xlabel("U \u2192", color=STYLE["axis"], fontsize=11)
    ax2.set_ylabel("V \u2192", color=STYLE["axis"], fontsize=11)
    ax2.tick_params(colors=STYLE["axis"], labelsize=8)
    for spine in ax2.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)

    ax2.set_title(
        "UV / Texture Space",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        pad=10,
    )

    # Show UV coordinate values
    uv_legend = [
        f"\u0394u\u2081={du1:+.1f}   \u0394v\u2081={dv1:+.1f}",
        f"\u0394u\u2082={du2:+.1f}   \u0394v\u2082={dv2:+.1f}",
    ]
    uv_colors = [STYLE["accent1"], STYLE["accent2"]]
    for i, (text, color) in enumerate(zip(uv_legend, uv_colors)):
        ax2.text(
            0.0,
            -0.07 - i * 0.07,
            text,
            color=color,
            fontsize=9,
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # -----------------------------------------------------------------------
    # Matrix equation annotation across the bottom
    # -----------------------------------------------------------------------
    eq_lines = [
        "T = (\u0394v\u2082\u00b7e\u2081 \u2212 \u0394v\u2081\u00b7e\u2082) / (\u0394u\u2081\u0394v\u2082 \u2212 \u0394u\u2082\u0394v\u2081)",
        "B = (\u0394u\u2081\u00b7e\u2082 \u2212 \u0394u\u2082\u00b7e\u2081) / (\u0394u\u2081\u0394v\u2082 \u2212 \u0394u\u2082\u0394v\u2081)",
    ]
    eq_colors = [STYLE["accent3"], STYLE["accent4"]]
    eq_y_start = 0.08
    eq_spacing = 0.035
    for i, (line, color) in enumerate(zip(eq_lines, eq_colors)):
        fig.text(
            0.50,
            eq_y_start - i * eq_spacing,
            line,
            color=color,
            fontsize=10,
            ha="center",
            va="center",
            fontfamily="monospace",
            fontweight="bold",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    fig.suptitle(
        "Eric Lengyel's Tangent Basis from Edge Vectors & UVs",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.98,
    )

    fig.tight_layout(rect=(0, 0.15, 1, 0.94))
    save(fig, "gpu/17-normal-maps", "lengyel_tangent_basis.png")


# ---------------------------------------------------------------------------
# gpu/20-linear-fog — fog_falloff_curves.png
# ---------------------------------------------------------------------------


def diagram_fog_falloff_curves():
    """Three fog falloff modes: linear, exponential, exponential-squared."""
    fig, ax = plt.subplots(figsize=(10, 5), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(0, 25), ylim=(-0.05, 1.1), grid=True, aspect=None)

    d = np.linspace(0, 25, 500)

    # Linear fog: ramps from 1 at start to 0 at end
    start, end = 2.0, 18.0
    linear = np.clip((end - d) / (end - start), 0.0, 1.0)

    # Exponential: e^(-density * d)
    exp_density = 0.12
    exponential = np.clip(np.exp(-exp_density * d), 0.0, 1.0)

    # Exp-squared: e^(-(density * d)^2)
    exp2_density = 0.08
    exp_squared = np.clip(np.exp(-((exp2_density * d) ** 2)), 0.0, 1.0)

    ax.plot(d, linear, color=STYLE["accent1"], lw=2.5, label="Linear")
    ax.plot(d, exponential, color=STYLE["accent2"], lw=2.5, label="Exponential")
    ax.plot(d, exp_squared, color=STYLE["accent3"], lw=2.5, label="Exp-squared")

    # Annotate regions
    ax.axhspan(0.95, 1.1, color=STYLE["accent1"], alpha=0.06)
    ax.text(
        1.0,
        1.05,
        "Fully visible",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
        va="center",
    )
    ax.axhspan(-0.05, 0.05, color=STYLE["accent2"], alpha=0.06)
    ax.text(
        20,
        -0.02,
        "Fully fogged",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
        va="center",
    )

    # Mark linear start/end
    ax.axvline(start, color=STYLE["accent1"], ls="--", lw=0.8, alpha=0.5)
    ax.axvline(end, color=STYLE["accent1"], ls="--", lw=0.8, alpha=0.5)
    ax.text(
        start,
        -0.02,
        f"start={start:.0f}",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="top",
    )
    ax.text(
        end,
        -0.02,
        f"end={end:.0f}",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="top",
    )

    ax.set_xlabel("Distance from camera", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("Fog factor (visibility)", color=STYLE["text"], fontsize=11)

    leg = ax.legend(
        loc="upper right", fontsize=10, framealpha=0.3, edgecolor=STYLE["grid"]
    )
    for text in leg.get_texts():
        text.set_color(STYLE["text"])

    fig.suptitle(
        "Fog Falloff Modes",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/20-linear-fog", "fog_falloff_curves.png")


# ---------------------------------------------------------------------------
# gpu/20-linear-fog — fog_blending.png
# ---------------------------------------------------------------------------


def diagram_fog_blending():
    """Horizontal gradient bars showing fog blending for each mode."""
    fig, axes = plt.subplots(3, 1, figsize=(10, 4), facecolor=STYLE["bg"])

    d = np.linspace(0, 25, 256)

    # Surface color (warm truck-like tone) and fog color (medium gray)
    surface = np.array([0.75, 0.60, 0.22])
    fog_col = np.array([0.5, 0.5, 0.5])

    modes = [
        ("Linear", np.clip((18.0 - d) / (18.0 - 2.0), 0.0, 1.0)),
        ("Exponential", np.clip(np.exp(-0.12 * d), 0.0, 1.0)),
        ("Exp-squared", np.clip(np.exp(-((0.08 * d) ** 2)), 0.0, 1.0)),
    ]
    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]

    for ax, (name, factor), col in zip(axes, modes, colors):
        # Build a 2D image: 1 row x 256 columns x 3 channels
        gradient = np.zeros((1, 256, 3))
        for i in range(256):
            f = factor[i]
            gradient[0, i] = f * surface + (1.0 - f) * fog_col

        ax.imshow(gradient, aspect="auto", extent=[0, 25, 0, 1])
        ax.set_facecolor(STYLE["bg"])
        ax.set_yticks([])
        ax.set_xlim(0, 25)
        ax.tick_params(colors=STYLE["axis"], labelsize=8)
        for spine in ax.spines.values():
            spine.set_color(STYLE["grid"])
            spine.set_linewidth(0.5)
        ax.set_ylabel(
            name,
            color=col,
            fontsize=10,
            fontweight="bold",
            rotation=0,
            labelpad=80,
            va="center",
        )

    axes[0].set_title(
        "Surface Color \u2192 Fog Color Blend over Distance",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    axes[-1].set_xlabel("Distance from camera", color=STYLE["text"], fontsize=10)

    # Labels
    axes[0].text(
        0.5,
        0.5,
        "Near\n(visible)",
        color=STYLE["text"],
        fontsize=8,
        ha="left",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    axes[0].text(
        24,
        0.5,
        "Far\n(fogged)",
        color=STYLE["text"],
        fontsize=8,
        ha="right",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    fig.tight_layout()
    save(fig, "gpu/20-linear-fog", "fog_blending.png")


# ---------------------------------------------------------------------------
# gpu/20-linear-fog — fog_scene_layout.png
# ---------------------------------------------------------------------------


def diagram_fog_scene_layout():
    """Top-down view of the scene with camera, truck, boxes, and fog gradient."""
    fig, ax = plt.subplots(figsize=(10, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-10, 10), ylim=(-10, 10), grid=False, aspect="equal")

    # Fog gradient background (radial — denser farther from center)
    for r_i in range(50):
        radius = 10.0 - r_i * 0.2
        alpha = 0.02 + 0.15 * (r_i / 50.0)
        circle = Circle(
            (0, 0), radius, color=STYLE["text"], alpha=alpha, fill=True, zorder=0
        )
        ax.add_patch(circle)

    # Camera position (at -6, 6 in world XZ)
    ax.plot(-6, 6, "^", color=STYLE["warn"], markersize=14, zorder=5)
    ax.text(
        -6,
        7.2,
        "Camera",
        color=STYLE["warn"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Truck at origin
    truck_rect = Rectangle((-1.2, -0.6), 2.4, 1.2, color="#cc6633", alpha=0.9, zorder=4)
    ax.add_patch(truck_rect)
    ax.text(
        0,
        0,
        "Truck",
        color=STYLE["bg"],
        fontsize=9,
        ha="center",
        va="center",
        fontweight="bold",
        zorder=5,
    )

    # 8 ground boxes in a ring at radius 5
    box_radius = 5.0
    box_color = "#b8860b"
    for i in range(8):
        angle = i * (2.0 * np.pi / 8.0)
        bx = np.cos(angle) * box_radius
        bz = np.sin(angle) * box_radius
        box = Rectangle(
            (bx - 0.5, bz - 0.5), 1.0, 1.0, color=box_color, alpha=0.8, zorder=4
        )
        ax.add_patch(box)

        # Mark stacked boxes (every other)
        if i % 2 == 0:
            stack = Rectangle(
                (bx - 0.4, bz - 0.4),
                0.8,
                0.8,
                color="#daa520",
                alpha=0.6,
                zorder=4,
                linestyle="--",
                linewidth=1.5,
                edgecolor=STYLE["text"],
                fill=False,
            )
            ax.add_patch(stack)

    # Ring radius annotation
    ax.annotate(
        "",
        xy=(box_radius, 0),
        xytext=(0, 0),
        arrowprops={"arrowstyle": "<->", "color": STYLE["accent1"], "lw": 1.5},
        zorder=6,
    )
    ax.text(
        2.5,
        -0.7,
        "r = 5",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Distance from camera to nearest/farthest objects
    ax.annotate(
        "",
        xy=(0, 0),
        xytext=(-6, 6),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["accent2"],
            "lw": 1.2,
            "ls": "--",
        },
        zorder=6,
    )
    ax.text(
        -4.0,
        3.5,
        "d \u2248 8.5",
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Legend
    ax.text(
        7.5,
        -8.5,
        "Fog density\nincreases\nwith distance",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax.set_xlabel("X (world)", color=STYLE["text"], fontsize=10)
    ax.set_ylabel("Z (world)", color=STYLE["text"], fontsize=10)

    fig.suptitle(
        "Scene Layout \u2014 Top-Down View",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/20-linear-fog", "fog_scene_layout.png")


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — hdr_pipeline.png
# ---------------------------------------------------------------------------


def diagram_hdr_pipeline():
    """Two-pass HDR rendering pipeline with sample pixel values."""
    fig, ax = plt.subplots(figsize=(12, 5), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12.5), ylim=(-2.5, 3.5), grid=False, aspect=None)
    ax.axis("off")

    # Pipeline stages as boxes
    stages = [
        (0.5, 0, 2.5, 2, "Scene\nShaders", "Blinn-Phong\nlighting"),
        (4.0, 0, 2.5, 2, "HDR Buffer", "R16G16B16A16\nFLOAT"),
        (7.5, 0, 2.5, 2, "Tone Map\nPass", "Reinhard\nor ACES"),
        (11.0, 0, 1.5, 2, "sRGB\nSwapchain", "Auto\ngamma"),
    ]

    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"], STYLE["accent4"]]

    for (x, y, w, h, title, subtitle), color in zip(stages, colors):
        rect = Rectangle(
            (x, y),
            w,
            h,
            linewidth=2,
            edgecolor=color,
            facecolor=STYLE["surface"],
            alpha=0.8,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            x + w / 2,
            y + h * 0.65,
            title,
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )
        ax.text(
            x + w / 2,
            y + h * 0.25,
            subtitle,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="center",
        )

    # Arrows between stages
    arrow_props = {
        "arrowstyle": "->,head_width=0.3,head_length=0.15",
        "color": STYLE["text_dim"],
        "lw": 2,
    }
    ax.annotate("", xy=(4.0, 1), xytext=(3.0, 1), arrowprops=arrow_props)
    ax.annotate("", xy=(7.5, 1), xytext=(6.5, 1), arrowprops=arrow_props)
    ax.annotate("", xy=(11.0, 1), xytext=(10.0, 1), arrowprops=arrow_props)

    # Sample pixel values below each stage
    samples = [
        (1.75, -0.7, "(0.8, 0.3, 0.1)\nSpecular: 4.2"),
        (5.25, -0.7, "Stored as-is:\n(0.8, 0.3, 0.1)\n(4.2, 4.2, 4.2)"),
        (8.75, -0.7, "Compressed:\n(0.57, 0.23, 0.09)\n(0.81, 0.81, 0.81)"),
        (11.75, -0.7, "Gamma\napplied"),
    ]
    for x, y, text in samples:
        ax.text(
            x,
            y,
            text,
            color=STYLE["warn"],
            fontsize=7.5,
            ha="center",
            va="top",
            family="monospace",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    fig.suptitle(
        "HDR Rendering Pipeline",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "gpu/21-hdr-tone-mapping", "hdr_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — ldr_clipping.png
# ---------------------------------------------------------------------------


def diagram_ldr_clipping():
    """Show how LDR clamps values above 1.0, losing highlight detail."""
    fig, axes = plt.subplots(1, 2, figsize=(10, 4), facecolor=STYLE["bg"])

    for ax in axes:
        setup_axes(ax, grid=False, aspect=None)
        ax.set_xlim(-0.2, 5.5)
        ax.set_ylim(-0.1, 1.4)

    # Generate a gradient of HDR values from 0 to 5
    x = np.linspace(0, 5, 200)

    # Left: LDR (clamped)
    ax = axes[0]
    ldr = np.clip(x, 0, 1)
    ax.fill_between(x, 0, ldr, color=STYLE["accent1"], alpha=0.3)
    ax.plot(x, ldr, color=STYLE["accent1"], lw=2.5, label="LDR output")
    ax.axhline(y=1.0, color=STYLE["warn"], lw=1, ls="--", alpha=0.7)
    ax.fill_between(x[x > 1], 1, 1.3, color="#ff3333", alpha=0.15)
    ax.text(
        3.0,
        1.15,
        "Lost detail",
        color="#ff6666",
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.set_xlabel("HDR input value", color=STYLE["text"], fontsize=10)
    ax.set_ylabel("Output value", color=STYLE["text"], fontsize=10)
    ax.set_title(
        "LDR (clamp to 1.0)", color=STYLE["text"], fontsize=12, fontweight="bold"
    )

    # Right: HDR with tone mapping (Reinhard)
    ax = axes[1]
    reinhard = x / (x + 1)
    ax.fill_between(x, 0, reinhard, color=STYLE["accent3"], alpha=0.3)
    ax.plot(x, reinhard, color=STYLE["accent3"], lw=2.5, label="Reinhard")
    ax.axhline(y=1.0, color=STYLE["warn"], lw=1, ls="--", alpha=0.7)

    # Show that different HDR values produce different outputs
    for hdr_val in [1.0, 2.0, 3.0, 4.0]:
        mapped = hdr_val / (hdr_val + 1)
        ax.plot(
            [hdr_val, hdr_val],
            [0, mapped],
            "--",
            color=STYLE["text_dim"],
            lw=0.8,
            alpha=0.5,
        )
        ax.plot(hdr_val, mapped, "o", color=STYLE["accent2"], markersize=5)
    ax.text(
        3.5,
        0.6,
        "Detail\npreserved",
        color=STYLE["accent3"],
        fontsize=10,
        ha="center",
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.set_xlabel("HDR input value", color=STYLE["text"], fontsize=10)
    ax.set_ylabel("Output value", color=STYLE["text"], fontsize=10)
    ax.set_title(
        "HDR + Tone Mapping (Reinhard)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
    )

    fig.suptitle(
        "Why Tone Mapping Matters",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.0,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "gpu/21-hdr-tone-mapping", "ldr_clipping.png")


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — tone_map_comparison.png
# ---------------------------------------------------------------------------


def diagram_tone_map_comparison():
    """Compare Reinhard, ACES, and linear clamp tone mapping operators."""
    fig, ax = plt.subplots(figsize=(8, 5), facecolor=STYLE["bg"])
    setup_axes(ax, grid=True, aspect=None)

    x = np.linspace(0, 8, 400)

    # Linear clamp
    clamp = np.clip(x, 0, 1)
    ax.plot(
        x,
        clamp,
        color=STYLE["text_dim"],
        lw=2,
        ls="--",
        label="Linear clamp",
        alpha=0.8,
    )

    # Reinhard: x / (x + 1)
    reinhard = x / (x + 1)
    ax.plot(x, reinhard, color=STYLE["accent1"], lw=2.5, label="Reinhard")

    # ACES (Narkowicz approximation)
    a, b, c, d, e = ACES_COEFFS
    aces = np.clip((x * (a * x + b)) / (x * (c * x + d) + e), 0, 1)
    ax.plot(x, aces, color=STYLE["accent2"], lw=2.5, label="ACES filmic")

    ax.set_xlim(0, 8)
    ax.set_ylim(0, 1.1)
    ax.set_xlabel("HDR input value", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("LDR output value", color=STYLE["text"], fontsize=11)

    # Annotations
    ax.annotate(
        "Highlights\ncompressed",
        xy=(4, reinhard[np.searchsorted(x, 4)]),
        xytext=(5.5, 0.55),
        color=STYLE["accent1"],
        fontsize=9,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 1.5},
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.annotate(
        "S-curve\ncontrast",
        xy=(0.5, aces[np.searchsorted(x, 0.5)]),
        xytext=(2.0, 0.15),
        color=STYLE["accent2"],
        fontsize=9,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 1.5},
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    legend = ax.legend(
        loc="lower right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
    )
    for text in legend.get_texts():
        text.set_color(STYLE["text"])

    fig.suptitle(
        "Tone Mapping Operators",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/21-hdr-tone-mapping", "tone_map_comparison.png")


# ---------------------------------------------------------------------------
# gpu/21-hdr-tone-mapping — exposure_effect.png
# ---------------------------------------------------------------------------


def diagram_exposure_effect():
    """Show how exposure multiplier shifts the tone curve."""
    fig, ax = plt.subplots(figsize=(8, 5), facecolor=STYLE["bg"])
    setup_axes(ax, grid=True, aspect=None)

    x = np.linspace(0, 5, 300)

    # ACES at different exposures
    exposures = [0.5, 1.0, 2.0, 4.0]
    colors_list = [STYLE["accent4"], STYLE["accent1"], STYLE["accent2"], STYLE["warn"]]

    a, b, c, d, e = ACES_COEFFS

    for exp, col in zip(exposures, colors_list):
        hdr = x * exp
        aces = np.clip((hdr * (a * hdr + b)) / (hdr * (c * hdr + d) + e), 0, 1)
        ax.plot(x, aces, color=col, lw=2.5, label=f"Exposure {exp:.1f}")

    ax.set_xlim(0, 5)
    ax.set_ylim(0, 1.1)
    ax.set_xlabel("Scene HDR value", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("Output after ACES", color=STYLE["text"], fontsize=11)

    # Annotations
    ax.text(
        1.0,
        0.95,
        "\u2191 Brighter",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax.text(
        3.5,
        0.25,
        "\u2193 Darker",
        color=STYLE["accent4"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    legend = ax.legend(
        loc="center right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
    )
    for text in legend.get_texts():
        text.set_color(STYLE["text"])

    fig.suptitle(
        "Exposure Control \u2014 Pre-Tone-Map Brightness",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/21-hdr-tone-mapping", "exposure_effect.png")


# ---------------------------------------------------------------------------
# gpu/22-bloom — Jimenez dual-filter bloom diagrams
# ---------------------------------------------------------------------------


def diagram_bloom_pipeline():
    """Full bloom render pipeline: scene → downsample chain → upsample chain → tonemap."""
    fig, ax = plt.subplots(figsize=(14, 7), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 11), ylim=(-4.5, 5), grid=False, aspect=None)
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    # --- Shared arrow style ---
    arrow_kw = {
        "arrowstyle": "->,head_width=0.25,head_length=0.12",
        "color": STYLE["text_dim"],
        "lw": 2,
    }
    green_arrow_kw = {
        "arrowstyle": "->,head_width=0.25,head_length=0.12",
        "color": STYLE["accent3"],
        "lw": 2,
    }

    # --- Row 1: Scene pass ---
    scene_box = Rectangle(
        (0.5, 3.0),
        3.0,
        1.6,
        linewidth=2,
        edgecolor=STYLE["accent1"],
        facecolor=STYLE["surface"],
        alpha=0.85,
        zorder=2,
    )
    ax.add_patch(scene_box)
    ax.text(
        2.0,
        4.05,
        "1. Scene Pass",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        2.0,
        3.4,
        "Grid + Models + Emissive\n\u2192 HDR target (CLEAR)",
        color=STYLE["text_dim"],
        fontsize=8.5,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Arrow from scene to downsample
    ax.annotate("", xy=(2.0, 2.2), xytext=(2.0, 2.9), arrowprops=arrow_kw)

    # --- Row 2: Downsample chain (left to right, shrinking boxes) ---
    ax.text(
        0.2,
        2.05,
        "2. Downsample (5 passes)",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # HDR source box
    hdr_x, hdr_y, hdr_w, hdr_h = 0.3, 0.0, 1.8, 1.6
    hdr_box = Rectangle(
        (hdr_x, hdr_y),
        hdr_w,
        hdr_h,
        linewidth=2,
        edgecolor=STYLE["accent1"],
        facecolor=STYLE["surface"],
        alpha=0.7,
        zorder=2,
    )
    ax.add_patch(hdr_box)
    ax.text(
        hdr_x + hdr_w / 2,
        hdr_y + hdr_h * 0.6,
        "HDR",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        hdr_x + hdr_w / 2,
        hdr_y + hdr_h * 0.25,
        "1280\u00d7720",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    mip_sizes = [
        ("Mip 0", "640\u00d7360"),
        ("Mip 1", "320\u00d7180"),
        ("Mip 2", "160\u00d790"),
        ("Mip 3", "80\u00d745"),
        ("Mip 4", "40\u00d722"),
    ]
    mip_widths = [1.5, 1.3, 1.1, 0.9, 0.8]
    mip_heights = [1.4, 1.2, 1.0, 0.8, 0.7]
    mip_x_starts = []
    cx = hdr_x + hdr_w + 0.6
    for i, ((label, size), mw, mh) in enumerate(
        zip(mip_sizes, mip_widths, mip_heights)
    ):
        my = hdr_y + (hdr_h - mh) / 2  # vertically center
        rect = Rectangle(
            (cx, my),
            mw,
            mh,
            linewidth=1.5,
            edgecolor=STYLE["accent2"],
            facecolor=STYLE["surface"],
            alpha=0.7,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            cx + mw / 2,
            my + mh * 0.6,
            label,
            color=STYLE["accent2"],
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            cx + mw / 2,
            my + mh * 0.25,
            size,
            color=STYLE["text_dim"],
            fontsize=6.5,
            ha="center",
            va="center",
            path_effects=stroke,
        )

        # Arrow from previous box
        if i == 0:
            ax.annotate(
                "",
                xy=(cx, hdr_y + hdr_h / 2),
                xytext=(hdr_x + hdr_w, hdr_y + hdr_h / 2),
                arrowprops=arrow_kw,
            )
        else:
            prev_x = mip_x_starts[-1] + mip_widths[i - 1]
            ax.annotate(
                "",
                xy=(cx, hdr_y + hdr_h / 2),
                xytext=(prev_x, hdr_y + hdr_h / 2),
                arrowprops=arrow_kw,
            )

        # First pass annotation
        if i == 0:
            ax.text(
                cx + mw / 2,
                my - 0.3,
                "Threshold\n+ Karis",
                color=STYLE["warn"],
                fontsize=7,
                fontweight="bold",
                ha="center",
                va="top",
                path_effects=stroke,
            )

        mip_x_starts.append(cx)
        cx += mw + 0.4

    # --- Row 3: Upsample chain (right to left, aligned below downsample) ---
    # Each upsample mip sits directly below its downsample counterpart.
    # Arrows flow right-to-left: Mip 4 → Mip 3 → ... → Mip 0.
    ax.text(
        0.2,
        -1.2,
        "3. Upsample (4 passes, additive blend)",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
    )

    us_y_base = -3.6
    us_positions = []  # (x, y, w, h) indexed by mip level 0..4
    for i, ((label, size), mw, mh) in enumerate(
        zip(mip_sizes, mip_widths, mip_heights)
    ):
        ux = mip_x_starts[i]
        uy = us_y_base + (hdr_h - mh) / 2
        rect = Rectangle(
            (ux, uy),
            mw,
            mh,
            linewidth=1.5,
            edgecolor=STYLE["accent3"],
            facecolor=STYLE["surface"],
            alpha=0.7,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            ux + mw / 2,
            uy + mh * 0.6,
            label,
            color=STYLE["accent3"],
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            ux + mw / 2,
            uy + mh * 0.25,
            size,
            color=STYLE["text_dim"],
            fontsize=6.5,
            ha="center",
            va="center",
            path_effects=stroke,
        )
        us_positions.append((ux, uy, mw, mh))

    # Arrows between upsample mips (right to left: Mip 4 → Mip 3 → ... → Mip 0)
    us_mid_y = us_y_base + hdr_h / 2
    for i in range(4, 0, -1):
        src_x = us_positions[i][0]
        dst_x, _, dst_w, _ = us_positions[i - 1]
        ax.annotate(
            "",
            xy=(dst_x + dst_w, us_mid_y),
            xytext=(src_x, us_mid_y),
            arrowprops=green_arrow_kw,
        )
        # "+" label between each pair
        gap_mid = (src_x + dst_x + dst_w) / 2
        ax.text(
            gap_mid,
            us_mid_y + 0.25,
            "+",
            color=STYLE["accent3"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Straight vertical arrow from downsample Mip 4 to upsample Mip 4
    mip4_cx = mip_x_starts[4] + mip_widths[4] / 2
    ds_mip4_bottom = hdr_y + (hdr_h - mip_heights[4]) / 2
    us_mip4_top = us_positions[4][1] + us_positions[4][3]
    ax.annotate(
        "",
        xy=(mip4_cx, us_mip4_top),
        xytext=(mip4_cx, ds_mip4_bottom),
        arrowprops=green_arrow_kw,
    )

    # --- Tone Map pass (below HDR, at the left end of the upsample row) ---
    tm_w, tm_h = 2.0, 1.4
    tm_x = hdr_x
    tm_y = us_y_base + (hdr_h - tm_h) / 2
    tm_box = Rectangle(
        (tm_x, tm_y),
        tm_w,
        tm_h,
        linewidth=2,
        edgecolor=STYLE["accent4"],
        facecolor=STYLE["surface"],
        alpha=0.85,
        zorder=2,
    )
    ax.add_patch(tm_box)
    ax.text(
        tm_x + tm_w / 2,
        tm_y + tm_h * 0.7,
        "4. Tone Map",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        tm_x + tm_w / 2,
        tm_y + tm_h * 0.3,
        "HDR + Bloom \u00d7 intensity\n\u2192 swapchain",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Arrow from upsample Mip 0 to Tone Map
    us_mip0_x = us_positions[0][0]
    ax.annotate(
        "",
        xy=(tm_x + tm_w, us_mid_y),
        xytext=(us_mip0_x, us_mid_y),
        arrowprops=green_arrow_kw,
    )

    # Straight dashed arrow from HDR down to Tone Map
    hdr_cx = hdr_x + hdr_w / 2
    ax.annotate(
        "",
        xy=(hdr_cx, tm_y + tm_h),
        xytext=(hdr_cx, hdr_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.12",
            "color": STYLE["accent1"],
            "lw": 1.5,
            "linestyle": "--",
        },
    )
    ax.text(
        hdr_cx - 0.15,
        -0.5,
        "HDR input",
        color=STYLE["accent1"],
        fontsize=7.5,
        fontstyle="italic",
        ha="right",
        va="center",
        path_effects=stroke,
    )

    fig.suptitle(
        "Bloom Pipeline \u2014 Jimenez Dual-Filter Method",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/22-bloom", "bloom_pipeline.png")


def diagram_downsample_13tap():
    """13-tap weighted downsample with 5 overlapping 2x2 box regions."""
    fig = plt.figure(figsize=(12, 6), facecolor=STYLE["bg"])

    # Left panel: sample positions on a 5x5 grid
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-0.8, 4.8), ylim=(-0.8, 4.8), grid=False)

    # Background grid
    for i in range(5):
        ax1.axhline(i, color=STYLE["grid"], lw=0.5, alpha=0.4)
        ax1.axvline(i, color=STYLE["grid"], lw=0.5, alpha=0.4)

    # 13 sample positions (mapped to 0..4 grid coordinates)
    # The shader samples at: (-2,-2), (0,-2), (2,-2), (-1,-1), (1,-1),
    # (-2,0), (0,0), (2,0), (-1,1), (1,1), (-2,2), (0,2), (2,2)
    # Map to grid: offset by 2 so center is at (2,2)
    samples = {
        "a": (0, 4),
        "b": (2, 4),
        "c": (4, 4),
        "d": (1, 3),
        "e": (3, 3),
        "f": (0, 2),
        "g": (2, 2),
        "h": (4, 2),
        "i": (1, 1),
        "j": (3, 1),
        "k": (0, 0),
        "l": (2, 0),
        "m": (4, 0),
    }

    # Draw all sample points
    for name, (sx, sy) in samples.items():
        ax1.plot(
            sx,
            sy,
            "o",
            color=STYLE["text"],
            markersize=10,
            zorder=5,
        )
        ax1.text(
            sx,
            sy + 0.3,
            name,
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="bottom",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax1.set_title(
        "13 Sample Positions",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax1.set_xticks([])
    ax1.set_yticks([])

    # Right panel: the 5 overlapping boxes
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, xlim=(-0.8, 4.8), ylim=(-0.8, 4.8), grid=False)

    # Background grid
    for i in range(5):
        ax2.axhline(i, color=STYLE["grid"], lw=0.5, alpha=0.4)
        ax2.axvline(i, color=STYLE["grid"], lw=0.5, alpha=0.4)

    # Draw sample points (dimmed)
    for _name, (sx, sy) in samples.items():
        ax2.plot(
            sx,
            sy,
            "o",
            color=STYLE["text_dim"],
            markersize=6,
            zorder=5,
        )

    # 5 overlapping boxes with color coding
    boxes = [
        (
            "Top-Left",
            [(0, 2), (2, 2), (2, 4), (0, 4)],
            STYLE["accent1"],
            "a,b,f,g",
            0.125,
        ),
        (
            "Top-Right",
            [(2, 2), (4, 2), (4, 4), (2, 4)],
            STYLE["accent2"],
            "b,c,g,h",
            0.125,
        ),
        (
            "Bot-Left",
            [(0, 0), (2, 0), (2, 2), (0, 2)],
            STYLE["accent3"],
            "f,g,k,l",
            0.125,
        ),
        (
            "Bot-Right",
            [(2, 0), (4, 0), (4, 2), (2, 2)],
            STYLE["accent4"],
            "g,h,l,m",
            0.125,
        ),
        ("Center", [(1, 1), (3, 1), (3, 3), (1, 3)], STYLE["warn"], "d,e,i,j", 0.500),
    ]

    for _label, verts, color, _samp_names, _weight in boxes:
        poly = Polygon(
            verts,
            closed=True,
            facecolor=color,
            edgecolor=color,
            alpha=0.15,
            linewidth=2,
            zorder=1,
        )
        ax2.add_patch(poly)
        # Box outline
        poly_outline = Polygon(
            verts,
            closed=True,
            facecolor="none",
            edgecolor=color,
            alpha=0.7,
            linewidth=2,
            zorder=3,
        )
        ax2.add_patch(poly_outline)

    # Weight annotations below
    weight_text = (
        "Weights: corner boxes \u00d7 0.125  |  center box \u00d7 0.500  |  total = 1.0"
    )
    ax2.text(
        2.0,
        -0.6,
        weight_text,
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="top",
    )

    # Legend entries
    for ci, (label, _, color, _, weight) in enumerate(boxes):
        ax2.text(
            4.8,
            4.6 - ci * 0.35,
            f"\u25a0 {label} (\u00d7{weight:.3f})",
            color=color,
            fontsize=8,
            fontweight="bold",
            ha="right",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax2.set_title(
        "5 Overlapping 2\u00d72 Boxes",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax2.set_xticks([])
    ax2.set_yticks([])

    fig.suptitle(
        "13-Tap Downsample Filter \u2014 Jimenez (SIGGRAPH 2014)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/22-bloom", "downsample_13tap.png")


def diagram_tent_filter():
    """Tent (bilinear) filter kernel: 2D heatmap + 3D surface plot."""
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

    fig = plt.figure(figsize=(12, 5), facecolor=STYLE["bg"])

    # Kernel weights
    kernel = (
        np.array(
            [
                [1, 2, 1],
                [2, 4, 2],
                [1, 2, 1],
            ]
        )
        / 16.0
    )

    # --- Left: Annotated 2D grid ---
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, grid=False, aspect="equal")

    # Draw the kernel as colored squares
    for row in range(3):
        for col in range(3):
            w = kernel[row, col]
            # Color intensity based on weight
            intensity = w / kernel.max()
            r_bg = int(STYLE["bg"][1:3], 16)
            g_bg = int(STYLE["bg"][3:5], 16)
            b_bg = int(STYLE["bg"][5:7], 16)
            r_ac = int(STYLE["accent1"][1:3], 16)
            g_ac = int(STYLE["accent1"][3:5], 16)
            b_ac = int(STYLE["accent1"][5:7], 16)
            r = int(r_bg + (r_ac - r_bg) * intensity)
            g = int(g_bg + (g_ac - g_bg) * intensity)
            b = int(b_bg + (b_ac - b_bg) * intensity)
            cell_color = f"#{r:02x}{g:02x}{b:02x}"

            rect = Rectangle(
                (col - 0.45, 2 - row - 0.45),
                0.9,
                0.9,
                facecolor=cell_color,
                edgecolor=STYLE["accent1"],
                linewidth=1.5,
                alpha=0.85,
                zorder=2,
            )
            ax1.add_patch(rect)

            # Weight text
            frac_text = f"{int(w * 16)}/16"
            ax1.text(
                col,
                2 - row,
                frac_text,
                color=STYLE["text"],
                fontsize=13,
                fontweight="bold",
                ha="center",
                va="center",
                zorder=3,
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

    ax1.set_xlim(-0.8, 2.8)
    ax1.set_ylim(-0.8, 2.8)
    ax1.set_xticks([])
    ax1.set_yticks([])

    # Axis labels
    for ci, label in enumerate(["-1", "0", "+1"]):
        ax1.text(
            ci,
            -0.65,
            label,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="center",
        )
        ax1.text(
            -0.65,
            ci,
            label,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="center",
        )

    ax1.set_title(
        "9-Tap Tent Kernel Weights",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # --- Right: 3D surface showing the "tent" shape ---
    ax2: Axes3D = fig.add_subplot(122, projection="3d")  # type: ignore[assignment]
    ax2.set_facecolor(STYLE["bg"])

    # Higher-res tent for smooth surface
    x = np.linspace(-1, 1, 50)
    y = np.linspace(-1, 1, 50)
    X, Y = np.meshgrid(x, y)
    # 2D tent = (1 - |x|) * (1 - |y|)
    Z = np.maximum(0, 1 - np.abs(X)) * np.maximum(0, 1 - np.abs(Y))

    ax2.plot_surface(
        X,
        Y,
        Z,
        cmap=FORGE_CMAP,
        alpha=0.85,
        edgecolor="none",
        rstride=2,
        cstride=2,
    )

    # Mark the 9 sample points on the surface
    sample_coords = [
        (-1, -1),
        (0, -1),
        (1, -1),
        (-1, 0),
        (0, 0),
        (1, 0),
        (-1, 1),
        (0, 1),
        (1, 1),
    ]
    for sx, sy in sample_coords:
        sz = max(0, 1 - abs(sx)) * max(0, 1 - abs(sy))
        ax2.scatter(
            [sx],
            [sy],
            [sz + 0.02],  # type: ignore[arg-type]  # zs typed as int in stubs
            color=STYLE["text"],
            s=30,
            zorder=10,
            depthshade=False,
        )

    ax2.set_xlabel("x", color=STYLE["text_dim"], fontsize=9, labelpad=-2)
    ax2.set_ylabel("y", color=STYLE["text_dim"], fontsize=9, labelpad=-2)
    ax2.set_zlabel("Weight", color=STYLE["text_dim"], fontsize=9, labelpad=-2)
    ax2.tick_params(colors=STYLE["axis"], labelsize=7)
    ax2.set_title(
        '"Tent" Shape \u2014 Bilinear Falloff',
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )
    ax2.view_init(elev=30, azim=-50)
    # Style the 3D axes panes
    ax2.xaxis.pane.set_facecolor(STYLE["surface"])  # type: ignore[attr-defined]
    ax2.yaxis.pane.set_facecolor(STYLE["surface"])  # type: ignore[attr-defined]
    ax2.zaxis.pane.set_facecolor(STYLE["surface"])
    ax2.xaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax2.yaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax2.zaxis.pane.set_edgecolor(STYLE["grid"])

    fig.suptitle(
        "Tent Filter \u2014 9-Tap Bilinear Upsample Kernel",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/22-bloom", "tent_filter.png")


def diagram_karis_averaging():
    """Karis averaging: how 1/(1+luminance) weighting suppresses fireflies."""
    fig, (ax1, ax2) = plt.subplots(
        1,
        2,
        figsize=(12, 5),
        facecolor=STYLE["bg"],
        gridspec_kw={"width_ratios": [1, 1]},
    )

    # --- Left: Weight curve ---
    setup_axes(ax1, grid=True, aspect=None)
    luma = np.linspace(0, 20, 500)
    weight = 1.0 / (1.0 + luma)

    ax1.plot(luma, weight, color=STYLE["accent1"], lw=2.5)
    ax1.fill_between(luma, weight, alpha=0.15, color=STYLE["accent1"])

    # Mark key points
    for lv, label in [(0.5, "Dim pixel"), (2.0, "Bright"), (10.0, "Firefly")]:
        w = 1.0 / (1.0 + lv)
        ax1.plot(lv, w, "o", color=STYLE["warn"], markersize=8, zorder=5)
        ax1.annotate(
            f"{label}\nluma={lv:.1f}\nw={w:.3f}",
            xy=(lv, w),
            xytext=(lv + 1.5, w + 0.12),
            color=STYLE["warn"],
            fontsize=8,
            fontweight="bold",
            arrowprops={
                "arrowstyle": "->",
                "color": STYLE["warn"],
                "lw": 1.5,
                "connectionstyle": "arc3,rad=0.2",
            },
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax1.set_xlim(0, 20)
    ax1.set_ylim(0, 1.1)
    ax1.set_xlabel("Luminance", color=STYLE["text"], fontsize=11)
    ax1.set_ylabel("Karis weight = 1 / (1 + luma)", color=STYLE["text"], fontsize=11)
    ax1.set_title(
        "Karis Weight Function",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # --- Right: Bar chart comparing uniform vs Karis-weighted box average ---
    setup_axes(ax2, grid=True, aspect=None)

    # Scenario: one firefly pixel among normal pixels in a 2x2 box
    pixels = [0.5, 0.6, 0.4, 50.0]  # last one is a firefly
    pixel_labels = ["0.5", "0.6", "0.4", "50.0"]

    # Uniform average
    uniform_avg = np.mean(pixels)

    # Karis-weighted average
    weights_k = [1.0 / (1.0 + p) for p in pixels]
    w_sum = sum(weights_k)
    karis_avg = sum(p * w / w_sum for p, w in zip(pixels, weights_k))

    bar_x = np.arange(3)
    bar_labels = ["Uniform\naverage", "Karis\naverage", "Without\nfirefly"]
    bar_values = [uniform_avg, karis_avg, np.mean(pixels[:3])]
    bar_colors = [STYLE["accent2"], STYLE["accent1"], STYLE["accent3"]]

    bars = ax2.bar(bar_x, bar_values, color=bar_colors, width=0.6, alpha=0.85)

    # Value labels on bars
    for bar, val in zip(bars, bar_values):
        ax2.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 0.4,
            f"{val:.2f}",
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="bottom",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax2.set_xticks(bar_x)
    ax2.set_xticklabels(bar_labels, color=STYLE["text"], fontsize=9)
    ax2.set_ylim(0, max(bar_values) * 1.3)
    ax2.set_ylabel("Result", color=STYLE["text"], fontsize=11)

    # Input pixels annotation
    ax2.text(
        1.0,
        max(bar_values) * 1.2,
        f"Input pixels: {', '.join(pixel_labels)}",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax2.set_title(
        "Firefly Suppression \u2014 2\u00d72 Box",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    fig.suptitle(
        "Karis Averaging \u2014 Suppressing HDR Fireflies",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/22-bloom", "karis_averaging.png")


def diagram_mip_chain_flow():
    """Mip-to-mip data flow for downsample (pull) and upsample (push) phases."""
    fig, (ax1, ax2) = plt.subplots(
        2,
        1,
        figsize=(12, 8),
        facecolor=STYLE["bg"],
        gridspec_kw={"height_ratios": [1, 1]},
    )

    for ax in (ax1, ax2):
        setup_axes(ax, grid=False, aspect=None)
        ax.axis("off")

    # Mip levels with proportional sizing
    mip_labels = [
        "HDR\n1280\u00d7720",
        "Mip 0\n640\u00d7360",
        "Mip 1\n320\u00d7180",
        "Mip 2\n160\u00d790",
        "Mip 3\n80\u00d745",
        "Mip 4\n40\u00d722",
    ]
    mip_scales = [1.0, 0.85, 0.7, 0.55, 0.42, 0.32]

    arrow_kw = {
        "arrowstyle": "->,head_width=0.3,head_length=0.15",
        "lw": 2.5,
    }

    # --- Top: Downsample (reading from larger, writing to smaller) ---
    ax1.set_xlim(-0.5, 12.5)
    ax1.set_ylim(-1.5, 2.5)

    ds_x_positions = [0.0, 2.2, 4.2, 6.0, 7.6, 9.0]
    ds_box_widths = [s * 1.8 for s in mip_scales]
    ds_box_heights = [s * 1.6 for s in mip_scales]

    for i, (x, label, bw, bh) in enumerate(
        zip(ds_x_positions, mip_labels, ds_box_widths, ds_box_heights)
    ):
        color = STYLE["accent1"] if i == 0 else STYLE["accent2"]
        y = (1.6 - bh) / 2  # vertically center
        rect = Rectangle(
            (x, y),
            bw,
            bh,
            linewidth=2,
            edgecolor=color,
            facecolor=STYLE["surface"],
            alpha=0.8,
            zorder=2,
        )
        ax1.add_patch(rect)
        ax1.text(
            x + bw / 2,
            y + bh / 2,
            label,
            color=color,
            fontsize=8 if i > 0 else 9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        # Arrows and "read/write" labels
        if i > 0:
            prev_x = ds_x_positions[i - 1] + ds_box_widths[i - 1]
            ax1.annotate(
                "",
                xy=(x, 0.8),
                xytext=(prev_x, 0.8),
                arrowprops={**arrow_kw, "color": STYLE["accent2"]},
            )
            # "Read → Write" label
            mid_x = (x + prev_x) / 2
            ax1.text(
                mid_x,
                1.7,
                "read \u2192 write",
                color=STYLE["text_dim"],
                fontsize=7,
                ha="center",
                va="center",
            )

    # First pass special annotation
    ax1.text(
        ds_x_positions[1] + ds_box_widths[1] / 2,
        -0.7,
        "Pass 0: Threshold + Karis averaging",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
    )
    ax1.text(
        6.5,
        -0.7,
        "Passes 1\u20134: Standard 13-tap weighting",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
    )

    ax1.set_title(
        "Downsample: Pull from Larger Mip \u2192 Write to Smaller Mip",
        color=STYLE["accent2"],
        fontsize=13,
        fontweight="bold",
        pad=15,
    )

    # Direction label
    ax1.annotate(
        "",
        xy=(11.0, 2.2),
        xytext=(10.0, 2.2),
        arrowprops={**arrow_kw, "color": STYLE["accent2"]},
    )
    ax1.text(
        11.2,
        2.2,
        "Resolution shrinks",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="center",
    )

    # --- Bottom: Upsample (reading from smaller, blending into larger) ---
    ax2.set_xlim(-0.5, 12.5)
    ax2.set_ylim(-1.5, 2.5)

    # Reverse order: start from smallest
    us_labels = list(reversed(mip_labels[1:]))  # Mip4 → Mip0 (no HDR)
    us_scales = list(reversed(mip_scales[1:]))
    us_x_positions = [0.0, 1.6, 3.4, 5.4, 7.6]
    us_box_widths = [s * 1.8 for s in us_scales]
    us_box_heights = [s * 1.6 for s in us_scales]

    for i, (x, label, bw, bh) in enumerate(
        zip(us_x_positions, us_labels, us_box_widths, us_box_heights)
    ):
        y = (1.6 - bh) / 2
        rect = Rectangle(
            (x, y),
            bw,
            bh,
            linewidth=2,
            edgecolor=STYLE["accent3"],
            facecolor=STYLE["surface"],
            alpha=0.8,
            zorder=2,
        )
        ax2.add_patch(rect)
        ax2.text(
            x + bw / 2,
            y + bh / 2,
            label,
            color=STYLE["accent3"],
            fontsize=8,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        if i > 0:
            prev_x = us_x_positions[i - 1] + us_box_widths[i - 1]
            ax2.annotate(
                "",
                xy=(x, 0.8),
                xytext=(prev_x, 0.8),
                arrowprops={**arrow_kw, "color": STYLE["accent3"]},
            )
            mid_x = (x + prev_x) / 2
            ax2.text(
                mid_x,
                1.7,
                "read \u2192 ADD",
                color=STYLE["accent3"],
                fontsize=7.5,
                fontweight="bold",
                ha="center",
                va="center",
            )

    ax2.text(
        4.0,
        -0.7,
        "Each pass: tent-filter upsample from smaller mip, "
        "additively blend (ONE + ONE) into larger",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
    )

    ax2.set_title(
        "Upsample: Read from Smaller Mip \u2192 Additively Blend into Larger Mip",
        color=STYLE["accent3"],
        fontsize=13,
        fontweight="bold",
        pad=15,
    )

    # Direction label
    ax2.annotate(
        "",
        xy=(11.0, 2.2),
        xytext=(10.0, 2.2),
        arrowprops={**arrow_kw, "color": STYLE["accent3"]},
    )
    ax2.text(
        11.2,
        2.2,
        "Resolution grows",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="center",
    )

    fig.suptitle(
        "Mip Chain Data Flow \u2014 Downsample vs. Upsample",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.95))
    save(fig, "gpu/22-bloom", "mip_chain_flow.png")


def diagram_brightness_threshold():
    """How brightness thresholding selects bloom-contributing pixels."""
    fig, (ax1, ax2) = plt.subplots(
        1,
        2,
        figsize=(12, 5),
        facecolor=STYLE["bg"],
        gridspec_kw={"width_ratios": [3, 2]},
    )

    # --- Left: Threshold function ---
    setup_axes(ax1, grid=True, aspect=None)

    luma = np.linspace(0, 5, 500)
    threshold = 1.0

    # contribution = max(luma - threshold, 0) / max(luma, 0.0001) * luma
    # Effectively: pixel * max(luma - threshold, 0) / max(luma, 0.0001)
    # For a white pixel (rgb all equal to luma), output = max(luma - thresh, 0)
    contribution = np.maximum(luma - threshold, 0)

    ax1.plot(
        luma,
        luma,
        "--",
        color=STYLE["text_dim"],
        lw=1.5,
        alpha=0.5,
        label="Original (no threshold)",
    )
    ax1.plot(
        luma,
        contribution,
        color=STYLE["accent1"],
        lw=2.5,
        label=f"After threshold = {threshold:.1f}",
    )
    ax1.fill_between(luma, contribution, alpha=0.12, color=STYLE["accent1"])

    # Mark the threshold point
    ax1.axvline(threshold, color=STYLE["warn"], ls="--", lw=1.5, alpha=0.7)
    ax1.text(
        threshold + 0.1,
        4.2,
        f"Threshold = {threshold:.1f}",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Shade the "no bloom" region
    ax1.axvspan(0, threshold, alpha=0.08, color=STYLE["accent2"])
    ax1.text(
        threshold / 2,
        0.5,
        "No bloom",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )
    ax1.text(
        threshold + 1.5,
        0.5,
        "Bloom",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    ax1.set_xlim(0, 5)
    ax1.set_ylim(0, 4.5)
    ax1.set_xlabel("Pixel luminance", color=STYLE["text"], fontsize=11)
    ax1.set_ylabel("Bloom contribution", color=STYLE["text"], fontsize=11)

    legend = ax1.legend(
        loc="upper left",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
    )
    for text in legend.get_texts():
        text.set_color(STYLE["text"])

    ax1.set_title(
        "Brightness Threshold Function",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    # --- Right: Example scene values ---
    setup_axes(ax2, grid=True, aspect=None)

    # Example pixel luminances from different scene elements
    elements = [
        ("Shadow", 0.1, STYLE["text_dim"]),
        ("Diffuse", 0.4, STYLE["accent1"]),
        ("Specular", 1.8, STYLE["accent2"]),
        ("Emissive", 50.0, STYLE["warn"]),
    ]

    y_pos = np.arange(len(elements))
    bars = []
    for i, (_name, luma_val, color) in enumerate(elements):
        # Show log scale for the bars since emissive is so bright
        bar_val = np.log10(luma_val + 1) * 2  # log scale for visual
        b = ax2.barh(i, bar_val, color=color, height=0.6, alpha=0.85)
        bars.append(b)
        ax2.text(
            bar_val + 0.1,
            i,
            f"luma = {luma_val}",
            color=color,
            fontsize=9,
            fontweight="bold",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    # Threshold line
    thresh_bar = np.log10(threshold + 1) * 2
    ax2.axvline(thresh_bar, color=STYLE["warn"], ls="--", lw=2, alpha=0.7)

    # Labels for bloom/no bloom
    for i, (_name, luma_val, _color) in enumerate(elements):
        blooms = luma_val > threshold
        status = "\u2714 Blooms" if blooms else "\u2718 No bloom"
        status_color = STYLE["accent3"] if blooms else STYLE["accent2"]
        ax2.text(
            -0.1,
            i,
            status,
            color=status_color,
            fontsize=8,
            fontweight="bold",
            ha="right",
            va="center",
        )

    ax2.set_yticks(y_pos)
    ax2.set_yticklabels([e[0] for e in elements], color=STYLE["text"], fontsize=10)
    ax2.set_xlim(-1.5, 5)
    ax2.set_xticks([])
    ax2.set_title(
        "Scene Elements at Threshold = 1.0",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=10,
    )

    fig.suptitle(
        "Brightness Thresholding \u2014 Selecting Bloom Pixels",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/22-bloom", "brightness_threshold.png")


# ---------------------------------------------------------------------------
# gpu/23-point-light-shadows — cube_face_layout.png
# ---------------------------------------------------------------------------


def diagram_cube_face_layout():
    """Cube map face layout for omnidirectional shadow mapping.

    Shows a point light at the center with six camera frustums pointing in
    the +X, -X, +Y, -Y, +Z, -Z directions.  Each face is labeled with its
    axis and colored distinctly.  A 2D top-down view (XZ plane) shows the
    four horizontal faces, and two inset arrows show +Y and -Y.
    """
    fig = plt.figure(figsize=(9, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-5.5, 5.5), ylim=(-5.5, 5.5), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Central light ---
    light = Circle(
        (0, 0),
        0.3,
        facecolor=STYLE["warn"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        zorder=10,
    )
    ax.add_patch(light)
    ax.text(
        0,
        -0.7,
        "Point Light",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- Face definitions: (label, direction_xy, color, label_pos) ---
    # Top-down view: X is right, Z is up (representing the XZ ground plane)
    face_len = 3.5
    face_half = 1.5  # half-width of the frustum at the far end

    faces = [
        ("+X", (1, 0), STYLE["accent1"], (4.8, 0)),
        ("\u2212X", (-1, 0), STYLE["accent2"], (-4.8, 0)),
        ("+Z", (0, 1), STYLE["accent3"], (0, 4.8)),
        ("\u2212Z", (0, -1), STYLE["accent4"], (0, -4.8)),
    ]

    for label, direction, color, label_pos in faces:
        dx, dy = direction
        # Frustum tip is at origin, far end is at face_len along direction
        far_center = np.array([dx * face_len, dy * face_len])

        # Perpendicular direction for frustum width
        perp = np.array([-dy, dx])
        far_left = far_center - perp * face_half
        far_right = far_center + perp * face_half

        # Draw frustum trapezoid
        trap = Polygon(
            [(0, 0), (far_left[0], far_left[1]), (far_right[0], far_right[1])],
            facecolor=color,
            alpha=0.15,
            edgecolor=color,
            linewidth=1.5,
            zorder=2,
        )
        ax.add_patch(trap)

        # Draw far edge
        ax.plot(
            [far_left[0], far_right[0]],
            [far_left[1], far_right[1]],
            color=color,
            linewidth=2,
            zorder=3,
        )

        # Direction arrow
        ax.annotate(
            "",
            xy=(dx * 2.5, dy * 2.5),
            xytext=(dx * 0.5, dy * 0.5),
            arrowprops={
                "arrowstyle": "->,head_width=0.25,head_length=0.15",
                "color": color,
                "lw": 2.5,
            },
            zorder=8,
        )

        # Face label
        ax.text(
            label_pos[0],
            label_pos[1],
            label,
            color=color,
            fontsize=13,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # --- +Y and -Y indicators (up/down out of plane) ---
    # Show as circles with arrows since they point out of the 2D view
    for label, y_offset, color in [
        ("+Y \u2299", 3.0, STYLE["warn"]),  # out of screen (dot)
        ("\u2212Y \u2297", -3.0, STYLE["text_dim"]),  # into screen (cross)
    ]:
        ax.text(
            4.0,
            y_offset,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            bbox={
                "facecolor": STYLE["surface"],
                "edgecolor": color,
                "linewidth": 1.5,
                "boxstyle": "round,pad=0.3",
                "alpha": 0.9,
            },
            zorder=9,
        )

    # --- Annotations ---
    ax.text(
        -4.8,
        4.8,
        "90\u00b0 FOV per face\n6 faces = full sphere",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
        va="top",
        path_effects=stroke,
    )
    ax.text(
        -4.8,
        -4.3,
        "Top-down view (XZ plane)\nY-axis faces shown as insets",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="left",
        va="top",
        style="italic",
    )

    ax.set_title(
        "Cube Map Face Layout \u2014 6 Cameras Around a Point Light",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/23-point-light-shadows", "cube_face_layout.png")


# ---------------------------------------------------------------------------
# gpu/23-point-light-shadows — linear_vs_hardware_depth.png
# ---------------------------------------------------------------------------


def diagram_linear_vs_hardware_depth():
    """Comparison of linear depth vs hardware (z/w) depth distribution.

    Shows two plots: hardware depth (non-linear, precision near camera) and
    linear depth (uniform precision).  Illustrates why R32_FLOAT with
    distance/far_plane gives better shadow comparison results.
    """
    fig, (ax_hw, ax_lin) = plt.subplots(
        1,
        2,
        figsize=(12, 5),
        facecolor=STYLE["bg"],
    )
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    near = 0.1
    far = 25.0
    distances = np.linspace(near, far, 500)

    # --- Hardware depth: z_ndc = (f * (d - n)) / (d * (f - n)) ---
    # For a [0,1] depth range (Vulkan/D3D12 convention)
    hw_depth = (far * (distances - near)) / (distances * (far - near))

    # --- Linear depth: d / far ---
    lin_depth = distances / far

    for ax, depth_values, title, color, description in [
        (
            ax_hw,
            hw_depth,
            "Hardware Depth (z/w)",
            STYLE["accent2"],
            "Precision concentrated near camera\n\u2192 coarse at distance",
        ),
        (
            ax_lin,
            lin_depth,
            "Linear Depth (distance / far)",
            STYLE["accent1"],
            "Uniform precision across range\n\u2192 consistent shadow tests",
        ),
    ]:
        setup_axes(ax, grid=True, aspect=None)
        ax.set_xlim(0, far)
        ax.set_ylim(-0.05, 1.1)
        ax.set_xlabel("World Distance", color=STYLE["axis"], fontsize=10)
        ax.set_ylabel("Stored Depth [0, 1]", color=STYLE["axis"], fontsize=10)

        ax.plot(distances, depth_values, color=color, linewidth=2.5, zorder=5)

        # Show equal-distance markers to visualize spacing
        marker_distances = np.array([1, 5, 10, 15, 20, 25])
        for d in marker_distances:
            if d <= far:
                idx = np.argmin(np.abs(distances - d))
                dv = depth_values[idx]
                ax.plot(d, dv, "o", color=color, markersize=5, zorder=6)
                ax.plot(
                    [d, d],
                    [0, dv],
                    "--",
                    color=STYLE["grid"],
                    linewidth=0.8,
                    alpha=0.6,
                    zorder=2,
                )

        ax.set_title(
            title,
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            pad=12,
        )

        ax.text(
            far * 0.55,
            0.15,
            description,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="bottom",
            path_effects=stroke,
        )

    fig.suptitle(
        "Linear Depth vs Hardware Depth \u2014 Why R32_FLOAT Stores distance / far",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.99,
    )
    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "gpu/23-point-light-shadows", "linear_vs_hardware_depth.png")


# ---------------------------------------------------------------------------
# gpu/23-point-light-shadows — shadow_lookup.png
# ---------------------------------------------------------------------------


def diagram_shadow_lookup():
    """Shadow cube map lookup: light-to-fragment direction samples the cube map.

    Shows a point light, a fragment point, and the direction vector between
    them.  The direction vector selects a cube map face and the stored depth
    is compared against the actual distance.
    """
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.5, 11), ylim=(-2, 8), grid=False)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Light source ---
    light_x, light_y = 1.5, 4.0
    light_circle = Circle(
        (light_x, light_y),
        0.25,
        facecolor=STYLE["warn"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        zorder=10,
    )
    ax.add_patch(light_circle)
    ax.text(
        light_x,
        light_y + 0.6,
        "Light",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # --- Occluder (box between light and fragment) ---
    occ_x, occ_y = 4.5, 2.5
    occ_w, occ_h = 1.2, 2.5
    occluder = Rectangle(
        (occ_x, occ_y),
        occ_w,
        occ_h,
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["text"],
        linewidth=1.5,
        alpha=0.7,
        zorder=6,
    )
    ax.add_patch(occluder)
    ax.text(
        occ_x + occ_w / 2,
        occ_y + occ_h - 0.4,
        "Occluder",
        color=STYLE["text"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        zorder=7,
    )

    # --- Fragment point (in shadow) ---
    frag_x, frag_y = 8.5, 1.5
    ax.plot(frag_x, frag_y, "o", color=STYLE["accent2"], markersize=8, zorder=10)
    ax.text(
        frag_x + 0.4,
        frag_y - 0.4,
        "Fragment P",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        path_effects=stroke,
    )

    # --- Direction vector from light to fragment ---
    dir_dx = frag_x - light_x
    dir_dy = frag_y - light_y
    ax.annotate(
        "",
        xy=(frag_x, frag_y),
        xytext=(light_x, light_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.15",
            "color": STYLE["accent3"],
            "lw": 2.5,
            "linestyle": "dashed",
        },
        zorder=4,
    )
    mid_x = light_x + dir_dx * 0.18
    mid_y = light_y + dir_dy * 0.18 + 1.4
    ax.text(
        mid_x,
        mid_y,
        "light_to_frag",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # --- Stored depth point (where the ray first hits the occluder) ---
    # Find intersection along the ray
    t_hit = (occ_x - light_x) / dir_dx  # left face of occluder
    hit_y = light_y + t_hit * dir_dy
    hit_x = occ_x

    ax.plot(hit_x, hit_y, "D", color=STYLE["warn"], markersize=8, zorder=10)
    ax.text(
        hit_x - 1.2,
        hit_y - 0.6,
        "stored\ndepth",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )
    # Leader line from label to diamond marker
    ax.plot(
        [hit_x - 1.2, hit_x],
        [hit_y - 0.5, hit_y],
        color=STYLE["warn"],
        lw=0.8,
        ls=":",
        zorder=5,
    )

    # --- Depth comparison annotations ---
    # Actual distance line
    actual_dist = np.sqrt(dir_dx**2 + dir_dy**2)
    stored_dist = np.sqrt((hit_x - light_x) ** 2 + (hit_y - light_y) ** 2)

    # Stored distance bracket
    ax.annotate(
        "",
        xy=(hit_x, -1.0),
        xytext=(light_x, -1.0),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
        zorder=5,
    )
    ax.text(
        (light_x + hit_x) / 2,
        -1.5,
        f"stored = {stored_dist / 25:.2f}",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Actual distance bracket
    ax.annotate(
        "",
        xy=(frag_x, -0.2),
        xytext=(light_x, -0.2),
        arrowprops={
            "arrowstyle": "<->",
            "color": STYLE["accent2"],
            "lw": 1.5,
        },
        zorder=5,
    )
    ax.text(
        (light_x + frag_x) / 2,
        -0.7,
        f"current = {actual_dist / 25:.2f}",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # --- Result annotation ---
    ax.text(
        8.5,
        6.5,
        "current > stored\n\u2192 IN SHADOW",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        bbox={
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["accent2"],
            "linewidth": 2,
            "boxstyle": "round,pad=0.5",
            "alpha": 0.9,
        },
        zorder=11,
    )

    # --- Cube map face indicator ---
    ax.text(
        8.5,
        7.5,
        "TextureCube.Sample(smp, light_to_frag)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
        path_effects=stroke,
    )

    ax.set_title(
        "Shadow Cube Map Lookup \u2014 Direction-Based Depth Comparison",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/23-point-light-shadows", "shadow_lookup.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — atmosphere_layers.png
# ---------------------------------------------------------------------------


def diagram_atmosphere_layers():
    """Planet cross-section with R_ground, R_atmo, density profiles."""
    fig, ax = plt.subplots(figsize=(10, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-1.3, 1.3), ylim=(-0.3, 1.5), grid=False)

    R_GROUND = 1.0
    R_ATMO = R_GROUND + 0.4  # scaled for visual clarity

    # Draw planet surface
    theta = np.linspace(0, np.pi, 200)
    ax.fill_between(
        R_GROUND * np.cos(theta),
        R_GROUND * np.sin(theta),
        0,
        color="#2d5016",
        alpha=0.6,
    )
    ax.plot(
        R_GROUND * np.cos(theta),
        R_GROUND * np.sin(theta),
        color=STYLE["accent3"],
        lw=2,
        label="Ground (6360 km)",
    )

    # Draw atmosphere boundary
    ax.plot(
        R_ATMO * np.cos(theta),
        R_ATMO * np.sin(theta),
        color=STYLE["accent1"],
        lw=2,
        ls="--",
        label="Atmosphere top (6460 km)",
    )

    # Rayleigh layer (dense near surface, fades)
    for i in range(8):
        r = R_GROUND + i * 0.05
        alpha = 0.3 * np.exp(-i / 2.0)
        ax.plot(
            r * np.cos(theta),
            r * np.sin(theta),
            color=STYLE["accent1"],
            lw=0.5,
            alpha=alpha,
        )

    # Mie layer (concentrated near surface)
    for i in range(3):
        r = R_GROUND + i * 0.02
        alpha = 0.4 * np.exp(-i / 0.5)
        ax.plot(
            r * np.cos(theta),
            r * np.sin(theta),
            color=STYLE["accent2"],
            lw=1.0,
            alpha=alpha,
        )

    # Ozone layer (band around 25 km ~ 0.1 in our scale)
    ozone_r = R_GROUND + 0.1
    ax.plot(
        ozone_r * np.cos(theta),
        ozone_r * np.sin(theta),
        color=STYLE["accent4"],
        lw=2,
        alpha=0.8,
        label="Ozone layer (~25 km)",
    )

    # Radial markers
    ax.annotate(
        "",
        xy=(0, R_ATMO + 0.02),
        xytext=(0, 0),
        arrowprops={"arrowstyle": "<->", "color": STYLE["text_dim"], "lw": 1.5},
    )
    ax.text(
        0.05,
        (R_GROUND + R_ATMO) / 2 + 0.05,
        "100 km\natmosphere",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
    )

    ax.legend(
        loc="upper right",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    ax.set_title(
        "Atmosphere Layers \u2014 Planet Cross-Section",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "atmosphere_layers.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — density_profiles.png
# ---------------------------------------------------------------------------


def diagram_density_profiles():
    """Rayleigh, Mie, ozone density vs altitude (0-100 km)."""
    fig, ax = plt.subplots(figsize=(10, 6), facecolor=STYLE["bg"])
    setup_axes(ax, grid=True, aspect=None)

    alt = np.linspace(0, 100, 500)

    # Rayleigh: exp(-h / 8)
    rho_rayleigh = np.exp(-alt / 8.0)

    # Mie: exp(-h / 1.2)
    rho_mie = np.exp(-alt / 1.2)

    # Ozone: tent centered at 25 km, width 15 km
    rho_ozone = np.maximum(0, 1.0 - np.abs(alt - 25.0) / 15.0)

    ax.plot(
        alt, rho_rayleigh, color=STYLE["accent1"], lw=2.5, label="Rayleigh (H=8 km)"
    )
    ax.plot(alt, rho_mie, color=STYLE["accent2"], lw=2.5, label="Mie (H=1.2 km)")
    ax.plot(
        alt, rho_ozone, color=STYLE["accent4"], lw=2.5, label="Ozone (tent @ 25 km)"
    )

    ax.set_xlabel("Altitude (km)", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("Relative Density", color=STYLE["text"], fontsize=11)
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 1.1)
    ax.legend(
        loc="upper right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    ax.set_title(
        "Density Profiles \u2014 Scattering Species vs Altitude",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "density_profiles.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — ray_sphere_intersection.png
# ---------------------------------------------------------------------------


def diagram_ray_sphere_intersection():
    """View ray through atmosphere, t_near/t_far, ground hit."""
    fig, ax = plt.subplots(figsize=(10, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-1.5, 1.5), ylim=(-0.2, 1.6), grid=False)

    # Planet and atmosphere circles (upper half)
    theta = np.linspace(0, np.pi, 200)
    R_G = 0.8
    R_A = 1.2

    ax.fill_between(
        R_G * np.cos(theta), R_G * np.sin(theta), 0, color="#2d5016", alpha=0.4
    )
    ax.plot(R_G * np.cos(theta), R_G * np.sin(theta), color=STYLE["accent3"], lw=2)
    ax.plot(
        R_A * np.cos(theta), R_A * np.sin(theta), color=STYLE["accent1"], lw=2, ls="--"
    )

    # Camera position (inside atmosphere)
    cam = np.array([-0.3, R_G + 0.05])
    ax.plot(*cam, "o", color=STYLE["warn"], markersize=10, zorder=10)
    ax.text(
        cam[0] - 0.15,
        cam[1] + 0.08,
        "Camera",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
    )

    # View ray direction
    ray_dir = np.array([0.7, 0.5])
    ray_dir = ray_dir / np.linalg.norm(ray_dir)

    # Find intersections with atmosphere sphere
    # Parametric: |cam + t*dir|^2 = R_A^2
    a = np.dot(ray_dir, ray_dir)
    b = 2 * np.dot(cam, ray_dir)
    c_coeff = np.dot(cam, cam) - R_A**2
    disc = b**2 - 4 * a * c_coeff
    if disc < 0:
        t_far = 2.0  # fallback: draw ray to a safe max distance
    else:
        t_near = (-b - np.sqrt(disc)) / (2 * a)  # noqa: F841
        t_far = (-b + np.sqrt(disc)) / (2 * a)

    # Draw ray
    ray_end = cam + ray_dir * t_far
    ax.annotate(
        "",
        xy=ray_end,
        xytext=cam,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 2.5},
    )

    # Mark t_far
    ax.plot(*ray_end, "s", color=STYLE["accent2"], markersize=8, zorder=10)
    ax.text(
        ray_end[0] + 0.05,
        ray_end[1] + 0.05,
        "$t_{far}$",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
    )

    # Labels
    ax.text(0.9, R_G + 0.05, "R_ground", color=STYLE["accent3"], fontsize=10)
    ax.text(1.0, R_A + 0.05, "R_atmo", color=STYLE["accent1"], fontsize=10)

    ax.set_title(
        "Ray-Sphere Intersection \u2014 View Ray Through Atmosphere",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "ray_sphere_intersection.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — scattering_geometry.png
# ---------------------------------------------------------------------------


def diagram_scattering_geometry():
    """Single ray march step: sun direction, scatter angle theta, vectors."""
    fig, ax = plt.subplots(figsize=(10, 6), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.8, 3.2), ylim=(-0.2, 2.6), grid=False)

    # Sample point P — center of the geometry
    P = np.array([1.0, 0.7])
    ax.plot(*P, "o", color=STYLE["warn"], markersize=12, zorder=10)
    ax.text(
        P[0],
        P[1] - 0.22,
        "P",
        color=STYLE["warn"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # View ray — incoming from camera (upper-left), arriving at P.
    # Angled so the label has space above the arrow, away from θ.
    view_dir = np.array([-0.9, -0.15])
    view_dir = view_dir / np.linalg.norm(view_dir)
    view_len = 1.1
    ax.annotate(
        "",
        xy=P,
        xytext=P - view_dir * view_len,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 2.5},
    )
    # Label above the incoming arrow, well clear of θ
    view_mid = P - view_dir * 0.65
    ax.text(
        view_mid[0] - 0.3,
        view_mid[1] + 0.2,
        "View ray",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Sun direction — steep upward to separate from the horizontal scatter
    sun_dir = np.array([0.35, 0.94])
    sun_dir = sun_dir / np.linalg.norm(sun_dir)
    sun_len = 1.2
    ax.annotate(
        "",
        xy=P + sun_dir * sun_len,
        xytext=P,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 2.5},
    )
    sun_label_pos = P + sun_dir * 0.9
    ax.text(
        sun_label_pos[0] + 0.2,
        sun_label_pos[1],
        "To Sun",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Scattered direction (= negative view_dir, toward the camera)
    scatter_dir = -view_dir
    scatter_len = 1.0
    ax.annotate(
        "",
        xy=P + scatter_dir * scatter_len,
        xytext=P,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent4"], "lw": 2, "ls": "--"},
    )
    scatter_label_pos = P + scatter_dir * 0.75
    ax.text(
        scatter_label_pos[0] + 0.15,
        scatter_label_pos[1] - 0.22,
        "Scattered\nto eye",
        color=STYLE["accent4"],
        fontsize=10,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Angle θ arc — between sun direction and scattered direction
    angle_sun = np.arctan2(sun_dir[1], sun_dir[0])
    angle_scatter = np.arctan2(scatter_dir[1], scatter_dir[0])
    arc_t = np.linspace(
        min(angle_scatter, angle_sun), max(angle_scatter, angle_sun), 50
    )
    arc_r = 0.5
    ax.plot(
        P[0] + arc_r * np.cos(arc_t),
        P[1] + arc_r * np.sin(arc_t),
        color=STYLE["warn"],
        lw=2,
    )
    # θ label at the midpoint of the arc, pushed outward
    mid_angle = (angle_sun + angle_scatter) / 2
    ax.text(
        P[0] + arc_r * 1.5 * np.cos(mid_angle),
        P[1] + arc_r * 1.5 * np.sin(mid_angle),
        "\u03b8",
        color=STYLE["warn"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_title(
        "Scattering Geometry \u2014 Phase Angle \u03b8",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "scattering_geometry.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — ray_march_diagram.png
# ---------------------------------------------------------------------------


def diagram_ray_march():
    """Full march along view ray with sample points and transmittance."""
    fig, ax = plt.subplots(figsize=(12, 5), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 10.5), ylim=(-1.5, 3.0), grid=False)

    N = 8  # number of sample points
    x_start, x_end = 0.5, 9.5

    # Draw the view ray
    ax.annotate(
        "",
        xy=(x_end + 0.3, 0),
        xytext=(x_start - 0.3, 0),
        arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 2},
    )
    ax.text(
        x_start - 0.4,
        0.3,
        "Camera",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        x_end + 0.1,
        0.3,
        "Atmo\nedge",
        color=STYLE["accent1"],
        fontsize=9,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Sample points along ray
    xs = np.linspace(x_start, x_end, N)
    for i, x in enumerate(xs):
        # Transmittance decreases along the ray
        t = np.exp(-i * 0.3)
        color = STYLE["accent2"]
        alpha = 0.3 + 0.7 * t
        ax.plot(x, 0, "o", color=color, markersize=8, alpha=alpha, zorder=5)

        # Sun direction arrow at each sample
        ax.annotate(
            "",
            xy=(x, 1.2),
            xytext=(x, 0.2),
            arrowprops={
                "arrowstyle": "->",
                "color": STYLE["warn"],
                "lw": 1,
                "alpha": 0.5,
            },
        )
        ax.text(
            x, -0.5, f"$P_{{{i}}}$", color=STYLE["text_dim"], fontsize=9, ha="center"
        )

    # Step size markers
    for i in range(N - 1):
        mid = (xs[i] + xs[i + 1]) / 2
        ax.annotate(
            "",
            xy=(xs[i + 1], -0.9),
            xytext=(xs[i], -0.9),
            arrowprops={"arrowstyle": "<->", "color": STYLE["text_dim"], "lw": 1},
        )
        if i == 0:
            ax.text(
                mid, -1.2, "\u0394s", color=STYLE["text_dim"], fontsize=10, ha="center"
            )

    # Transmittance bars — draw bars first, then label above them
    for i, x in enumerate(xs):
        t = np.exp(-i * 0.3)
        ax.barh(
            1.7,
            t * 0.8,
            left=x - 0.4,
            height=0.3,
            color=STYLE["warn"],
            alpha=0.3 + 0.5 * t,
        )

    # Label placed above the bar row, away from bars
    ax.text(
        (x_start + x_end) / 2,
        2.2,
        "Sun transmittance at each sample (decreasing along ray)",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_title(
        "Ray March \u2014 Accumulating Inscattered Light Along the View Ray",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "ray_march_diagram.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — phase_functions.png
# ---------------------------------------------------------------------------


def diagram_phase_functions():
    """Rayleigh vs Mie (g=0.8) angular plots, 0-180 degrees."""
    fig = plt.figure(figsize=(12, 5), facecolor=STYLE["bg"])
    ax1 = fig.add_subplot(121)
    ax2 = fig.add_subplot(122, polar=True, facecolor=STYLE["bg"])

    theta = np.linspace(0, np.pi, 500)
    cos_theta = np.cos(theta)
    theta_deg = np.degrees(theta)

    # Rayleigh: (3/16pi)(1 + cos^2 theta)
    rayleigh = (3.0 / (16.0 * np.pi)) * (1.0 + cos_theta**2)

    # Henyey-Greenstein: (1-g^2) / (4pi * (1+g^2-2g*cos)^1.5)
    g = 0.8
    mie = (1 - g**2) / (4 * np.pi * (1 + g**2 - 2 * g * cos_theta) ** 1.5)

    # Cartesian plot
    setup_axes(ax1, aspect=None)
    ax1.plot(theta_deg, rayleigh, color=STYLE["accent1"], lw=2.5, label="Rayleigh")
    ax1.plot(theta_deg, mie, color=STYLE["accent2"], lw=2.5, label="Mie (g=0.8)")
    ax1.set_xlabel(
        "Scattering Angle \u03b8 (degrees)", color=STYLE["text"], fontsize=11
    )
    ax1.set_ylabel("Phase Function Value", color=STYLE["text"], fontsize=11)
    ax1.set_xlim(0, 180)
    ax1.set_yscale("log")
    ax1.set_ylim(1e-3, 1e2)
    ax1.legend(
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    ax1.set_title(
        "Phase Functions (log scale)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
    )

    # Polar plot (ax2 was already created with polar=True above)
    ax2.plot(
        theta, rayleigh / rayleigh.max(), color=STYLE["accent1"], lw=2, label="Rayleigh"
    )
    ax2.plot(theta, mie / mie.max(), color=STYLE["accent2"], lw=2, label="Mie (g=0.8)")
    ax2.set_theta_zero_location("E")  # type: ignore[attr-defined]
    ax2.set_theta_direction(-1)  # type: ignore[attr-defined]
    ax2.tick_params(colors=STYLE["axis"], labelsize=8)
    ax2.set_rlabel_position(135)  # type: ignore[attr-defined]
    ax2.grid(True, color=STYLE["grid"], alpha=0.3)
    ax2.spines["polar"].set_color(STYLE["grid"])
    ax2.set_title(
        "Polar (normalized)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=20,
    )

    fig.suptitle(
        "Phase Functions \u2014 Rayleigh vs Mie Scattering",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "phase_functions.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — sun_transmittance.png
# ---------------------------------------------------------------------------


def diagram_sun_transmittance():
    """Inner march from sample to sun, illustrating orange sunsets."""
    fig, ax = plt.subplots(figsize=(10, 7), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-1.5, 1.5), ylim=(-0.3, 1.6), grid=False)

    theta = np.linspace(0, np.pi, 200)
    R_G = 0.8
    R_A = 1.2

    # Planet and atmosphere
    ax.fill_between(
        R_G * np.cos(theta), R_G * np.sin(theta), 0, color="#2d5016", alpha=0.4
    )
    ax.plot(R_G * np.cos(theta), R_G * np.sin(theta), color=STYLE["accent3"], lw=2)
    ax.plot(
        R_A * np.cos(theta), R_A * np.sin(theta), color=STYLE["accent1"], lw=2, ls="--"
    )

    # Noon sample — short path through atmosphere
    P_noon = np.array([0.0, R_G + 0.05])
    sun_noon = np.array([0.0, 1.0])
    noon_end = P_noon + sun_noon * 0.35
    ax.annotate(
        "",
        xy=noon_end,
        xytext=P_noon,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 2.5},
    )
    ax.plot(*P_noon, "o", color=STYLE["accent1"], markersize=8, zorder=10)
    ax.text(
        0.35,
        R_G + 0.42,
        "Noon: short path\nblue sky",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Sunset sample — long path through atmosphere
    P_sunset = np.array([-0.5, R_G + 0.03])
    sun_sunset = np.array([0.95, 0.3])
    sun_sunset = sun_sunset / np.linalg.norm(sun_sunset)
    sunset_end = P_sunset + sun_sunset * 1.2
    ax.annotate(
        "",
        xy=sunset_end,
        xytext=P_sunset,
        arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 2.5},
    )
    ax.plot(*P_sunset, "o", color=STYLE["accent2"], markersize=8, zorder=10)
    ax.text(
        -1.1,
        R_G + 0.15,
        "Sunset: long path\norange light",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
    )

    # Inner march sample dots along sunset path
    for i in range(6):
        t = 0.15 + i * 0.17
        pt = P_sunset + sun_sunset * t
        ax.plot(*pt, ".", color=STYLE["warn"], markersize=5, alpha=0.7)

    ax.set_title(
        "Sun Transmittance \u2014 Why Sunsets Are Orange",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "sun_transmittance.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — sun_limb_darkening.png
# ---------------------------------------------------------------------------


def diagram_sun_limb_darkening():
    """Brightness profile across sun disc radius."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5), facecolor=STYLE["bg"])

    # Left: brightness curve
    setup_axes(ax1, aspect=None)
    r = np.linspace(0, 1, 200)
    cos_limb = np.sqrt(np.maximum(0, 1 - r**2))
    brightness = 1.0 - 0.6 * (1.0 - cos_limb)

    ax1.fill_between(r, brightness, alpha=0.2, color=STYLE["accent2"])
    ax1.plot(r, brightness, color=STYLE["accent2"], lw=2.5)
    ax1.set_xlabel(
        "Normalized Radius (0=center, 1=edge)", color=STYLE["text"], fontsize=11
    )
    ax1.set_ylabel("Brightness", color=STYLE["text"], fontsize=11)
    ax1.set_xlim(0, 1)
    ax1.set_ylim(0, 1.1)
    ax1.set_title(
        "Limb Darkening Profile (u=0.6)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
    )

    # Right: sun disc visualization
    setup_axes(ax2, xlim=(-1.5, 1.5), ylim=(-1.5, 1.5), grid=False)
    disc_r = np.linspace(0, 1, 100)
    disc_t = np.linspace(0, 2 * np.pi, 100)
    R, T = np.meshgrid(disc_r, disc_t)
    X = R * np.cos(T)
    Y = R * np.sin(T)
    cos_l = np.sqrt(np.maximum(0, 1 - R**2))
    Z = 1.0 - 0.6 * (1.0 - cos_l)
    ax2.pcolormesh(X, Y, Z, cmap="hot", shading="auto", vmin=0.3, vmax=1.0)
    circ = Circle((0, 0), 1.0, fill=False, ec=STYLE["accent2"], lw=2)
    ax2.add_patch(circ)
    ax2.set_title(
        "Sun Disc Appearance", color=STYLE["text"], fontsize=12, fontweight="bold"
    )
    ax2.axis("off")

    fig.suptitle(
        "Sun Limb Darkening \u2014 Center Brighter Than Edge",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "sun_limb_darkening.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — time_of_day_colors.png
# ---------------------------------------------------------------------------


def diagram_time_of_day_colors():
    """Sky color strips at 8 sun elevations (twilight to noon)."""
    fig, ax = plt.subplots(figsize=(12, 4), facecolor=STYLE["bg"])
    setup_axes(ax, grid=False, aspect=None)

    # Approximate sky colors for different sun elevations
    # (artistic approximation — real values would need actual ray marching)
    elevations = [-10, -5, 0, 5, 15, 30, 60, 90]
    labels = [
        "-10\u00b0\nNight",
        "-5\u00b0\nTwilight",
        "0\u00b0\nHorizon",
        "5\u00b0\nDawn",
        "15\u00b0\nMorning",
        "30\u00b0\nDay",
        "60\u00b0\nAfternoon",
        "90\u00b0\nNoon",
    ]
    # Zenith colors at each elevation (approximate)
    colors = [
        "#0a0a1a",  # night — dark blue-black
        "#1a1040",  # twilight — deep purple
        "#4a2040",  # horizon — purple-orange
        "#c06030",  # dawn — orange
        "#4080c0",  # morning — blue
        "#5090d0",  # day — bright blue
        "#60a0e0",  # afternoon — sky blue
        "#70b0f0",  # noon — light blue
    ]

    n = len(elevations)
    bar_width = 1.0
    for i in range(n):
        ax.barh(i, bar_width, height=0.8, color=colors[i], left=0)
        ax.text(
            -0.05,
            i,
            labels[i],
            color=STYLE["text"],
            fontsize=9,
            ha="right",
            va="center",
        )
        ax.text(
            bar_width + 0.05,
            i,
            f"Sun: {elevations[i]}\u00b0",
            color=STYLE["text_dim"],
            fontsize=9,
            va="center",
        )

    ax.set_xlim(-0.6, 1.5)
    ax.set_ylim(-0.6, n - 0.4)
    ax.axis("off")
    ax.set_title(
        "Time of Day \u2014 Sky Color vs Sun Elevation",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "time_of_day_colors.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — render_pipeline.png
# ---------------------------------------------------------------------------


def diagram_sky_render_pipeline():
    """HDR -> bloom downsample/upsample chain -> tonemap flow."""
    fig, ax = plt.subplots(figsize=(14, 5), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 14), ylim=(-2, 3.5), grid=False)

    box_h = 1.2
    box_w = 2.0

    def draw_box(cx, cy, label, color, w=box_w, h=box_h):
        rect = Rectangle(
            (cx - w / 2, cy - h / 2),
            w,
            h,
            facecolor=STYLE["surface"],
            edgecolor=color,
            lw=2,
            zorder=5,
        )
        ax.add_patch(rect)
        ax.text(
            cx,
            cy,
            label,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=6,
        )

    # Main pipeline boxes
    draw_box(1.0, 2.0, "Sky Pass\n(ray march)", STYLE["accent1"])
    draw_box(4.0, 2.0, "HDR Target\n(R16G16B16A16)", STYLE["warn"])

    # Arrow: sky -> HDR
    ax.annotate(
        "",
        xy=(2.9, 2.0),
        xytext=(2.1, 2.0),
        arrowprops={"arrowstyle": "->", "color": STYLE["text_dim"], "lw": 2},
    )

    # Downsample chain
    ds_labels = [
        "Mip 0\n640\u00d7360",
        "Mip 1\n320\u00d7180",
        "Mip 2\n160\u00d790",
        "Mip 3\n80\u00d745",
        "Mip 4\n40\u00d722",
    ]
    for i, lbl in enumerate(ds_labels):
        x = 6.5 + i * 1.5
        draw_box(x, 0.0, lbl, STYLE["accent2"], w=1.3, h=box_h)
        if i > 0:
            ax.annotate(
                "",
                xy=(x - 0.65, 0.0),
                xytext=(x - 0.85, 0.0),
                arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 1.5},
            )

    # Arrow from HDR to downsample chain
    ax.annotate(
        "",
        xy=(5.85, 0.0),
        xytext=(5.1, 2.0),
        arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 2},
    )
    ax.text(
        5.8,
        1.2,
        "Downsample\n(5 passes)",
        color=STYLE["accent2"],
        fontsize=9,
        ha="left",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Upsample arrows (going back)
    for i in range(4):
        x_from = 6.5 + (4 - i) * 1.5
        x_to = 6.5 + (3 - i) * 1.5
        ax.annotate(
            "",
            xy=(x_to + 0.65, -1.2),
            xytext=(x_from - 0.65, -1.2),
            arrowprops={"arrowstyle": "->", "color": STYLE["accent4"], "lw": 1.5},
        )

    ax.text(
        9.5,
        -1.6,
        "Upsample (4 passes, additive blend)",
        color=STYLE["accent4"],
        fontsize=9,
        ha="center",
    )

    # Tonemap box
    draw_box(4.0, -1.2, "Tonemap\n(ACES + bloom)", STYLE["accent3"])
    # Arrow: HDR -> tonemap
    ax.annotate(
        "",
        xy=(4.0, -0.6),
        xytext=(4.0, 1.3),
        arrowprops={"arrowstyle": "->", "color": STYLE["text_dim"], "lw": 2},
    )
    # Arrow: bloom -> tonemap
    ax.annotate(
        "",
        xy=(5.1, -1.2),
        xytext=(5.85, -1.2),
        arrowprops={"arrowstyle": "->", "color": STYLE["accent4"], "lw": 2},
    )

    # Swapchain
    draw_box(1.0, -1.2, "Swapchain\n(sRGB)", STYLE["accent3"])
    ax.annotate(
        "",
        xy=(2.1, -1.2),
        xytext=(2.9, -1.2),
        arrowprops={"arrowstyle": "<-", "color": STYLE["accent3"], "lw": 2},
    )

    ax.set_title(
        "Render Pipeline \u2014 HDR Sky \u2192 Bloom \u2192 Tonemap",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "render_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/27-ssao — ssao_render_pipeline.png
# ---------------------------------------------------------------------------


def diagram_ssao_render_pipeline():
    """5-pass SSAO rendering pipeline showing data flow between passes."""
    from matplotlib.patches import FancyBboxPatch

    fig = plt.figure(figsize=(12, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 11.5), ylim=(-1.0, 7.0), grid=False, aspect=None)
    ax.axis("off")

    # Pass boxes: (x, y, w, h, label, sublabel, color)
    passes = [
        (0.0, 4.8, 2.0, 1.6, "Pass 1", "Shadow", STYLE["accent4"]),
        (2.8, 4.8, 2.0, 1.6, "Pass 2", "Geometry", STYLE["accent1"]),
        (5.6, 4.8, 2.0, 1.6, "Pass 3", "SSAO", STYLE["accent2"]),
        (8.4, 4.8, 2.0, 1.6, "Pass 4", "Blur", STYLE["accent3"]),
        (8.4, 1.4, 2.0, 1.6, "Pass 5", "Composite", STYLE["warn"]),
    ]
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    for x, y, w, h, title, sub, color in passes:
        box = FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.5,
            zorder=3,
        )
        ax.add_patch(box)
        ax.text(
            x + w / 2,
            y + h * 0.65,
            title,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )
        ax.text(
            x + w / 2,
            y + h * 0.3,
            sub,
            color=STYLE["text"],
            fontsize=13,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Arrows between passes (horizontal for 1→2→3→4, then down for 4→5)
    arrow_kw = {"arrowstyle": "->,head_width=0.25,head_length=0.15", "lw": 2}

    for x_start, x_end in [(2.0, 2.8), (4.8, 5.6), (7.6, 8.4)]:
        ax.annotate(
            "",
            xy=(x_end, 5.6),
            xytext=(x_start, 5.6),
            arrowprops={**arrow_kw, "color": STYLE["text_dim"]},
        )

    # Arrow from Pass 4 down to Pass 5
    ax.annotate(
        "",
        xy=(9.4, 3.0),
        xytext=(9.4, 4.8),
        arrowprops={**arrow_kw, "color": STYLE["text_dim"]},
    )

    # Also arrow from Pass 2 color output to Pass 5
    ax.annotate(
        "",
        xy=(8.4, 2.2),
        xytext=(4.8, 2.2),
        arrowprops={
            **arrow_kw,
            "color": STYLE["accent1"],
            "connectionstyle": "arc3,rad=0",
        },
    )

    # Output textures beneath each pass
    outputs = [
        (1.0, 4.3, "shadow_depth\nD32_FLOAT\n2048\u00d72048", STYLE["accent4"]),
        (3.8, 3.1, "scene_color\nR8G8B8A8_UNORM", STYLE["accent1"]),
        (3.8, 1.8, "view_normals\nR16G16B16A16_FLOAT", STYLE["accent1"]),
        (3.8, 0.5, "scene_depth\nD32_FLOAT", STYLE["accent1"]),
        (6.6, 4.3, "ssao_raw\nR8_UNORM", STYLE["accent2"]),
        (9.4, 4.3, "ssao_blurred\nR8_UNORM", STYLE["accent3"]),
        (9.4, 0.2, "swapchain\nsRGB output", STYLE["warn"]),
    ]

    for x, y, label, color in outputs:
        ax.text(
            x,
            y,
            label,
            color=color,
            fontsize=8,
            ha="center",
            va="top",
            family="monospace",
            path_effects=stroke,
            zorder=5,
        )

    # Label the scene_color → composite arrow
    ax.text(
        6.6,
        2.5,
        "scene_color",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "SSAO Render Pipeline \u2014 5 Passes per Frame",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/27-ssao", "ssao_render_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/27-ssao — gbuffer_mrt.png
# ---------------------------------------------------------------------------


def diagram_gbuffer_mrt():
    """G-buffer layout showing how MRT writes 3 textures from one draw call."""
    from matplotlib.patches import FancyBboxPatch

    fig = plt.figure(figsize=(11, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 11), ylim=(-0.5, 5.5), grid=False, aspect=None)
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Fragment shader box on the left
    fs_box = FancyBboxPatch(
        (0.0, 1.5),
        2.5,
        2.5,
        boxstyle="round,pad=0.12",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent4"],
        linewidth=2.5,
        zorder=3,
    )
    ax.add_patch(fs_box)
    ax.text(
        1.25,
        3.5,
        "Fragment Shader",
        color=STYLE["accent4"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        1.25,
        2.75,
        "PSOutput {\n  color: SV_Target0\n  normal: SV_Target1\n}",
        color=STYLE["text"],
        fontsize=8,
        ha="center",
        va="center",
        family="monospace",
        path_effects=stroke,
        zorder=5,
    )

    # Three G-buffer textures on the right
    textures = [
        (
            4.0,
            3.5,
            "SV_Target0",
            "scene_color",
            "R8G8B8A8_UNORM",
            "Lit scene color\n(Blinn-Phong + shadow)",
            STYLE["accent1"],
        ),
        (
            4.0,
            1.5,
            "SV_Target1",
            "view_normals",
            "R16G16B16A16_FLOAT",
            "View-space surface normal\n(for SSAO hemisphere)",
            STYLE["accent2"],
        ),
        (
            7.5,
            2.5,
            "Depth attachment",
            "scene_depth",
            "D32_FLOAT",
            "Hardware depth buffer\n(for position reconstruction)",
            STYLE["accent3"],
        ),
    ]

    for x, y, _sem, name, fmt, desc, color in textures:
        box = FancyBboxPatch(
            (x, y),
            3.0,
            1.5,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
            zorder=3,
        )
        ax.add_patch(box)
        ax.text(
            x + 1.5,
            y + 1.2,
            name,
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )
        ax.text(
            x + 1.5,
            y + 0.75,
            fmt,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke,
            zorder=5,
        )
        ax.text(
            x + 1.5,
            y + 0.3,
            desc,
            color=STYLE["text"],
            fontsize=7.5,
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Arrows from shader to textures
    arrow_kw = {
        "arrowstyle": "->,head_width=0.2,head_length=0.12",
        "lw": 2,
    }
    # To scene_color
    ax.annotate(
        "",
        xy=(4.0, 4.25),
        xytext=(2.5, 3.2),
        arrowprops={**arrow_kw, "color": STYLE["accent1"]},
    )
    # To view_normals
    ax.annotate(
        "",
        xy=(4.0, 2.25),
        xytext=(2.5, 2.5),
        arrowprops={**arrow_kw, "color": STYLE["accent2"]},
    )
    # To scene_depth (written automatically by pipeline)
    ax.annotate(
        "",
        xy=(7.5, 3.25),
        xytext=(2.5, 2.75),
        arrowprops={
            **arrow_kw,
            "color": STYLE["accent3"],
            "linestyle": "dashed",
        },
    )
    ax.text(
        5.5,
        3.5,
        "automatic\ndepth write",
        color=STYLE["accent3"],
        fontsize=7,
        ha="center",
        va="center",
        style="italic",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "G-Buffer \u2014 Multiple Render Targets (MRT)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/27-ssao", "gbuffer_mrt.png")


# ---------------------------------------------------------------------------
# gpu/27-ssao — hemisphere_sampling.png
# ---------------------------------------------------------------------------


def diagram_hemisphere_sampling():
    """Hemisphere kernel sampling: samples oriented along surface normal."""
    fig = plt.figure(figsize=(8, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-2.0, 2.0), ylim=(-0.8, 2.5))

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Surface line
    ax.plot([-2.0, 2.0], [0, 0], "-", color=STYLE["text_dim"], lw=2, zorder=2)
    ax.fill_between(
        [-2.0, 2.0],
        [-0.8, -0.8],
        [0, 0],
        color=STYLE["surface"],
        alpha=0.6,
        zorder=1,
    )
    ax.text(
        1.5,
        -0.3,
        "surface",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Fragment position (origin)
    ax.plot(0, 0, "o", color=STYLE["warn"], markersize=10, zorder=6)
    ax.text(
        0.25,
        -0.2,
        "P (fragment)",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Normal vector
    draw_vector(
        ax,
        (0, 0),
        (0, 1.4),
        STYLE["accent3"],
        label="N",
        label_offset=(0.2, 0.0),
        lw=3,
    )

    # Hemisphere outline
    theta = np.linspace(0, np.pi, 100)
    r = 1.2
    hx = r * np.cos(theta)
    hy = r * np.sin(theta)
    ax.plot(hx, hy, "--", color=STYLE["accent1"], lw=1.5, alpha=0.6, zorder=2)
    ax.text(
        -1.35,
        0.75,
        f"radius = {r}",
        color=STYLE["accent1"],
        fontsize=9,
        style="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Generate sample points using the same quadratic distribution
    rng = np.random.RandomState(42)
    n_samples = 40
    for i in range(n_samples):
        # Random direction in hemisphere
        angle = rng.uniform(0, np.pi)
        t = i / n_samples
        scale = 0.1 + 0.9 * t * t
        rand_r = rng.uniform(0.05, 1.0) * scale * r

        sx = rand_r * np.cos(angle)
        sy = rand_r * np.sin(angle)

        # Color by distance: near=bright, far=dim
        alpha = 0.4 + 0.6 * (1.0 - rand_r / r)
        ax.plot(
            sx,
            sy,
            "o",
            color=STYLE["accent2"],
            markersize=4,
            alpha=alpha,
            zorder=4,
        )

    # Mark an example occluded sample
    ax.plot(0.4, 0.35, "o", color=STYLE["accent2"], markersize=7, zorder=6)
    ax.text(
        0.62,
        0.35,
        "sample",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Nearby occluding geometry (a wall/box cross-section)
    wall_x = [0.6, 0.6, 0.8, 0.8]
    wall_y = [0.0, 0.9, 0.9, 0.0]
    ax.fill(wall_x, wall_y, color=STYLE["grid"], alpha=0.7, zorder=3)
    ax.plot(wall_x, wall_y, "-", color=STYLE["text_dim"], lw=1.5, zorder=3)
    ax.text(
        0.7,
        1.05,
        "occluder",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Annotation: "occluded" arrow
    ax.annotate(
        "occluded\n(behind surface)",
        xy=(0.4, 0.35),
        xytext=(-1.0, 1.5),
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        fontweight="bold",
        path_effects=stroke,
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent2"],
            "lw": 1.5,
        },
        zorder=5,
    )

    ax.set_title(
        "Hemisphere Kernel Sampling",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/27-ssao", "hemisphere_sampling.png")


# ---------------------------------------------------------------------------
# gpu/27-ssao — kernel_distribution.png
# ---------------------------------------------------------------------------


def diagram_kernel_distribution():
    """Quadratic vs uniform kernel sample distribution."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Left: uniform distribution
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-1.5, 1.5), ylim=(-0.3, 1.6))

    theta = np.linspace(0, np.pi, 100)
    ax1.plot(
        1.2 * np.cos(theta),
        1.2 * np.sin(theta),
        "--",
        color=STYLE["accent1"],
        lw=1.5,
        alpha=0.5,
    )
    ax1.plot([-1.5, 1.5], [0, 0], "-", color=STYLE["text_dim"], lw=1.5)

    rng = np.random.RandomState(123)
    for _ in range(50):
        a = rng.uniform(0, np.pi)
        r = rng.uniform(0.05, 1.15)
        ax1.plot(
            r * np.cos(a),
            r * np.sin(a),
            "o",
            color=STYLE["accent2"],
            markersize=4,
            alpha=0.7,
        )

    ax1.plot(0, 0, "o", color=STYLE["warn"], markersize=8, zorder=6)
    ax1.set_title(
        "Uniform Distribution",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax1.text(
        0,
        -0.18,
        "Wastes samples far\nfrom surface",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Right: quadratic distribution
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, xlim=(-1.5, 1.5), ylim=(-0.3, 1.6))

    ax2.plot(
        1.2 * np.cos(theta),
        1.2 * np.sin(theta),
        "--",
        color=STYLE["accent1"],
        lw=1.5,
        alpha=0.5,
    )
    ax2.plot([-1.5, 1.5], [0, 0], "-", color=STYLE["text_dim"], lw=1.5)

    rng2 = np.random.RandomState(123)
    n_samp = 50
    for i in range(n_samp):
        a = rng2.uniform(0, np.pi)
        t = i / n_samp
        scale = 0.1 + 0.9 * t * t  # quadratic falloff
        r = rng2.uniform(0.05, 1.0) * scale * 1.15
        ax2.plot(
            r * np.cos(a),
            r * np.sin(a),
            "o",
            color=STYLE["accent2"],
            markersize=4,
            alpha=0.7,
        )

    ax2.plot(0, 0, "o", color=STYLE["warn"], markersize=8, zorder=6)
    ax2.set_title(
        "Quadratic Falloff (t\u00b2)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax2.text(
        0,
        -0.18,
        "Concentrates samples\nnear the surface",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    fig.suptitle(
        "SSAO Kernel Sample Distribution",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )

    fig.tight_layout()
    save(fig, "gpu/27-ssao", "kernel_distribution.png")


# ---------------------------------------------------------------------------
# gpu/27-ssao — depth_reconstruction.png
# ---------------------------------------------------------------------------


def diagram_depth_reconstruction():
    """Transform chain from depth buffer to view-space position."""
    from matplotlib.patches import FancyBboxPatch

    fig = plt.figure(figsize=(11, 4), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 11), ylim=(-0.5, 3.5), grid=False, aspect=None)
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Four boxes representing coordinate spaces
    spaces = [
        (0.0, 1.0, "UV Space", "[0, 1]", STYLE["accent1"]),
        (2.8, 1.0, "NDC", "[\u22121, 1]", STYLE["accent2"]),
        (5.6, 1.0, "Clip Space", "[x, y, z, w]", STYLE["accent3"]),
        (8.4, 1.0, "View Space", "[x/w, y/w, z/w]", STYLE["warn"]),
    ]

    for x, y, label, sublabel, color in spaces:
        box = FancyBboxPatch(
            (x, y),
            2.2,
            1.5,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
            zorder=3,
        )
        ax.add_patch(box)
        ax.text(
            x + 1.1,
            y + 1.05,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )
        ax.text(
            x + 1.1,
            y + 0.45,
            sublabel,
            color=STYLE["text_dim"],
            fontsize=10,
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke,
            zorder=5,
        )

    # Arrows between spaces with transform labels
    arrow_kw = {"arrowstyle": "->,head_width=0.2,head_length=0.12", "lw": 2}
    transforms = [
        (2.2, 2.8, "uv\u00d72 \u2212 1\nflip Y", STYLE["text"]),
        (5.0, 5.6, "float4(\n  ndc, depth, 1)", STYLE["text"]),
        (7.8, 8.4, "inv_proj \u00d7\nthen /w", STYLE["text"]),
    ]

    for x_start, x_end, label, color in transforms:
        ax.annotate(
            "",
            xy=(x_end, 1.75),
            xytext=(x_start, 1.75),
            arrowprops={**arrow_kw, "color": STYLE["text_dim"]},
        )
        ax.text(
            (x_start + x_end) / 2,
            2.85,
            label,
            color=color,
            fontsize=8,
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke,
            zorder=5,
        )

    # Input label
    ax.text(
        1.1,
        0.6,
        "from depth\nbuffer sample",
        color=STYLE["accent1"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
        path_effects=stroke,
        zorder=5,
    )
    # Output label
    ax.text(
        9.5,
        0.6,
        "fragment position\nin camera space",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "Depth Reconstruction \u2014 Depth Buffer to View-Space Position",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/27-ssao", "depth_reconstruction.png")


# ---------------------------------------------------------------------------
# gpu/27-ssao — tbn_construction.png
# ---------------------------------------------------------------------------


def diagram_tbn_construction():
    """Gram-Schmidt orthonormalization building a TBN basis from N + random."""
    fig = plt.figure(figsize=(8, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-1.5, 2.0), ylim=(-0.5, 2.5))

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    origin = (0, 0)

    # Normal vector N (pointing up-right, representing an arbitrary surface)
    N = (0.3, 1.3)
    draw_vector(
        ax,
        origin,
        N,
        STYLE["accent3"],
        label="N (normal)",
        label_offset=(0.35, 0.0),
        lw=3,
    )

    # Random vector R (from noise texture)
    R = (1.2, 0.8)
    draw_vector(
        ax,
        origin,
        R,
        STYLE["accent4"],
        label="R (noise)",
        label_offset=(0.15, 0.2),
        lw=2,
    )

    # Show the projection of R onto N
    N_arr = np.array(N)
    R_arr = np.array(R)
    proj_scale = np.dot(R_arr, N_arr) / np.dot(N_arr, N_arr)
    proj = N_arr * proj_scale

    # Dashed line showing projection
    ax.plot(
        [R[0], proj[0]],
        [R[1], proj[1]],
        "--",
        color=STYLE["text_dim"],
        lw=1.5,
        alpha=0.6,
    )
    ax.plot(proj[0], proj[1], "o", color=STYLE["text_dim"], markersize=5)
    ax.text(
        proj[0] + 0.15,
        proj[1] - 0.1,
        "proj\u2099R",
        color=STYLE["text_dim"],
        fontsize=9,
        path_effects=stroke,
        zorder=5,
    )

    # Tangent vector T = normalize(R - proj_N(R))
    T_raw = R_arr - N_arr * np.dot(R_arr, N_arr) / np.dot(N_arr, N_arr)
    T_norm = T_raw / np.linalg.norm(T_raw)
    T_scaled = T_norm * 1.2
    draw_vector(
        ax,
        origin,
        tuple(T_scaled),
        STYLE["accent1"],
        label="T (tangent)",
        label_offset=(0.0, -0.25),
        lw=3,
    )

    # Bitangent B = cross(N, T) — show as a dot (pointing out of the screen)
    ax.text(
        -0.7,
        1.8,
        "B = cross(N, T)\n(out of screen)",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.plot(-0.7, 1.5, "o", color=STYLE["accent2"], markersize=12, zorder=6)
    # Inner dot for "out of screen" convention
    ax.plot(-0.7, 1.5, "o", color=STYLE["bg"], markersize=6, zorder=7)
    ax.plot(-0.7, 1.5, "o", color=STYLE["accent2"], markersize=3, zorder=8)

    # Right-angle mark between T and N
    rsize = 0.1
    T_hat = T_norm
    N_hat = N_arr / np.linalg.norm(N_arr)
    sq = np.array(
        [
            T_hat * rsize,
            T_hat * rsize + N_hat * rsize,
            N_hat * rsize,
        ]
    )
    ax.plot(sq[:, 0], sq[:, 1], "-", color=STYLE["text_dim"], lw=1)

    # Formula annotation
    ax.text(
        -1.0,
        -0.3,
        "T = normalize(R \u2212 N \u00b7 dot(R, N))",
        color=STYLE["text"],
        fontsize=10,
        family="monospace",
        path_effects=stroke,
        zorder=5,
    )

    # Origin dot
    ax.plot(0, 0, "o", color=STYLE["warn"], markersize=8, zorder=9)
    ax.text(
        0.1,
        -0.15,
        "P",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "TBN Construction \u2014 Gram-Schmidt Orthonormalization",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/27-ssao", "tbn_construction.png")


# ---------------------------------------------------------------------------
# gpu/27-ssao — occlusion_test.png
# ---------------------------------------------------------------------------


def diagram_occlusion_test():
    """How a single SSAO sample is tested: project, depth compare, range check."""
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 10.0), ylim=(-0.5, 5.8), grid=False, aspect=None)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # ---- Geometry: floor + step block ----
    floor_y = 0.5
    step_left, step_right, step_top = 3.0, 4.0, 2.5

    # Filled cross-section (floor extends full width, step rises in the middle)
    geom_x = [-0.5, -0.5, step_left, step_left, step_right, step_right, 10.0, 10.0]
    geom_y = [-0.5, floor_y, floor_y, step_top, step_top, floor_y, floor_y, -0.5]
    ax.fill(geom_x, geom_y, color=STYLE["surface"], alpha=0.7, zorder=1)

    # Visible surface outline
    outline_x = [-0.5, step_left, step_left, step_right, step_right, 10.0]
    outline_y = [floor_y, floor_y, step_top, step_top, floor_y, floor_y]
    ax.plot(outline_x, outline_y, "-", color=STYLE["text_dim"], lw=2, zorder=2)

    # ---- Camera ----
    cam_x, cam_y = 0.5, 4.5
    ax.plot(cam_x, cam_y, "s", color=STYLE["text"], markersize=12, zorder=6)
    ax.text(
        cam_x,
        cam_y + 0.3,
        "camera",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=5,
    )

    # ---- Fragment P on the floor just past the step ----
    px, py = 4.5, 0.5
    ax.plot(px, py, "o", color=STYLE["warn"], markersize=10, zorder=6)
    ax.text(
        px,
        py - 0.3,
        "P",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Normal at P (pointing up from the floor)
    draw_vector(
        ax,
        (px, py),
        (0, 1.2),
        STYLE["accent3"],
        label="N",
        label_offset=(0.25, 0.0),
        lw=2.5,
    )

    # ---- Hemisphere arc (dashed) showing the sampling region above P ----
    hemisphere_r = 2.0
    theta = np.linspace(0, np.pi, 60)
    arc_x = px + hemisphere_r * np.cos(theta)
    arc_y = py + hemisphere_r * np.sin(theta)
    ax.plot(arc_x, arc_y, "--", color=STYLE["text_dim"], lw=1, alpha=0.5, zorder=3)

    # ---- S1: occluded sample (toward the step) ----
    s1x, s1y = 3.5, 2.0
    ax.plot(s1x, s1y, "D", color=STYLE["accent2"], markersize=8, zorder=6)
    ax.text(
        s1x - 0.3,
        s1y + 0.55,
        "S\u2081",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Dotted line from P to S1 (hemisphere sample direction)
    ax.plot(
        [px, s1x], [py, s1y], ":", color=STYLE["text_dim"], lw=1, alpha=0.4, zorder=3
    )

    # Camera projection ray through S1 → hits step front face at x = 3.0
    # Ray dir: (3.0, -2.5); at x=3.0 ⇒ t=5/6, y = 4.5 − 2.5·(5/6) ≈ 2.417
    hit1_x = 3.0
    hit1_y = cam_y + (s1y - cam_y) * (hit1_x - cam_x) / (s1x - cam_x)
    ax.plot(
        [cam_x, hit1_x],
        [cam_y, hit1_y],
        "--",
        color=STYLE["accent2"],
        lw=1.2,
        alpha=0.6,
        zorder=3,
    )
    ax.plot(
        hit1_x,
        hit1_y,
        "x",
        color=STYLE["accent2"],
        markersize=10,
        markeredgewidth=2.5,
        zorder=6,
    )
    ax.text(
        hit1_x - 0.15,
        hit1_y - 0.35,
        "depth buffer\nsurface",
        color=STYLE["accent2"],
        fontsize=7,
        ha="right",
        va="top",
        path_effects=stroke,
        zorder=5,
    )

    # Annotation: OCCLUDED — step face is closer to camera than S1
    ax.annotate(
        "OCCLUDED\nstored surface closer\nto camera",
        xy=(s1x, s1y),
        xytext=(1.8, 3.5),
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent2"],
            "lw": 1.5,
        },
        zorder=5,
    )

    # ---- S2: not occluded sample (away from the step, open air) ----
    s2x, s2y = 6.0, 1.8
    ax.plot(s2x, s2y, "D", color=STYLE["accent1"], markersize=8, zorder=6)
    ax.text(
        s2x + 0.35,
        s2y + 0.55,
        "S\u2082",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Dotted line from P to S2 (hemisphere sample direction)
    ax.plot(
        [px, s2x], [py, s2y], ":", color=STYLE["text_dim"], lw=1, alpha=0.4, zorder=3
    )

    # Camera projection ray through S2 → clears step, hits floor at y = 0.5
    # Ray dir: (5.5, -2.7); at y=0.5 ⇒ t=4/2.7, x = 0.5 + 5.5·(4/2.7) ≈ 8.648
    hit2_y = floor_y
    t2 = (hit2_y - cam_y) / (s2y - cam_y)
    hit2_x = cam_x + (s2x - cam_x) * t2
    ax.plot(
        [cam_x, hit2_x],
        [cam_y, hit2_y],
        "--",
        color=STYLE["accent1"],
        lw=1.2,
        alpha=0.6,
        zorder=3,
    )
    ax.plot(
        hit2_x,
        hit2_y,
        "x",
        color=STYLE["accent1"],
        markersize=10,
        markeredgewidth=2.5,
        zorder=6,
    )
    ax.text(
        hit2_x + 0.15,
        hit2_y + 0.3,
        "depth buffer\nsurface",
        color=STYLE["accent1"],
        fontsize=7,
        ha="left",
        va="bottom",
        path_effects=stroke,
        zorder=5,
    )

    # Annotation: NOT OCCLUDED — floor is farther from camera than S2
    ax.annotate(
        "NOT OCCLUDED\nstored surface farther\nfrom camera",
        xy=(s2x, s2y),
        xytext=(8.2, 3.5),
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent1"],
            "lw": 1.5,
        },
        zorder=5,
    )

    ax.set_title(
        "Occlusion Test \u2014 Depth Buffer Comparison per Sample",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/27-ssao", "occlusion_test.png")


# ---------------------------------------------------------------------------
# gpu/27-ssao — range_check.png
# ---------------------------------------------------------------------------


def diagram_range_check():
    """Range check prevents distant geometry from causing false occlusion."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Left: without range check
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, xlim=(-0.5, 6), ylim=(-0.5, 4), grid=False, aspect=None)

    # Near surface
    ax1.plot([0, 3], [1, 1], "-", color=STYLE["text_dim"], lw=2)
    ax1.fill_between([0, 3], [0, 0], [1, 1], color=STYLE["surface"], alpha=0.5)

    # Far surface (floor far below)
    ax1.plot([3.5, 6], [0.2, 0.2], "-", color=STYLE["text_dim"], lw=2)
    ax1.fill_between([3.5, 6], [0, 0], [0.2, 0.2], color=STYLE["surface"], alpha=0.5)

    # Fragment on near surface
    ax1.plot(2.0, 1.0, "o", color=STYLE["warn"], markersize=8, zorder=6)
    ax1.text(
        2.0,
        1.3,
        "P",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
        zorder=5,
    )

    # Sample projected to far surface
    ax1.plot(4.5, 1.6, "D", color=STYLE["accent2"], markersize=7, zorder=6)
    ax1.text(
        4.5,
        1.9,
        "sample",
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
        zorder=5,
    )
    ax1.plot([2.0, 4.5], [1.0, 1.6], "--", color=STYLE["accent2"], lw=1, alpha=0.5)

    # Stored depth much closer
    ax1.plot(
        4.5,
        0.2,
        "x",
        color=STYLE["accent2"],
        markersize=10,
        markeredgewidth=2,
        zorder=6,
    )
    ax1.plot([4.5, 4.5], [0.2, 1.6], ":", color=STYLE["accent2"], lw=1.5, alpha=0.5)

    ax1.text(
        3.0,
        3.2,
        "FALSE POSITIVE\nLarge depth difference\nbut counts as occluded!",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax1.set_title(
        "Without Range Check",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    # Right: with range check
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, xlim=(-0.5, 6), ylim=(-0.5, 4), grid=False, aspect=None)

    # Same geometry
    ax2.plot([0, 3], [1, 1], "-", color=STYLE["text_dim"], lw=2)
    ax2.fill_between([0, 3], [0, 0], [1, 1], color=STYLE["surface"], alpha=0.5)
    ax2.plot([3.5, 6], [0.2, 0.2], "-", color=STYLE["text_dim"], lw=2)
    ax2.fill_between([3.5, 6], [0, 0], [0.2, 0.2], color=STYLE["surface"], alpha=0.5)

    ax2.plot(2.0, 1.0, "o", color=STYLE["warn"], markersize=8, zorder=6)
    ax2.text(
        2.0,
        1.3,
        "P",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
        zorder=5,
    )

    ax2.plot(4.5, 1.6, "D", color=STYLE["accent1"], markersize=7, zorder=6)
    ax2.text(
        4.5,
        1.9,
        "sample",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
        zorder=5,
    )
    ax2.plot([2.0, 4.5], [1.0, 1.6], "--", color=STYLE["accent1"], lw=1, alpha=0.5)

    ax2.plot(
        4.5,
        0.2,
        "x",
        color=STYLE["accent1"],
        markersize=10,
        markeredgewidth=2,
        zorder=6,
    )
    ax2.plot([4.5, 4.5], [0.2, 1.6], ":", color=STYLE["accent1"], lw=1.5, alpha=0.5)

    ax2.text(
        3.0,
        3.2,
        "REJECTED\nsmoothstep attenuates\nwhen |P.z \u2212 S.z| > radius",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax2.set_title(
        "With Range Check (smoothstep)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    fig.suptitle(
        "Range Check \u2014 Preventing False Occlusion from Distant Surfaces",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )

    fig.tight_layout()
    save(fig, "gpu/27-ssao", "range_check.png")


# ---------------------------------------------------------------------------
# gpu/27-ssao — noise_and_blur.png
# ---------------------------------------------------------------------------


def diagram_noise_and_blur():
    """Before/after: raw SSAO with tiling pattern vs blurred result."""
    fig = plt.figure(figsize=(10, 5), facecolor=STYLE["bg"])

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Generate synthetic AO-like data
    rng = np.random.RandomState(42)
    size = 64

    # Base AO: smooth gradient simulating a corner
    y_grid, x_grid = np.mgrid[0:size, 0:size]
    corner_dist = np.sqrt((x_grid - size * 0.3) ** 2 + (y_grid - size * 0.3) ** 2)
    base_ao = np.clip(corner_dist / (size * 0.6), 0, 1)

    # Add 4x4 tiling noise pattern for raw SSAO
    noise_tile = rng.uniform(-0.15, 0.15, (4, 4))
    tiled_noise = np.tile(noise_tile, (size // 4, size // 4))
    raw_ao = np.clip(base_ao + tiled_noise, 0, 1)

    # Box blur 4x4 of the raw AO (edge-padded to avoid wrap-around artifacts)
    kernel_size = 4
    padded = np.pad(raw_ao, pad_width=((1, 2), (1, 2)), mode="edge")
    blurred_ao = np.zeros_like(raw_ao)
    for dy in range(kernel_size):
        for dx in range(kernel_size):
            blurred_ao += padded[dy : dy + size, dx : dx + size]
    blurred_ao /= kernel_size * kernel_size

    # Left: raw SSAO
    ax1 = fig.add_subplot(121)
    setup_axes(ax1, grid=False, aspect="equal")
    ax1.imshow(raw_ao, cmap="gray", vmin=0, vmax=1, origin="lower")
    ax1.set_xticks([])
    ax1.set_yticks([])
    ax1.set_title(
        "Raw SSAO Output",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax1.text(
        size / 2,
        -5,
        "Visible 4\u00d74 tile pattern\nfrom noise texture",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
        zorder=5,
    )

    # Right: blurred SSAO
    ax2 = fig.add_subplot(122)
    setup_axes(ax2, grid=False, aspect="equal")
    ax2.imshow(blurred_ao, cmap="gray", vmin=0, vmax=1, origin="lower")
    ax2.set_xticks([])
    ax2.set_yticks([])
    ax2.set_title(
        "After 4\u00d74 Box Blur",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )
    ax2.text(
        size / 2,
        -5,
        "Smooth AO factor\nready for compositing",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
        zorder=5,
    )

    # Arrow between panels
    fig.text(
        0.50,
        0.5,
        "\u2192",
        color=STYLE["warn"],
        fontsize=28,
        ha="center",
        va="center",
        fontweight="bold",
    )
    fig.text(
        0.50,
        0.42,
        "4\u00d74 box blur",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
    )

    fig.suptitle(
        "Blur Pass \u2014 Removing the Noise Tile Pattern",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )

    fig.tight_layout()
    save(fig, "gpu/27-ssao", "noise_and_blur.png")


# ---------------------------------------------------------------------------
# gpu/27-ssao — composite_modes.png
# ---------------------------------------------------------------------------


def diagram_composite_modes():
    """Three display modes: AO only, full render + AO, full render without AO."""
    fig = plt.figure(figsize=(10, 4), facecolor=STYLE["bg"])

    # Generate synthetic data for each mode
    size = 48
    y_g, x_g = np.mgrid[0:size, 0:size]
    corner_dist = np.sqrt((x_g - size * 0.25) ** 2 + (y_g - size * 0.25) ** 2)
    ao = np.clip(corner_dist / (size * 0.5), 0, 1)

    # Synthetic scene color (gradient to simulate lit surface)
    scene_r = np.clip(0.3 + 0.5 * x_g / size, 0, 1)
    scene_g = np.clip(0.2 + 0.3 * y_g / size, 0, 1)
    scene_b = np.full_like(scene_r, 0.15)
    scene_color = np.stack([scene_r, scene_g, scene_b], axis=-1)

    modes = [
        ("AO Only (Key 1)", np.stack([ao, ao, ao], axis=-1)),
        ("With AO (Key 2)", scene_color * ao[..., np.newaxis]),
        ("Without AO (Key 3)", scene_color),
    ]

    colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]

    for i, ((title, img), color) in enumerate(zip(modes, colors)):
        ax = fig.add_subplot(1, 3, i + 1)
        setup_axes(ax, grid=False, aspect="equal")
        ax.imshow(np.clip(img, 0, 1), origin="lower")
        ax.set_xticks([])
        ax.set_yticks([])
        ax.set_title(
            title,
            color=color,
            fontsize=11,
            fontweight="bold",
            pad=12,
        )

    fig.suptitle(
        "Composite Display Modes",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )

    fig.tight_layout()
    save(fig, "gpu/27-ssao", "composite_modes.png")


# ---------------------------------------------------------------------------
# gpu/29-screen-space-reflections — render_pipeline.png
# ---------------------------------------------------------------------------


def _ssr_draw_pass_box(ax, x, y, w, h, title, number, color, stroke):
    """Draw a rounded pass box with a numbered title."""
    from matplotlib.patches import FancyBboxPatch

    rect = FancyBboxPatch(
        (x, y),
        w,
        h,
        boxstyle="round,pad=0.12",
        linewidth=2.5,
        edgecolor=color,
        facecolor=STYLE["surface"],
        alpha=0.9,
        zorder=3,
    )
    ax.add_patch(rect)
    ax.text(
        x + w / 2,
        y + h * 0.65,
        f"{number}. {title}",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=4,
    )


def _ssr_draw_texture_tag(ax, x, y, label, fmt, color, stroke_thin):
    """Draw a small texture output tag (label + format)."""
    tag_w = 1.55
    tag_h = 0.55
    rect = Rectangle(
        (x - tag_w / 2, y - tag_h / 2),
        tag_w,
        tag_h,
        linewidth=1.2,
        edgecolor=color,
        facecolor=STYLE["bg"],
        alpha=0.85,
        zorder=5,
    )
    ax.add_patch(rect)
    ax.text(
        x,
        y + 0.07,
        label,
        color=color,
        fontsize=7.5,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=6,
    )
    ax.text(
        x,
        y - 0.17,
        fmt,
        color=STYLE["text_dim"],
        fontsize=6,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=6,
    )


def _ssr_draw_arrow(ax, x_start, y_start, x_end, y_end, color=None):
    """Draw a connecting arrow between elements."""
    if color is None:
        color = STYLE["text_dim"]
    ax.annotate(
        "",
        xy=(x_end, y_end),
        xytext=(x_start, y_start),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.12",
            "color": color,
            "lw": 2,
        },
        zorder=2,
    )


def diagram_ssr_render_pipeline():
    """SSR render pipeline: 4 passes with texture outputs flowing between them."""
    stroke = [pe.withStroke(linewidth=2.5, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    fig, ax = plt.subplots(figsize=(16, 7.5), facecolor=STYLE["bg"])
    ax.set_xlim(-0.5, 16)
    ax.set_ylim(-4.5, 4.5)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.axis("off")

    # --- Pass box dimensions and positions ---
    box_w = 2.8
    box_h = 1.8
    box_y = 0.8
    gap = 1.2

    x1 = 0.3
    x2 = x1 + box_w + gap
    x3 = x2 + box_w + gap
    x4 = x3 + box_w + gap

    pass_colors = [
        STYLE["accent4"],  # Shadow — purple
        STYLE["accent1"],  # G-Buffer — cyan
        STYLE["accent2"],  # SSR Ray March — orange
        STYLE["accent3"],  # Composite — green
    ]

    # --- Draw the four pass boxes ---
    _ssr_draw_pass_box(
        ax, x1, box_y, box_w, box_h, "Shadow\nPass", 1, pass_colors[0], stroke
    )
    _ssr_draw_pass_box(
        ax, x2, box_y, box_w, box_h, "G-Buffer\nPass", 2, pass_colors[1], stroke
    )
    _ssr_draw_pass_box(
        ax, x3, box_y, box_w, box_h, "SSR Ray\nMarch", 3, pass_colors[2], stroke
    )
    _ssr_draw_pass_box(
        ax, x4, box_y, box_w, box_h, "Composite", 4, pass_colors[3], stroke
    )

    # --- Arrows between pass boxes ---
    arrow_y = box_y + box_h / 2
    for src_x, dst_x in [(x1, x2), (x2, x3), (x3, x4)]:
        _ssr_draw_arrow(ax, src_x + box_w + 0.08, arrow_y, dst_x - 0.08, arrow_y)

    # --- Shadow Pass output: single Depth texture ---
    shadow_out_y = box_y - 0.9
    shadow_cx = x1 + box_w / 2
    _ssr_draw_texture_tag(
        ax, shadow_cx, shadow_out_y, "Shadow Depth", "D32F", pass_colors[0], stroke_thin
    )
    _ssr_draw_arrow(
        ax, shadow_cx, box_y - 0.02, shadow_cx, shadow_out_y + 0.3, pass_colors[0]
    )

    # --- G-Buffer Pass outputs: 4 textures fanning out below ---
    gbuf_textures = [
        ("Color", "RGBA8"),
        ("Normals", "RGBA16F"),
        ("World Pos", "RGBA16F"),
        ("Depth", "D32F"),
    ]
    gbuf_start_x = x2 - 0.6
    gbuf_spacing = 1.7
    gbuf_out_y = box_y - 1.2
    gbuf_tag_y = gbuf_out_y - 0.7

    gbuf_cx = x2 + box_w / 2

    for i, (label, fmt) in enumerate(gbuf_textures):
        tag_x = gbuf_start_x + i * gbuf_spacing
        _ssr_draw_texture_tag(
            ax, tag_x, gbuf_tag_y, label, fmt, pass_colors[1], stroke_thin
        )
        _ssr_draw_arrow(
            ax, gbuf_cx, box_y - 0.15, tag_x, gbuf_tag_y + 0.35, pass_colors[1]
        )

    # --- Curved arrows from G-Buffer textures up into SSR Ray March ---
    ssr_cx = x3 + box_w / 2

    ax.text(
        (gbuf_start_x + gbuf_start_x + 3 * gbuf_spacing) / 2,
        gbuf_tag_y - 0.7,
        "All G-Buffer textures feed into SSR",
        color=STYLE["text_dim"],
        fontsize=8,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=4,
    )

    for i in range(4):
        tag_x = gbuf_start_x + i * gbuf_spacing
        ax.annotate(
            "",
            xy=(ssr_cx - 0.8 + i * 0.55, box_y + 0.05),
            xytext=(tag_x, gbuf_tag_y - 0.3),
            arrowprops={
                "arrowstyle": "->,head_width=0.15,head_length=0.08",
                "color": pass_colors[1],
                "lw": 1.2,
                "alpha": 0.5,
                "connectionstyle": "arc3,rad=-0.25",
            },
            zorder=2,
        )

    # --- Shadow map also feeds into G-Buffer pass (for shadow testing) ---
    ax.annotate(
        "",
        xy=(x2, box_y + box_h * 0.3),
        xytext=(shadow_cx + 0.6, shadow_out_y + 0.05),
        arrowprops={
            "arrowstyle": "->,head_width=0.15,head_length=0.08",
            "color": pass_colors[0],
            "lw": 1.2,
            "alpha": 0.5,
            "connectionstyle": "arc3,rad=0.3",
        },
        zorder=2,
    )

    # --- SSR Ray March output ---
    ssr_out_y = box_y - 0.9
    ssr_out_cx = x3 + box_w / 2
    _ssr_draw_texture_tag(
        ax, ssr_out_cx, ssr_out_y, "SSR Result", "RGBA8", pass_colors[2], stroke_thin
    )
    _ssr_draw_arrow(
        ax, ssr_out_cx, box_y - 0.02, ssr_out_cx, ssr_out_y + 0.3, pass_colors[2]
    )

    # --- Composite inputs: Scene Color (from G-Buffer Color) + SSR Result ---
    # Arrow from SSR Result to Composite
    ax.annotate(
        "",
        xy=(x4, box_y + box_h * 0.3),
        xytext=(ssr_out_cx + 0.6, ssr_out_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.18,head_length=0.10",
            "color": pass_colors[2],
            "lw": 1.5,
            "alpha": 0.7,
            "connectionstyle": "arc3,rad=-0.3",
        },
        zorder=2,
    )

    # Arrow from G-Buffer Color tag to Composite
    color_tag_x = gbuf_start_x
    ax.annotate(
        "",
        xy=(x4, box_y + box_h * 0.6),
        xytext=(color_tag_x + 0.5, gbuf_tag_y - 0.3),
        arrowprops={
            "arrowstyle": "->,head_width=0.15,head_length=0.08",
            "color": pass_colors[3],
            "lw": 1.2,
            "alpha": 0.5,
            "connectionstyle": "arc3,rad=-0.35",
        },
        zorder=2,
    )

    # --- Composite output: Swapchain ---
    swap_out_y = box_y - 0.9
    swap_cx = x4 + box_w / 2
    _ssr_draw_texture_tag(
        ax, swap_cx, swap_out_y, "Swapchain", "Present", pass_colors[3], stroke_thin
    )
    _ssr_draw_arrow(
        ax, swap_cx, box_y - 0.02, swap_cx, swap_out_y + 0.3, pass_colors[3]
    )

    # --- Input labels above the first pass ---
    ax.text(
        x1 + box_w / 2,
        box_y + box_h + 0.55,
        "Scene geometry",
        color=STYLE["text_dim"],
        fontsize=8.5,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=4,
    )
    _ssr_draw_arrow(
        ax,
        x1 + box_w / 2,
        box_y + box_h + 0.3,
        x1 + box_w / 2,
        box_y + box_h + 0.05,
        STYLE["text_dim"],
    )

    # --- Title ---
    fig.suptitle(
        "SSR Render Pipeline  (4 Passes)",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        y=0.97,
    )

    # --- Subtitle line ---
    ax.text(
        8.0,
        4.2,
        "Shadow  \u2192  G-Buffer  \u2192  SSR Ray March  \u2192  Composite  \u2192  Display",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=4,
    )

    # --- Legend ---
    legend_y = -3.8
    legend_items = [
        (pass_colors[0], "Shadow Pass", "Depth-only render from light view"),
        (
            pass_colors[1],
            "G-Buffer Pass",
            "Geometry attributes to multiple render targets",
        ),
        (pass_colors[2], "SSR Ray March", "Screen-space ray marching for reflections"),
        (pass_colors[3], "Composite", "Blend scene color with SSR result"),
    ]
    legend_x_start = 0.5
    legend_spacing = 3.9

    for i, (color, name, desc) in enumerate(legend_items):
        lx = legend_x_start + i * legend_spacing
        ax.plot(lx, legend_y, "s", color=color, markersize=8, zorder=5)
        ax.text(
            lx + 0.3,
            legend_y + 0.05,
            name,
            color=STYLE["text"],
            fontsize=8,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )
        ax.text(
            lx + 0.3,
            legend_y - 0.45,
            desc,
            color=STYLE["text_dim"],
            fontsize=7,
            ha="left",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

    fig.tight_layout(rect=(0, 0.02, 1, 0.93))
    save(fig, "gpu/29-screen-space-reflections", "render_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/29-screen-space-reflections — ray_march.png
# ---------------------------------------------------------------------------


def _ssr_depth_profile(x):
    """Synthetic depth-buffer profile (side view).

    Returns height values representing the scene surface from the side.
    The profile has a floor, a reflective surface (where P sits), a gap,
    and a tall wall that the reflected ray will hit.
    """
    y = np.ones_like(x, dtype=float) * 0.4
    # Reflective floor / platform where P sits
    mask_plat = (x >= 1.0) & (x < 4.0)
    y[mask_plat] = 1.5
    # Drop to floor after platform
    mask_gap = (x >= 4.0) & (x < 7.0)
    y[mask_gap] = 0.4
    # Small step
    mask_step = (x >= 5.2) & (x < 6.2)
    y[mask_step] = 0.75
    # Tall wall — the reflection target
    mask_wall = (x >= 8.0) & (x < 10.5)
    y[mask_wall] = 2.8
    # Trailing floor
    mask_trail = x >= 10.5
    y[mask_trail] = 0.4
    return y


def diagram_ssr_ray_march():
    """SSR ray march: side-view cross-section showing reflected ray through depth buffer."""
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    fig = plt.figure(figsize=(14, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)

    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-2.5, 13.0)
    ax.set_ylim(-0.5, 4.8)
    ax.set_aspect("equal")
    ax.axis("off")

    # -----------------------------------------------------------------
    # 1. Depth buffer surface profile (the "scene" seen from the side)
    # -----------------------------------------------------------------
    xs = np.linspace(-1.5, 12.5, 3000)
    ys = _ssr_depth_profile(xs)

    ax.fill_between(
        xs,
        -0.5,
        ys,
        color=STYLE["surface"],
        alpha=0.6,
        zorder=1,
    )
    ax.plot(xs, ys, color=STYLE["axis"], lw=2.0, zorder=3)

    ax.text(
        11.5,
        0.15,
        "Depth buffer\n(scene surface)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 2. Camera eye
    # -----------------------------------------------------------------
    cam = np.array([-1.8, 2.5])
    ax.plot(
        cam[0],
        cam[1],
        "D",
        color=STYLE["warn"],
        markersize=11,
        zorder=8,
    )
    ax.text(
        cam[0],
        cam[1] + 0.25,
        "Camera",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 3. Surface point P — top of the reflective platform
    # -----------------------------------------------------------------
    p_point = np.array([2.5, 1.5])
    ax.plot(
        p_point[0],
        p_point[1],
        "o",
        color=STYLE["accent1"],
        markersize=10,
        zorder=8,
    )
    ax.text(
        p_point[0] - 0.3,
        p_point[1] + 0.2,
        "P",
        color=STYLE["accent1"],
        fontsize=14,
        fontweight="bold",
        ha="right",
        va="bottom",
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 4. View direction (camera -> P)
    # -----------------------------------------------------------------
    view_dir = p_point - cam
    view_len = np.linalg.norm(view_dir)
    view_unit = view_dir / view_len

    arrow_start = cam + view_unit * 0.5
    arrow_end = p_point - view_unit * 0.4
    ax.annotate(
        "",
        xy=arrow_end,
        xytext=arrow_start,
        arrowprops={
            "arrowstyle": "->,head_width=0.22,head_length=0.13",
            "color": STYLE["warn"],
            "lw": 2.0,
            "linestyle": "--",
        },
        zorder=5,
    )
    lbl_pos = cam + view_dir * 0.35
    view_angle = np.degrees(np.arctan2(view_unit[1], view_unit[0]))
    ax.text(
        lbl_pos[0] - 0.1,
        lbl_pos[1] + 0.35,
        "View direction",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        rotation=view_angle,
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 5. Surface normal at P (upward)
    # -----------------------------------------------------------------
    normal = np.array([0.0, 1.0])
    normal_len = 0.9
    ax.annotate(
        "",
        xy=(p_point[0], p_point[1] + normal_len),
        xytext=p_point,
        arrowprops={
            "arrowstyle": "->,head_width=0.22,head_length=0.13",
            "color": STYLE["accent3"],
            "lw": 2.5,
        },
        zorder=7,
    )
    ax.text(
        p_point[0] + 0.25,
        p_point[1] + normal_len * 0.6,
        "Normal",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 6. Reflected ray direction
    # -----------------------------------------------------------------
    v_dir = view_unit
    r_dir = v_dir - 2.0 * np.dot(v_dir, normal) * normal
    r_dir = r_dir / np.linalg.norm(r_dir)

    num_steps = 9
    step_size = 0.95
    step_positions = [p_point + r_dir * step_size * (i + 1) for i in range(num_steps)]

    hit_index = None
    thickness = 0.25
    source_depth = _ssr_depth_profile(np.array([p_point[0]]))[0]
    for i, sp in enumerate(step_positions):
        if sp[0] < -1.5 or sp[0] > 12.5:
            continue
        depth_val = _ssr_depth_profile(np.array([sp[0]]))[0]
        # Skip self-intersection with the originating reflective surface.
        if abs(depth_val - source_depth) < 1e-4:
            continue
        if abs(sp[1] - depth_val) <= thickness and hit_index is None:
            hit_index = i
            break

    last_step = hit_index if hit_index is not None else num_steps - 1
    ray_end = step_positions[last_step]
    ax.plot(
        [p_point[0], ray_end[0]],
        [p_point[1], ray_end[1]],
        "--",
        color=STYLE["accent1"],
        lw=1.2,
        alpha=0.4,
        zorder=4,
    )

    label_t = min(2.0, last_step * 0.5)
    label_pos = p_point + r_dir * step_size * label_t
    angle_deg = np.degrees(np.arctan2(r_dir[1], r_dir[0]))
    ax.text(
        label_pos[0],
        label_pos[1] + 0.28,
        "Reflected ray",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        rotation=angle_deg,
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # 7. Ray march steps — dots along the ray, depth comparisons
    # -----------------------------------------------------------------
    for i, sp in enumerate(step_positions):
        if sp[0] < -1.5 or sp[0] > 12.5:
            continue

        depth_val = _ssr_depth_profile(np.array([sp[0]]))[0]
        is_hit = i == hit_index

        if hit_index is not None and i > hit_index:
            break

        dot_color = STYLE["accent2"] if is_hit else STYLE["accent1"]
        dot_size = 10 if is_hit else 6
        ax.plot(
            sp[0],
            sp[1],
            "o",
            color=dot_color,
            markersize=dot_size,
            zorder=8,
        )

        line_color = STYLE["accent2"] if is_hit else STYLE["text_dim"]
        line_alpha = 0.85 if is_hit else 0.35
        ax.plot(
            [sp[0], sp[0]],
            [sp[1], depth_val],
            ":",
            color=line_color,
            lw=1.3,
            alpha=line_alpha,
            zorder=4,
        )

        ax.plot(
            sp[0],
            depth_val,
            "s",
            color=line_color,
            markersize=4,
            alpha=line_alpha,
            zorder=5,
        )

        if not is_hit:
            step_labels = {0: "Step 1", 1: "Step 2", 2: "Step 3"}
            if i in step_labels:
                ax.text(
                    sp[0] + 0.2,
                    sp[1] + 0.18,
                    step_labels[i],
                    color=STYLE["text_dim"],
                    fontsize=8,
                    ha="left",
                    va="bottom",
                    path_effects=stroke,
                    zorder=10,
                )

    # -----------------------------------------------------------------
    # 8. Hit point annotation
    # -----------------------------------------------------------------
    if hit_index is not None:
        hp = step_positions[hit_index]
        depth_at_hit = _ssr_depth_profile(np.array([hp[0]]))[0]

        ax.text(
            hp[0] + 0.4,
            hp[1] + 0.25,
            "Hit!",
            color=STYLE["accent2"],
            fontsize=15,
            fontweight="bold",
            ha="left",
            va="bottom",
            path_effects=stroke,
            zorder=10,
        )

        ax.plot(
            hp[0],
            depth_at_hit,
            "o",
            color=STYLE["accent2"],
            markersize=13,
            markerfacecolor="none",
            markeredgewidth=2.5,
            zorder=9,
        )

        # -----------------------------------------------------------------
        # 9. Thickness threshold bracket
        # -----------------------------------------------------------------
        bracket_x = hp[0] + 0.65
        bracket_top = depth_at_hit + thickness
        bracket_bot = depth_at_hit - thickness

        cap_hw = 0.12
        for by in (bracket_top, bracket_bot):
            ax.plot(
                [bracket_x - cap_hw, bracket_x + cap_hw],
                [by, by],
                "-",
                color=STYLE["accent4"],
                lw=1.5,
                zorder=7,
            )
        ax.annotate(
            "",
            xy=(bracket_x, bracket_top),
            xytext=(bracket_x, bracket_bot),
            arrowprops={
                "arrowstyle": "<->",
                "color": STYLE["accent4"],
                "lw": 1.5,
            },
            zorder=7,
        )
        ax.text(
            bracket_x + 0.25,
            depth_at_hit,
            "Thickness\nthreshold",
            color=STYLE["accent4"],
            fontsize=9,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=10,
        )

    # -----------------------------------------------------------------
    # 10. Legend
    # -----------------------------------------------------------------
    legend_x, legend_y = 10.2, 4.3
    ax.plot(legend_x, legend_y, "o", color=STYLE["accent1"], markersize=6, zorder=8)
    ax.text(
        legend_x + 0.3,
        legend_y,
        "Ray steps",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=10,
    )
    ax.plot(
        legend_x, legend_y - 0.4, "o", color=STYLE["accent2"], markersize=6, zorder=8
    )
    ax.text(
        legend_x + 0.3,
        legend_y - 0.4,
        "Intersection (hit)",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=10,
    )
    ax.plot(
        [legend_x - 0.15, legend_x + 0.15],
        [legend_y - 0.8, legend_y - 0.8],
        ":",
        color=STYLE["text_dim"],
        lw=1.3,
        alpha=0.6,
        zorder=8,
    )
    ax.text(
        legend_x + 0.3,
        legend_y - 0.8,
        "Depth comparison",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=10,
    )

    # -----------------------------------------------------------------
    # Title
    # -----------------------------------------------------------------
    ax.set_title(
        "Screen-Space Reflections: Ray Marching Through the Depth Buffer",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=16,
    )

    save(fig, "gpu/29-screen-space-reflections", "ray_march.png")


# ---------------------------------------------------------------------------
# gpu/29-screen-space-reflections — gbuffer_layout.png
# ---------------------------------------------------------------------------


def diagram_ssr_gbuffer_layout():
    """G-buffer layout: 4 stacked render targets for screen-space reflections."""
    from matplotlib.patches import FancyBboxPatch

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_xlim(-0.5, 10.5)
    ax.set_ylim(-0.8, 8.0)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("auto")
    ax.axis("off")

    # --- G-buffer render targets (stacked bottom to top) ---
    targets = [
        (
            0.0,
            "Depth",
            "D32_FLOAT",
            "Hardware depth buffer",
            STYLE["text_dim"],
        ),
        (
            1.8,
            "World Position",
            "R16G16B16A16_FLOAT",
            "World-space position  (alpha = reflectivity)",
            STYLE["accent3"],
        ),
        (
            3.6,
            "View Normals",
            "R16G16B16A16_FLOAT",
            "View-space surface normals",
            STYLE["accent2"],
        ),
        (
            5.4,
            "Scene Color",
            "R8G8B8A8_UNORM",
            "Lit color with Blinn-Phong + shadows",
            STYLE["accent1"],
        ),
    ]

    bar_width = 7.0
    bar_height = 1.3
    bar_x = 1.5

    for y, name, fmt, desc, color in targets:
        box = FancyBboxPatch(
            (bar_x, y),
            bar_width,
            bar_height,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.2,
            zorder=3,
        )
        ax.add_patch(box)

        ax.text(
            bar_x + 0.35,
            y + bar_height / 2 + 0.15,
            name,
            color=color,
            fontsize=12,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        ax.text(
            bar_x + 0.35,
            y + bar_height / 2 - 0.25,
            desc,
            color=STYLE["text"],
            fontsize=8.5,
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        ax.text(
            bar_x + bar_width - 0.35,
            y + bar_height / 2,
            fmt,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="right",
            va="center",
            family="monospace",
            path_effects=stroke,
            zorder=5,
        )

    # --- Bracket and label on the left ---
    bracket_x = 0.9
    bracket_top = 5.4 + bar_height
    bracket_bot = 0.0

    ax.plot(
        [bracket_x, bracket_x],
        [bracket_bot + 0.15, bracket_top - 0.15],
        "-",
        color=STYLE["warn"],
        lw=2,
        zorder=4,
    )
    ax.plot(
        [bracket_x, bracket_x + 0.25],
        [bracket_top - 0.15, bracket_top - 0.15],
        "-",
        color=STYLE["warn"],
        lw=2,
        zorder=4,
    )
    ax.plot(
        [bracket_x, bracket_x + 0.25],
        [bracket_bot + 0.15, bracket_bot + 0.15],
        "-",
        color=STYLE["warn"],
        lw=2,
        zorder=4,
    )

    mid_y = (bracket_top + bracket_bot) / 2
    ax.text(
        bracket_x - 0.15,
        mid_y,
        "G-Buffer",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        rotation=90,
        path_effects=stroke,
        zorder=5,
    )

    # --- SV_Target labels on the right ---
    sv_labels = [
        (5.4, "SV_Target0"),
        (3.6, "SV_Target1"),
        (1.8, "SV_Target2"),
        (0.0, "Depth attachment"),
    ]
    label_x = bar_x + bar_width + 0.5

    for y, label in sv_labels:
        ax.text(
            label_x,
            y + bar_height / 2,
            label,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="left",
            va="center",
            family="monospace",
            style="italic",
            path_effects=stroke,
            zorder=5,
        )

    # --- Title ---
    ax.set_title(
        "G-Buffer Layout \u2014 Screen-Space Reflections",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, "gpu/29-screen-space-reflections", "gbuffer_layout.png")


# ===================================================================
# GPU Lesson 29 — SSR Self-Intersection Guard
# ===================================================================


def diagram_ssr_self_intersection():
    """Visualise why SSR needs a minimum-travel guard to avoid self-hits."""
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    fig, axes = plt.subplots(
        1, 2, figsize=(12, 4.5), facecolor=STYLE["bg"],
        gridspec_kw={"wspace": 0.35},
    )

    for ax in axes:
        ax.set_facecolor(STYLE["bg"])
        ax.set_xlim(-0.5, 10.5)
        ax.set_ylim(-1.0, 5.5)
        ax.set_aspect("equal")
        ax.axis("off")

    # -----------------------------------------------------------------
    # Shared geometry: a flat floor surface and a vertical wall
    # -----------------------------------------------------------------
    def _draw_scene(ax):
        # Floor surface
        ax.plot([-0.5, 10.5], [0.0, 0.0], color=STYLE["axis"], lw=2.5, zorder=3)
        ax.fill_between([-0.5, 10.5], -1.0, 0.0,
                        color=STYLE["surface"], alpha=0.6, zorder=1)
        ax.text(1.0, -0.5, "Reflective floor (depth buffer)",
                color=STYLE["text_dim"], fontsize=8, ha="left",
                path_effects=stroke, zorder=10)

        # Wall / object
        wall_x = 7.0
        ax.plot([wall_x, wall_x], [0.0, 4.5], color=STYLE["axis"], lw=2.5, zorder=3)
        ax.fill_betweenx([0.0, 4.5], wall_x, 10.5,
                         color=STYLE["surface"], alpha=0.6, zorder=1)
        ax.text(8.5, 2.5, "Object", color=STYLE["text_dim"], fontsize=10,
                ha="center", va="center", path_effects=stroke, zorder=10)

    # -----------------------------------------------------------------
    # Panel A: WITHOUT guard — self-hit on first step
    # -----------------------------------------------------------------
    ax_a = axes[0]
    _draw_scene(ax_a)
    ax_a.set_title("Without self-intersection guard",
                   color=STYLE["accent2"], fontsize=11, fontweight="bold", pad=10)

    # Ray origin on the floor
    origin = np.array([2.0, 0.0])
    # Reflected direction (shallow angle upward)
    r_dir = np.array([0.85, 0.25])
    r_dir = r_dir / np.linalg.norm(r_dir)

    # Surface normal
    ax_a.annotate("", xy=(origin[0], origin[1] + 1.2),
                  xytext=origin,
                  arrowprops={"arrowstyle": "->,head_width=0.2,head_length=0.12",
                              "color": STYLE["accent3"], "lw": 2.0},
                  zorder=6)
    ax_a.text(origin[0] - 0.35, origin[1] + 0.7, "N",
              color=STYLE["accent3"], fontsize=11, fontweight="bold",
              ha="center", path_effects=stroke, zorder=10)

    # Draw ray steps — first one is very close to surface and false-hits
    step_size = 0.6
    wall_x = 7.0
    travel_to_wall = (wall_x - origin[0]) / r_dir[0]
    n_steps = int(np.ceil(travel_to_wall / step_size)) + 1
    steps = [origin + r_dir * step_size * (i + 1) for i in range(n_steps)]

    # Step 1 is close to floor — false hit
    false_hit = steps[0]
    # Draw dashed ray up to false hit only
    ax_a.plot([origin[0], false_hit[0]], [origin[1], false_hit[1]],
              "--", color=STYLE["accent1"], lw=1.2, alpha=0.5, zorder=4)

    # Step dots — only first one shown (it's a false hit)
    ax_a.plot(false_hit[0], false_hit[1], "o",
              color=STYLE["accent2"], markersize=10, zorder=8)

    # Depth comparison line from step to floor
    ax_a.plot([false_hit[0], false_hit[0]], [false_hit[1], 0.0],
              ":", color=STYLE["accent2"], lw=1.5, alpha=0.8, zorder=4)
    ax_a.plot(false_hit[0], 0.0, "s",
              color=STYLE["accent2"], markersize=5, zorder=5)

    # Label: false hit
    ax_a.text(false_hit[0] + 0.3, false_hit[1] + 0.3, "False hit!",
              color=STYLE["accent2"], fontsize=11, fontweight="bold",
              ha="left", path_effects=stroke, zorder=10)

    # Small annotation showing the tiny depth diff
    ax_a.annotate("", xy=(false_hit[0] + 0.15, false_hit[1]),
                  xytext=(false_hit[0] + 0.15, 0.0),
                  arrowprops={"arrowstyle": "<->", "color": STYLE["warn"], "lw": 1.2},
                  zorder=7)
    ax_a.text(false_hit[0] + 0.5, false_hit[1] / 2, "tiny\ndepth\ndiff",
              color=STYLE["warn"], fontsize=7, ha="left", va="center",
              path_effects=stroke, zorder=10)

    # Show where the ray SHOULD have gone (ghosted)
    for sp in steps[1:5]:
        ax_a.plot(sp[0], sp[1], "o", color=STYLE["text_dim"],
                  markersize=4, alpha=0.3, zorder=4)
    ax_a.text(5.5, 2.5, "Missed\n(ray stopped\ntoo early)",
              color=STYLE["text_dim"], fontsize=9, ha="center", va="center",
              style="italic", path_effects=stroke, zorder=10)

    # -----------------------------------------------------------------
    # Panel B: WITH guard — skips early steps, finds correct hit
    # -----------------------------------------------------------------
    ax_b = axes[1]
    _draw_scene(ax_b)
    ax_b.set_title("With self-intersection guard",
                   color=STYLE["accent3"], fontsize=11, fontweight="bold", pad=10)

    # Same origin and direction
    ax_b.annotate("", xy=(origin[0], origin[1] + 1.2),
                  xytext=origin,
                  arrowprops={"arrowstyle": "->,head_width=0.2,head_length=0.12",
                              "color": STYLE["accent3"], "lw": 2.0},
                  zorder=6)
    ax_b.text(origin[0] - 0.35, origin[1] + 0.7, "N",
              color=STYLE["accent3"], fontsize=11, fontweight="bold",
              ha="center", path_effects=stroke, zorder=10)

    # Draw all steps — first 2 are skipped (guard zone), then marching
    guard_steps = 2  # steps within SSR_MIN_TRAVEL
    hit_step = None

    for i, sp in enumerate(steps):
        if sp[0] >= 7.0:
            hit_step = i
            break

        if i < guard_steps:
            # Skipped steps — dimmed, with "skip" marker
            ax_b.plot(sp[0], sp[1], "o", color=STYLE["text_dim"],
                      markersize=6, alpha=0.4, zorder=6)
            ax_b.plot(sp[0], sp[1], "x", color=STYLE["accent2"],
                      markersize=8, markeredgewidth=2.0, alpha=0.7, zorder=7)
        else:
            # Active steps — normal color
            ax_b.plot(sp[0], sp[1], "o", color=STYLE["accent1"],
                      markersize=6, zorder=8)
            # Depth comparison line
            ax_b.plot([sp[0], sp[0]], [sp[1], 0.0],
                      ":", color=STYLE["text_dim"], lw=1.0, alpha=0.3, zorder=4)

    # Draw ray line up to hit point (only show wall hit if a step reached it)
    if hit_step is not None:
        t_wall = (wall_x - origin[0]) / r_dir[0]
        hit_on_wall = origin + r_dir * t_wall

        ax_b.plot([origin[0], hit_on_wall[0]], [origin[1], hit_on_wall[1]],
                  "--", color=STYLE["accent1"], lw=1.2, alpha=0.5, zorder=4)

        # Hit marker on wall
        ax_b.plot(hit_on_wall[0], hit_on_wall[1], "o",
                  color=STYLE["accent2"], markersize=10, zorder=8)
        ax_b.plot(hit_on_wall[0], hit_on_wall[1], "o",
                  color=STYLE["accent2"], markersize=16, markerfacecolor="none",
                  markeredgewidth=2.5, zorder=9)
        ax_b.text(hit_on_wall[0] - 0.4, hit_on_wall[1] + 0.4, "Correct hit!",
                  color=STYLE["accent2"], fontsize=11, fontweight="bold",
                  ha="right", path_effects=stroke, zorder=10)
    else:
        # No step reached the wall — draw the full ray with a "miss" label
        last = steps[-1]
        ax_b.plot([origin[0], last[0]], [origin[1], last[1]],
                  "--", color=STYLE["accent1"], lw=1.2, alpha=0.5, zorder=4)
        ax_b.text(last[0] + 0.3, last[1], "No hit",
                  color=STYLE["text_dim"], fontsize=9, style="italic",
                  path_effects=stroke, zorder=10)

    # Guard zone bracket
    guard_end = origin + r_dir * step_size * guard_steps
    ax_b.annotate("", xy=(guard_end[0], -0.6),
                  xytext=(origin[0], -0.6),
                  arrowprops={"arrowstyle": "<->", "color": STYLE["accent4"], "lw": 1.5},
                  zorder=7)
    ax_b.text((origin[0] + guard_end[0]) / 2, -0.85,
              "Guard zone\n(no hit test)",
              color=STYLE["accent4"], fontsize=8, fontweight="bold",
              ha="center", va="top", path_effects=stroke, zorder=10)

    # Legend
    legend_items = [
        ("x", STYLE["accent2"], "Skipped (guard)"),
        ("o", STYLE["accent1"], "Active step"),
        ("o", STYLE["accent2"], "Hit detected"),
    ]
    lx, ly = 0.0, 5.2
    for marker, color, label in legend_items:
        ax_b.plot(lx, ly, marker, color=color, markersize=6, zorder=8)
        ax_b.text(lx + 0.3, ly, label, color=color, fontsize=8,
                  fontweight="bold", ha="left", va="center",
                  path_effects=stroke, zorder=10)
        ly -= 0.5

    fig.tight_layout()
    save(fig, "gpu/29-screen-space-reflections", "self_intersection.png")
