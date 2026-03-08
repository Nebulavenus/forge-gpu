"""Diagrams for gpu/30."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Polygon, Rectangle

from .._common import STYLE, save, setup_axes


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


_pr_draw_arrow = _ssr_draw_arrow


# ---------------------------------------------------------------------------
# gpu/29-screen-space-reflections — render_pipeline.png
# ---------------------------------------------------------------------------


def _ssr_draw_pass_box(ax, x, y, w, h, title, number, color, stroke, fontsize=12):
    """Draw a rounded pass box with a numbered title."""
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
        fontsize=fontsize,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=4,
    )


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — render_pipeline.png
# ---------------------------------------------------------------------------


# Planar reflections uses the same drawing helpers as SSR.
# _pr_draw_pass_box uses fontsize=11 instead of the SSR default 12.
def _pr_draw_pass_box(ax, x, y, w, h, title, number, color, stroke):
    """Draw a rounded pass box with a numbered title (fontsize=11)."""
    _ssr_draw_pass_box(ax, x, y, w, h, title, number, color, stroke, fontsize=11)


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


_pr_draw_texture_tag = _ssr_draw_texture_tag


# ===========================================================================
# gpu/30-planar-reflections
# ===========================================================================


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — reflection_camera.png
# ---------------------------------------------------------------------------


def diagram_reflection_camera():
    """Real camera vs mirrored camera across a water plane.

    Shows the original camera above the water and its reflection below,
    with view frustums mirrored across the horizontal water surface.
    Demonstrates how the reflected camera produces a mirror image of the
    scene for planar reflections.
    """
    stroke = [pe.withStroke(linewidth=2.5, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    fig, ax = plt.subplots(figsize=(10, 8), facecolor=STYLE["bg"])
    ax.set_xlim(-3, 11)
    ax.set_ylim(-6.5, 6.5)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.axis("off")

    # --- Water plane ---
    ax.axhline(0, color=STYLE["accent1"], lw=2.5, alpha=0.6, zorder=2)
    ax.fill_between([-3, 11], -0.3, 0.3, color=STYLE["accent1"], alpha=0.08, zorder=1)
    ax.text(
        10.5,
        0.4,
        "Water Plane\n(Y = 0)",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="right",
        va="bottom",
        path_effects=stroke,
        zorder=5,
    )

    # --- Camera positions ---
    cam_x, cam_y = 1.0, 4.0
    mirror_y = -cam_y

    # --- Draw frustum (real camera) ---
    frust_far_l = (9.5, 0.5)
    frust_far_r = (9.5, 6.0)

    frustum_pts = [
        (cam_x, cam_y),
        frust_far_l,
        frust_far_r,
    ]
    frustum_patch = Polygon(
        frustum_pts,
        closed=True,
        facecolor=STYLE["accent3"],
        edgecolor=STYLE["accent3"],
        alpha=0.08,
        lw=1.5,
        zorder=1,
    )
    ax.add_patch(frustum_patch)
    ax.plot(
        [cam_x, frust_far_l[0]],
        [cam_y, frust_far_l[1]],
        "-",
        color=STYLE["accent3"],
        lw=1.2,
        alpha=0.5,
        zorder=2,
    )
    ax.plot(
        [cam_x, frust_far_r[0]],
        [cam_y, frust_far_r[1]],
        "-",
        color=STYLE["accent3"],
        lw=1.2,
        alpha=0.5,
        zorder=2,
    )

    # --- Draw frustum (mirrored camera) ---
    m_frust_far_l = (frust_far_l[0], -frust_far_l[1])
    m_frust_far_r = (frust_far_r[0], -frust_far_r[1])

    m_frustum_pts = [
        (cam_x, mirror_y),
        m_frust_far_l,
        m_frust_far_r,
    ]
    m_frustum_patch = Polygon(
        m_frustum_pts,
        closed=True,
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["accent2"],
        alpha=0.08,
        lw=1.5,
        zorder=1,
    )
    ax.add_patch(m_frustum_patch)
    ax.plot(
        [cam_x, m_frust_far_l[0]],
        [mirror_y, m_frust_far_l[1]],
        "--",
        color=STYLE["accent2"],
        lw=1.2,
        alpha=0.5,
        zorder=2,
    )
    ax.plot(
        [cam_x, m_frust_far_r[0]],
        [mirror_y, m_frust_far_r[1]],
        "--",
        color=STYLE["accent2"],
        lw=1.2,
        alpha=0.5,
        zorder=2,
    )

    # --- Camera icons ---
    ax.plot(cam_x, cam_y, "s", color=STYLE["accent3"], markersize=14, zorder=6)
    ax.text(
        cam_x - 0.2,
        cam_y + 0.6,
        "Real Camera",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        va="bottom",
        path_effects=stroke,
        zorder=7,
    )

    ax.plot(cam_x, mirror_y, "s", color=STYLE["accent2"], markersize=14, zorder=6)
    ax.text(
        cam_x - 0.2,
        mirror_y - 0.6,
        "Mirrored Camera",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        va="top",
        path_effects=stroke,
        zorder=7,
    )

    # --- Dashed vertical line connecting cameras ---
    ax.plot(
        [cam_x, cam_x],
        [cam_y - 0.4, mirror_y + 0.4],
        "--",
        color=STYLE["warn"],
        lw=1.5,
        alpha=0.6,
        zorder=3,
    )
    ax.text(
        cam_x + 0.4,
        0.0,
        "d",
        color=STYLE["warn"],
        fontsize=10,
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        cam_x + 0.4,
        2.0,
        "d",
        color=STYLE["warn"],
        fontsize=10,
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # --- Scene object above water ---
    obj_x, obj_y = 7.0, 3.0
    boat_pts = [
        (obj_x - 1.0, obj_y - 0.3),
        (obj_x + 1.0, obj_y - 0.3),
        (obj_x + 0.7, obj_y + 0.3),
        (obj_x - 0.7, obj_y + 0.3),
    ]
    boat_patch = Polygon(
        boat_pts,
        closed=True,
        facecolor=STYLE["accent3"],
        edgecolor=STYLE["accent3"],
        alpha=0.3,
        lw=1.5,
        zorder=4,
    )
    ax.add_patch(boat_patch)
    ax.text(
        obj_x,
        obj_y + 0.7,
        "Scene Object",
        color=STYLE["accent3"],
        fontsize=9,
        ha="center",
        va="bottom",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Mirrored scene object below water ---
    m_obj_y = -obj_y
    m_boat_pts = [
        (obj_x - 1.0, m_obj_y + 0.3),
        (obj_x + 1.0, m_obj_y + 0.3),
        (obj_x + 0.7, m_obj_y - 0.3),
        (obj_x - 0.7, m_obj_y - 0.3),
    ]
    m_boat_patch = Polygon(
        m_boat_pts,
        closed=True,
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["accent2"],
        alpha=0.15,
        lw=1.5,
        linestyle="--",
        zorder=4,
    )
    ax.add_patch(m_boat_patch)
    ax.text(
        obj_x,
        m_obj_y - 0.7,
        "Reflected Image",
        color=STYLE["accent2"],
        fontsize=9,
        fontstyle="italic",
        ha="center",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Reflection formula ---
    ax.text(
        -2.0,
        -5.5,
        "For plane Y=0:  y\u2032 = \u2212y\nGeneral (unit n):\np\u2032 = p \u2212 2(n\u22c5p + d)n",
        color=STYLE["text"],
        fontsize=10,
        fontfamily="monospace",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=5,
        bbox={
            "boxstyle": "round,pad=0.4",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["accent1"],
            "alpha": 0.8,
        },
    )

    # --- Title ---
    fig.suptitle(
        "Camera Reflection Across Water Plane",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0.02, 1, 0.94))
    save(fig, "gpu/30-planar-reflections", "reflection_camera.png")


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — oblique_clip_planes.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — oblique_clip_planes.png
# ---------------------------------------------------------------------------


def diagram_oblique_clip_planes():
    """Standard near plane vs oblique near plane in a view frustum.

    Side-by-side comparison showing how the standard near plane clips at a
    fixed distance while the oblique near plane coincides with the water
    surface, preventing underwater geometry from appearing in reflections.
    """
    stroke = [pe.withStroke(linewidth=2.5, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    fig, (ax_a, ax_b) = plt.subplots(1, 2, figsize=(14, 6), facecolor=STYLE["bg"])
    fig.suptitle(
        "Oblique Near-Plane Clipping",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    for ax, title in [(ax_a, "Standard Near Plane"), (ax_b, "Oblique Near Plane")]:
        ax.set_xlim(-1, 10)
        ax.set_ylim(-3.5, 5)
        ax.set_facecolor(STYLE["bg"])
        ax.set_aspect("equal")
        ax.axis("off")
        ax.set_title(
            title,
            color=STYLE["text"],
            fontsize=12,
            fontweight="bold",
            pad=10,
        )

    # --- Draw both frustums ---
    cam_x = 0.0
    near_x_standard = 2.0
    far_x = 9.0

    for ax, oblique in [(ax_a, False), (ax_b, True)]:
        # Water plane
        ax.axhline(0, color=STYLE["accent1"], lw=2, alpha=0.5, zorder=2)
        ax.text(
            9.5,
            0.3,
            "Water",
            color=STYLE["accent1"],
            fontsize=8,
            ha="right",
            va="bottom",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Camera
        ax.plot(cam_x, 0, "s", color=STYLE["text"], markersize=10, zorder=6)
        ax.text(
            cam_x,
            0.6,
            "Camera\n(mirrored)",
            color=STYLE["text"],
            fontsize=8,
            ha="center",
            va="bottom",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Far plane
        ax.plot(
            [far_x, far_x],
            [-3.0, 3.0],
            "-",
            color=STYLE["grid"],
            lw=1.5,
            zorder=2,
        )
        ax.text(
            far_x,
            3.3,
            "Far",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="bottom",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Frustum edges
        ax.plot(
            [cam_x, far_x],
            [0, 3.0],
            "-",
            color=STYLE["grid"],
            lw=1,
            alpha=0.5,
            zorder=1,
        )
        ax.plot(
            [cam_x, far_x],
            [0, -3.0],
            "-",
            color=STYLE["grid"],
            lw=1,
            alpha=0.5,
            zorder=1,
        )

        if oblique:
            # Oblique near plane coincides with the water surface
            near_color = STYLE["accent2"]
            # Draw the oblique plane as a thick horizontal line at water level
            ax.plot(
                [near_x_standard - 0.5, far_x],
                [0, 0],
                "-",
                color=near_color,
                lw=3,
                alpha=0.8,
                zorder=3,
            )
            ax.text(
                5.5,
                -0.5,
                "Oblique near plane\n= water surface",
                color=near_color,
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="top",
                path_effects=stroke,
                zorder=5,
            )

            # Clipped region below water (hatched/shaded)
            clip_pts = [
                (near_x_standard - 0.5, 0),
                (far_x, 0),
                (far_x, -3.0),
                (cam_x, 0),
            ]
            clip_patch = Polygon(
                clip_pts,
                closed=True,
                facecolor=STYLE["accent2"],
                alpha=0.08,
                edgecolor="none",
                zorder=1,
            )
            ax.add_patch(clip_patch)
            ax.text(
                5.5,
                -2.0,
                "CLIPPED\n(underwater geometry\nnever rendered)",
                color=STYLE["accent2"],
                fontsize=8,
                fontstyle="italic",
                ha="center",
                va="center",
                alpha=0.7,
                path_effects=stroke_thin,
                zorder=5,
            )
        else:
            # Standard near plane — vertical line
            near_color = STYLE["accent3"]
            near_half = 0.7  # height at near distance
            ax.plot(
                [near_x_standard, near_x_standard],
                [-near_half, near_half],
                "-",
                color=near_color,
                lw=3,
                alpha=0.8,
                zorder=3,
            )
            ax.text(
                near_x_standard,
                near_half + 0.3,
                "Standard\nnear plane",
                color=near_color,
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="bottom",
                path_effects=stroke,
                zorder=5,
            )

            # Problem: underwater geometry visible
            leak_pts = [
                (near_x_standard, -near_half),
                (far_x, -3.0),
                (far_x, 0),
                (near_x_standard + 2, 0),
            ]
            leak_patch = Polygon(
                leak_pts,
                closed=True,
                facecolor=STYLE["warn"],
                alpha=0.08,
                edgecolor="none",
                zorder=1,
            )
            ax.add_patch(leak_patch)
            ax.text(
                6.0,
                -1.5,
                "PROBLEM:\nunderwater geometry\nleaks into reflection",
                color=STYLE["warn"],
                fontsize=8,
                fontstyle="italic",
                ha="center",
                va="center",
                path_effects=stroke_thin,
                zorder=5,
            )

    fig.tight_layout(rect=(0, 0.02, 1, 0.93))
    save(fig, "gpu/30-planar-reflections", "oblique_clip_planes.png")


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — fresnel_curve.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — frustum_plane_extraction.png
# ---------------------------------------------------------------------------


def diagram_frustum_plane_extraction():
    """Clip-space cube with 6 labeled frustum planes.

    Shows clip-space frustum bounds with each face labeled as a frustum
    plane (left, right, top, bottom, near, far) and the corresponding
    inequalities (x, y in [-w, w], z in [0, w] for Vulkan/D3D).
    """
    stroke = [pe.withStroke(linewidth=2.5, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    fig, ax = plt.subplots(figsize=(9, 8), facecolor=STYLE["bg"])
    ax.set_xlim(-2.5, 4.5)
    ax.set_ylim(-2.5, 4.5)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.axis("off")

    # Simple isometric projection of a frustum volume.
    # Vulkan/D3D convention: x,y in [-1, 1], z in [0, 1].
    # The iso mapping remaps z from [0,1] to [-1,1] for visual centering.
    # Simple iso: screen_x = x + z'*0.5, screen_y = y + z'*0.35
    iso_x_fact = 0.5
    iso_y_fact = 0.35
    s = 1.3  # scale factor

    def iso(x, y, z):
        # Remap z from [0,1] to [-1,1] for centered isometric layout
        z_centered = z * 2.0 - 1.0
        sx = (x + z_centered * iso_x_fact) * s + 1.0
        sy = (y + z_centered * iso_y_fact) * s + 1.0
        return (sx, sy)

    # Near face (z=0)
    b0 = iso(-1, -1, 0)
    b1 = iso(1, -1, 0)
    b2 = iso(1, 1, 0)
    b3 = iso(-1, 1, 0)

    # Far face (z=1)
    f0 = iso(-1, -1, 1)
    f1 = iso(1, -1, 1)
    f2 = iso(1, 1, 1)
    f3 = iso(-1, 1, 1)

    # Draw back edges (dashed)
    for p1, p2 in [(b0, b1), (b0, b3), (b0, f0)]:
        ax.plot(
            [p1[0], p2[0]],
            [p1[1], p2[1]],
            "--",
            color=STYLE["grid"],
            lw=1,
            alpha=0.5,
            zorder=1,
        )

    # Draw front edges (solid)
    front_edges = [
        (f0, f1),
        (f1, f2),
        (f2, f3),
        (f3, f0),
        (b1, f1),
        (b2, f2),
        (b3, f3),
        (b1, b2),
        (b2, b3),
    ]
    for p1, p2 in front_edges:
        ax.plot(
            [p1[0], p2[0]],
            [p1[1], p2[1]],
            "-",
            color=STYLE["text_dim"],
            lw=1.5,
            zorder=2,
        )

    # Face labels with clip-space inequalities
    planes = [
        ("Near", iso(0, 0, 0), STYLE["accent1"], "0 \u2264 z"),
        ("Far", iso(0, 0, 1), STYLE["accent4"], "z \u2264 w"),
        ("Right", iso(1, 0, 0.5), STYLE["accent2"], "x \u2264 w"),
        ("Left", iso(-1, 0.3, 0.65), STYLE["accent3"], "\u2212w \u2264 x"),
        ("Top", iso(0, 1, 0.5), STYLE["warn"], "y \u2264 w"),
        ("Bottom", iso(0, -1, 0.5), STYLE["text_dim"], "\u2212w \u2264 y"),
    ]

    for name, pos, color, ineq in planes:
        ax.text(
            pos[0],
            pos[1],
            f"{name}\n{ineq}",
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=6,
            bbox={
                "boxstyle": "round,pad=0.25",
                "facecolor": STYLE["surface"],
                "edgecolor": color,
                "alpha": 0.85,
            },
        )

    # --- Oblique replacement note ---
    ax.text(
        3.5,
        -1.5,
        "Oblique clipping replaces\nthe near plane with the\nwater surface plane",
        color=STYLE["accent1"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        bbox={
            "boxstyle": "round,pad=0.4",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["accent1"],
            "alpha": 0.8,
            "linestyle": "dashed",
        },
        zorder=5,
    )

    # Arrow from note to near plane
    ax.annotate(
        "",
        xy=iso(0, -0.3, 0),
        xytext=(3.0, -0.9),
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent1"],
            "lw": 1.5,
            "connectionstyle": "arc3,rad=0.3",
        },
        zorder=4,
    )

    # Axis labels
    ax.annotate(
        "",
        xy=iso(1.5, 0, 0),
        xytext=iso(0, 0, 0),
        arrowprops={
            "arrowstyle": "->,head_width=0.15",
            "color": STYLE["accent2"],
            "lw": 1.5,
        },
        zorder=3,
    )
    ax.text(
        iso(1.7, 0, 0)[0],
        iso(1.7, 0, 0)[1],
        "X",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        path_effects=stroke_thin,
        zorder=5,
    )

    ax.annotate(
        "",
        xy=iso(0, 1.5, 0),
        xytext=iso(0, 0, 0),
        arrowprops={
            "arrowstyle": "->,head_width=0.15",
            "color": STYLE["accent3"],
            "lw": 1.5,
        },
        zorder=3,
    )
    ax.text(
        iso(0, 1.7, 0)[0],
        iso(0, 1.7, 0)[1],
        "Y",
        color=STYLE["accent3"],
        fontsize=9,
        fontweight="bold",
        path_effects=stroke_thin,
        zorder=5,
    )

    ax.annotate(
        "",
        xy=iso(0, 0, 1.5),
        xytext=iso(0, 0, 0),
        arrowprops={
            "arrowstyle": "->,head_width=0.15",
            "color": STYLE["accent1"],
            "lw": 1.5,
        },
        zorder=3,
    )
    ax.text(
        iso(0, 0, 1.7)[0],
        iso(0, 0, 1.7)[1],
        "Z",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        path_effects=stroke_thin,
        zorder=5,
    )

    fig.suptitle(
        "Clip-Space Frustum Planes",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/30-planar-reflections", "frustum_plane_extraction.png")


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — screen_space_projection.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — fresnel_curve.png
# ---------------------------------------------------------------------------


def diagram_fresnel_curve():
    """Schlick Fresnel approximation: reflectance vs viewing angle.

    Plots F(theta) = F0 + (1 - F0) * (1 - cos(theta))^5 for water (F0=0.02)
    and glass (F0=0.04) showing how reflectance increases at grazing angles.
    Annotates key angles and the physical meaning for water rendering.
    """
    stroke = [pe.withStroke(linewidth=2.5, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    fig, ax = plt.subplots(figsize=(10, 6), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-2, 92), ylim=(-0.05, 1.1), aspect=None, grid=True)

    ax.set_xlabel(
        "Viewing Angle (degrees from normal)",
        color=STYLE["axis"],
        fontsize=10,
    )
    ax.set_ylabel("Fresnel Reflectance", color=STYLE["axis"], fontsize=10)

    # --- Schlick Fresnel curves ---
    angles = np.linspace(0, 90, 500)
    cos_theta = np.cos(np.radians(angles))

    materials = [
        ("Water (F\u2080 = 0.02)", 0.02, STYLE["accent1"], 2.5),
        ("Glass (F\u2080 = 0.04)", 0.04, STYLE["accent3"], 2.0),
        ("Plastic (F\u2080 = 0.05)", 0.05, STYLE["accent4"], 1.5),
    ]

    for label, f0, color, lw in materials:
        fresnel = f0 + (1.0 - f0) * np.power(1.0 - cos_theta, 5.0)
        ax.plot(angles, fresnel, "-", color=color, lw=lw, label=label, zorder=4)

    # --- Annotations ---
    # Looking straight down (0°)
    ax.annotate(
        "Looking straight down\n(mostly transparent)",
        xy=(0, 0.02),
        xytext=(15, 0.35),
        color=STYLE["text"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke_thin,
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent1"],
            "lw": 1.5,
        },
        zorder=5,
    )

    # Grazing angle (85-90°)
    ax.annotate(
        "Grazing angle\n(fully reflective)",
        xy=(87, 0.95),
        xytext=(65, 0.75),
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["accent1"],
            "lw": 1.5,
        },
        zorder=5,
    )

    # --- Highlight the 60° mark ---
    f0_water = 0.02
    cos_60 = np.cos(np.radians(60))
    f_60 = f0_water + (1.0 - f0_water) * (1.0 - cos_60) ** 5.0
    ax.plot(60, f_60, "o", color=STYLE["warn"], markersize=8, zorder=6)
    ax.annotate(
        f"60\u00b0: F = {f_60:.3f}",
        xy=(60, f_60),
        xytext=(42, f_60 + 0.15),
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
        arrowprops={
            "arrowstyle": "->",
            "color": STYLE["warn"],
            "lw": 1.2,
        },
        zorder=5,
    )

    # --- Formula ---
    ax.text(
        50,
        0.95,
        "Schlick approximation:\nF(\u03b8) = F\u2080 + (1 \u2212 F\u2080)\u22c5(1 \u2212 cos\u03b8)\u2075",
        color=STYLE["text"],
        fontsize=10,
        fontfamily="monospace",
        ha="center",
        va="center",
        path_effects=stroke,
        bbox={
            "boxstyle": "round,pad=0.4",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["grid"],
            "alpha": 0.9,
        },
        zorder=7,
    )

    ax.legend(
        loc="upper left",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )

    fig.suptitle(
        "Schlick Fresnel Approximation",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0, 1, 0.94))
    save(fig, "gpu/30-planar-reflections", "fresnel_curve.png")


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — water_layers.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — water_layers.png
# ---------------------------------------------------------------------------


def diagram_water_layers():
    """Cross-section of the water surface showing reflection vs see-through.

    A vertical slice through the scene showing a camera looking at the water
    from the side.  At grazing angles the Fresnel term is high and reflection
    dominates; looking straight down the Fresnel term is low and the sandy
    floor is visible through the water.
    """
    stroke = [pe.withStroke(linewidth=2.5, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    fig, ax = plt.subplots(figsize=(12, 7), facecolor=STYLE["bg"])
    ax.set_xlim(-1, 13)
    ax.set_ylim(-4, 6.5)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.axis("off")

    # --- Water surface ---
    ax.fill_between([-1, 13], -0.15, 0.15, color=STYLE["accent1"], alpha=0.15, zorder=2)
    ax.plot([-1, 13], [0, 0], "-", color=STYLE["accent1"], lw=2.5, zorder=3)
    ax.text(
        12.5,
        0.4,
        "Water Surface",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="right",
        va="bottom",
        path_effects=stroke,
        zorder=5,
    )

    # --- Sandy floor ---
    ax.fill_between([-1, 13], -3.5, -2.5, color=STYLE["warn"], alpha=0.1, zorder=1)
    ax.plot([-1, 13], [-2.5, -2.5], "-", color=STYLE["warn"], lw=1.5, alpha=0.5)
    ax.text(
        6.0,
        -3.0,
        "Sandy Floor",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        va="center",
        alpha=0.7,
        path_effects=stroke_thin,
    )

    # --- Camera ---
    cam_x, cam_y = 0.5, 4.0
    ax.plot(cam_x, cam_y, "s", color=STYLE["text"], markersize=12, zorder=6)
    ax.text(
        cam_x,
        cam_y + 0.5,
        "Camera",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=7,
    )

    # --- Ray A: Steep angle (looking down) - sees through water ---
    hit_a_x = 3.0
    ax.annotate(
        "",
        xy=(hit_a_x, 0),
        xytext=(cam_x, cam_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.15",
            "color": STYLE["accent3"],
            "lw": 2.5,
        },
        zorder=4,
    )
    # Continue below water (refracted)
    ax.plot(
        [hit_a_x, hit_a_x + 0.5],
        [0, -2.5],
        "--",
        color=STYLE["accent3"],
        lw=1.5,
        alpha=0.6,
        zorder=3,
    )
    ax.text(
        hit_a_x + 1.2,
        1.5,
        "Steep angle\nF \u2248 0.02",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        hit_a_x + 1.2,
        0.5,
        "See floor through water",
        color=STYLE["accent3"],
        fontsize=8,
        ha="left",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Ray B: Grazing angle - reflects ---
    hit_b_x = 9.0
    ax.annotate(
        "",
        xy=(hit_b_x, 0),
        xytext=(cam_x, cam_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.15",
            "color": STYLE["accent2"],
            "lw": 2.5,
        },
        zorder=4,
    )
    # Reflected ray going up
    ax.annotate(
        "",
        xy=(11.5, 3.5),
        xytext=(hit_b_x, 0),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.12",
            "color": STYLE["accent2"],
            "lw": 1.8,
            "linestyle": "dashed",
        },
        zorder=3,
    )
    ax.text(
        hit_b_x + 0.5,
        1.8,
        "Grazing angle\nF \u2248 1.0",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        hit_b_x + 0.5,
        0.8,
        "Full reflection",
        color=STYLE["accent2"],
        fontsize=8,
        ha="left",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Blend bar at bottom ---
    bar_y = -3.8
    bar_w = 10.0
    bar_x0 = 1.5
    grad_n = 200
    for i in range(grad_n):
        t = i / (grad_n - 1)
        x_pos = bar_x0 + t * bar_w
        # Blend from accent3 (transparent) to accent2 (reflective)
        r_s = int(STYLE["accent3"][1:3], 16) / 255
        g_s = int(STYLE["accent3"][3:5], 16) / 255
        b_s = int(STYLE["accent3"][5:7], 16) / 255
        r_e = int(STYLE["accent2"][1:3], 16) / 255
        g_e = int(STYLE["accent2"][3:5], 16) / 255
        b_e = int(STYLE["accent2"][5:7], 16) / 255
        c = (r_s + t * (r_e - r_s), g_s + t * (g_e - g_s), b_s + t * (b_e - b_s))
        ax.plot([x_pos, x_pos], [bar_y - 0.15, bar_y + 0.15], color=c, lw=1.5)

    ax.text(
        bar_x0 - 0.3,
        bar_y,
        "See-through",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="right",
        va="center",
        path_effects=stroke_thin,
    )
    ax.text(
        bar_x0 + bar_w + 0.3,
        bar_y,
        "Reflective",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke_thin,
    )
    ax.text(
        bar_x0 + bar_w / 2,
        bar_y - 0.5,
        "Fresnel blend (angle-dependent)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="center",
        va="top",
        path_effects=stroke_thin,
    )

    fig.suptitle(
        "Water Rendering: Fresnel-Blended Layers",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0.02, 1, 0.94))
    save(fig, "gpu/30-planar-reflections", "water_layers.png")


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — render_pipeline.png
# ---------------------------------------------------------------------------


# Planar reflections uses the same drawing helpers as SSR.
# _pr_draw_pass_box uses fontsize=11 instead of the SSR default 12.


def diagram_planar_render_pipeline():
    """Planar reflections render pipeline: 4 passes with texture I/O."""
    stroke = [pe.withStroke(linewidth=2.5, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    fig, ax = plt.subplots(figsize=(16, 7.5), facecolor=STYLE["bg"])
    ax.set_xlim(-0.5, 16)
    ax.set_ylim(-4.5, 4.5)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.axis("off")

    # --- Pass box dimensions ---
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
        STYLE["accent1"],  # Reflection — cyan
        STYLE["accent3"],  # Main Scene — green
        STYLE["accent2"],  # Water — orange
    ]

    # --- Draw four pass boxes ---
    _pr_draw_pass_box(
        ax, x1, box_y, box_w, box_h, "Shadow\nPass", 1, pass_colors[0], stroke
    )
    _pr_draw_pass_box(
        ax, x2, box_y, box_w, box_h, "Reflection\nPass", 2, pass_colors[1], stroke
    )
    _pr_draw_pass_box(
        ax, x3, box_y, box_w, box_h, "Main Scene\nPass", 3, pass_colors[2], stroke
    )
    _pr_draw_pass_box(
        ax, x4, box_y, box_w, box_h, "Water\nPass", 4, pass_colors[3], stroke
    )

    # --- Arrows between pass boxes ---
    arrow_y = box_y + box_h / 2
    for src_x, dst_x in [(x1, x2), (x2, x3), (x3, x4)]:
        _pr_draw_arrow(ax, src_x + box_w + 0.08, arrow_y, dst_x - 0.08, arrow_y)

    # --- Shadow Pass output: Depth texture ---
    shadow_out_y = box_y - 0.9
    shadow_cx = x1 + box_w / 2
    _pr_draw_texture_tag(
        ax,
        shadow_cx,
        shadow_out_y,
        "Shadow Depth",
        "D32F 2048\u00b2",
        pass_colors[0],
        stroke_thin,
    )
    _pr_draw_arrow(
        ax, shadow_cx, box_y - 0.02, shadow_cx, shadow_out_y + 0.3, pass_colors[0]
    )

    # Shadow depth feeds into Main Scene pass
    ax.annotate(
        "",
        xy=(x3, box_y + box_h * 0.3),
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

    # --- Reflection Pass output: Color texture ---
    refl_out_y = box_y - 0.9
    refl_cx = x2 + box_w / 2
    _pr_draw_texture_tag(
        ax,
        refl_cx,
        refl_out_y,
        "Reflection",
        "Swapchain fmt",
        pass_colors[1],
        stroke_thin,
    )
    _pr_draw_arrow(ax, refl_cx, box_y - 0.02, refl_cx, refl_out_y + 0.3, pass_colors[1])

    # Reflection texture feeds into Water pass
    ax.annotate(
        "",
        xy=(x4, box_y + box_h * 0.3),
        xytext=(refl_cx + 0.6, refl_out_y + 0.05),
        arrowprops={
            "arrowstyle": "->,head_width=0.15,head_length=0.08",
            "color": pass_colors[1],
            "lw": 1.2,
            "alpha": 0.5,
            "connectionstyle": "arc3,rad=-0.3",
        },
        zorder=2,
    )

    # --- Main Scene output: Swapchain ---
    scene_out_y = box_y - 0.9
    scene_cx = x3 + box_w / 2
    _pr_draw_texture_tag(
        ax,
        scene_cx,
        scene_out_y,
        "Swapchain",
        "Render target",
        pass_colors[2],
        stroke_thin,
    )
    _pr_draw_arrow(
        ax, scene_cx, box_y - 0.02, scene_cx, scene_out_y + 0.3, pass_colors[2]
    )

    # --- Water Pass output: Display ---
    water_out_y = box_y - 0.9
    water_cx = x4 + box_w / 2
    _pr_draw_texture_tag(
        ax,
        water_cx,
        water_out_y,
        "Display",
        "Present",
        pass_colors[3],
        stroke_thin,
    )
    _pr_draw_arrow(
        ax, water_cx, box_y - 0.02, water_cx, water_out_y + 0.3, pass_colors[3]
    )

    # --- Detail labels above each pass ---
    detail_y = box_y + box_h + 0.3
    details = [
        ("Depth-only\nlight view", pass_colors[0]),
        ("Mirrored camera\noblique clip", pass_colors[1]),
        ("Forward render\n+ shadows", pass_colors[2]),
        ("Fresnel blend\nalpha = F(\u03b8)", pass_colors[3]),
    ]
    for i, (label, color) in enumerate(details):
        cx = [x1, x2, x3, x4][i] + box_w / 2
        ax.text(
            cx,
            detail_y + 0.4,
            label,
            color=color,
            fontsize=8,
            ha="center",
            va="bottom",
            path_effects=stroke_thin,
            zorder=5,
        )

    # --- Rendered geometry per pass ---
    geom_y = box_y - 2.2
    geometries = [
        "Boat, Rocks, Floor",
        "Boat, Rocks, Skybox",
        "Boat, Rocks, Floor,\nSkybox",
        "Water Quad",
    ]
    for i, label in enumerate(geometries):
        cx = [x1, x2, x3, x4][i] + box_w / 2
        ax.text(
            cx,
            geom_y,
            label,
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="top",
            fontstyle="italic",
            path_effects=stroke_thin,
            zorder=5,
        )

    # --- Title ---
    fig.suptitle(
        "Planar Reflections Render Pipeline  (4 Passes)",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        y=0.97,
    )

    # --- Subtitle ---
    ax.text(
        8.0,
        4.2,
        "Shadow  \u2192  Reflection  \u2192  Main Scene  \u2192  Water  \u2192  Display",
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
        (pass_colors[0], "Shadow", "Depth-only from light"),
        (pass_colors[1], "Reflection", "Mirrored camera + oblique clip"),
        (pass_colors[2], "Main Scene", "Forward render to swapchain"),
        (pass_colors[3], "Water", "Fresnel-blended alpha surface"),
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
    save(fig, "gpu/30-planar-reflections", "render_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — frustum_plane_extraction.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — screen_space_projection.png
# ---------------------------------------------------------------------------


def diagram_screen_space_projection():
    """Fragment projection from clip space to screen-space UV for reflection
    texture sampling.

    Shows the transformation chain: world position -> clip position ->
    NDC -> screen UV, annotating each step for the water fragment shader.
    """
    stroke = [pe.withStroke(linewidth=2.5, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    fig, ax = plt.subplots(figsize=(14, 5), facecolor=STYLE["bg"])
    ax.set_xlim(-0.5, 14)
    ax.set_ylim(-1.5, 3.5)
    ax.set_facecolor(STYLE["bg"])
    ax.set_aspect("equal")
    ax.axis("off")

    # --- Step boxes ---
    box_w = 2.5
    box_h = 1.4
    gap = 0.9
    y0 = 0.5

    colors = [
        STYLE["accent3"],  # World pos
        STYLE["accent1"],  # Clip pos
        STYLE["accent4"],  # NDC
        STYLE["accent2"],  # Screen UV
    ]
    labels = [
        "World\nPosition",
        "Clip\nPosition",
        "NDC",
        "Screen UV",
    ]
    formulas = [
        "(x, y, z)",
        "MVP \u00d7 pos",
        "xy / w",
        "ndc \u00d7 0.5 + 0.5",
    ]

    for i in range(4):
        x = 0.3 + i * (box_w + gap)
        rect = FancyBboxPatch(
            (x, y0),
            box_w,
            box_h,
            boxstyle="round,pad=0.12",
            linewidth=2.5,
            edgecolor=colors[i],
            facecolor=STYLE["surface"],
            alpha=0.9,
            zorder=3,
        )
        ax.add_patch(rect)
        ax.text(
            x + box_w / 2,
            y0 + box_h * 0.65,
            labels[i],
            color=STYLE["text"],
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=4,
        )
        ax.text(
            x + box_w / 2,
            y0 - 0.3,
            formulas[i],
            color=colors[i],
            fontsize=9,
            fontfamily="monospace",
            ha="center",
            va="top",
            path_effects=stroke_thin,
            zorder=5,
        )

        # Arrow to next
        if i < 3:
            _pr_draw_arrow(
                ax,
                x + box_w + 0.08,
                y0 + box_h / 2,
                x + box_w + gap - 0.08,
                y0 + box_h / 2,
            )

    # --- Final sampling step ---
    sample_x = 0.3 + 3 * (box_w + gap) + box_w + gap
    ax.text(
        sample_x,
        y0 + box_h / 2,
        "\u2192 Sample\n   reflection\n   texture",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # --- Note about Y flip ---
    ax.text(
        7.0,
        y0 + box_h + 0.7,
        "Note: screen_uv.y = 1.0 \u2212 screen_uv.y  (Vulkan Y-flip)",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    fig.suptitle(
        "Screen-Space Projection for Reflection Sampling",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0.02, 1, 0.93))
    save(fig, "gpu/30-planar-reflections", "screen_space_projection.png")


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — underwater_camera_guard.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/30-planar-reflections — underwater_camera_guard.png
# ---------------------------------------------------------------------------


def diagram_underwater_camera_guard():
    """Side-view showing why planar reflections break underwater.

    Left half: camera above water — valid reflection with mirrored camera
    below the surface. Right half: camera below water — mirrored camera
    above, oblique clip and Fresnel both fail.
    """
    fig = plt.figure(figsize=(12, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 13.5), ylim=(-4.5, 5.5))
    ax.set_aspect("equal")
    ax.set_xticks([])
    ax.set_yticks([])
    ax.grid(False)

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
    stroke_thin = [pe.withStroke(linewidth=2, foreground=STYLE["bg"])]

    water_color = "#2288aa"
    underwater_color = "#0d3d5c"
    valid_color = STYLE["accent3"]  # green
    broken_color = STYLE["accent2"]  # orange/red

    # --- Water surface line across the full width ---
    ax.axhline(y=0, color=water_color, lw=3, zorder=3)
    ax.fill_between(
        [-0.5, 13.5],
        -4.5,
        0,
        color=underwater_color,
        alpha=0.25,
        zorder=1,
    )
    ax.text(
        6.75,
        0.25,
        "Water Plane  (Y = 0)",
        color=water_color,
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=5,
    )

    # --- Dividing line between the two halves ---
    ax.axvline(x=6.75, color=STYLE["grid"], lw=1, ls="--", zorder=2)

    # =====================================================================
    # LEFT HALF: Camera above water (valid)
    # =====================================================================
    cam_x, cam_y = 3.0, 3.5
    refl_x, refl_y = 3.0, -3.5  # mirrored across Y=0

    # Camera icon (above water)
    ax.plot(cam_x, cam_y, "s", color=valid_color, ms=14, zorder=6)
    ax.text(
        cam_x,
        cam_y + 0.6,
        "Camera",
        color=valid_color,
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        cam_x,
        cam_y - 0.5,
        "Y = 3",
        color=valid_color,
        fontsize=8,
        ha="center",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Reflected camera (below water)
    ax.plot(refl_x, refl_y, "s", color=valid_color, ms=12, alpha=0.5, zorder=6)
    ax.text(
        refl_x,
        refl_y - 0.6,
        "Reflected\nCamera",
        color=valid_color,
        fontsize=9,
        alpha=0.7,
        ha="center",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
    )
    ax.text(
        refl_x,
        refl_y + 0.5,
        "Y = \u22123",
        color=valid_color,
        fontsize=8,
        alpha=0.7,
        ha="center",
        va="bottom",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Mirror line between camera and reflection
    ax.plot(
        [cam_x, refl_x],
        [cam_y, refl_y],
        "--",
        color=valid_color,
        lw=1.5,
        alpha=0.4,
        zorder=3,
    )

    # Oblique clip region (hatched area below water on left)
    ax.fill_between(
        [0.5, 5.5],
        -4.5,
        0,
        color=valid_color,
        alpha=0.08,
        zorder=1,
        hatch="//",
    )
    ax.text(
        1.0,
        -2.5,
        "Clipped\n(correct)",
        color=valid_color,
        fontsize=8,
        alpha=0.6,
        ha="left",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Check mark
    ax.text(
        3.0,
        5.0,
        "\u2713  Valid",
        color=valid_color,
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Scene object above water (on the left side)
    rect_left = FancyBboxPatch(
        (1.2, 1.0),
        1.4,
        1.8,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["text_dim"],
        lw=1.5,
        zorder=4,
    )
    ax.add_patch(rect_left)
    ax.text(
        1.9,
        1.9,
        "Scene",
        color=STYLE["text"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # View ray from camera to scene
    ax.annotate(
        "",
        xy=(2.6, 2.0),
        xytext=(cam_x, cam_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.1",
            "color": STYLE["accent1"],
            "lw": 1.5,
        },
        zorder=4,
    )

    # Reflected view ray (from reflected camera up through water to scene)
    ax.annotate(
        "",
        xy=(2.6, 1.0),
        xytext=(refl_x, refl_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.1",
            "color": STYLE["accent1"],
            "lw": 1.5,
            "alpha": 0.4,
            "linestyle": "--",
        },
        zorder=3,
    )

    # =====================================================================
    # RIGHT HALF: Camera below water (broken)
    # =====================================================================
    cam2_x, cam2_y = 10.0, -2.0
    refl2_x, refl2_y = 10.0, 2.0  # mirrored to above water

    # Camera icon (below water)
    ax.plot(cam2_x, cam2_y, "s", color=broken_color, ms=14, zorder=6)
    ax.text(
        cam2_x,
        cam2_y - 0.6,
        "Camera",
        color=broken_color,
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        cam2_x,
        cam2_y + 0.5,
        "Y = \u22122",
        color=broken_color,
        fontsize=8,
        ha="center",
        va="bottom",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Reflected camera (above water — wrong!)
    ax.plot(refl2_x, refl2_y, "s", color=broken_color, ms=12, alpha=0.5, zorder=6)
    ax.text(
        refl2_x,
        refl2_y + 0.6,
        "Reflected\nCamera",
        color=broken_color,
        fontsize=9,
        alpha=0.7,
        ha="center",
        va="bottom",
        path_effects=stroke_thin,
        zorder=5,
    )
    ax.text(
        refl2_x,
        refl2_y - 0.5,
        "Y = 2",
        color=broken_color,
        fontsize=8,
        alpha=0.7,
        ha="center",
        va="top",
        path_effects=stroke_thin,
        zorder=5,
    )

    # Mirror line
    ax.plot(
        [cam2_x, refl2_x],
        [cam2_y, refl2_y],
        "--",
        color=broken_color,
        lw=1.5,
        alpha=0.4,
        zorder=3,
    )

    # Oblique clip region (above water — clipping the wrong side!)
    ax.fill_between(
        [7.5, 12.5],
        0,
        5.0,
        color=broken_color,
        alpha=0.08,
        zorder=1,
        hatch="xx",
    )
    ax.text(
        12.0,
        2.5,
        "Clipped\n(wrong!)",
        color=broken_color,
        fontsize=8,
        alpha=0.8,
        ha="center",
        va="center",
        path_effects=stroke_thin,
        zorder=5,
    )

    # X mark
    ax.text(
        10.0,
        5.0,
        "\u2717  Broken",
        color=broken_color,
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Scene object (on right side, above water — but gets clipped)
    rect_right = FancyBboxPatch(
        (8.2, 1.0),
        1.4,
        1.8,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        edgecolor=broken_color,
        lw=1.5,
        ls="--",
        alpha=0.4,
        zorder=4,
    )
    ax.add_patch(rect_right)
    ax.text(
        8.9,
        1.9,
        "Scene",
        color=broken_color,
        fontsize=9,
        ha="center",
        va="center",
        alpha=0.5,
        path_effects=stroke_thin,
        zorder=5,
    )

    # --- Annotations at the bottom ---
    notes = [
        (
            3.0,
            -4.2,
            "Reflected camera below surface\n"
            "Oblique clip removes underwater geometry\n"
            "Fresnel: N\u00b7V > 0 \u2192 correct blend",
            valid_color,
        ),
        (
            10.0,
            -4.2,
            "Reflected camera above surface\n"
            "Oblique clip removes above-water geometry\n"
            "Fresnel: N\u00b7V < 0 \u2192 inverted output",
            broken_color,
        ),
    ]
    for nx, ny, text, color in notes:
        ax.text(
            nx,
            ny,
            text,
            color=color,
            fontsize=8,
            ha="center",
            va="center",
            path_effects=stroke_thin,
            zorder=5,
        )

    fig.suptitle(
        "Underwater Camera Guard — Why Planar Reflections Require an Above-Water Camera",
        color=STYLE["text"],
        fontsize=13,
        fontweight="bold",
        y=0.97,
    )

    fig.tight_layout(rect=(0, 0.02, 1, 0.93))
    save(fig, "gpu/30-planar-reflections", "underwater_camera_guard.png")


# ---------------------------------------------------------------------------
# gpu/31-transform-animations — keyframe_interpolation.png
# ---------------------------------------------------------------------------
