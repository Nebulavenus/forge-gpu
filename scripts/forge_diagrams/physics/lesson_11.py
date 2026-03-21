"""Diagrams for physics/11 — Contact Manifold."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np

from .._common import STYLE, save, setup_axes

_STROKE = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]
_LESSON = "physics/11-contact-manifold"


# ---------------------------------------------------------------------------
# physics/11-contact-manifold — clipping_pipeline.png
# ---------------------------------------------------------------------------


def diagram_clipping_pipeline():
    """Sutherland-Hodgman clipping: incident polygon clipped step by step."""
    fig, axes = plt.subplots(2, 3, figsize=(14, 9), facecolor=STYLE["bg"])
    fig.suptitle(
        "Sutherland-Hodgman Clipping Pipeline",
        color=STYLE["text"],
        fontsize=15,
        fontweight="bold",
        y=0.97,
    )
    fig.subplots_adjust(top=0.90, hspace=0.35, wspace=0.3)

    # Reference face (blue square) — fixed across all panels
    ref = np.array([[-1, -1], [1, -1], [1, 1], [-1, 1], [-1, -1]])
    # Incident polygon (orange) — offset and rotated, overlapping reference
    inc = np.array([[-0.3, -1.4], [1.5, -0.6], [1.3, 1.6], [-0.5, 1.2], [-0.3, -1.4]])

    # Side plane definitions: normal (inward), point on plane
    # Clip planes are the 4 edges of the reference face
    planes = [
        (np.array([0, -1]), 1.0, "Bottom"),  # dot(p,(0,-1)) <= 1 → y >= -1
        (np.array([-1, 0]), 1.0, "Left"),  # dot(p,(-1,0)) <= 1 → x >= -1
        (np.array([0, 1]), 1.0, "Top"),  # dot(p,(0,1))  <= 1 → y <=  1
        (np.array([1, 0]), 1.0, "Right"),  # dot(p,(1,0))  <= 1 → x <=  1
    ]

    def clip_poly(verts, normal, dist):
        """Clip polygon against half-plane: dot(p, n) <= d."""
        result = []
        n = len(verts) - 1  # last == first
        for i in range(n):
            curr = verts[i]
            nxt = verts[i + 1]
            d_curr = np.dot(curr, normal) - dist
            d_next = np.dot(nxt, normal) - dist
            if d_curr <= 0:
                result.append(curr.copy())
            if (d_curr > 0) != (d_next > 0):
                denom = d_curr - d_next
                if abs(denom) > 1e-8:
                    t = d_curr / denom
                    result.append(curr + t * (nxt - curr))
        if len(result) > 0:
            result.append(result[0].copy())
        return np.array(result) if result else np.zeros((0, 2))

    panels = [
        ("1. Original", None),
        ("2. Clip Bottom", 0),
        ("3. Clip Left", 1),
        ("4. Clip Top", 2),
        ("5. Clip Right", 3),
        ("6. Final Contacts", None),
    ]

    current_poly = inc.copy()
    polys = [inc.copy()]
    for _, plane_idx in panels[1:5]:
        n, d, _ = planes[plane_idx]
        current_poly = clip_poly(current_poly, n, d)
        polys.append(current_poly.copy())
    polys.append(polys[-1].copy())  # final is same as after last clip

    for idx, (ax, (title, plane_idx)) in enumerate(zip(axes.flat, panels, strict=True)):
        setup_axes(ax, xlim=(-2.0, 2.2), ylim=(-2.0, 2.2))
        ax.set_title(title, color=STYLE["text"], fontsize=11, fontweight="bold", pad=12)

        # Draw reference face
        ax.fill(ref[:, 0], ref[:, 1], color=STYLE["accent1"], alpha=0.15)
        ax.plot(
            ref[:, 0],
            ref[:, 1],
            color=STYLE["accent1"],
            lw=2,
            label="Reference face" if idx == 0 else None,
        )

        # Draw clip plane for this step
        if plane_idx is not None and idx < 5:
            n, d, name = planes[plane_idx]
            # Draw the clip plane as a dashed line
            perp = np.array([-n[1], n[0]])
            p0 = n * d + perp * 3
            p1 = n * d - perp * 3
            ax.plot(
                [p0[0], p1[0]],
                [p0[1], p1[1]],
                color=STYLE["warn"],
                lw=1.5,
                ls="--",
                alpha=0.7,
            )
            ax.text(
                p0[0] * 0.5,
                p0[1] * 0.5 + 0.3,
                name,
                color=STYLE["warn"],
                fontsize=8,
                ha="center",
                path_effects=_STROKE,
            )

        # Draw current polygon
        poly = polys[idx]
        if len(poly) > 0:
            color = STYLE["accent3"] if idx == 5 else STYLE["accent2"]
            ax.fill(poly[:, 0], poly[:, 1], color=color, alpha=0.2)
            ax.plot(poly[:, 0], poly[:, 1], color=color, lw=2)
            # Mark vertices
            if len(poly) > 1:
                verts = poly[:-1]  # exclude closing vertex
                ax.scatter(
                    verts[:, 0],
                    verts[:, 1],
                    color=color,
                    s=40,
                    zorder=5,
                    edgecolors=STYLE["bg"],
                    linewidths=0.5,
                )

        # Final panel: mark contact points
        if idx == 5 and len(poly) > 1:
            verts = poly[:-1]
            for i, v in enumerate(verts):
                ax.annotate(
                    f"C{i}",
                    xy=(v[0], v[1]),
                    xytext=(v[0] + 0.2, v[1] + 0.2),
                    color=STYLE["accent3"],
                    fontsize=9,
                    fontweight="bold",
                    path_effects=_STROKE,
                    arrowprops={"arrowstyle": "->", "color": STYLE["accent3"], "lw": 1},
                )

        ax.set_xticks([])
        ax.set_yticks([])

    save(fig, _LESSON, "clipping_pipeline.png")


# ---------------------------------------------------------------------------
# physics/11-contact-manifold — reference_incident_faces.png
# ---------------------------------------------------------------------------


def diagram_reference_incident_faces():
    """Reference vs incident face selection based on EPA normal alignment."""
    fig = plt.figure(figsize=(10, 7), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-3.5, 5.5), ylim=(-2.0, 4.5))

    # Box A (reference) — axis-aligned
    box_a = np.array([[-1, 0], [1, 0], [1, 2], [-1, 2], [-1, 0]])
    ax.fill(box_a[:, 0], box_a[:, 1], color=STYLE["accent1"], alpha=0.2)
    ax.plot(box_a[:, 0], box_a[:, 1], color=STYLE["accent1"], lw=2.5)
    ax.text(
        0,
        1.0,
        "Body A\n(Reference)",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
    )

    # Box B (incident) — shifted right, slightly overlapping
    box_b = np.array([[0.7, -0.5], [3.5, -0.5], [3.5, 2.5], [0.7, 2.5], [0.7, -0.5]])
    ax.fill(box_b[:, 0], box_b[:, 1], color=STYLE["accent2"], alpha=0.2)
    ax.plot(box_b[:, 0], box_b[:, 1], color=STYLE["accent2"], lw=2.5)
    ax.text(
        2.1,
        1.0,
        "Body B\n(Incident)",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
    )

    # EPA normal arrow — drawn A→B to match the selection rule below.
    # The code stores EPA normal as B→A, but the diagram uses the negated
    # direction so the "higher absolute dot product" rule is visually obvious:
    # A's +X face normal (+1,0) dotted with arrow direction (+1,0) = +1.
    ax.annotate(
        "",
        xy=(1.5, 1.0),
        xytext=(-0.5, 1.0),
        arrowprops={
            "arrowstyle": "->,head_width=0.3,head_length=0.15",
            "color": STYLE["warn"],
            "lw": 3,
        },
    )
    ax.text(
        0.5,
        1.4,
        "EPA Normal\n(separation direction)",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Highlight reference face (A's +X face — highest |dot(face_n, epa_n)|)
    ax.plot([1, 1], [0, 2], color=STYLE["accent3"], lw=5, alpha=0.8)
    ax.text(
        1.0,
        2.3,
        "Reference\nFace",
        color=STYLE["accent3"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Highlight incident face (B's -X face — most anti-aligned with reference normal)
    ax.plot([0.7, 0.7], [-0.5, 2.5], color=STYLE["accent4"], lw=5, alpha=0.8)
    ax.text(
        0.7,
        -1.0,
        "Incident\nFace",
        color=STYLE["accent4"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        path_effects=_STROKE,
    )

    # Dot product annotations — show the absolute dot product that selects
    # the reference face, matching the code's SDL_fabsf() comparison.
    ax.text(
        1.0,
        -1.5,
        "A +X normal · EPA normal → |+1| (highest)",
        color=STYLE["accent3"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
    )
    ax.text(
        3.5,
        -1.5,
        "B −X normal · EPA normal → |−1| (tied)",
        color=STYLE["accent4"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
    )

    # Selection rule
    ax.text(
        2.0,
        3.8,
        "Each body's touching face is found, then compared.\n"
        "The face whose normal has the highest |dot| with the\n"
        "EPA normal becomes the reference (ties use A);\n"
        "the other is incident.",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        va="top",
        path_effects=_STROKE,
        bbox={
            "boxstyle": "round,pad=0.5",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["grid"],
            "alpha": 0.9,
        },
    )

    ax.set_title(
        "Reference vs. Incident Face Selection",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.set_xticks([])
    ax.set_yticks([])

    fig.tight_layout()
    save(fig, _LESSON, "reference_incident_faces.png")


# ---------------------------------------------------------------------------
# physics/11-contact-manifold — contact_reduction.png
# ---------------------------------------------------------------------------


def diagram_contact_reduction():
    """Contact point reduction: selecting 4 points that maximize area."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 6), facecolor=STYLE["bg"])
    fig.suptitle(
        "Contact Point Reduction (6 → 4)",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.subplots_adjust(top=0.88, wspace=0.3)

    # 6 candidate contact points with depths
    points = np.array(
        [
            [0.0, 0.0],
            [2.0, 0.2],
            [3.5, 0.0],
            [0.3, 2.5],
            [1.8, 2.8],
            [3.2, 2.3],
        ]
    )
    depths = np.array([0.08, 0.15, 0.10, 0.06, 0.03, 0.12])

    # Simulate reduction algorithm
    # Step 1: deepest (index 1, depth 0.15)
    i0 = 1
    # Step 2: farthest from deepest (index 3)
    i1 = 3
    # Step 3: max triangle area (index 5)
    i2 = 5
    # Step 4: max quad area (index 0)
    i3 = 0
    selected = [i0, i1, i2, i3]
    rejected = [i for i in range(6) if i not in selected]

    labels = ["P0", "P1\n(deepest)", "P2", "P3", "P4", "P5"]

    # --- Left panel: all 6 candidates ---
    setup_axes(ax1, xlim=(-0.8, 4.3), ylim=(-0.8, 3.6))
    ax1.set_title(
        "All Candidates", color=STYLE["text"], fontsize=12, fontweight="bold", pad=12
    )

    # Draw convex hull of all points
    hull_idx = [0, 2, 5, 4, 3, 0]
    hull = mpatches.Polygon(
        points[hull_idx],
        closed=True,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        lw=1,
        alpha=0.5,
    )
    ax1.add_patch(hull)

    for i, (pt, d) in enumerate(zip(points, depths, strict=True)):
        size = 60 + d * 800  # size proportional to depth
        ax1.scatter(
            pt[0],
            pt[1],
            s=size,
            color=STYLE["accent1"],
            alpha=0.7,
            edgecolors=STYLE["bg"],
            linewidths=0.5,
            zorder=5,
        )
        ax1.text(
            pt[0] + 0.15,
            pt[1] + 0.25,
            f"{labels[i]}\nd={d:.2f}",
            color=STYLE["text"],
            fontsize=8,
            path_effects=_STROKE,
        )

    ax1.set_xticks([])
    ax1.set_yticks([])

    # --- Right panel: selected 4 ---
    setup_axes(ax2, xlim=(-0.8, 4.3), ylim=(-0.8, 3.6))
    ax2.set_title(
        "Selected 4 (Maximum Area)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=12,
    )

    # Draw the quadrilateral formed by selected points
    sel_pts = points[selected]
    # Order for drawing: i0, i3, i1, i2 makes a proper quad
    quad_order = [0, 3, 1, 2, 0]  # indices into sel_pts
    quad = mpatches.Polygon(
        sel_pts[quad_order],
        closed=True,
        facecolor=STYLE["accent3"],
        edgecolor=STYLE["accent3"],
        lw=2,
        alpha=0.15,
    )
    ax2.add_patch(quad)
    ax2.plot(
        sel_pts[quad_order, 0],
        sel_pts[quad_order, 1],
        color=STYLE["accent3"],
        lw=2,
        alpha=0.6,
    )

    # Draw rejected points dimly
    for i in rejected:
        ax2.scatter(
            points[i, 0],
            points[i, 1],
            s=40,
            color=STYLE["text_dim"],
            alpha=0.3,
            marker="x",
            linewidths=2,
            zorder=5,
        )

    # Draw selected points brightly with step labels
    step_labels = ["1. Deepest", "2. Farthest", "3. Max △", "4. Max ◻"]
    step_colors = [
        STYLE["accent2"],
        STYLE["accent1"],
        STYLE["accent3"],
        STYLE["accent4"],
    ]
    for _j, (si, sl, sc) in enumerate(
        zip(selected, step_labels, step_colors, strict=True)
    ):
        pt = points[si]
        ax2.scatter(
            pt[0],
            pt[1],
            s=100,
            color=sc,
            edgecolors=STYLE["bg"],
            linewidths=1,
            zorder=6,
        )
        offset_y = 0.3 if si != 4 else -0.4
        ax2.text(
            pt[0] + 0.15,
            pt[1] + offset_y,
            sl,
            color=sc,
            fontsize=9,
            fontweight="bold",
            path_effects=_STROKE,
        )

    ax2.set_xticks([])
    ax2.set_yticks([])

    save(fig, _LESSON, "contact_reduction.png")


# ---------------------------------------------------------------------------
# physics/11-contact-manifold — manifold_cache.png
# ---------------------------------------------------------------------------


def diagram_manifold_cache():
    """Manifold cache: persistent contact IDs enable warm-starting."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 6), facecolor=STYLE["bg"])
    fig.suptitle(
        "Manifold Cache — Persistent Contact IDs & Warm-Starting",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )
    fig.subplots_adjust(top=0.88, wspace=0.3)

    def draw_box(ax, cx, cy, w, h, color, label):
        rect = mpatches.FancyBboxPatch(
            (cx - w / 2, cy - h / 2),
            w,
            h,
            boxstyle="round,pad=0.05",
            facecolor=color,
            edgecolor=color,
            alpha=0.2,
            lw=2,
        )
        ax.add_patch(rect)
        rect2 = mpatches.FancyBboxPatch(
            (cx - w / 2, cy - h / 2),
            w,
            h,
            boxstyle="round,pad=0.05",
            facecolor="none",
            edgecolor=color,
            lw=2,
        )
        ax.add_patch(rect2)
        ax.text(
            cx,
            cy,
            label,
            color=color,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=_STROKE,
        )

    # --- Frame N (left panel) ---
    setup_axes(ax1, xlim=(-2, 4), ylim=(-1.5, 4), grid=False)
    ax1.set_title(
        "Frame N", color=STYLE["text"], fontsize=12, fontweight="bold", pad=12
    )

    # Two boxes
    draw_box(ax1, 0.5, 2.0, 2.0, 1.5, STYLE["accent1"], "A")
    draw_box(ax1, 2.0, 1.0, 2.0, 1.5, STYLE["accent2"], "B")

    # Contact points with IDs
    contacts_n = [
        (1.2, 1.6, "id=42"),
        (1.5, 1.2, "id=43"),
        (1.0, 1.0, "id=44"),
        (1.6, 1.5, "id=45"),
    ]
    for x, y, label in contacts_n:
        ax1.scatter(
            x,
            y,
            s=60,
            color=STYLE["accent3"],
            zorder=5,
            edgecolors=STYLE["bg"],
            linewidths=0.5,
        )
        ax1.text(
            x + 0.15,
            y + 0.15,
            label,
            color=STYLE["accent3"],
            fontsize=8,
            path_effects=_STROKE,
        )

    # Impulse annotations
    ax1.text(
        0.5,
        -0.5,
        "Accumulated impulses:\nJ₄₂=5.2  J₄₃=3.1\nJ₄₄=4.8  J₄₅=2.9",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
        bbox={
            "boxstyle": "round,pad=0.3",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["grid"],
            "alpha": 0.9,
        },
    )

    ax1.set_xticks([])
    ax1.set_yticks([])

    # --- Frame N+1 (right panel) ---
    setup_axes(ax2, xlim=(-2, 4), ylim=(-1.5, 4), grid=False)
    ax2.set_title(
        "Frame N+1", color=STYLE["text"], fontsize=12, fontweight="bold", pad=12
    )

    # Same boxes, slightly moved
    draw_box(ax2, 0.6, 2.05, 2.0, 1.5, STYLE["accent1"], "A")
    draw_box(ax2, 1.95, 0.95, 2.0, 1.5, STYLE["accent2"], "B")

    # Contact points — same IDs, slightly shifted positions
    contacts_n1 = [
        (1.25, 1.55, "id=42", True),
        (1.55, 1.15, "id=43", True),
        (1.05, 0.95, "id=44", True),
        (1.65, 1.45, "id=45", True),
    ]
    for x, y, label, matched in contacts_n1:
        color = STYLE["accent3"] if matched else STYLE["text_dim"]
        ax2.scatter(
            x, y, s=60, color=color, zorder=5, edgecolors=STYLE["bg"], linewidths=0.5
        )
        ax2.text(
            x + 0.15, y + 0.15, label, color=color, fontsize=8, path_effects=_STROKE
        )

    # Warm-start annotation
    ax2.text(
        0.5,
        -0.5,
        "Warm-started impulses (×0.85):\nJ₄₂=4.4  J₄₃=2.6\nJ₄₄=4.1  J₄₅=2.5",
        color=STYLE["accent3"],
        fontsize=9,
        ha="center",
        path_effects=_STROKE,
        bbox={
            "boxstyle": "round,pad=0.3",
            "facecolor": STYLE["surface"],
            "edgecolor": STYLE["grid"],
            "alpha": 0.9,
        },
    )

    # Arrow between panels
    fig.text(
        0.50,
        0.50,
        "→ ID Match →",
        color=STYLE["warn"],
        fontsize=13,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=_STROKE,
    )

    ax2.set_xticks([])
    ax2.set_yticks([])

    save(fig, _LESSON, "manifold_cache.png")
