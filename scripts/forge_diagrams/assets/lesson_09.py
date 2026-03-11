"""Diagrams for Asset Lesson 09 — Scene Hierarchy."""

import matplotlib.patches as mpatches
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# 1. CesiumMilkTruck scene hierarchy tree
# ---------------------------------------------------------------------------


def diagram_scene_hierarchy():
    """Node tree for the CesiumMilkTruck glTF model.

    Shows 6 nodes with parent-child edges, mesh references, and transforms.
    Transform-only nodes are one color; mesh-bearing nodes are another.
    """
    fig, ax = plt.subplots(figsize=(10, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, grid=False)
    ax.set_xlim(-0.5, 10.5)
    ax.set_ylim(-0.5, 6.5)
    ax.set_aspect("equal")
    ax.axis("off")

    ct = STYLE["text"]
    c_transform = STYLE["accent4"]  # purple — transform-only nodes
    c_mesh = STYLE["accent1"]  # cyan — nodes with mesh references

    # Node definitions: (x, y, label, sublabel, color)
    nodes = [
        (5.0, 5.5, "[5] Yup2Zup", "root, transform-only", c_transform),
        (5.0, 4.0, "[4] Cesium_Milk_Truck", "mesh 1 (body, 3 submeshes)", c_mesh),
        (2.5, 2.5, "[1] Node", "T=(1.43, 0, -0.43)", c_transform),
        (7.5, 2.5, "[3] Node.001", "T=(-1.35, 0, -0.43)", c_transform),
        (2.5, 1.0, "[0] Wheels", "mesh 0 (wheel)", c_mesh),
        (7.5, 1.0, "[2] Wheels.001", "mesh 0 (same wheel)", c_mesh),
    ]

    node_w = 2.4
    node_h = 0.65

    # Draw edges first (behind nodes)
    edges = [
        (5.0, 5.5, 5.0, 4.0),  # Yup2Zup -> Cesium_Milk_Truck
        (5.0, 4.0, 2.5, 2.5),  # Cesium_Milk_Truck -> Node
        (5.0, 4.0, 7.5, 2.5),  # Cesium_Milk_Truck -> Node.001
        (2.5, 2.5, 2.5, 1.0),  # Node -> Wheels
        (7.5, 2.5, 7.5, 1.0),  # Node.001 -> Wheels.001
    ]
    for x1, y1, x2, y2 in edges:
        ax.plot(
            [x1, x2],
            [y1 - node_h / 2, y2 + node_h / 2],
            color=STYLE["text_dim"],
            linewidth=1.5,
            zorder=1,
        )

    # Draw nodes
    for x, y, label, sublabel, color in nodes:
        rect = mpatches.FancyBboxPatch(
            (x - node_w / 2, y - node_h / 2),
            node_w,
            node_h,
            boxstyle="round,pad=0.08",
            facecolor=color,
            edgecolor=ct,
            linewidth=1.2,
            alpha=0.85,
            zorder=2,
        )
        ax.add_patch(rect)
        ax.text(
            x,
            y + 0.08,
            label,
            color="#ffffff",
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=3,
        )
        ax.text(
            x,
            y - 0.2,
            sublabel,
            color="#ddddee",
            fontsize=7,
            ha="center",
            va="center",
            zorder=3,
        )

    # Legend
    legend_y = 0.1
    for color, desc in [
        (c_transform, "Transform-only node"),
        (c_mesh, "Node with mesh reference"),
    ]:
        rect = mpatches.FancyBboxPatch(
            (0.0, legend_y - 0.15),
            0.3,
            0.3,
            boxstyle="round,pad=0.02",
            facecolor=color,
            edgecolor=ct,
            linewidth=0.8,
            alpha=0.85,
        )
        ax.add_patch(rect)
        ax.text(
            0.45,
            legend_y,
            desc,
            color=ct,
            fontsize=8,
            va="center",
        )
        legend_y -= 0.45

    save(fig, "assets/09-scene-hierarchy", "scene_hierarchy.png")
