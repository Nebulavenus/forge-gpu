"""Diagrams for engine/11."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

from .._common import STYLE, save


# ---------------------------------------------------------------------------
# engine/11-git-version-control — three_areas.png
# ---------------------------------------------------------------------------
def diagram_three_areas():
    """Git's three areas: working directory, staging area (index), and HEAD."""
    fig, ax = plt.subplots(figsize=(12, 6), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 12.5)
    ax.set_ylim(-0.5, 6.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Title
    ax.text(
        6.0,
        6.0,
        "Git's Three Areas",
        color=STYLE["text"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Box parameters: (x, y, width, height, label, subtitle, color)
    boxes = [
        (0.2, 1.5, 3.2, 3.0, "Working\nDirectory", "Files on disk", STYLE["accent2"]),
        (
            4.4,
            1.5,
            3.2,
            3.0,
            "Staging Area\n(Index)",
            "Next commit\npreview",
            STYLE["warn"],
        ),
        (
            8.6,
            1.5,
            3.2,
            3.0,
            "Repository\n(HEAD)",
            "Committed\nhistory",
            STYLE["accent3"],
        ),
    ]

    for bx, by, bw, bh, label, sub, color in boxes:
        rect = FancyBboxPatch(
            (bx, by),
            bw,
            bh,
            boxstyle="round,pad=0.15",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2.5,
        )
        ax.add_patch(rect)
        ax.text(
            bx + bw / 2,
            by + bh * 0.65,
            label,
            color=color,
            fontsize=13,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            bx + bw / 2,
            by + bh * 0.2,
            sub,
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            va="center",
        )

    # Arrows between boxes
    arrow_props = {
        "arrowstyle": "->,head_width=0.3,head_length=0.2",
        "lw": 2.5,
    }

    # Working -> Staging
    ax.annotate(
        "",
        xy=(4.2, 3.0),
        xytext=(3.6, 3.0),
        arrowprops={**arrow_props, "color": STYLE["accent1"]},
    )
    ax.text(
        3.9,
        3.7,
        "git add",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # Staging -> HEAD
    ax.annotate(
        "",
        xy=(8.4, 3.0),
        xytext=(7.8, 3.0),
        arrowprops={**arrow_props, "color": STYLE["accent1"]},
    )
    ax.text(
        8.1,
        3.7,
        "git commit",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # HEAD -> Working (checkout)
    ax.annotate(
        "",
        xy=(0.4, 1.3),
        xytext=(8.8, 1.3),
        arrowprops={
            **arrow_props,
            "color": STYLE["text_dim"],
            "connectionstyle": "arc3,rad=-0.3",
        },
    )
    ax.text(
        4.6,
        0.1,
        "git checkout",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
    )

    # Status comparison annotations
    ax.text(
        3.9,
        5.2,
        "git diff\n(unstaged)",
        color=STYLE["accent2"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
    )
    ax.text(
        8.1,
        5.2,
        "git diff --staged\n(staged)",
        color=STYLE["warn"],
        fontsize=8,
        ha="center",
        va="center",
        style="italic",
    )

    save(fig, "engine/11-git-version-control", "three_areas.png")


# ---------------------------------------------------------------------------
# engine/11-git-version-control — worktree_architecture.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# engine/11-git-version-control — worktree_architecture.png
# ---------------------------------------------------------------------------
def diagram_worktree_architecture():
    """Git worktrees sharing a single .git repository."""
    fig, ax = plt.subplots(figsize=(12, 7), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-0.5, 12.5)
    ax.set_ylim(-0.5, 7.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Title
    ax.text(
        6.0,
        7.0,
        "Git Worktrees: Shared History, Independent Work",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Central .git repo
    git_rect = FancyBboxPatch(
        (4.0, 3.5),
        4.0,
        2.0,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent4"],
        linewidth=3,
    )
    ax.add_patch(git_rect)
    ax.text(
        6.0,
        4.8,
        ".git/ (shared)",
        color=STYLE["accent4"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        6.0,
        4.1,
        "commits, branches,\nobjects, refs",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
    )

    # Worktree boxes
    wt_data = [
        (0.2, 0.2, "forge-gpu/", "main", STYLE["accent1"], "You work here"),
        (
            4.2,
            0.2,
            "forge-gpu-bloom/",
            "feature-bloom",
            STYLE["accent2"],
            "Agent works here",
        ),
        (8.2, 0.2, "forge-gpu-fog/", "feature-fog", STYLE["accent3"], "Another agent"),
    ]

    for wx, wy, label, branch, color, note in wt_data:
        wt_rect = FancyBboxPatch(
            (wx, wy),
            3.6,
            2.5,
            boxstyle="round,pad=0.12",
            facecolor=STYLE["surface"],
            edgecolor=color,
            linewidth=2,
        )
        ax.add_patch(wt_rect)
        ax.text(
            wx + 1.8,
            wy + 2.0,
            label,
            color=color,
            fontsize=11,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            wx + 1.8,
            wy + 1.3,
            f"branch: {branch}",
            color=STYLE["text"],
            fontsize=9,
            ha="center",
            va="center",
            family="monospace",
        )
        ax.text(
            wx + 1.8,
            wy + 0.7,
            "build/ (independent)",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="center",
        )
        ax.text(
            wx + 1.8,
            wy + 0.2,
            note,
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            va="center",
            style="italic",
        )

        # Connection line to .git
        ax.plot(
            [wx + 1.8, 6.0],
            [wy + 2.7, 3.5],
            color=color,
            linewidth=1.5,
            linestyle="--",
            alpha=0.6,
        )

    save(fig, "engine/11-git-version-control", "worktree_architecture.png")


# ---------------------------------------------------------------------------
# engine/11-git-version-control — submodule_vs_fetchcontent.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# engine/11-git-version-control — submodule_vs_fetchcontent.png
# ---------------------------------------------------------------------------
def diagram_submodule_vs_fetchcontent():
    """Side-by-side comparison of submodules vs FetchContent."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 6), facecolor=STYLE["bg"])

    for ax in (ax1, ax2):
        ax.set_facecolor(STYLE["bg"])
        ax.set_xlim(-0.5, 6.5)
        ax.set_ylim(-0.5, 7.0)
        ax.set_aspect("equal")
        ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # --- Left: Submodules ---
    ax1.text(
        3.0,
        6.5,
        "Git Submodules",
        color=STYLE["accent1"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Project repo box
    proj_rect = FancyBboxPatch(
        (0.3, 0.5),
        5.4,
        5.2,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
    )
    ax1.add_patch(proj_rect)
    ax1.text(
        3.0,
        5.3,
        "my-project/",
        color=STYLE["accent1"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    items_left = [
        ("src/", STYLE["text"]),
        ("CMakeLists.txt", STYLE["text"]),
        (".gitmodules", STYLE["warn"]),
        ("third_party/stb/  (commit: a1b2c3)", STYLE["accent3"]),
        ("third_party/SDL/  (commit: d4e5f6)", STYLE["accent3"]),
    ]
    for i, (txt, color) in enumerate(items_left):
        ax1.text(
            1.0,
            4.5 - i * 0.8,
            txt,
            color=color,
            fontsize=9,
            ha="left",
            va="center",
            family="monospace",
        )

    ax1.text(
        3.0,
        0.2,
        "Source visible in repo tree",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
    )

    # --- Right: FetchContent ---
    ax2.text(
        3.0,
        6.5,
        "FetchContent",
        color=STYLE["accent2"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Project repo box
    proj_rect2 = FancyBboxPatch(
        (0.3, 2.8),
        5.4,
        2.9,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
    )
    ax2.add_patch(proj_rect2)
    ax2.text(
        3.0,
        5.3,
        "my-project/",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    items_right_proj = [
        ("src/", STYLE["text"]),
        ("CMakeLists.txt  (FetchContent_Declare)", STYLE["warn"]),
    ]
    for i, (txt, color) in enumerate(items_right_proj):
        ax2.text(
            1.0,
            4.5 - i * 0.8,
            txt,
            color=color,
            fontsize=9,
            ha="left",
            va="center",
            family="monospace",
        )

    # Build dir box
    build_rect = FancyBboxPatch(
        (0.3, 0.5),
        5.4,
        1.8,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
        linestyle="--",
    )
    ax2.add_patch(build_rect)
    ax2.text(
        3.0,
        2.0,
        "build/_deps/",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="center",
        family="monospace",
    )
    ax2.text(
        3.0,
        1.3,
        "SDL3-src/    SDL3-build/",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        family="monospace",
    )
    ax2.text(
        3.0,
        0.2,
        "Downloaded at configure time",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        va="center",
        style="italic",
    )

    # Arrow from project to build
    ax2.annotate(
        "",
        xy=(3.0, 2.5),
        xytext=(3.0, 2.8),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.15",
            "color": STYLE["text_dim"],
            "lw": 1.5,
        },
    )

    fig.tight_layout()
    save(fig, "engine/11-git-version-control", "submodule_vs_fetchcontent.png")
