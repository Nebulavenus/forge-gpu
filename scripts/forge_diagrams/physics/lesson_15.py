"""Diagrams for Physics Lesson 15 — Sleep & Islands."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import FancyBboxPatch

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# physics/15-simulation-loop — simulation_pipeline.png
# ---------------------------------------------------------------------------


def diagram_simulation_pipeline():
    """Vertical flowchart of the 14 phases in forge_physics_world_step()."""
    fig = plt.figure(figsize=(8, 14), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(0, 8)
    ax.set_ylim(0, 14)
    ax.set_aspect("auto")
    ax.axis("off")

    ax.set_title(
        "forge_physics_world_step() — 14 Phases",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    # Phase definitions: (number, label, category)
    # category: "all" = orange, "skip_sleep" = cyan, "island" = green
    phases = [
        (1, "Save previous state", "all"),
        (2, "Apply gravity", "skip_sleep"),
        (3, "Integrate velocities", "skip_sleep"),
        (4, "Clear manifolds", "all"),
        (5, "Ground contacts", "skip_sleep"),
        (6, "Body-body contacts", "skip_sleep"),
        (7, "Cache prune", "all"),
        (8, "Contact solve", "all"),
        (9, "Joint solve", "all"),
        (10, "Cache store", "all"),
        (11, "Position correction + integrate", "skip_sleep"),
        (12, "Build islands", "island"),
        (13, "Evaluate sleep", "island"),
        (14, "Statistics", "all"),
    ]

    color_map = {
        "skip_sleep": STYLE["accent1"],
        "all": STYLE["accent2"],
        "island": STYLE["accent3"],
    }

    box_w = 5.6
    box_h = 0.62
    cx = 4.0  # horizontal center
    x0 = cx - box_w / 2

    # Top y for the first box (leave room for title)
    top_y = 13.2
    step_y = 0.88  # vertical distance between box tops

    box_positions = []  # (x_center, y_center) for arrow anchors

    for i, (num, label, cat) in enumerate(phases):
        color = color_map[cat]
        y_top = top_y - i * step_y
        y_center = y_top - box_h / 2

        box = FancyBboxPatch(
            (x0, y_top - box_h),
            box_w,
            box_h,
            boxstyle="round,pad=0.05",
            linewidth=1.5,
            edgecolor=color,
            facecolor=STYLE["surface"],
        )
        ax.add_patch(box)

        # Phase number badge
        badge = FancyBboxPatch(
            (x0 + 0.06, y_top - box_h + 0.07),
            0.46,
            box_h - 0.14,
            boxstyle="round,pad=0.03",
            linewidth=0,
            facecolor=color,
            alpha=0.85,
        )
        ax.add_patch(badge)

        ax.text(
            x0 + 0.29,
            y_center,
            str(num),
            color=STYLE["bg"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
        )

        ax.text(
            x0 + 0.65,
            y_center,
            label,
            color=STYLE["text"],
            fontsize=10,
            ha="left",
            va="center",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

        box_positions.append((cx, y_top - box_h / 2))

    # Draw arrows between consecutive boxes
    for i in range(len(box_positions) - 1):
        bx, by = box_positions[i]
        nx, ny = box_positions[i + 1]
        ax.annotate(
            "",
            xy=(nx, ny + box_h / 2 + 0.02),
            xytext=(bx, by - box_h / 2 - 0.02),
            arrowprops={
                "arrowstyle": "->,head_width=0.18,head_length=0.1",
                "color": STYLE["axis"],
                "lw": 1.2,
            },
        )

    # Legend
    legend_y = 0.42
    legend_items = [
        (STYLE["accent1"], "Skip sleeping bodies"),
        (STYLE["accent2"], "Runs on all bodies"),
        (STYLE["accent3"], "Island / sleep phase"),
    ]
    legend_x_start = 0.5
    for j, (col, lbl) in enumerate(legend_items):
        lx = legend_x_start + j * 2.5
        rect = FancyBboxPatch(
            (lx, legend_y - 0.18),
            0.32,
            0.32,
            boxstyle="round,pad=0.03",
            linewidth=0,
            facecolor=col,
            alpha=0.85,
        )
        ax.add_patch(rect)
        ax.text(
            lx + 0.42,
            legend_y,
            lbl,
            color=STYLE["text"],
            fontsize=9,
            va="center",
        )

    fig.tight_layout()
    save(fig, "physics/15-simulation-loop", "simulation_pipeline.png")


# ---------------------------------------------------------------------------
# physics/15-simulation-loop — island_detection.png
# ---------------------------------------------------------------------------


def diagram_island_detection():
    """Union-find island detection: bodies grouped by contact connectivity."""
    fig, (ax_before, ax_after) = plt.subplots(
        1, 2, figsize=(10, 5), facecolor=STYLE["bg"]
    )
    fig.subplots_adjust(left=0.04, right=0.96, top=0.88, bottom=0.08, wspace=0.12)

    fig.suptitle(
        "Island Detection via Union-Find",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    # Body layout: two groups of 3, plus a static body bridging them
    # Left group: bodies 0,1,2  — roughly triangle
    # Right group: bodies 3,4,5 — roughly triangle
    # Static body: body 6, sits between groups but doesn't union them
    body_pos = np.array(
        [
            [1.2, 2.5],  # 0 — left group
            [2.1, 3.2],  # 1
            [2.0, 1.8],  # 2
            [5.0, 2.5],  # 3 — right group
            [5.9, 3.2],  # 4
            [5.8, 1.8],  # 5
            [3.5, 2.5],  # 6 — static body (bridges left and right)
        ]
    )

    # Contact pairs (index pairs that are touching)
    contacts = [
        (0, 1),
        (1, 2),
        (0, 2),  # left group internal
        (3, 4),
        (4, 5),
        (3, 5),  # right group internal
        (2, 6),
        (6, 3),  # static body touches both groups
    ]

    island_colors = [STYLE["accent1"], STYLE["accent3"]]  # cyan / green
    body_island = [0, 0, 0, 1, 1, 1, -1]  # -1 = static

    radius = 0.32

    for ax, colored in [(ax_before, False), (ax_after, True)]:
        setup_axes(ax, xlim=(0, 7), ylim=(0, 5), grid=False, aspect="equal")
        ax.set_xticks([])
        ax.set_yticks([])

        # Draw contact lines
        for a, b in contacts:
            pa, pb = body_pos[a], body_pos[b]
            ax.plot(
                [pa[0], pb[0]],
                [pa[1], pb[1]],
                color=STYLE["grid"],
                linewidth=2,
                alpha=0.7,
                zorder=1,
            )

        # Draw bodies
        for idx, (px, py) in enumerate(body_pos):
            is_static = idx == 6

            if is_static:
                face = STYLE["axis"]
                edge = STYLE["text_dim"]
                hatch = "//"
            elif colored:
                isl = body_island[idx]
                face = island_colors[isl]
                edge = STYLE["text"]
                hatch = None
            else:
                face = STYLE["surface"]
                edge = STYLE["accent1"]
                hatch = None

            circle = mpatches.Circle(
                (px, py),
                radius,
                facecolor=face,
                edgecolor=edge,
                linewidth=2,
                alpha=0.9,
                zorder=2,
                hatch=hatch,
            )
            ax.add_patch(circle)

            ax.text(
                px,
                py,
                str(idx) if idx < 6 else "S",
                color=STYLE["bg"] if colored and not is_static else STYLE["text"],
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="center",
                zorder=3,
            )

        # Island labels (after panel only)
        if colored:
            ax.text(
                1.75,
                0.4,
                "Island 0",
                color=island_colors[0],
                fontsize=11,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )
            ax.text(
                5.5,
                0.4,
                "Island 1",
                color=island_colors[1],
                fontsize=11,
                fontweight="bold",
                ha="center",
                va="center",
                path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
            )
            ax.text(
                3.5,
                4.5,
                "Static body (S) contacts both\ngroups but is not unioned",
                color=STYLE["warn"],
                fontsize=9,
                ha="center",
                va="center",
                path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
            )

        title = (
            "Before: contacts detected" if not colored else "After: colored by island"
        )
        ax.set_title(title, color=STYLE["text_dim"], fontsize=11, pad=6)

    save(fig, "physics/15-simulation-loop", "island_detection.png")


# ---------------------------------------------------------------------------
# physics/15-simulation-loop — sleep_evaluation.png
# ---------------------------------------------------------------------------


def diagram_sleep_evaluation():
    """Timeline showing velocity decay and sleep timer for two bodies."""
    fig = plt.figure(figsize=(9, 5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(0, 3.0), ylim=(-0.02, 1.2), grid=True, aspect="auto")

    t = np.linspace(0, 3.0, 600)

    # Body A: settles quickly — exponential decay, crosses threshold ~0.5 s
    vel_a = 1.0 * np.exp(-5.5 * t)
    vel_a = np.maximum(vel_a, 0.0)

    # Body B: settles later — slower decay, crosses threshold ~1.5 s
    vel_b = 0.9 * np.exp(-2.0 * t)
    vel_b = np.maximum(vel_b, 0.0)

    sleep_threshold = 0.05
    sleep_delay = 0.5  # seconds both bodies must stay below threshold

    # Find when each body first drops below threshold and stays there
    def first_crossing(vel, thresh):
        below = vel < thresh
        for i in range(len(below)):
            if below[i]:
                return t[i]
        return None

    t_a = first_crossing(vel_a, sleep_threshold)
    t_b = first_crossing(vel_b, sleep_threshold)
    t_both = max(t_a, t_b)  # island timer starts when both are quiet
    t_sleep = t_both + sleep_delay  # island declared asleep

    # Velocity traces
    ax.plot(
        t,
        vel_a,
        color=STYLE["accent1"],
        linewidth=2.5,
        label="Body A velocity",
        path_effects=[pe.withStroke(linewidth=4, foreground=STYLE["bg"])],
    )
    ax.plot(
        t,
        vel_b,
        color=STYLE["accent2"],
        linewidth=2.5,
        label="Body B velocity",
        path_effects=[pe.withStroke(linewidth=4, foreground=STYLE["bg"])],
    )

    # Sleep threshold line
    ax.axhline(
        y=sleep_threshold,
        color=STYLE["warn"],
        linewidth=1.5,
        linestyle="--",
        alpha=0.85,
    )
    ax.text(
        2.85,
        sleep_threshold + 0.03,
        "sleep threshold (0.05 m/s)",
        color=STYLE["warn"],
        fontsize=9,
        ha="right",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Shaded sleep timer region
    timer_mask = (t >= t_both) & (t <= t_sleep)
    ax.fill_between(
        t,
        0,
        sleep_threshold,
        where=timer_mask,
        color=STYLE["accent4"],
        alpha=0.30,
        label="Sleep timer counting",
    )
    # Extend shading label
    ax.text(
        (t_both + t_sleep) / 2,
        sleep_threshold / 2,
        "sleep\ntimer",
        color=STYLE["accent4"],
        fontsize=9,
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
    )

    # Vertical marker lines
    for tx, col, lbl, yoff in [
        (t_a, STYLE["accent1"], "Body A settles", 0.12),
        (t_b, STYLE["accent2"], "Body B settles", 0.22),
        (t_sleep, STYLE["accent3"], "Island sleeps", 0.12),
    ]:
        ax.axvline(x=tx, color=col, linewidth=1.2, linestyle=":", alpha=0.8)
        ax.text(
            tx + 0.04,
            sleep_threshold + yoff,
            lbl,
            color=col,
            fontsize=9,
            ha="left",
            path_effects=[pe.withStroke(linewidth=2, foreground=STYLE["bg"])],
        )

    ax.set_xlabel("Time (s)", color=STYLE["axis"], fontsize=11)
    ax.set_ylabel("Velocity magnitude (m/s)", color=STYLE["axis"], fontsize=11)

    ax.set_title(
        "Sleep Evaluation: Velocity Decay and Island Sleep Timer",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    leg = ax.legend(
        loc="upper right", fontsize=10, framealpha=0.3, edgecolor=STYLE["grid"]
    )
    for text in leg.get_texts():
        text.set_color(STYLE["text"])

    fig.tight_layout()
    save(fig, "physics/15-simulation-loop", "sleep_evaluation.png")
