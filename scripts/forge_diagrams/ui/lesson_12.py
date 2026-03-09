"""Diagrams for ui/12."""

import matplotlib.patches as mpatches
import matplotlib.patheffects as pe
import matplotlib.pyplot as plt

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# UI Lesson 12 — Font Scaling and Spacing
# ---------------------------------------------------------------------------

LESSON_12_PATH = "ui/12-font-scaling-and-spacing"


def diagram_scale_factor_effect():
    """Show the same button rendered at scale 0.75, 1.0, 1.5, and 2.0 with
    proportional growth of rect dimensions, font size, and padding."""

    fig, ax = plt.subplots(figsize=(13, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 16.5), ylim=(-1.5, 7), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        8.0,
        6.3,
        "Scale Factor Effect on Widget Dimensions",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Base dimensions at scale 1.0
    base_w, base_h = 160.0, 40.0
    base_font = 14.0
    base_pad = 8.0
    scales = [0.75, 1.0, 1.5, 2.0]
    colors = [STYLE["accent4"], STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]

    # Compute vis_scale dynamically so all four buttons fit within the axes
    x_start = 0.3
    x_end = 16.0  # leave 0.5 margin from xlim=16.5
    gap = 0.8
    total_pixel_w = sum(base_w * s for s in scales)
    gap_total = (len(scales) - 1) * gap
    vis_scale = (x_end - x_start - gap_total) / total_pixel_w
    x_cursor = x_start

    for _i, (s, col) in enumerate(zip(scales, colors, strict=True)):
        w = base_w * s * vis_scale
        h = base_h * s * vis_scale
        font_px = base_font * s
        pad_px = base_pad * s
        dim_w = int(base_w * s)
        dim_h = int(base_h * s)

        # Center vertically around y=3
        y_bot = 3.0 - h / 2

        # Button rectangle
        btn = mpatches.FancyBboxPatch(
            (x_cursor, y_bot),
            w,
            h,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["surface"],
            edgecolor=col,
            linewidth=2,
        )
        ax.add_patch(btn)

        # Button label — scale proportionally, floor at 8pt
        ax.text(
            x_cursor + w / 2,
            y_bot + h / 2,
            "OK",
            color=col,
            fontsize=max(8, font_px * 0.7),
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

        # Dimension label below button
        ax.text(
            x_cursor + w / 2,
            y_bot - 0.45,
            f"{dim_w}x{dim_h}",
            color=col,
            fontsize=9,
            fontweight="bold",
            ha="center",
            path_effects=stroke,
        )

        # Scale label above button
        ax.text(
            x_cursor + w / 2,
            y_bot + h + 0.35,
            f"scale = {s}",
            color=STYLE["text"],
            fontsize=10,
            fontweight="bold",
            ha="center",
            path_effects=stroke,
        )

        # Font size and padding annotation
        ax.text(
            x_cursor + w / 2,
            y_bot - 0.95,
            f"font {font_px:.0f}px  pad {pad_px:.0f}px",
            color=STYLE["text_dim"],
            fontsize=8,
            ha="center",
            path_effects=stroke,
        )

        # Width dimension arrow (horizontal double-headed arrow)
        arrow_y = y_bot + h + 0.75
        ax.annotate(
            "",
            xy=(x_cursor + w, arrow_y),
            xytext=(x_cursor, arrow_y),
            arrowprops={
                "arrowstyle": "<->,head_width=0.15,head_length=0.1",
                "color": col,
                "lw": 1.2,
            },
        )
        ax.text(
            x_cursor + w / 2,
            arrow_y + 0.2,
            f"{dim_w}px",
            color=col,
            fontsize=8,
            ha="center",
            path_effects=stroke,
        )

        x_cursor += w + gap

    # Footer formula
    ax.text(
        8.0,
        -0.8,
        "final_size = base_size * ctx->scale",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        family="monospace",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, LESSON_12_PATH, "scale_factor_effect.png")


def diagram_spacing_anatomy():
    """Vertical layout with three widgets showing widget_padding, item_spacing,
    and panel_padding with labeled dimension arrows."""

    fig, ax = plt.subplots(figsize=(10, 7))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-3, 13), ylim=(-1, 11), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        5.0,
        10.3,
        "Spacing Anatomy — Padding and Gaps",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Panel outer rect
    panel_x, panel_y = 1.5, 0.8
    panel_w, panel_h = 7.0, 8.5
    panel_pad = 0.8  # visual panel_padding (larger for clarity)

    panel_rect = mpatches.FancyBboxPatch(
        (panel_x, panel_y),
        panel_w,
        panel_h,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["bg"],
        edgecolor=STYLE["accent4"],
        linewidth=2.5,
    )
    ax.add_patch(panel_rect)

    # Panel padding region (shaded)
    inner_x = panel_x + panel_pad
    inner_y = panel_y + panel_pad
    inner_w = panel_w - 2 * panel_pad
    inner_h = panel_h - 2 * panel_pad

    # Draw three widgets inside the panel
    widget_h = 1.6
    item_spacing = 0.8
    widget_pad = 0.35  # visual widget_padding
    widget_labels = ["Button: Submit", "Slider: Volume", "Checkbox: Mute"]
    widget_colors = [STYLE["accent1"], STYLE["accent2"], STYLE["accent3"]]

    widgets = []
    wy = inner_y + inner_h - widget_h  # start from top
    for _i, (label, col) in enumerate(zip(widget_labels, widget_colors, strict=True)):
        # Widget background (full rect)
        wrect = mpatches.FancyBboxPatch(
            (inner_x, wy),
            inner_w,
            widget_h,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["surface"],
            edgecolor=col,
            linewidth=2,
        )
        ax.add_patch(wrect)

        # Content region inside widget (showing widget_padding)
        content_rect = mpatches.FancyBboxPatch(
            (inner_x + widget_pad, wy + widget_pad),
            inner_w - 2 * widget_pad,
            widget_h - 2 * widget_pad,
            boxstyle="round,pad=0.05",
            facecolor=STYLE["bg"],
            edgecolor=col,
            linewidth=1,
            linestyle="--",
        )
        ax.add_patch(content_rect)

        # Widget label
        ax.text(
            inner_x + inner_w / 2,
            wy + widget_h / 2,
            label,
            color=col,
            fontsize=10,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

        widgets.append((inner_x, wy, inner_w, widget_h))
        wy -= widget_h + item_spacing

    # --- Dimension annotations placed clearly outside the panel ---

    # 1. panel_padding — left side, tall bracket spanning panel edge to content
    pp_x = panel_x - 0.3
    pp_top = panel_y + panel_h
    pp_bot = inner_y + inner_h
    ax.annotate(
        "",
        xy=(pp_x, pp_bot),
        xytext=(pp_x, pp_top),
        arrowprops={
            "arrowstyle": "<->,head_width=0.18,head_length=0.1",
            "color": STYLE["accent4"],
            "lw": 2,
        },
    )
    ax.text(
        pp_x - 0.4,
        (pp_top + pp_bot) / 2,
        "panel_padding",
        color=STYLE["accent4"],
        fontsize=10,
        fontweight="bold",
        ha="right",
        va="center",
        path_effects=stroke,
    )

    # 2. panel_padding — bottom, horizontal bracket
    pp_b_y = panel_y - 0.3
    ax.annotate(
        "",
        xy=(panel_x, pp_b_y),
        xytext=(panel_x + panel_pad, pp_b_y),
        arrowprops={
            "arrowstyle": "<->,head_width=0.18,head_length=0.1",
            "color": STYLE["accent4"],
            "lw": 2,
        },
    )
    ax.text(
        panel_x + panel_pad / 2,
        pp_b_y - 0.45,
        "panel_padding",
        color=STYLE["accent4"],
        fontsize=9,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # 3. widget_padding — right side of first widget, clear vertical bracket
    w0_x, w0_y, w0_w, w0_h = widgets[0]
    wp_x = panel_x + panel_w + 0.4
    wp_top = w0_y + w0_h
    wp_bot = w0_y + w0_h - widget_pad
    ax.annotate(
        "",
        xy=(wp_x, wp_bot),
        xytext=(wp_x, wp_top),
        arrowprops={
            "arrowstyle": "<->,head_width=0.15,head_length=0.08",
            "color": STYLE["warn"],
            "lw": 2,
        },
    )
    # Horizontal leader line from widget edge to arrow
    ax.plot(
        [w0_x + w0_w, wp_x],
        [wp_top, wp_top],
        color=STYLE["warn"],
        lw=0.8,
        linestyle=":",
    )
    ax.plot(
        [w0_x + w0_w, wp_x],
        [wp_bot, wp_bot],
        color=STYLE["warn"],
        lw=0.8,
        linestyle=":",
    )
    ax.text(
        wp_x + 0.3,
        (wp_top + wp_bot) / 2,
        "widget_padding",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # 4. item_spacing — right side, between first and second widget
    w1_x, w1_y, w1_w, w1_h = widgets[1]
    gap_top = w0_y  # bottom edge of first widget
    gap_bot = w1_y + w1_h  # top edge of second widget
    is_x = wp_x  # align with widget_padding arrow
    ax.annotate(
        "",
        xy=(is_x, gap_bot),
        xytext=(is_x, gap_top),
        arrowprops={
            "arrowstyle": "<->,head_width=0.15,head_length=0.08",
            "color": STYLE["accent1"],
            "lw": 2,
        },
    )
    # Horizontal leader lines
    ax.plot(
        [w0_x + w0_w, is_x],
        [gap_top, gap_top],
        color=STYLE["accent1"],
        lw=0.8,
        linestyle=":",
    )
    ax.plot(
        [w1_x + w1_w, is_x],
        [gap_bot, gap_bot],
        color=STYLE["accent1"],
        lw=0.8,
        linestyle=":",
    )
    ax.text(
        is_x + 0.3,
        (gap_top + gap_bot) / 2,
        "item_spacing",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        va="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, LESSON_12_PATH, "spacing_anatomy.png")


def diagram_spacing_struct_overview():
    """ForgeUiSpacing struct fields visualized as a reference card: a mini UI
    mockup on the right with each spacing field labeled inline via colored
    dimension arrows and callout labels — no crossing arrows."""

    fig, ax = plt.subplots(figsize=(11, 8))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-1, 15), ylim=(-0.5, 12), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        7.0,
        11.3,
        "ForgeUiSpacing Struct — Field Reference",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # --- Central UI mockup ---
    mock_x, mock_y = 3.0, 0.5
    mock_w, mock_h = 8.0, 10.0

    # Panel outer rect
    panel = mpatches.FancyBboxPatch(
        (mock_x, mock_y),
        mock_w,
        mock_h,
        boxstyle="round,pad=0.12",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent4"],
        linewidth=2,
    )
    ax.add_patch(panel)

    # Title bar
    title_bar_h = 0.9
    title_y = mock_y + mock_h - title_bar_h
    title_bar = mpatches.FancyBboxPatch(
        (mock_x + 0.15, title_y),
        mock_w - 0.3,
        title_bar_h,
        boxstyle="round,pad=0.05",
        facecolor=STYLE["accent1"],
        edgecolor=STYLE["accent1"],
        linewidth=1,
        alpha=0.4,
    )
    ax.add_patch(title_bar)
    ax.text(
        mock_x + mock_w / 2,
        title_y + title_bar_h / 2,
        "Panel Title",
        color=STYLE["text"],
        fontsize=10,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # Panel padding inset zone
    pp = 0.7  # panel_padding visual
    content_x = mock_x + pp
    content_y = mock_y + pp
    content_w = mock_w - 2 * pp
    content_top = title_y - 0.2

    # Button widget
    btn_h = 1.0
    btn_y = content_top - btn_h
    btn_rect = mpatches.FancyBboxPatch(
        (content_x, btn_y),
        content_w,
        btn_h,
        boxstyle="round,pad=0.06",
        facecolor=STYLE["bg"],
        edgecolor=STYLE["accent1"],
        linewidth=1.5,
    )
    ax.add_patch(btn_rect)
    ax.text(
        content_x + content_w / 2,
        btn_y + btn_h / 2,
        "Button",
        color=STYLE["accent1"],
        fontsize=10,
        ha="center",
        va="center",
        path_effects=stroke,
    )

    # item_spacing gap
    isp = 0.5

    # Checkbox widget
    cb_y = btn_y - isp - 1.0
    cb_h = 1.0
    cb_box_size = 0.6
    cb_box = mpatches.FancyBboxPatch(
        (content_x + 0.3, cb_y + (cb_h - cb_box_size) / 2),
        cb_box_size,
        cb_box_size,
        boxstyle="round,pad=0.03",
        facecolor=STYLE["bg"],
        edgecolor=STYLE["accent3"],
        linewidth=1.5,
    )
    ax.add_patch(cb_box)
    ax.text(
        content_x + 0.3 + cb_box_size + 0.4,
        cb_y + cb_h / 2,
        "Checkbox label",
        color=STYLE["accent3"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # Slider widget
    sl_y = cb_y - isp - 1.0
    sl_h = 1.0
    track_h = 0.15
    track_y = sl_y + sl_h / 2 - track_h / 2
    track_rect = mpatches.FancyBboxPatch(
        (content_x + 0.3, track_y),
        content_w - 0.6,
        track_h,
        boxstyle="round,pad=0.03",
        facecolor=STYLE["grid"],
        edgecolor=STYLE["grid"],
        linewidth=1,
    )
    ax.add_patch(track_rect)
    thumb_w = 0.45
    thumb_h = 0.65
    thumb_x = content_x + content_w * 0.5
    thumb_y = sl_y + sl_h / 2 - thumb_h / 2
    thumb_rect = mpatches.FancyBboxPatch(
        (thumb_x, thumb_y),
        thumb_w,
        thumb_h,
        boxstyle="round,pad=0.03",
        facecolor=STYLE["accent2"],
        edgecolor=STYLE["accent2"],
        linewidth=1,
    )
    ax.add_patch(thumb_rect)

    # Text input widget
    ti_y = sl_y - isp - 1.0
    ti_h = 1.0
    ti_rect = mpatches.FancyBboxPatch(
        (content_x, ti_y),
        content_w,
        ti_h,
        boxstyle="round,pad=0.06",
        facecolor=STYLE["bg"],
        edgecolor=STYLE["text_dim"],
        linewidth=1.5,
    )
    ax.add_patch(ti_rect)
    ti_pad = 0.35
    ax.text(
        content_x + ti_pad,
        ti_y + ti_h / 2,
        "Text input...",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
        va="center",
        path_effects=stroke,
    )

    # Scrollbar (right edge)
    sb_w = 0.3
    sb_x = mock_x + mock_w - pp - sb_w
    sb_y = content_y
    sb_h = content_top - content_y
    sb_rect = mpatches.FancyBboxPatch(
        (sb_x, sb_y),
        sb_w,
        sb_h,
        boxstyle="round,pad=0.03",
        facecolor=STYLE["grid"],
        edgecolor=STYLE["text_dim"],
        linewidth=1,
    )
    ax.add_patch(sb_rect)

    # --- Inline dimension annotations (no crossing arrows) ---

    # 1. title_bar_height — right side of title bar
    tbh_x = mock_x + mock_w + 0.3
    ax.annotate(
        "",
        xy=(tbh_x, title_y),
        xytext=(tbh_x, title_y + title_bar_h),
        arrowprops={
            "arrowstyle": "<->,head_width=0.15,head_length=0.08",
            "color": STYLE["accent1"],
            "lw": 2,
        },
    )
    ax.text(
        tbh_x + 0.3,
        title_y + title_bar_h / 2,
        "title_bar_height",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="center",
        family="monospace",
        path_effects=stroke,
    )

    # 2. panel_padding — bottom-left side of panel (shows exact pp distance)
    ppd_x = mock_x - 0.3
    ax.annotate(
        "",
        xy=(ppd_x, mock_y),
        xytext=(ppd_x, content_y),
        arrowprops={
            "arrowstyle": "<->,head_width=0.15,head_length=0.08",
            "color": STYLE["accent4"],
            "lw": 2,
        },
    )
    ax.text(
        ppd_x - 0.3,
        (mock_y + content_y) / 2,
        "panel_padding",
        color=STYLE["accent4"],
        fontsize=9,
        fontweight="bold",
        ha="right",
        va="center",
        family="monospace",
        path_effects=stroke,
    )

    # 3. widget_padding — horizontal bracket inside button
    wp_y = btn_y - 0.25
    ax.annotate(
        "",
        xy=(content_x, wp_y),
        xytext=(content_x + 0.35, wp_y),
        arrowprops={
            "arrowstyle": "<->,head_width=0.12,head_length=0.06",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
    )
    ax.text(
        content_x + 0.17,
        wp_y - 0.35,
        "widget_padding",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        family="monospace",
        path_effects=stroke,
    )

    # 4. item_spacing — right side between button and checkbox
    is_x = mock_x + mock_w + 0.3
    is_top = btn_y
    is_bot = cb_y + cb_h
    ax.annotate(
        "",
        xy=(is_x, is_bot),
        xytext=(is_x, is_top),
        arrowprops={
            "arrowstyle": "<->,head_width=0.15,head_length=0.08",
            "color": STYLE["accent1"],
            "lw": 2,
        },
    )
    ax.text(
        is_x + 0.3,
        (is_top + is_bot) / 2,
        "item_spacing",
        color=STYLE["accent1"],
        fontsize=9,
        fontweight="bold",
        ha="left",
        va="center",
        family="monospace",
        path_effects=stroke,
    )

    # 5. checkbox_box_size — below checkbox box
    cb_center_x = content_x + 0.3 + cb_box_size / 2
    cb_bot_y = cb_y + (cb_h - cb_box_size) / 2
    cbs_y = cb_bot_y - 0.25
    ax.annotate(
        "",
        xy=(content_x + 0.3, cbs_y),
        xytext=(content_x + 0.3 + cb_box_size, cbs_y),
        arrowprops={
            "arrowstyle": "<->,head_width=0.12,head_length=0.06",
            "color": STYLE["accent3"],
            "lw": 1.5,
        },
    )
    ax.text(
        cb_center_x,
        cbs_y - 0.35,
        "checkbox_box_size",
        color=STYLE["accent3"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        family="monospace",
        path_effects=stroke,
    )

    # 6. slider_thumb_width — below thumb
    stw_y = thumb_y - 0.25
    ax.annotate(
        "",
        xy=(thumb_x, stw_y),
        xytext=(thumb_x + thumb_w, stw_y),
        arrowprops={
            "arrowstyle": "<->,head_width=0.12,head_length=0.06",
            "color": STYLE["accent2"],
            "lw": 1.5,
        },
    )
    ax.text(
        thumb_x + thumb_w / 2,
        stw_y - 0.35,
        "slider_thumb_width",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        family="monospace",
        path_effects=stroke,
    )

    # 6b. slider_thumb_height — right side of thumb
    sthh_x = thumb_x + thumb_w + 0.15
    ax.annotate(
        "",
        xy=(sthh_x, thumb_y),
        xytext=(sthh_x, thumb_y + thumb_h),
        arrowprops={
            "arrowstyle": "<->,head_width=0.1,head_length=0.05",
            "color": STYLE["accent2"],
            "lw": 1.5,
        },
    )
    ax.text(
        sthh_x + 0.2,
        thumb_y + thumb_h / 2,
        "slider_thumb_height",
        color=STYLE["accent2"],
        fontsize=8,
        fontweight="bold",
        ha="left",
        va="center",
        family="monospace",
        path_effects=stroke,
    )

    # 7. slider_track_height — left side of track
    sth_x = content_x - 0.15
    ax.annotate(
        "",
        xy=(sth_x, track_y),
        xytext=(sth_x, track_y + track_h),
        arrowprops={
            "arrowstyle": "<->,head_width=0.1,head_length=0.05",
            "color": STYLE["text_dim"],
            "lw": 1.5,
        },
    )
    ax.text(
        sth_x - 0.3,
        track_y + track_h / 2,
        "slider_track_height",
        color=STYLE["text_dim"],
        fontsize=8,
        fontweight="bold",
        ha="right",
        va="center",
        family="monospace",
        path_effects=stroke,
    )

    # 8. text_input_padding — inside text input, left side
    tip_y = ti_y + ti_h + 0.15
    ax.annotate(
        "",
        xy=(content_x, tip_y),
        xytext=(content_x + ti_pad, tip_y),
        arrowprops={
            "arrowstyle": "<->,head_width=0.12,head_length=0.06",
            "color": STYLE["warn"],
            "lw": 1.5,
        },
    )
    ax.text(
        content_x + ti_pad / 2,
        tip_y + 0.3,
        "text_input_padding",
        color=STYLE["warn"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        family="monospace",
        path_effects=stroke,
    )

    # 9. scrollbar_width — below scrollbar
    sbw_y = sb_y - 0.25
    ax.annotate(
        "",
        xy=(sb_x, sbw_y),
        xytext=(sb_x + sb_w, sbw_y),
        arrowprops={
            "arrowstyle": "<->,head_width=0.12,head_length=0.06",
            "color": STYLE["text_dim"],
            "lw": 1.5,
        },
    )
    ax.text(
        sb_x + sb_w / 2,
        sbw_y - 0.35,
        "scrollbar_width",
        color=STYLE["text_dim"],
        fontsize=8,
        fontweight="bold",
        ha="center",
        family="monospace",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, LESSON_12_PATH, "spacing_struct_overview.png")


def diagram_atlas_rebuild_at_scale():
    """Side-by-side atlas texture excerpts at scale 1.0 and 2.0 showing that
    scaling requires rebuilding the atlas, not just stretching UVs."""

    fig, ax = plt.subplots(figsize=(11, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 14), ylim=(-1.5, 6), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    ax.text(
        7.0,
        5.5,
        "Atlas Rebuild at Different Scales",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    configs = [
        ("Scale 1.0", "16px", "256 x 128", 1.0, STYLE["accent1"], 0.5),
        ("Scale 2.0", "32px", "512 x 256", 2.0, STYLE["accent2"], 7.5),
    ]

    atlas_centers = []
    for title, font_label, dims, scale, col, base_x in configs:
        # Atlas rectangle — both aligned at same y, right one larger
        atlas_w = 3.0 + (scale - 1.0) * 1.5
        atlas_h = 2.0 + (scale - 1.0) * 0.8
        atlas_x = base_x
        atlas_y = 1.2

        atlas_rect = mpatches.FancyBboxPatch(
            (atlas_x, atlas_y),
            atlas_w,
            atlas_h,
            boxstyle="round,pad=0.08",
            facecolor=STYLE["surface"],
            edgecolor=col,
            linewidth=2,
        )
        ax.add_patch(atlas_rect)
        atlas_centers.append((atlas_x + atlas_w / 2, atlas_y + atlas_h / 2))

        # Title above atlas
        ax.text(
            atlas_x + atlas_w / 2,
            atlas_y + atlas_h + 0.45,
            title,
            color=col,
            fontsize=13,
            fontweight="bold",
            ha="center",
            path_effects=stroke,
        )

        # "Aa" text inside atlas at varying sizes
        font_size = 14 + int(scale * 8)
        ax.text(
            atlas_x + atlas_w * 0.3,
            atlas_y + atlas_h * 0.5,
            "Aa",
            color=col,
            fontsize=font_size,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )
        ax.text(
            atlas_x + atlas_w * 0.7,
            atlas_y + atlas_h * 0.5,
            "Bg",
            color=col,
            fontsize=font_size,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
            alpha=0.6,
        )

        # Font size label below
        ax.text(
            atlas_x + atlas_w / 2,
            atlas_y - 0.35,
            f"Font rasterized at {font_label}",
            color=STYLE["text"],
            fontsize=10,
            ha="center",
            path_effects=stroke,
        )
        ax.text(
            atlas_x + atlas_w / 2,
            atlas_y - 0.8,
            f"Atlas: {dims}",
            color=STYLE["text_dim"],
            fontsize=9,
            ha="center",
            family="monospace",
            path_effects=stroke,
        )

    # Clean horizontal arrow between the two atlas boxes
    arrow_start = atlas_centers[0][0] + 2.0
    arrow_end = configs[1][5] - 0.3  # left edge of second atlas
    arrow_y = 2.5
    ax.annotate(
        "",
        xy=(arrow_end, arrow_y),
        xytext=(arrow_start, arrow_y),
        arrowprops={
            "arrowstyle": "->,head_width=0.25,head_length=0.15",
            "color": STYLE["warn"],
            "lw": 3,
        },
    )
    ax.text(
        (arrow_start + arrow_end) / 2,
        arrow_y + 0.45,
        "rebuild",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Footer message
    ax.text(
        7.0,
        -1.1,
        "Rebuild required \u2014 not just stretching UVs",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, LESSON_12_PATH, "atlas_rebuild_at_scale.png")


def diagram_before_after_spacing():
    """Split comparison showing hardcoded spacing (left) vs ForgeUiSpacing
    struct (right) using a two-subplot layout."""

    fig, (ax_left, ax_right) = plt.subplots(1, 2, figsize=(10, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    fig.suptitle(
        "Before vs After: Consistent Spacing",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=0.97,
    )

    # --- Left side: hardcoded, inconsistent ---
    ax_left.set_title(
        "Before (hardcoded)",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    setup_axes(ax_left, xlim=(0, 6), ylim=(0, 8), grid=False, aspect=None)
    ax_left.axis("off")

    # Panel background
    panel_l = mpatches.FancyBboxPatch(
        (0.3, 0.3),
        5.4,
        7.2,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent2"],
        linewidth=2,
        linestyle="--",
    )
    ax_left.add_patch(panel_l)

    # Widgets with inconsistent spacing
    bad_widgets = [
        (0.5, 6.2, 4.8, 0.9, "Button A"),
        (0.8, 5.0, 4.2, 0.7, "Slider"),  # different left offset
        (0.5, 3.5, 5.0, 1.1, "Checkbox"),  # bigger gap, different height
        (1.0, 2.8, 3.5, 0.5, "Label"),  # crammed, different width
        (0.5, 1.6, 4.8, 0.9, "Button B"),
    ]

    for wx, wy, ww, wh, label in bad_widgets:
        r = mpatches.FancyBboxPatch(
            (wx, wy),
            ww,
            wh,
            boxstyle="round,pad=0.05",
            facecolor=STYLE["bg"],
            edgecolor=STYLE["accent2"],
            linewidth=1.5,
            alpha=0.7,
        )
        ax_left.add_patch(r)
        ax_left.text(
            wx + ww / 2,
            wy + wh / 2,
            label,
            color=STYLE["accent2"],
            fontsize=9,
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # Annotation: red Xs for problems
    problems = [(5.1, 5.5), (5.1, 3.2), (5.1, 2.6)]
    for px, py in problems:
        ax_left.text(
            px,
            py,
            "\u2717",
            color=STYLE["accent2"],
            fontsize=14,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    # --- Right side: ForgeUiSpacing, consistent ---
    ax_right.set_title(
        "After (ForgeUiSpacing)",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        pad=8,
    )
    setup_axes(ax_right, xlim=(0, 6), ylim=(0, 8), grid=False, aspect=None)
    ax_right.axis("off")

    # Panel background
    panel_r = mpatches.FancyBboxPatch(
        (0.3, 0.3),
        5.4,
        7.2,
        boxstyle="round,pad=0.1",
        facecolor=STYLE["surface"],
        edgecolor=STYLE["accent3"],
        linewidth=2,
    )
    ax_right.add_patch(panel_r)

    # Widgets with consistent spacing
    pad = 0.6
    w_w = 5.4 - 2 * pad
    w_h = 0.85
    gap = 0.45
    labels = ["Button A", "Slider", "Checkbox", "Label", "Button B"]
    wy = 0.3 + 7.2 - pad - w_h

    for label in labels:
        r = mpatches.FancyBboxPatch(
            (0.3 + pad, wy),
            w_w,
            w_h,
            boxstyle="round,pad=0.05",
            facecolor=STYLE["bg"],
            edgecolor=STYLE["accent3"],
            linewidth=1.5,
        )
        ax_right.add_patch(r)
        ax_right.text(
            0.3 + pad + w_w / 2,
            wy + w_h / 2,
            label,
            color=STYLE["accent3"],
            fontsize=9,
            ha="center",
            va="center",
            path_effects=stroke,
        )
        wy -= w_h + gap

    # Checkmarks
    checks = [(5.1, 6.3), (5.1, 4.8), (5.1, 3.4)]
    for cx, cy in checks:
        ax_right.text(
            cx,
            cy,
            "\u2713",
            color=STYLE["accent3"],
            fontsize=14,
            fontweight="bold",
            ha="center",
            va="center",
            path_effects=stroke,
        )

    fig.tight_layout(rect=(0, 0, 1, 0.93))
    save(fig, LESSON_12_PATH, "before_after_spacing.png")


def diagram_scaled_dimensions_formula():
    """Flowchart: base_value defined in ForgeUiSpacing, multiplied by
    ctx->scale, producing final pixel value. Boxes and arrows with formula."""

    fig, ax = plt.subplots(figsize=(10, 5))
    fig.patch.set_facecolor(STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 12), ylim=(-1, 7), grid=False, aspect=None)
    ax.axis("off")
    stroke = [pe.withStroke(linewidth=3, foreground=STYLE["bg"])]

    # Lighter fill for boxes so they are clearly distinguishable from the
    # diagram background (#1a1a2e).  STYLE["surface"] (#252545) is only ~11
    # RGB units brighter — not enough.  #3a3a60 gives ~32 units of contrast.
    box_fill = "#3a3a60"

    ax.text(
        5.75,
        6.3,
        "Scaled Dimensions Formula",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )

    # Box 1: Base value in ForgeUiSpacing
    box1_x, box1_y = 0.2, 3.0
    box1_w, box1_h = 3.2, 2.2
    box1 = mpatches.FancyBboxPatch(
        (box1_x, box1_y),
        box1_w,
        box1_h,
        boxstyle="round,pad=0.15",
        facecolor=box_fill,
        edgecolor=STYLE["accent1"],
        linewidth=2.5,
    )
    ax.add_patch(box1)
    ax.text(
        box1_x + box1_w / 2,
        box1_y + box1_h - 0.4,
        "ForgeUiSpacing",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )
    ax.text(
        box1_x + box1_w / 2,
        box1_y + box1_h / 2 - 0.15,
        "item_spacing = 6.0",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        family="monospace",
        path_effects=stroke,
    )
    ax.text(
        box1_x + box1_w / 2,
        box1_y + 0.35,
        "(base value)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    # Arrow 1 -> multiply
    ax.annotate(
        "",
        xy=(4.2, 4.1),
        xytext=(3.5, 4.1),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["warn"],
            "lw": 2.5,
        },
    )

    # Box 2: Multiply operator
    box2_x, box2_y = 4.2, 3.0
    box2_w, box2_h = 3.2, 2.2
    box2 = mpatches.FancyBboxPatch(
        (box2_x, box2_y),
        box2_w,
        box2_h,
        boxstyle="round,pad=0.15",
        facecolor=box_fill,
        edgecolor=STYLE["warn"],
        linewidth=2.5,
    )
    ax.add_patch(box2)
    ax.text(
        box2_x + box2_w / 2,
        box2_y + box2_h - 0.4,
        "Multiply",
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )
    ax.text(
        box2_x + box2_w / 2,
        box2_y + box2_h / 2 - 0.15,
        "ctx->scale = 1.5",
        color=STYLE["text"],
        fontsize=10,
        ha="center",
        family="monospace",
        path_effects=stroke,
    )
    ax.text(
        box2_x + box2_w / 2,
        box2_y + 0.35,
        "(runtime scale factor)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    # Arrow 2 -> result
    ax.annotate(
        "",
        xy=(8.2, 4.1),
        xytext=(7.5, 4.1),
        arrowprops={
            "arrowstyle": "->,head_width=0.2,head_length=0.12",
            "color": STYLE["accent3"],
            "lw": 2.5,
        },
    )

    # Box 3: Final pixel value
    box3_x, box3_y = 8.2, 3.0
    box3_w, box3_h = 3.2, 2.2
    box3 = mpatches.FancyBboxPatch(
        (box3_x, box3_y),
        box3_w,
        box3_h,
        boxstyle="round,pad=0.15",
        facecolor=box_fill,
        edgecolor=STYLE["accent3"],
        linewidth=2.5,
    )
    ax.add_patch(box3)
    ax.text(
        box3_x + box3_w / 2,
        box3_y + box3_h - 0.4,
        "Final Value",
        color=STYLE["accent3"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=stroke,
    )
    ax.text(
        box3_x + box3_w / 2,
        box3_y + box3_h / 2 - 0.15,
        "9.0 px",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        family="monospace",
        path_effects=stroke,
    )
    ax.text(
        box3_x + box3_w / 2,
        box3_y + 0.35,
        "(used for layout)",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="center",
        path_effects=stroke,
    )

    # Formula bar at bottom
    formula_rect = mpatches.FancyBboxPatch(
        (1.5, 0.3),
        8.5,
        1.2,
        boxstyle="round,pad=0.15",
        facecolor=STYLE["bg"],
        edgecolor=STYLE["accent1"],
        linewidth=2,
        linestyle="--",
    )
    ax.add_patch(formula_rect)
    ax.text(
        5.75,
        0.9,
        "final = base \u00d7 scale    \u2192    6.0 \u00d7 1.5 = 9.0 px",
        color=STYLE["warn"],
        fontsize=12,
        fontweight="bold",
        ha="center",
        family="monospace",
        path_effects=stroke,
    )

    fig.tight_layout()
    save(fig, LESSON_12_PATH, "scaled_dimensions_formula.png")


# ---------------------------------------------------------------------------
# UI Lesson 13 — Theming and Color System
# ---------------------------------------------------------------------------
