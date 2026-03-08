"""Diagrams for Asset Lesson 05 — Asset Bundles."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# 1. Bundle file layout — header, entries, TOC
# ---------------------------------------------------------------------------


def diagram_bundle_layout():
    """Binary layout of a .forgepak bundle showing header, entry data, and TOC."""
    fig, ax = plt.subplots(figsize=(7, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False)
    ax.set_xlim(-0.5, 6.5)
    ax.set_ylim(-0.5, 8.5)
    ax.set_aspect("equal")
    ax.axis("off")

    c1 = STYLE["accent1"]  # header
    c2 = STYLE["accent2"]  # entries
    c3 = STYLE["accent3"]  # TOC
    ct = STYLE["text"]

    # Draw blocks from top to bottom.
    blocks = [
        (
            7.0,
            1.0,
            c1,
            "Header (24 bytes)",
            [
                'magic: "FPAK"  (4 B)',
                "version: u32   (4 B)",
                "entry_count    (4 B)",
                "toc_offset     (8 B)",
                "toc_size       (4 B)",
            ],
        ),
        (
            4.5,
            2.0,
            c2,
            "Entry Data",
            [
                "Entry 0  (zstd compressed)",
                "Entry 1  (zstd compressed)",
                "Entry 2  (zstd compressed)",
                "…",
            ],
        ),
        (
            2.0,
            1.5,
            c3,
            "Table of Contents",
            [
                "JSON array of entries",
                "(zstd compressed)",
                "paths, offsets, sizes",
            ],
        ),
    ]

    for y_center, height, color, title, labels in blocks:
        rect = mpatches.FancyBboxPatch(
            (0.5, y_center - height / 2),
            5.0,
            height,
            boxstyle="round,pad=0.1",
            facecolor=color,
            edgecolor=ct,
            linewidth=1.5,
            alpha=0.85,
        )
        ax.add_patch(rect)
        ax.text(
            3.0,
            y_center + height / 2 - 0.15,
            title,
            ha="center",
            va="top",
            fontsize=11,
            fontweight="bold",
            color=ct,
        )
        for i, label in enumerate(labels):
            ax.text(
                3.0,
                y_center + height / 2 - 0.4 - i * 0.25,
                label,
                ha="center",
                va="top",
                fontsize=8,
                color=ct,
                alpha=0.85,
                family="monospace",
            )

    # Arrows between blocks.
    arrow_kw = dict(
        arrowstyle="->",
        color=ct,
        lw=1.5,
        connectionstyle="arc3,rad=0",
    )
    ax.annotate(
        "",
        xy=(3.0, 5.6),
        xytext=(3.0, 6.4),
        arrowprops=arrow_kw,
    )
    ax.annotate(
        "",
        xy=(3.0, 3.0),
        xytext=(3.0, 3.5),
        arrowprops=arrow_kw,
    )

    # Offset annotation.
    ax.annotate(
        "toc_offset",
        xy=(5.7, 2.75),
        xytext=(5.7, 7.2),
        fontsize=8,
        color=STYLE["accent4"],
        ha="center",
        arrowprops=dict(arrowstyle="->", color=STYLE["accent4"], lw=1.2),
    )

    ax.set_title("Bundle File Layout (.forgepak)", fontsize=13, color=ct, pad=12)
    fig.tight_layout()
    save(fig, "assets/05-asset-bundles", "bundle_layout.png")


# ---------------------------------------------------------------------------
# 2. Random access vs sequential — seek comparison
# ---------------------------------------------------------------------------


def diagram_random_vs_sequential():
    """Compare sequential decompression (tar.gz) vs random access (.forgepak)."""
    fig, axes = plt.subplots(1, 2, figsize=(10, 4))
    fig.patch.set_facecolor(STYLE["bg"])

    ct = STYLE["text"]
    c_skip = STYLE["text_dim"]
    c_read = STYLE["accent2"]
    c_target = STYLE["accent1"]
    c_header = STYLE["accent3"]

    n_entries = 6
    target_idx = 4  # which entry we want to read

    for _ax_idx, (ax, title, mode) in enumerate(
        zip(
            axes,
            ["Sequential (tar.gz)", "Random Access (.forgepak)"],
            ["sequential", "random"],
            strict=True,
        )
    ):
        setup_axes(ax, grid=False)
        ax.set_xlim(-0.5, 8)
        ax.set_ylim(-0.5, n_entries + 1.5)
        ax.axis("off")
        ax.set_title(title, fontsize=11, color=ct, pad=8)

        for i in range(n_entries):
            y = n_entries - i
            if mode == "sequential":
                # Must read all entries up to target.
                if i < target_idx:
                    color = c_skip
                    alpha = 0.4
                    label = f"Entry {i} (decompress)"
                elif i == target_idx:
                    color = c_target
                    alpha = 0.9
                    label = f"Entry {i} (TARGET)"
                else:
                    color = STYLE["grid"]
                    alpha = 0.2
                    label = f"Entry {i} (skip)"
            else:
                # Random access — only read target.
                if i == target_idx:
                    color = c_target
                    alpha = 0.9
                    label = f"Entry {i} (TARGET)"
                else:
                    color = STYLE["grid"]
                    alpha = 0.2
                    label = f"Entry {i} (skip)"

            rect = mpatches.FancyBboxPatch(
                (0.5, y - 0.35),
                5.0,
                0.7,
                boxstyle="round,pad=0.05",
                facecolor=color,
                alpha=alpha,
                edgecolor=ct,
                linewidth=0.8,
            )
            ax.add_patch(rect)
            ax.text(
                3.0,
                y,
                label,
                ha="center",
                va="center",
                fontsize=8,
                color=ct,
            )

        # Cost annotation.
        if mode == "sequential":
            ax.text(
                7.0,
                n_entries / 2,
                f"Read {target_idx + 1}\nentries",
                ha="center",
                va="center",
                fontsize=9,
                color=STYLE["warn"],
                fontweight="bold",
            )
        else:
            # Show seek arrow.
            ax.annotate(
                "seek",
                xy=(0.3, n_entries - target_idx),
                xytext=(0.3, n_entries + 0.5),
                fontsize=8,
                color=c_header,
                ha="center",
                arrowprops=dict(arrowstyle="->", color=c_header, lw=1.5),
            )
            ax.text(
                7.0,
                n_entries / 2,
                "Read 1\nentry\n(after TOC)",
                ha="center",
                va="center",
                fontsize=9,
                color=c_read,
                fontweight="bold",
            )

    fig.suptitle(
        "Loading One Asset: Sequential vs Random Access",
        fontsize=13,
        color=ct,
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "assets/05-asset-bundles", "random_vs_sequential.png")


# ---------------------------------------------------------------------------
# 3. Dependency graph — asset relationships
# ---------------------------------------------------------------------------


def diagram_dependency_graph():
    """Dependency graph showing how assets reference each other."""
    fig, ax = plt.subplots(figsize=(8, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False)
    ax.set_xlim(-1, 9)
    ax.set_ylim(-0.5, 5.5)
    ax.set_aspect("equal")
    ax.axis("off")

    ct = STYLE["text"]

    # Node positions and colors.
    nodes = {
        "scene.json": (4.0, 4.5, STYLE["accent1"]),
        "hero.fmesh": (2.0, 3.0, STYLE["accent2"]),
        "env.fmesh": (6.0, 3.0, STYLE["accent2"]),
        "hero_diff.ktx2": (0.5, 1.5, STYLE["accent3"]),
        "hero_norm.ktx2": (3.0, 1.5, STYLE["accent3"]),
        "env_diff.ktx2": (5.5, 1.5, STYLE["accent3"]),
        "shared.ktx2": (7.5, 1.5, STYLE["accent4"]),
    }

    # Edges: (from, to) — "from" depends on "to".
    edges = [
        ("scene.json", "hero.fmesh"),
        ("scene.json", "env.fmesh"),
        ("hero.fmesh", "hero_diff.ktx2"),
        ("hero.fmesh", "hero_norm.ktx2"),
        ("env.fmesh", "env_diff.ktx2"),
        ("env.fmesh", "shared.ktx2"),
        ("hero.fmesh", "shared.ktx2"),
    ]

    # Draw edges first (behind nodes).
    for src, dst in edges:
        sx, sy, _ = nodes[src]
        dx, dy, _ = nodes[dst]
        ax.annotate(
            "",
            xy=(dx, dy + 0.3),
            xytext=(sx, sy - 0.3),
            arrowprops=dict(
                arrowstyle="->",
                color=ct,
                lw=1.2,
                alpha=0.6,
                connectionstyle="arc3,rad=0.05",
            ),
        )

    # Draw nodes.
    for name, (x, y, color) in nodes.items():
        circle = plt.Circle(
            (x, y),
            0.4,
            facecolor=color,
            edgecolor=ct,
            linewidth=1.5,
            alpha=0.85,
            zorder=3,
        )
        ax.add_patch(circle)
        ax.text(
            x,
            y - 0.6,
            name,
            ha="center",
            va="top",
            fontsize=7.5,
            color=ct,
            family="monospace",
        )

    # Legend.
    ax.text(
        0.0,
        5.2,
        "depends on",
        fontsize=8,
        color=ct,
        alpha=0.7,
        style="italic",
    )
    ax.annotate(
        "",
        xy=(1.8, 5.2),
        xytext=(1.0, 5.2),
        arrowprops=dict(arrowstyle="->", color=ct, lw=1.2, alpha=0.6),
    )

    # Highlight shared dependency.
    ax.text(
        7.5,
        0.7,
        "shared by\ntwo meshes",
        ha="center",
        va="top",
        fontsize=7,
        color=STYLE["accent4"],
        fontstyle="italic",
    )

    ax.set_title(
        "Asset Dependency Graph",
        fontsize=13,
        color=ct,
        pad=12,
    )
    fig.tight_layout()
    save(fig, "assets/05-asset-bundles", "dependency_graph.png")
