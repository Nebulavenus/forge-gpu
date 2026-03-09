"""Diagrams for gpu/39 — Loading Processed Assets."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch, Polygon, Rectangle

from .._common import STYLE, save, setup_axes

LESSON_PATH = "gpu/39-pipeline-processed-assets"


# ---------------------------------------------------------------------------
# gpu/39-loading-processed-assets — asset_pipeline_flow.png
# ---------------------------------------------------------------------------


def diagram_asset_pipeline_flow():
    """Data flow from raw assets through pipeline to GPU-ready data."""
    fig = plt.figure(figsize=(16, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 16.5), ylim=(-2, 8.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # ── Column 1: Raw Files ──────────────────────────────────────────
    col1_x = 0.3
    col1_box = FancyBboxPatch(
        (col1_x, 0.5),
        4.0,
        7.0,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(col1_box)
    ax.text(
        col1_x + 2.0,
        7.2,
        "Raw Assets",
        color=STYLE["accent1"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    raw_files = [
        ("WaterBottle.gltf", "scene description"),
        ("WaterBottle.bin", "geometry + UVs"),
        ("color.png", "base color (RGBA)"),
        ("normal.png", "normal map (RGB)"),
        ("metalRough.png", "metal/rough (RGB)"),
        ("emissive.png", "emissive (RGB)"),
    ]
    for i, (name, desc) in enumerate(raw_files):
        y = 6.3 - i * 0.95
        r = Rectangle(
            (col1_x + 0.2, y - 0.3),
            3.6,
            0.6,
            facecolor=STYLE["grid"],
            edgecolor=STYLE["accent1"],
            linewidth=0.8,
            zorder=3,
        )
        ax.add_patch(r)
        ax.text(
            col1_x + 0.4,
            y,
            name,
            color=STYLE["text"],
            fontsize=8,
            fontweight="bold",
            ha="left",
            va="center",
            fontfamily="monospace",
            path_effects=stroke,
            zorder=5,
        )
        ax.text(
            col1_x + 3.7,
            y,
            desc,
            color=STYLE["text_dim"],
            fontsize=7,
            ha="right",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # ── Column 2: Pipeline Processing ────────────────────────────────
    col2_x = 5.8
    col2_box = FancyBboxPatch(
        (col2_x, 0.5),
        4.5,
        7.0,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(col2_box)
    ax.text(
        col2_x + 2.25,
        7.2,
        "Pipeline Processing",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    pipeline_steps = [
        ("forge-mesh-tool", "optimize + LOD + tangents"),
        ("texture plugin", "validate + .meta.json sidecar"),
    ]
    for i, (name, desc) in enumerate(pipeline_steps):
        y = 6.0 - i * 1.6
        r = FancyBboxPatch(
            (col2_x + 0.3, y - 0.5),
            3.9,
            1.1,
            boxstyle="round,pad=0.1",
            facecolor=STYLE["bg"],
            edgecolor=STYLE["accent2"],
            linewidth=1.5,
            zorder=3,
        )
        ax.add_patch(r)
        ax.text(
            col2_x + 2.25,
            y + 0.1,
            name,
            color=STYLE["accent2"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )
        ax.text(
            col2_x + 2.25,
            y - 0.3,
            desc,
            color=STYLE["text_dim"],
            fontsize=7,
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Size comparison annotation
    ax.text(
        col2_x + 2.25,
        1.3,
        "48 bytes/vert (with tangents)\nvs 32 bytes/vert (raw)",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # ── Column 3: Processed Files ────────────────────────────────────
    col3_x = 11.8
    col3_box = FancyBboxPatch(
        (col3_x, 0.5),
        4.2,
        7.0,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent3"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(col3_box)
    ax.text(
        col3_x + 2.1,
        7.2,
        "Processed Files",
        color=STYLE["accent3"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    processed_files = [
        ("WaterBottle.fmesh", "optimized mesh + LODs"),
        ("color.png", "PNG + .meta.json"),
        ("normal.png", "PNG + .meta.json"),
        ("metalRough.png", "PNG + .meta.json"),
        ("emissive.png", "PNG + .meta.json"),
    ]
    for i, (name, desc) in enumerate(processed_files):
        y = 6.3 - i * 1.1
        r = Rectangle(
            (col3_x + 0.2, y - 0.3),
            3.8,
            0.6,
            facecolor=STYLE["grid"],
            edgecolor=STYLE["accent3"],
            linewidth=0.8,
            zorder=3,
        )
        ax.add_patch(r)
        ax.text(
            col3_x + 0.4,
            y,
            name,
            color=STYLE["text"],
            fontsize=8,
            fontweight="bold",
            ha="left",
            va="center",
            fontfamily="monospace",
            path_effects=stroke,
            zorder=5,
        )
        ax.text(
            col3_x + 3.9,
            y,
            desc,
            color=STYLE["text_dim"],
            fontsize=7,
            ha="right",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # ── Arrows between columns ───────────────────────────────────────
    for y in [5.5, 3.5]:
        ax.annotate(
            "",
            xy=(col2_x, y),
            xytext=(col1_x + 4.0, y),
            arrowprops=dict(
                arrowstyle="->,head_width=0.2,head_length=0.1",
                color=STYLE["text_dim"],
                lw=2,
            ),
            zorder=4,
        )
    for y in [5.5, 3.5]:
        ax.annotate(
            "",
            xy=(col3_x, y),
            xytext=(col2_x + 4.5, y),
            arrowprops=dict(
                arrowstyle="->,head_width=0.2,head_length=0.1",
                color=STYLE["text_dim"],
                lw=2,
            ),
            zorder=4,
        )

    # ── Bottom: GPU upload arrows ────────────────────────────────────
    ax.text(
        8.25,
        -0.8,
        "GPU Upload",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    for x in [col3_x + 0.6, col3_x + 1.6, col3_x + 2.6, col3_x + 3.5]:
        ax.annotate(
            "",
            xy=(x, -0.3),
            xytext=(x, 0.5),
            arrowprops=dict(
                arrowstyle="->,head_width=0.15,head_length=0.08",
                color=STYLE["warn"],
                lw=1.5,
            ),
            zorder=4,
        )
    ax.text(
        col3_x + 2.1,
        -1.4,
        "SDL_UploadToGPUBuffer / SDL_UploadToGPUTexture",
        color=STYLE["text_dim"],
        fontsize=8,
        fontfamily="monospace",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "Asset Pipeline: Raw Files to GPU-Ready Data",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "asset_pipeline_flow.png")


# ---------------------------------------------------------------------------
# gpu/39-loading-processed-assets — bc7_block_compression.png
# ---------------------------------------------------------------------------


def diagram_bc7_block_compression():
    """Illustrate BC7 block compression: 4x4 pixels to 128-bit block."""
    fig = plt.figure(figsize=(14, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 14.5), ylim=(-1.5, 6), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # ── Left: 4x4 pixel grid ────────────────────────────────────────
    grid_x, grid_y = 0.5, 1.0
    cell = 0.8
    colors_4x4 = [
        ["#cc4444", "#dd5555", "#cc3333", "#bb4444"],
        ["#dd6666", "#ee7777", "#dd5555", "#cc5555"],
        ["#cc5555", "#dd6666", "#cc4444", "#bb3333"],
        ["#bb3333", "#cc4444", "#bb3333", "#aa2222"],
    ]
    for row in range(4):
        for col in range(4):
            r = Rectangle(
                (grid_x + col * cell, grid_y + (3 - row) * cell),
                cell,
                cell,
                facecolor=colors_4x4[row][col],
                edgecolor=STYLE["text_dim"],
                linewidth=0.5,
                zorder=3,
            )
            ax.add_patch(r)

    ax.text(
        grid_x + 2 * cell,
        grid_y + 4 * cell + 0.4,
        "4 x 4 Pixel Block",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        grid_x + 2 * cell,
        grid_y - 0.5,
        "16 pixels x 4 bytes = 64 bytes\n(uncompressed RGBA8)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # ── Arrow ────────────────────────────────────────────────────────
    ax.annotate(
        "",
        xy=(5.5, 2.6),
        xytext=(4.0, 2.6),
        arrowprops=dict(
            arrowstyle="->,head_width=0.3,head_length=0.15",
            color=STYLE["warn"],
            lw=3,
        ),
        zorder=4,
    )
    ax.text(
        4.75,
        3.3,
        "BC7\nEncode",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # ── Right: Compressed block ──────────────────────────────────────
    block_x, block_y = 5.8, 1.5
    block_w, block_h = 5.5, 2.2
    block_rect = FancyBboxPatch(
        (block_x, block_y),
        block_w,
        block_h,
        boxstyle="round,pad=0.12",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent3"],
        linewidth=2,
        zorder=3,
    )
    ax.add_patch(block_rect)
    ax.text(
        block_x + block_w / 2,
        block_y + block_h - 0.35,
        "128-bit Compressed Block",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Sub-fields
    fields = ["mode", "partition", "endpoints", "indices"]
    field_w = block_w / len(fields) - 0.15
    for i, f in enumerate(fields):
        fx = block_x + 0.2 + i * (field_w + 0.1)
        r = Rectangle(
            (fx, block_y + 0.3),
            field_w,
            0.8,
            facecolor=STYLE["grid"],
            edgecolor=STYLE["accent3"],
            linewidth=0.8,
            zorder=4,
        )
        ax.add_patch(r)
        ax.text(
            fx + field_w / 2,
            block_y + 0.7,
            f,
            color=STYLE["text"],
            fontsize=8,
            fontfamily="monospace",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    ax.text(
        block_x + block_w / 2,
        block_y - 0.5,
        "16 bytes (= 1 byte/pixel)",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # ── Right side: summary ──────────────────────────────────────────
    summary_x = 12.0
    ax.text(
        summary_x,
        4.8,
        "4:1 Compression Ratio",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        summary_x,
        4.1,
        "64 bytes  ->  16 bytes\n(per 4x4 block)",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        va="center",
        fontfamily="monospace",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        summary_x,
        0.3,
        "BC7 preserves alpha channel\n(8 mode bits select endpoint precision)",
        color=STYLE["text_dim"],
        fontsize=9,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "BC7 Block Compression",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "bc7_block_compression.png")


# ---------------------------------------------------------------------------
# gpu/39-loading-processed-assets — bc5_normal_z_reconstruction.png
# ---------------------------------------------------------------------------


def diagram_bc5_normal_z_reconstruction():
    """BC5 normal map compression with Z reconstruction from unit length."""
    fig = plt.figure(figsize=(16, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 16.5), ylim=(-1.5, 7.5), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # ── Left: RGB Normal Map ─────────────────────────────────────────
    left_x = 0.5
    left_box = FancyBboxPatch(
        (left_x, 1.5),
        3.5,
        4.5,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(left_box)
    ax.text(
        left_x + 1.75,
        5.7,
        "RGB Normal Map",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    channels_left = [
        ("R", "X (tangent)", "#cc4444"),
        ("G", "Y (bitangent)", "#44cc44"),
        ("B", "Z (normal)", "#4444cc"),
    ]
    for i, (ch, label, col) in enumerate(channels_left):
        y = 4.8 - i * 1.2
        r = Rectangle(
            (left_x + 0.3, y - 0.35),
            2.9,
            0.7,
            facecolor=col + "40",
            edgecolor=col,
            linewidth=1.5,
            zorder=3,
        )
        ax.add_patch(r)
        ax.text(
            left_x + 1.75,
            y,
            f"{ch}: {label}",
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    ax.text(
        left_x + 1.75,
        1.8,
        "3 channels stored",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # ── Arrow left to middle ─────────────────────────────────────────
    ax.annotate(
        "",
        xy=(5.0, 3.75),
        xytext=(4.2, 3.75),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.1",
            color=STYLE["warn"],
            lw=2.5,
        ),
        zorder=4,
    )
    ax.text(
        4.6,
        4.5,
        "BC5\nEncode",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # ── Middle: BC5 Storage ──────────────────────────────────────────
    mid_x = 5.2
    mid_box = FancyBboxPatch(
        (mid_x, 1.5),
        3.5,
        4.5,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(mid_box)
    ax.text(
        mid_x + 1.75,
        5.7,
        "BC5 Compressed",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    channels_mid = [
        ("R", "X (tangent)", "#cc4444"),
        ("G", "Y (bitangent)", "#44cc44"),
    ]
    for i, (ch, label, col) in enumerate(channels_mid):
        y = 4.5 - i * 1.2
        r = Rectangle(
            (mid_x + 0.3, y - 0.35),
            2.9,
            0.7,
            facecolor=col + "40",
            edgecolor=col,
            linewidth=1.5,
            zorder=3,
        )
        ax.add_patch(r)
        ax.text(
            mid_x + 1.75,
            y,
            f"{ch}: {label}",
            color=STYLE["text"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Crossed-out Z
    z_y = 2.1
    r_z = Rectangle(
        (mid_x + 0.3, z_y - 0.35),
        2.9,
        0.7,
        facecolor="#4444cc10",
        edgecolor="#4444cc",
        linewidth=1.0,
        linestyle="--",
        zorder=3,
    )
    ax.add_patch(r_z)
    ax.text(
        mid_x + 1.75,
        z_y,
        "B: Z discarded",
        color="#666688",
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    # X through Z channel
    ax.plot(
        [mid_x + 0.3, mid_x + 3.2],
        [z_y - 0.35, z_y + 0.35],
        "-",
        color="#cc4444",
        lw=2,
        zorder=4,
    )
    ax.plot(
        [mid_x + 0.3, mid_x + 3.2],
        [z_y + 0.35, z_y - 0.35],
        "-",
        color="#cc4444",
        lw=2,
        zorder=4,
    )

    ax.text(
        mid_x + 1.75,
        1.8,
        "2 channels stored",
        color=STYLE["accent2"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # ── Arrow middle to right ────────────────────────────────────────
    ax.annotate(
        "",
        xy=(9.8, 3.75),
        xytext=(9.0, 3.75),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.1",
            color=STYLE["warn"],
            lw=2.5,
        ),
        zorder=4,
    )
    ax.text(
        9.4,
        4.5,
        "Shader\nReconstruct",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # ── Right: Reconstructed + Unit Sphere ───────────────────────────
    right_x = 10.0
    right_box = FancyBboxPatch(
        (right_x, 1.5),
        6.0,
        4.5,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent3"],
        linewidth=2,
        zorder=2,
    )
    ax.add_patch(right_box)
    ax.text(
        right_x + 3.0,
        5.7,
        "Z Reconstruction in Shader",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Formula
    ax.text(
        right_x + 3.0,
        4.8,
        "z = sqrt(1 - x\u00b2 - y\u00b2)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        fontfamily="monospace",
        path_effects=stroke,
        zorder=5,
    )

    # Unit sphere illustration
    sphere_cx, sphere_cy = right_x + 3.0, 2.8
    sphere_r = 1.3
    # Draw sphere outline
    theta = np.linspace(0, 2 * np.pi, 100)
    ax.plot(
        sphere_cx + sphere_r * np.cos(theta),
        sphere_cy + sphere_r * np.sin(theta),
        "-",
        color=STYLE["accent3"],
        lw=2,
        zorder=4,
    )
    # Draw equator (ellipse)
    ax.plot(
        sphere_cx + sphere_r * np.cos(theta),
        sphere_cy + 0.3 * sphere_r * np.sin(theta),
        "-",
        color=STYLE["accent3"],
        lw=1,
        alpha=0.5,
        zorder=4,
    )

    # Normal vector on sphere surface
    angle = np.radians(35)
    nx = sphere_r * np.cos(angle) * 0.8
    ny = sphere_r * np.sin(angle) * 0.8
    ax.annotate(
        "",
        xy=(sphere_cx + nx, sphere_cy + ny),
        xytext=(sphere_cx, sphere_cy),
        arrowprops=dict(
            arrowstyle="->,head_width=0.15,head_length=0.08",
            color=STYLE["warn"],
            lw=2,
        ),
        zorder=5,
    )
    ax.text(
        sphere_cx + nx + 0.3,
        sphere_cy + ny + 0.15,
        "N",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.text(
        sphere_cx,
        sphere_cy - sphere_r - 0.5,
        "|N| = 1 (unit sphere)",
        color=STYLE["text_dim"],
        fontsize=9,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # ── Bottom annotation ────────────────────────────────────────────
    ax.text(
        8.25,
        -0.8,
        "2-channel storage, 3rd channel reconstructed from unit length constraint",
        color=STYLE["text_dim"],
        fontsize=10,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "BC5 Normal Map Compression with Z Reconstruction",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "bc5_normal_z_reconstruction.png")


# ---------------------------------------------------------------------------
# gpu/39-loading-processed-assets — fmesh_binary_layout.png
# ---------------------------------------------------------------------------


def diagram_fmesh_binary_layout():
    """Binary file layout of the .fmesh format as stacked blocks."""
    fig = plt.figure(figsize=(12, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-0.5, 12.5), ylim=(-3.5, 9), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    block_x = 1.0
    block_w = 10.0

    # Sections: (label, height, color, fields, size_label)
    sections = [
        (
            "Header",
            STYLE["accent1"],
            [
                ('magic "FMSH"', "4B"),
                ("version", "4B"),
                ("vertex_count", "4B"),
                ("vertex_stride", "4B"),
                ("lod_count", "4B"),
                ("flags", "4B"),
                ("reserved", "8B"),
            ],
            "32 bytes",
        ),
        (
            "LOD Table",
            STYLE["accent2"],
            [
                ("index_count", "4B"),
                ("index_offset", "4B"),
                ("target_error", "4B"),
            ],
            "12 bytes x lod_count",
        ),
        (
            "Vertex Data",
            STYLE["accent3"],
            [
                ("position (float3)", "12B"),
                ("normal (float3)", "12B"),
                ("uv (float2)", "8B"),
                ("tangent (float4) — optional", "16B"),
            ],
            "32 or 48 bytes/vertex",
        ),
        (
            "Index Data",
            STYLE["accent4"],
            [
                ("uint32 indices", "4B each"),
                ("all LODs concatenated", ""),
            ],
            "4 bytes x total_indices",
        ),
    ]

    y_cursor = 8.0
    for section_label, color, fields, size_label in sections:
        field_count = len(fields)
        section_h = 0.35 + field_count * 0.45 + 0.35

        # Section box
        r = FancyBboxPatch(
            (block_x, y_cursor - section_h),
            block_w,
            section_h,
            boxstyle="round,pad=0.08",
            facecolor=color + "18",
            edgecolor=color,
            linewidth=2,
            zorder=3,
        )
        ax.add_patch(r)

        # Section title
        ax.text(
            block_x + 0.3,
            y_cursor - 0.3,
            section_label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

        # Size label on right
        ax.text(
            block_x + block_w - 0.3,
            y_cursor - 0.3,
            size_label,
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="right",
            va="center",
            fontfamily="monospace",
            path_effects=stroke,
            zorder=5,
        )

        # Fields
        for i, (field_name, field_size) in enumerate(fields):
            fy = y_cursor - 0.65 - i * 0.45
            field_rect = Rectangle(
                (block_x + 0.4, fy - 0.15),
                block_w - 0.8,
                0.35,
                facecolor=STYLE["grid"],
                edgecolor=color,
                linewidth=0.5,
                zorder=4,
            )
            ax.add_patch(field_rect)
            ax.text(
                block_x + 0.6,
                fy,
                field_name,
                color=STYLE["text"],
                fontsize=8,
                fontfamily="monospace",
                ha="left",
                va="center",
                path_effects=stroke,
                zorder=5,
            )
            if field_size:
                ax.text(
                    block_x + block_w - 0.6,
                    fy,
                    field_size,
                    color=STYLE["text_dim"],
                    fontsize=8,
                    fontfamily="monospace",
                    ha="right",
                    va="center",
                    path_effects=stroke,
                    zorder=5,
                )

        y_cursor -= section_h + 0.25

    # Byte offset arrows on the left
    ax.text(
        0.3,
        4.0,
        "offset\n  0",
        color=STYLE["text_dim"],
        fontsize=7,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )
    ax.annotate(
        "",
        xy=(block_x - 0.05, 8.0),
        xytext=(block_x - 0.05, y_cursor + 0.25),
        arrowprops=dict(
            arrowstyle="<->",
            color=STYLE["text_dim"],
            lw=1.5,
        ),
        zorder=3,
    )

    ax.set_title(
        ".fmesh Binary File Layout",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "fmesh_binary_layout.png")


# ---------------------------------------------------------------------------
# gpu/39-loading-processed-assets — lod_distance_selection.png
# ---------------------------------------------------------------------------


def diagram_lod_distance_selection():
    """LOD distance thresholds with concentric zones and model silhouettes."""
    fig = plt.figure(figsize=(12, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-10, 10), ylim=(-2, 14), grid=False, aspect="equal")
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    cam_x, cam_y = 0.0, -0.5

    # Concentric distance zones (arcs from camera)
    zones = [
        (3.0, STYLE["accent3"], "LOD 0\nFull Detail", "100% triangles"),
        (8.0, STYLE["accent1"], "LOD 1\nMedium", "~50% triangles"),
        (12.0, STYLE["accent2"], "LOD 2\nLow", "~25% triangles"),
    ]

    for radius, color, _label, _tri_label in reversed(zones):
        # Draw filled arc (semicircle above camera)
        theta = np.linspace(0, np.pi, 100)
        arc_x = cam_x + radius * np.cos(theta)
        arc_y = cam_y + radius * np.sin(theta)
        # Fill
        verts = np.column_stack(
            [
                np.concatenate([[cam_x - radius], arc_x, [cam_x + radius]]),
                np.concatenate([[cam_y], arc_y, [cam_y]]),
            ]
        )
        poly = Polygon(
            verts,
            closed=True,
            facecolor=color + "15",
            edgecolor=color,
            linewidth=2,
            linestyle="--",
            zorder=2,
        )
        ax.add_patch(poly)

    # Zone labels and model representations
    zone_data = [
        (1.5, STYLE["accent3"], "LOD 0", "100%\ntriangles", 12),
        (5.5, STYLE["accent1"], "LOD 1", "~50%\ntriangles", 8),
        (10.0, STYLE["accent2"], "LOD 2", "~25%\ntriangles", 5),
    ]

    for dist, color, lod_label, tri_label, n_sides in zone_data:
        # Model silhouette (polygon with decreasing detail)
        model_r = 0.6
        angles = np.linspace(0, 2 * np.pi, n_sides + 1)
        mx = 0.0 + model_r * np.cos(angles)
        my = cam_y + dist + model_r * np.sin(angles)
        model_poly = Polygon(
            np.column_stack([mx, my]),
            closed=True,
            facecolor=color + "50",
            edgecolor=color,
            linewidth=2,
            zorder=5,
        )
        ax.add_patch(model_poly)

        # LOD label
        ax.text(
            3.5,
            cam_y + dist,
            lod_label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=6,
        )
        ax.text(
            5.5,
            cam_y + dist,
            tri_label,
            color=color,
            fontsize=9,
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=6,
        )

    # Distance labels on right edge
    for radius, color, _, _ in zones:
        ax.text(
            -5.0,
            cam_y + radius,
            f"d = {radius:.0f}",
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            zorder=6,
        )

    # Camera icon
    ax.plot(cam_x, cam_y, "s", color=STYLE["warn"], markersize=14, zorder=7)
    ax.text(
        cam_x,
        cam_y - 0.8,
        "Camera",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    # Annotation
    ax.text(
        0.0,
        -1.7,
        "Closer objects use higher-detail LODs; distant objects use simplified meshes",
        color=STYLE["text_dim"],
        fontsize=9,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "LOD Distance Selection",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "lod_distance_selection.png")


# ---------------------------------------------------------------------------
# gpu/39-loading-processed-assets — tbn_gram_schmidt.png
# ---------------------------------------------------------------------------


def diagram_tbn_gram_schmidt():
    """Gram-Schmidt re-orthogonalization of the TBN basis."""
    fig = plt.figure(figsize=(12, 8), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-3, 5), ylim=(-1.5, 5.5), grid=False, aspect="equal")
    ax.set_xticks([])
    ax.set_yticks([])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    origin = np.array([0.0, 0.0])

    def _pt(v: np.ndarray) -> tuple[float, float]:
        """Convert ndarray to tuple for matplotlib annotate xytext."""
        return float(v[0]), float(v[1])

    # Normal vector (pointing up)
    n_vec = np.array([0.0, 3.5])
    ax.annotate(
        "",
        xy=(origin[0] + n_vec[0], origin[1] + n_vec[1]),
        xytext=_pt(origin),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.12",
            color=STYLE["accent3"],
            lw=3,
        ),
        zorder=5,
    )
    ax.text(
        n_vec[0] + 0.3,
        n_vec[1],
        "N (normal)",
        color=STYLE["accent3"],
        fontsize=12,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    # Original tangent (not perpendicular to N)
    t_vec = np.array([2.8, 1.5])
    ax.annotate(
        "",
        xy=(origin[0] + t_vec[0], origin[1] + t_vec[1]),
        xytext=_pt(origin),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.12",
            color=STYLE["accent2"],
            lw=2.5,
            linestyle="dashed",
        ),
        zorder=4,
    )
    ax.text(
        t_vec[0] + 0.2,
        t_vec[1] + 0.3,
        "T (original)",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    # Projection of T onto N: proj_N(T) = N * dot(N,T) / dot(N,N)
    n_unit = n_vec / np.linalg.norm(n_vec)
    dot_nt = np.dot(t_vec, n_unit)
    proj = n_unit * dot_nt

    # Draw projection line (dashed, from T tip down to N axis)
    ax.plot(
        [t_vec[0], proj[0]],
        [t_vec[1], proj[1]],
        "--",
        color=STYLE["text_dim"],
        lw=1.5,
        zorder=3,
    )
    # Draw projection vector on N
    ax.annotate(
        "",
        xy=(proj[0], proj[1]),
        xytext=_pt(origin),
        arrowprops=dict(
            arrowstyle="->,head_width=0.15,head_length=0.08",
            color=STYLE["text_dim"],
            lw=1.5,
        ),
        zorder=3,
    )
    ax.text(
        proj[0] - 0.8,
        proj[1] * 0.5,
        "N * dot(N, T)",
        color=STYLE["text_dim"],
        fontsize=9,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Re-orthogonalized tangent: T' = T - N * dot(N,T)
    t_prime = t_vec - proj
    ax.annotate(
        "",
        xy=(origin[0] + t_prime[0], origin[1] + t_prime[1]),
        xytext=_pt(origin),
        arrowprops=dict(
            arrowstyle="->,head_width=0.2,head_length=0.12",
            color=STYLE["accent1"],
            lw=3,
        ),
        zorder=5,
    )
    ax.text(
        t_prime[0] + 0.2,
        t_prime[1] - 0.4,
        "T' (re-orthogonalized)",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
        zorder=6,
    )

    # Right angle marker between N and T'
    marker_size = 0.3
    t_prime_unit = t_prime / np.linalg.norm(t_prime)
    n_u = n_vec / np.linalg.norm(n_vec)
    corner = origin + marker_size * t_prime_unit
    corner2 = origin + marker_size * n_u
    corner3 = corner + marker_size * n_u
    ax.plot(
        [corner[0], corner3[0], corner2[0]],
        [corner[1], corner3[1], corner2[1]],
        "-",
        color=STYLE["warn"],
        lw=1.5,
        zorder=4,
    )

    # Step labels on the right
    steps_x = -2.5
    step_data = [
        ("Subtract:", "T' = T - N * dot(N, T)", STYLE["accent1"]),
        ("Normalize:", "T' = normalize(T')", STYLE["accent1"]),
        ("Bitangent:", "B = cross(N, T') * tangent.w", STYLE["accent4"]),
    ]
    for i, (step, formula, color) in enumerate(step_data):
        y = 5.0 - i * 0.7
        ax.text(
            steps_x,
            y,
            step,
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=5,
        )
        ax.text(
            steps_x + 1.0,
            y,
            formula,
            color=STYLE["text"],
            fontsize=9,
            fontfamily="monospace",
            ha="left",
            va="center",
            path_effects=stroke,
            zorder=5,
        )

    # Bitangent indication (coming out of the page)
    ax.plot(
        origin[0],
        origin[1],
        "o",
        color=STYLE["accent4"],
        markersize=8,
        zorder=6,
    )
    ax.plot(
        origin[0],
        origin[1],
        ".",
        color=STYLE["bg"],
        markersize=4,
        zorder=7,
    )
    ax.text(
        origin[0] - 0.6,
        origin[1] - 0.4,
        "B (out of page)",
        color=STYLE["accent4"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    # Annotation
    ax.text(
        1.0,
        -1.0,
        "Gram-Schmidt ensures T' is perpendicular to N\n"
        "Handedness (stored in tangent.w) determines B direction",
        color=STYLE["text_dim"],
        fontsize=9,
        fontstyle="italic",
        ha="center",
        va="center",
        path_effects=stroke,
        zorder=5,
    )

    ax.set_title(
        "Gram-Schmidt TBN Re-orthogonalization",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, LESSON_PATH, "tbn_gram_schmidt.png")
