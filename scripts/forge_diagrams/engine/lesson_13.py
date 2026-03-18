"""Diagrams for engine/13-stretchy-containers."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, Rectangle

from .._common import STYLE, save


# ---------------------------------------------------------------------------
# engine/13-stretchy-containers — fat_pointer_layout.png
# ---------------------------------------------------------------------------
def diagram_fat_pointer_layout():
    """Show hidden header + element data with user pointer annotated."""
    fig, ax = plt.subplots(figsize=(12, 5), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1.5, 16.5)
    ax.set_ylim(-1.5, 4.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        7.5,
        4.3,
        "Fat Pointer Layout: Hidden Header Before User Data",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- Memory block: header section ---
    header_x = 0.0
    header_w = 6.0
    data_x = header_w
    data_w = 9.0
    block_y = 1.5
    block_h = 1.4

    # Header background
    r_hdr = FancyBboxPatch(
        (header_x, block_y),
        header_w,
        block_h,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["accent4"],
        alpha=0.18,
        edgecolor=STYLE["accent4"],
        linewidth=2.0,
    )
    ax.add_patch(r_hdr)
    ax.text(
        header_x + header_w / 2,
        block_y + block_h + 0.15,
        "Header",
        color=STYLE["accent4"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # Header fields
    field_names = ["length", "capacity", "hash_table", "temp"]
    field_colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"], STYLE["warn"]]
    fw = header_w / len(field_names)
    for i, (name, color) in enumerate(zip(field_names, field_colors, strict=False)):
        fx = header_x + i * fw
        r_field = Rectangle(
            (fx + 0.06, block_y + 0.12),
            fw - 0.12,
            block_h - 0.24,
            facecolor=color,
            alpha=0.22,
            edgecolor=color,
            linewidth=1.2,
        )
        ax.add_patch(r_field)
        ax.text(
            fx + fw / 2,
            block_y + block_h / 2,
            name,
            color=color,
            fontsize=8.5,
            fontweight="bold",
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke,
        )

    # Element data background
    r_data = FancyBboxPatch(
        (data_x, block_y),
        data_w,
        block_h,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["accent1"],
        alpha=0.13,
        edgecolor=STYLE["accent1"],
        linewidth=2.0,
    )
    ax.add_patch(r_data)
    ax.text(
        data_x + data_w / 2,
        block_y + block_h + 0.15,
        "Element Data",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # Element slots [0], [1], [2], ...
    slot_labels = ["[0]", "[1]", "[2]", "[3]", "…"]
    sw = data_w / len(slot_labels)
    for i, label in enumerate(slot_labels):
        sx = data_x + i * sw
        r_slot = Rectangle(
            (sx + 0.06, block_y + 0.12),
            sw - 0.12,
            block_h - 0.24,
            facecolor=STYLE["accent1"],
            alpha=0.18,
            edgecolor=STYLE["accent1"],
            linewidth=0.9,
        )
        ax.add_patch(r_slot)
        ax.text(
            sx + sw / 2,
            block_y + block_h / 2,
            label,
            color=STYLE["accent1"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke,
        )

    # User pointer arrow pointing to start of element data
    ptr_x = data_x
    ax.annotate(
        "",
        xy=(ptr_x, block_y - 0.05),
        xytext=(ptr_x, block_y - 0.85),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.15",
            "color": STYLE["warn"],
            "lw": 2.2,
        },
    )
    ax.text(
        ptr_x,
        block_y - 1.05,
        "user pointer",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Bottom note
    ax.text(
        7.5,
        -1.3,
        "sizeof(header) bytes are hidden before the user pointer",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        va="top",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/13-stretchy-containers", "fat_pointer_layout.png")


# ---------------------------------------------------------------------------
# engine/13-stretchy-containers — growth_policy.png
# ---------------------------------------------------------------------------
def diagram_growth_policy():
    """Growth sequence: NULL → cap 4 → 8 → 16 → 32 with doubling realloc."""
    fig, ax = plt.subplots(figsize=(11, 8), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-3.5, 13.5)
    ax.set_ylim(-0.5, 11.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        5.0,
        11.3,
        "Growth Policy: Capacity Doubling on Append",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Each state: (label, capacity, filled_count, color)
    states = [
        ("NULL", 0, 0, STYLE["text_dim"]),
        ("cap = 4", 4, 1, STYLE["accent1"]),
        ("cap = 8", 8, 5, STYLE["accent2"]),
        ("cap = 16", 16, 9, STYLE["accent3"]),
        ("cap = 32", 32, 17, STYLE["accent4"]),
    ]

    row_h = 1.6
    slot_w = 0.38
    slot_h = 0.9
    start_y = 9.5

    for row_idx, (label, cap, filled, color) in enumerate(states):
        y = start_y - row_idx * row_h

        # Row label on left
        ax.text(
            -3.2,
            y + slot_h / 2,
            label,
            color=color if color != STYLE["text_dim"] else STYLE["text_dim"],
            fontsize=10,
            fontweight="bold",
            ha="right",
            va="center",
            family="monospace",
            path_effects=stroke,
        )

        if cap == 0:
            # NULL — show empty marker
            r_null = FancyBboxPatch(
                (0.0, y),
                2.0,
                slot_h,
                boxstyle="round,pad=0.08",
                facecolor=STYLE["surface"],
                alpha=0.25,
                edgecolor=STYLE["grid"],
                linewidth=1.2,
                linestyle="--",
            )
            ax.add_patch(r_null)
            ax.text(
                1.0,
                y + slot_h / 2,
                "(not yet allocated)",
                color=STYLE["text_dim"],
                fontsize=9,
                ha="center",
                va="center",
                style="italic",
            )
        else:
            # Scale slot width so 32 slots still fit in ~12 units
            sw = min(slot_w, 11.5 / cap)
            for s in range(cap):
                sx = s * sw
                is_filled = s < filled
                fc = color if is_filled else STYLE["surface"]
                alpha = 0.28 if is_filled else 0.15
                ls = "-" if is_filled else "--"
                ec = color if is_filled else STYLE["grid"]
                r_slot = Rectangle(
                    (sx, y),
                    sw - 0.04,
                    slot_h,
                    facecolor=fc,
                    alpha=alpha,
                    edgecolor=ec,
                    linewidth=0.8,
                    linestyle=ls,
                )
                ax.add_patch(r_slot)

            # Capacity annotation on the right
            total_w = cap * sw
            ax.text(
                total_w + 0.25,
                y + slot_h / 2,
                f"cap={cap}",
                color=color,
                fontsize=8,
                fontweight="bold",
                ha="left",
                va="center",
                family="monospace",
                path_effects=stroke,
            )

        # Arrow between rows
        if row_idx < len(states) - 1:
            arrow_x = -1.6
            ax.annotate(
                "",
                xy=(arrow_x, y - 0.12),
                xytext=(arrow_x, y - row_h + 0.62 + slot_h),
                arrowprops={
                    "arrowstyle": "->,head_width=0.22,head_length=0.13",
                    "color": STYLE["warn"],
                    "lw": 1.6,
                },
            )
            ax.text(
                arrow_x - 0.25,
                y - row_h / 2 + slot_h / 2,
                "append triggers\nrealloc",
                color=STYLE["warn"],
                fontsize=7.5,
                fontweight="bold",
                ha="right",
                va="center",
                path_effects=stroke,
            )

    # Key insight at bottom
    ax.text(
        5.0,
        -0.3,
        "Each realloc doubles capacity \u2014 O(log n) total reallocations for n appends",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        va="top",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/13-stretchy-containers", "growth_policy.png")


# ---------------------------------------------------------------------------
# engine/13-stretchy-containers — hash_map_dual_structure.png
# ---------------------------------------------------------------------------
def diagram_hash_map_dual_structure():
    """Data array + hash index with cross-reference arrows."""
    fig, ax = plt.subplots(figsize=(13, 7), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1.5, 16.5)
    ax.set_ylim(-1.5, 8.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        7.5,
        8.3,
        "Hash Map: Parallel Data Array and Hash Index",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # --- Data Array (top block) ---
    data_y = 5.0
    data_h = 1.5
    data_entries = ["(default)", "entry 0", "entry 1", "entry 2"]
    entry_colors = [
        STYLE["text_dim"],
        STYLE["accent1"],
        STYLE["accent2"],
        STYLE["accent3"],
    ]
    dw = 3.0  # per-slot width

    ax.text(
        len(data_entries) * dw / 2,
        data_y + data_h + 0.2,
        "Data Array",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    data_slot_centers = []
    for i, (entry, color) in enumerate(zip(data_entries, entry_colors, strict=False)):
        dx = i * dw
        r_entry = FancyBboxPatch(
            (dx, data_y),
            dw - 0.1,
            data_h,
            boxstyle="round,pad=0.07",
            facecolor=color,
            alpha=0.18,
            edgecolor=color,
            linewidth=1.5,
        )
        ax.add_patch(r_entry)
        ax.text(
            dx + dw / 2 - 0.05,
            data_y + data_h / 2,
            f"[{i}]\n{entry}",
            color=color,
            fontsize=8.5,
            fontweight="bold",
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke,
        )
        data_slot_centers.append(dx + dw / 2 - 0.05)

    # --- Hash Index (bottom block) ---
    idx_y = 1.5
    idx_h = 2.0
    bucket_count = 4
    bw = 3.0  # per-bucket width

    ax.text(
        bucket_count * bw / 2,
        idx_y + idx_h + 0.2,
        "Hash Index",
        color=STYLE["text"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="bottom",
        path_effects=stroke,
    )

    # Buckets: each shows a hash and an index pointing into data array
    # bucket_data: (hash_val, data_index or None)
    bucket_data = [
        ("hash=0x2A", 2),  # bucket 0 → entry 2
        ("hash=0x11", 0),  # bucket 1 → entry 0
        ("hash=0x5C", 1),  # bucket 2 → entry 1
        ("hash=0x3F", 3),  # bucket 3 → entry 3 (not shown in data array)
    ]
    bucket_colors = [
        STYLE["accent3"],
        STYLE["text_dim"],
        STYLE["accent1"],
        STYLE["accent2"],
    ]

    idx_centers = []
    for i, ((hash_txt, didx), bcolor) in enumerate(
        zip(bucket_data, bucket_colors, strict=False)
    ):
        bx = i * bw
        r_bucket = FancyBboxPatch(
            (bx, idx_y),
            bw - 0.1,
            idx_h,
            boxstyle="round,pad=0.07",
            facecolor=bcolor,
            alpha=0.15,
            edgecolor=bcolor,
            linewidth=1.5,
        )
        ax.add_patch(r_bucket)
        ax.text(
            bx + bw / 2 - 0.05,
            idx_y + idx_h - 0.35,
            hash_txt,
            color=bcolor,
            fontsize=8,
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke,
        )
        ax.text(
            bx + bw / 2 - 0.05,
            idx_y + 0.45,
            f"index \u2192 {didx}",
            color=bcolor,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            family="monospace",
            path_effects=stroke,
        )
        idx_centers.append(bx + bw / 2 - 0.05)

    # Arrows from bucket index values to data array entries
    arrow_pairs = [
        (0, 2),  # bucket 0 → data[2]
        (1, 0),  # bucket 1 → data[0]
        (2, 1),  # bucket 2 → data[1]
        (3, 3),  # bucket 3 → data[3]
    ]
    arrow_colors = [
        STYLE["accent3"],
        STYLE["text_dim"],
        STYLE["accent1"],
        STYLE["accent2"],
    ]
    for (bi, di), ac in zip(arrow_pairs, arrow_colors, strict=False):
        ax.annotate(
            "",
            xy=(data_slot_centers[di], data_y - 0.08),
            xytext=(idx_centers[bi], idx_y + idx_h + 0.08),
            arrowprops={
                "arrowstyle": "->,head_width=0.18,head_length=0.12",
                "color": ac,
                "lw": 1.4,
                "connectionstyle": f"arc3,rad={0.0 + 0.15 * (bi - 1.5)}",
            },
        )

    # "hash_table pointer" label: from data array header area to hash index
    ax.annotate(
        "",
        xy=(bucket_count * bw / 2, idx_y + idx_h + 0.08),
        xytext=(bucket_count * bw / 2, data_y - 0.08),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["warn"],
            "lw": 1.8,
            "linestyle": "dashed",
            "connectionstyle": "arc3,rad=0.35",
        },
    )
    ax.text(
        bucket_count * bw / 2 + 2.2,
        (data_y + idx_y + idx_h) / 2,
        "hash_table pointer\n(in header)",
        color=STYLE["warn"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # Key insight at bottom
    ax.text(
        7.5,
        -1.2,
        "The data array holds items in insertion order; the hash index maps keys to data indices",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        va="top",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/13-stretchy-containers", "hash_map_dual_structure.png")


# ---------------------------------------------------------------------------
# engine/13-stretchy-containers — bucket_probing.png
# ---------------------------------------------------------------------------
def diagram_bucket_probing():
    """Quadratic probing across buckets with slot states and probe path."""
    fig, ax = plt.subplots(figsize=(14, 6), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1.5, 16.5)
    ax.set_ylim(-1.5, 6.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        7.5,
        6.3,
        "Quadratic Probing: Resolving Hash Collisions",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # 4 buckets, each with 8 slots
    num_buckets = 4
    slots_per_bucket = 8
    bw = 3.8  # bucket width
    bh = 1.3  # bucket height
    sw = bw / slots_per_bucket
    bucket_y = 3.0
    bucket_gap = 0.25

    # Slot state codes: 'O' = occupied, 'T' = tombstone, 'E' = empty
    # We're illustrating insertion of a key that probes bucket 0, then bucket 1
    slot_states = [
        # bucket 0: all occupied
        ["O", "O", "O", "O", "O", "O", "O", "O"],
        # bucket 1: mostly occupied, one tombstone, then one empty at index 5
        ["O", "O", "T", "O", "O", "E", "O", "O"],
        # bucket 2: mix
        ["O", "E", "E", "O", "E", "O", "O", "E"],
        # bucket 3: mostly empty
        ["E", "E", "O", "E", "E", "E", "E", "O"],
    ]

    bucket_x_starts = []
    for b in range(num_buckets):
        bx = b * (bw + bucket_gap)
        bucket_x_starts.append(bx)

        # Bucket outline
        r_bg = FancyBboxPatch(
            (bx - 0.05, bucket_y - 0.05),
            bw + 0.1,
            bh + 0.1,
            boxstyle="round,pad=0.05",
            facecolor=STYLE["surface"],
            alpha=0.35,
            edgecolor=STYLE["grid"],
            linewidth=1.5,
        )
        ax.add_patch(r_bg)
        ax.text(
            bx + bw / 2,
            bucket_y + bh + 0.15,
            f"Bucket {b}",
            color=STYLE["text_dim"],
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="bottom",
            path_effects=stroke,
        )

        for s, state in enumerate(slot_states[b]):
            sx = bx + s * sw
            if state == "O":
                fc = STYLE["accent1"]
                alpha = 0.35
                ec = STYLE["accent1"]
                lbl = ""
            elif state == "T":
                fc = STYLE["warn"]
                alpha = 0.25
                ec = STYLE["warn"]
                lbl = "\u2020"  # dagger = tombstone
            else:  # E = empty
                fc = STYLE["surface"]
                alpha = 0.1
                ec = STYLE["grid"]
                lbl = ""

            r_slot = Rectangle(
                (sx + 0.03, bucket_y + 0.1),
                sw - 0.06,
                bh - 0.2,
                facecolor=fc,
                alpha=alpha,
                edgecolor=ec,
                linewidth=0.9,
                linestyle="--" if state == "E" else "-",
            )
            ax.add_patch(r_slot)
            if lbl:
                ax.text(
                    sx + sw / 2,
                    bucket_y + bh / 2,
                    lbl,
                    color=STYLE["warn"],
                    fontsize=11,
                    fontweight="bold",
                    ha="center",
                    va="center",
                    path_effects=stroke,
                )

    # Probe path annotation
    # Step 0: Start at bucket 0 (all occupied → skip)
    probe_y = bucket_y - 0.55
    ax.annotate(
        "",
        xy=(bucket_x_starts[0] + bw / 2, bucket_y - 0.08),
        xytext=(bucket_x_starts[0] + bw / 2, probe_y + 0.12),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.13",
            "color": STYLE["warn"],
            "lw": 2.0,
        },
    )
    ax.text(
        bucket_x_starts[0] + bw / 2,
        probe_y - 0.08,
        "probe 0\n(full \u2014 skip)",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Arrow from bucket 0 to bucket 1
    ax.annotate(
        "",
        xy=(bucket_x_starts[1] + 0.15, probe_y + 0.07),
        xytext=(bucket_x_starts[0] + bw - 0.15, probe_y + 0.07),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.13",
            "color": STYLE["warn"],
            "lw": 1.8,
            "connectionstyle": "arc3,rad=-0.3",
        },
    )
    ax.text(
        (bucket_x_starts[0] + bw + bucket_x_starts[1]) / 2,
        probe_y - 0.45,
        "step +1",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Step 1: Land in bucket 1, find empty slot at position 5
    probe1_y = bucket_y - 0.55
    ax.annotate(
        "",
        xy=(bucket_x_starts[1] + bw / 2, bucket_y - 0.08),
        xytext=(bucket_x_starts[1] + bw / 2, probe1_y + 0.12),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.13",
            "color": STYLE["accent3"],
            "lw": 2.0,
        },
    )
    # Highlight the empty slot in bucket 1 slot 5
    empty_sx = bucket_x_starts[1] + 5 * sw
    r_target = Rectangle(
        (empty_sx + 0.03, bucket_y + 0.1),
        sw - 0.06,
        bh - 0.2,
        facecolor=STYLE["accent3"],
        alpha=0.45,
        edgecolor=STYLE["accent3"],
        linewidth=2.0,
    )
    ax.add_patch(r_target)
    ax.text(
        empty_sx + sw / 2,
        bucket_y + bh / 2,
        "\u2713",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )
    ax.text(
        bucket_x_starts[1] + bw / 2,
        probe1_y - 0.08,
        "probe 1\n(insert here)",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Legend
    legend_items = [
        (STYLE["accent1"], "-", "Occupied"),
        (STYLE["warn"], "-", "Tombstone (deleted)"),
        (STYLE["grid"], "--", "Empty"),
        (STYLE["accent3"], "-", "Insert target"),
    ]
    lx = 1.0
    legend_y = -0.8
    for lc, ls, ldesc in legend_items:
        r_leg = Rectangle(
            (lx, legend_y),
            0.6,
            0.5,
            facecolor=lc,
            alpha=0.3,
            edgecolor=lc,
            linewidth=1,
            linestyle=ls,
        )
        ax.add_patch(r_leg)
        ax.text(
            lx + 0.8,
            legend_y + 0.25,
            ldesc,
            color=STYLE["text_dim"],
            fontsize=8.5,
            ha="left",
            va="center",
        )
        lx += 3.5

    fig.tight_layout()
    save(fig, "engine/13-stretchy-containers", "bucket_probing.png")


# ---------------------------------------------------------------------------
# engine/13-stretchy-containers — string_map_arena.png
# ---------------------------------------------------------------------------
def diagram_string_map_arena():
    """Arena block chain with strings packed high-to-low."""
    fig, ax = plt.subplots(figsize=(12, 6), facecolor=STYLE["bg"])
    ax.set_facecolor(STYLE["bg"])
    ax.set_xlim(-1.5, 16.5)
    ax.set_ylim(-1.5, 7.5)
    ax.set_aspect("equal")
    ax.axis("off")

    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        7.5,
        7.3,
        "String Map Arena: Strings Packed High-to-Low",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        va="top",
        path_effects=stroke,
    )

    # Block geometry
    block_w = 6.0
    block_h = 2.0
    block_ys = [4.5, 1.5]
    block_xs = [0.5, 0.5]

    block_colors = [STYLE["accent1"], STYLE["accent2"]]

    # String data for each block: list of (label, relative_width) from right to left
    block_strings = [
        [('"hello"', 2.0, STYLE["accent1"]), ('"world"', 2.0, STYLE["accent2"])],
        [('"foo"', 1.4, STYLE["accent3"])],
    ]

    for bi, (bx, by, bcolor, strings) in enumerate(
        zip(block_xs, block_ys, block_colors, block_strings, strict=False)
    ):
        # Block outline
        r_block = FancyBboxPatch(
            (bx, by),
            block_w,
            block_h,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["surface"],
            alpha=0.3,
            edgecolor=bcolor,
            linewidth=1.8,
        )
        ax.add_patch(r_block)
        ax.text(
            bx + block_w / 2,
            by + block_h + 0.15,
            f"Arena Block {bi + 1}",
            color=bcolor,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="bottom",
            path_effects=stroke,
        )

        # Low address label
        ax.text(
            bx + 0.1,
            by - 0.15,
            "low",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="left",
            va="top",
        )
        # High address label
        ax.text(
            bx + block_w - 0.1,
            by - 0.15,
            "high",
            color=STYLE["text_dim"],
            fontsize=7,
            ha="right",
            va="top",
        )

        # Pack strings from the right (high address) going left
        right_cursor = bx + block_w - 0.12
        for stext, sw, sc in strings:
            sx = right_cursor - sw
            r_str = Rectangle(
                (sx, by + 0.2),
                sw - 0.06,
                block_h - 0.4,
                facecolor=sc,
                alpha=0.28,
                edgecolor=sc,
                linewidth=1.3,
            )
            ax.add_patch(r_str)
            ax.text(
                sx + (sw - 0.06) / 2,
                by + block_h / 2,
                stext,
                color=sc,
                fontsize=9,
                fontweight="bold",
                ha="center",
                va="center",
                family="monospace",
                path_effects=stroke,
            )
            right_cursor = sx

        # Free space (left portion)
        free_w = right_cursor - bx - 0.12
        if free_w > 0.3:
            r_free = Rectangle(
                (bx + 0.12, by + 0.2),
                free_w,
                block_h - 0.4,
                facecolor=STYLE["surface"],
                alpha=0.12,
                edgecolor=STYLE["grid"],
                linewidth=0.8,
                linestyle="--",
            )
            ax.add_patch(r_free)
            ax.text(
                bx + 0.12 + free_w / 2,
                by + block_h / 2,
                "free",
                color=STYLE["text_dim"],
                fontsize=8,
                ha="center",
                va="center",
                style="italic",
            )

        # Growth direction arrow (pointing right → left within block)
        if free_w > 0.8:
            ax.annotate(
                "",
                xy=(right_cursor + 0.05, by + block_h / 2),
                xytext=(right_cursor + 0.8, by + block_h / 2),
                arrowprops={
                    "arrowstyle": "->,head_width=0.18,head_length=0.12",
                    "color": STYLE["warn"],
                    "lw": 1.5,
                },
            )
            ax.text(
                right_cursor + 0.85,
                by + block_h / 2 + 0.25,
                "growth\ndirection",
                color=STYLE["warn"],
                fontsize=7.5,
                fontweight="bold",
                ha="left",
                va="bottom",
                path_effects=stroke,
            )

    # "next" pointer linking block 1 to block 2
    b1x, b1y = block_xs[0], block_ys[0]
    b2x, b2y = block_xs[1], block_ys[1]
    ax.annotate(
        "",
        xy=(b2x + block_w * 0.3, b2y + block_h + 0.08),
        xytext=(b1x + block_w * 0.3, b1y - 0.08),
        arrowprops={
            "arrowstyle": "->,head_width=0.22,head_length=0.14",
            "color": STYLE["accent4"],
            "lw": 1.8,
            "connectionstyle": "arc3,rad=0.25",
        },
    )
    ax.text(
        b1x - 0.8,
        (b1y + b2y + block_h) / 2,
        "next",
        color=STYLE["accent4"],
        fontsize=10,
        fontweight="bold",
        ha="right",
        va="center",
        family="monospace",
        path_effects=stroke,
    )

    # Second block's "next = NULL" label
    ax.text(
        b2x + block_w * 0.3 + 0.5,
        b2y - 0.2,
        "next = NULL",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="left",
        va="top",
        family="monospace",
    )

    # Right-side annotation explaining packing direction
    ax.text(
        block_xs[0] + block_w + 0.4,
        block_ys[0] + block_h / 2,
        "\u2190 strings grow\ntoward low address",
        color=STYLE["text_dim"],
        fontsize=8.5,
        ha="left",
        va="center",
    )

    # Key insight at bottom
    ax.text(
        7.5,
        -1.2,
        "String bytes are packed from the high end of each block; only pointers into the block are stored in map entries",
        color=STYLE["text"],
        fontsize=9.5,
        ha="center",
        va="top",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, "engine/13-stretchy-containers", "string_map_arena.png")
