"""Diagrams for gpu/27."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch

from .._common import STYLE, draw_vector, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/27-ssao — ssao_render_pipeline.png
# ---------------------------------------------------------------------------


def diagram_ssao_render_pipeline():
    """5-pass SSAO rendering pipeline showing data flow between passes."""

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


# ---------------------------------------------------------------------------
# gpu/27-ssao — gbuffer_mrt.png
# ---------------------------------------------------------------------------


def diagram_gbuffer_mrt():
    """G-buffer layout showing how MRT writes 3 textures from one draw call."""

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


# ---------------------------------------------------------------------------
# gpu/27-ssao — depth_reconstruction.png
# ---------------------------------------------------------------------------


def diagram_depth_reconstruction():
    """Transform chain from depth buffer to view-space position."""

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

    for i, ((title, img), color) in enumerate(zip(modes, colors, strict=True)):
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
