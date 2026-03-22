"""Diagrams for assets/18 — Scene Editor."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# Diagram 1 — Scene data model: flat list with parent references
# ---------------------------------------------------------------------------


def diagram_scene_data_model():
    """Show how a flat object list with parent_id forms a hierarchy tree."""
    fig, (ax_flat, ax_tree) = plt.subplots(1, 2, figsize=(14, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Scene Data Model: Flat List with Parent References",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    # ── Left: flat JSON-like table ──────────────────────────────────────
    setup_axes(ax_flat, xlim=(-0.5, 7.0), ylim=(-0.5, 5.5), grid=False, aspect=None)
    ax_flat.set_title(
        "Stored: flat array of objects", color=STYLE["text"], fontsize=11, pad=12
    )
    ax_flat.set_xticks([])
    ax_flat.set_yticks([])

    rows = [
        ("w01", "World", None, STYLE["accent1"]),
        ("p01", "Player", "w01", STYLE["accent2"]),
        ("p02", "Weapon", "p01", STYLE["accent3"]),
        ("e01", "Enemy", "w01", STYLE["accent4"]),
        ("t01", "Tree", None, STYLE["warn"]),
    ]
    headers = ["id", "name", "parent_id"]
    col_x = [1.0, 3.0, 5.5]
    header_y = 5.0

    # Header row
    for j, header in enumerate(headers):
        ax_flat.text(
            col_x[j],
            header_y,
            header,
            color=STYLE["text_dim"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
        )
    ax_flat.plot(
        [-0.2, 6.8], [header_y - 0.3, header_y - 0.3], color=STYLE["grid"], lw=1
    )

    for i, (obj_id, name, parent, color) in enumerate(rows):
        y = header_y - 1.0 - i * 0.9
        # Row background
        rect = mpatches.FancyBboxPatch(
            (-0.1, y - 0.3),
            6.8,
            0.6,
            boxstyle="round,pad=0.05",
            facecolor=STYLE["surface"],
            edgecolor=STYLE["grid"],
            linewidth=0.5,
            alpha=0.6,
        )
        ax_flat.add_patch(rect)
        # ID
        ax_flat.text(
            col_x[0],
            y,
            f'"{obj_id}"',
            color=STYLE["text_dim"],
            fontsize=10,
            ha="center",
            va="center",
        )
        # Name
        ax_flat.text(
            col_x[1],
            y,
            f'"{name}"',
            color=color,
            fontsize=10,
            ha="center",
            va="center",
            fontweight="bold",
        )
        # Parent
        parent_text = f'"{parent}"' if parent else "null"
        parent_color = STYLE["text_dim"] if parent is None else STYLE["text"]
        ax_flat.text(
            col_x[2],
            y,
            parent_text,
            color=parent_color,
            fontsize=10,
            ha="center",
            va="center",
        )

    # ── Right: tree visualization ───────────────────────────────────────
    setup_axes(ax_tree, xlim=(-1, 7), ylim=(-0.5, 5.5), grid=False, aspect=None)
    ax_tree.set_title(
        "Rendered: hierarchy tree", color=STYLE["text"], fontsize=11, pad=12
    )
    ax_tree.set_xticks([])
    ax_tree.set_yticks([])

    # Tree node positions
    nodes = {
        "World": (2.0, 4.5, STYLE["accent1"]),
        "Player": (1.0, 3.0, STYLE["accent2"]),
        "Weapon": (1.0, 1.5, STYLE["accent3"]),
        "Enemy": (3.5, 3.0, STYLE["accent4"]),
        "Tree": (5.5, 4.5, STYLE["warn"]),
    }
    edges = [
        ("World", "Player"),
        ("World", "Enemy"),
        ("Player", "Weapon"),
    ]

    # Draw edges first
    for parent_name, child_name in edges:
        px, py, _ = nodes[parent_name]
        cx, cy, _ = nodes[child_name]
        ax_tree.plot(
            [px, cx],
            [py, cy],
            color=STYLE["grid"],
            lw=2,
            zorder=1,
        )

    # Draw nodes
    for name, (x, y, color) in nodes.items():
        circle = mpatches.FancyBboxPatch(
            (x - 0.7, y - 0.3),
            1.4,
            0.6,
            boxstyle="round,pad=0.1",
            facecolor=color,
            alpha=0.8,
            edgecolor=STYLE["text"],
            linewidth=1,
            zorder=2,
        )
        ax_tree.add_patch(circle)
        ax_tree.text(
            x,
            y,
            name,
            color=STYLE["bg"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=3,
        )

    # Root indicators
    for name in ["World", "Tree"]:
        x, y, _ = nodes[name]
        ax_tree.text(
            x,
            y + 0.6,
            "root",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            fontstyle="italic",
        )

    plt.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, "assets/18-scene-editor", "scene_data_model.png")


# ---------------------------------------------------------------------------
# Diagram 2 — Undo/redo snapshot stack flow
# ---------------------------------------------------------------------------


def diagram_undo_redo_flow():
    """Show the snapshot-based undo/redo stack mechanics."""
    fig, axes = plt.subplots(1, 3, figsize=(15, 6))
    fig.patch.set_facecolor(STYLE["bg"])
    fig.suptitle(
        "Undo/Redo: Snapshot Stack Mechanics",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    def draw_stack(ax, title, undo_items, current, redo_items, action_label):
        setup_axes(ax, xlim=(-1, 9), ylim=(-1, 8), grid=False, aspect=None)
        ax.set_xticks([])
        ax.set_yticks([])
        ax.set_title(title, color=STYLE["text"], fontsize=11, pad=12)

        # Undo stack (left)
        ux = 1.0
        ax.text(
            ux,
            7.2,
            "Undo Stack",
            color=STYLE["accent1"],
            fontsize=9,
            fontweight="bold",
            ha="center",
        )
        for i, item in enumerate(undo_items):
            y = 6.0 - i * 1.1
            rect = mpatches.FancyBboxPatch(
                (ux - 0.8, y - 0.35),
                1.6,
                0.7,
                boxstyle="round,pad=0.05",
                facecolor=STYLE["accent1"],
                alpha=0.3,
                edgecolor=STYLE["accent1"],
                linewidth=1,
            )
            ax.add_patch(rect)
            ax.text(
                ux, y, item, color=STYLE["text"], fontsize=8, ha="center", va="center"
            )

        # Current state (center)
        cx = 4.0
        ax.text(
            cx,
            7.2,
            "Current",
            color=STYLE["warn"],
            fontsize=9,
            fontweight="bold",
            ha="center",
        )
        rect = mpatches.FancyBboxPatch(
            (cx - 1.0, 5.65),
            2.0,
            0.7,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["warn"],
            alpha=0.5,
            edgecolor=STYLE["warn"],
            linewidth=2,
        )
        ax.add_patch(rect)
        ax.text(
            cx,
            6.0,
            current,
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            va="center",
            fontweight="bold",
        )

        # Redo stack (right)
        rx = 7.0
        ax.text(
            rx,
            7.2,
            "Redo Stack",
            color=STYLE["accent2"],
            fontsize=9,
            fontweight="bold",
            ha="center",
        )
        for i, item in enumerate(redo_items):
            y = 6.0 - i * 1.1
            rect = mpatches.FancyBboxPatch(
                (rx - 0.8, y - 0.35),
                1.6,
                0.7,
                boxstyle="round,pad=0.05",
                facecolor=STYLE["accent2"],
                alpha=0.3,
                edgecolor=STYLE["accent2"],
                linewidth=1,
            )
            ax.add_patch(rect)
            ax.text(
                rx, y, item, color=STYLE["text"], fontsize=8, ha="center", va="center"
            )

        if not redo_items:
            ax.text(
                rx,
                5.5,
                "(empty)",
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                fontstyle="italic",
            )

        # Action label at bottom
        ax.text(
            4.0,
            -0.3,
            action_label,
            color=STYLE["accent3"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            bbox={
                "boxstyle": "round,pad=0.4",
                "facecolor": STYLE["surface"],
                "edgecolor": STYLE["accent3"],
                "alpha": 0.8,
            },
            path_effects=stroke,
        )

    # Panel 1: After two mutations
    draw_stack(
        axes[0],
        "Step 1: Two Edits Made",
        ["Scene v0", "Scene v1"],
        "Scene v2",
        [],
        "Add Object → Move Object",
    )

    # Panel 2: After undo
    draw_stack(
        axes[1],
        "Step 2: Undo (Ctrl+Z)",
        ["Scene v0"],
        "Scene v1",
        ["Scene v2"],
        "Pop undo → Push redo",
    )

    # Panel 3: After new mutation (redo cleared)
    draw_stack(
        axes[2],
        "Step 3: New Edit (Redo Cleared)",
        ["Scene v0", "Scene v1"],
        "Scene v3",
        [],
        "New mutation clears redo",
    )

    # Arrows between panels
    for i in range(2):
        fig.patches.append(
            mpatches.FancyArrowPatch(
                posA=(0.36 + i * 0.33, 0.5),
                posB=(0.37 + i * 0.33, 0.5),
                arrowstyle="->,head_width=0.02,head_length=0.01",
                color=STYLE["text_dim"],
                transform=fig.transFigure,
                lw=2,
            )
        )

    plt.tight_layout(rect=(0, 0.04, 1, 0.93))
    save(fig, "assets/18-scene-editor", "undo_redo_flow.png")


# ---------------------------------------------------------------------------
# Diagram 3 — Editor component architecture
# ---------------------------------------------------------------------------


def diagram_editor_layout():
    """Show the scene editor UI layout and component responsibilities."""
    fig = plt.figure(figsize=(12, 8))
    fig.patch.set_facecolor(STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(0, 12), ylim=(0, 9), grid=False, aspect=None)
    ax.set_xticks([])
    ax.set_yticks([])
    ax.set_title(
        "Scene Editor Component Layout",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=16,
    )

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    def draw_panel(x, y, w, h, label, sublabel, color, items=None):
        rect = mpatches.FancyBboxPatch(
            (x, y),
            w,
            h,
            boxstyle="round,pad=0.1",
            facecolor=color,
            alpha=0.15,
            edgecolor=color,
            linewidth=2,
        )
        ax.add_patch(rect)
        ax.text(
            x + w / 2,
            y + h - 0.25,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="top",
            path_effects=stroke,
        )
        if sublabel:
            ax.text(
                x + w / 2,
                y + h - 0.65,
                sublabel,
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                va="top",
            )
        if items:
            for i, item in enumerate(items):
                ax.text(
                    x + 0.3,
                    y + h - 1.2 - i * 0.4,
                    f"  {item}",
                    color=STYLE["text"],
                    fontsize=8,
                    va="top",
                )

    # Toolbar (top bar)
    draw_panel(
        0.3,
        7.3,
        11.4,
        1.2,
        "Toolbar",
        "toolbar.tsx",
        STYLE["accent1"],
        [
            "Gizmo Mode (Move / Rotate / Scale)    |    Add  Delete    |    Undo  Redo    |    Save"
        ],
    )

    # Hierarchy panel (left)
    draw_panel(
        0.3,
        1.0,
        2.5,
        6.0,
        "Hierarchy",
        "hierarchy-panel.tsx",
        STYLE["accent3"],
        ["World", "  Player", "    Weapon", "  Enemy", "Tree"],
    )

    # Viewport (center)
    draw_panel(
        3.1,
        1.0,
        5.4,
        6.0,
        "Viewport",
        "viewport.tsx",
        STYLE["warn"],
        [
            "R3F Canvas",
            "OrbitControls",
            "TransformControls",
            "SceneObject (per item)",
            "Grid + Lighting",
        ],
    )

    # Inspector (right)
    draw_panel(
        8.8,
        1.0,
        2.9,
        6.0,
        "Inspector",
        "inspector-panel.tsx",
        STYLE["accent4"],
        [
            "Name",
            "Position  X Y Z",
            "Rotation  X Y Z",
            "Scale     X Y Z",
            "Parent dropdown",
            "Visible toggle",
        ],
    )

    # State store (bottom)
    rect = mpatches.FancyBboxPatch(
        (3.5, 0.1),
        5.0,
        0.7,
        boxstyle="round,pad=0.08",
        facecolor=STYLE["accent2"],
        alpha=0.2,
        edgecolor=STYLE["accent2"],
        linewidth=1.5,
        linestyle="--",
    )
    ax.add_patch(rect)
    ax.text(
        6.0,
        0.45,
        "useSceneStore (useReducer + undo/redo)  →  shared via props",
        color=STYLE["accent2"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "assets/18-scene-editor", "editor_layout.png")
