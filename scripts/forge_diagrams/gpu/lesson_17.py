"""Diagrams for gpu/17."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon, Rectangle

from .._common import STYLE, draw_vector, save

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
    for i, (text, color) in enumerate(zip(uv_legend, uv_colors, strict=True)):
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
    for i, (line, color) in enumerate(zip(eq_lines, eq_colors, strict=True)):
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
