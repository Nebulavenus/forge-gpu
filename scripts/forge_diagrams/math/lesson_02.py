"""Diagrams for math/02."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Polygon, Rectangle

from .._common import STYLE, save

# ── Transformation helpers ────────────────────────────────────────────────


def _mat4_rot_y(deg):
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    m = np.eye(4)
    m[0, 0], m[0, 2] = c, s
    m[2, 0], m[2, 2] = -s, c
    return m


_SCR_W, _SCR_H = 1920.0, 1080.0


_CAM_EYE = np.array([8.0, 5.0, 10.0])


# Each face is a list of vertex indices forming a polygon.
# Winding is outward-facing for shading orientation.
_HOUSE_FACES = [
    # Body walls
    [0, 1, 3, 2],  # front
    [6, 4, 5, 7],  # back
    [0, 6, 7, 1],  # bottom
    [0, 2, 4, 6],  # left
    [1, 7, 5, 3],  # right
    # Roof
    [2, 3, 8],  # front gable
    [4, 9, 5],  # back gable
    [2, 8, 9, 4],  # left slope
    [3, 5, 9, 8],  # right slope
]

# Flat-shade colours — index matches _HOUSE_FACES.
# Front wall is accent1, sides darker, roof accent3.


# Flat-shade colours — index matches _HOUSE_FACES.
# Front wall is accent1, sides darker, roof accent3.
_HOUSE_COLORS = [
    "#5dc8f0",  # front wall  (bright cyan)
    "#3a8daa",  # back wall   (darker)
    "#2a6a80",  # bottom      (darkest, rarely seen)
    "#3596b8",  # left wall
    "#3596b8",  # right wall
    "#78d080",  # front gable (green)
    "#4ea85a",  # back gable
    "#5cb868",  # left slope
    "#5cb868",  # right slope
]


# ---------------------------------------------------------------------------
# math/02-coordinate-spaces — 3-D coordinate space visualizations
# ---------------------------------------------------------------------------
#
# A simple 3-D house (box body + triangular-prism roof) is transformed
# through the six-stage rendering pipeline.  Each diagram renders the
# house with flat-shaded faces so learners can follow it from local
# space all the way to screen pixels.  World-space diagrams include a
# yard (ground plane) and road for spatial context.

_COORD_LESSON = "math/02-coordinate-spaces"

# ── 3-D house geometry (local space) ──────────────────────────────────────
#
# Y is up.  The house body is a 1×0.7×0.6 box sitting on the XZ ground
# plane (y = 0).  The roof is a triangular prism peaking at y = 1.0.
#
#   Vertices (x, y, z):
#
#        8─────9          roof peak   (y = 1.0)
#       /|\   /|\
#      / | \ / | \
#     2──+──3  |  |       wall top    (y = 0.7)
#     |  4──|──5  |       (back wall top, same y)
#     | /   | /  /
#     |/    |/  /
#     0─────1  /          ground      (y = 0)
#      \       /
#       6─────7           back ground (y = 0)
#
# Faces reference these indices.


def _mat4_perspective(fov_deg, aspect, near, far):
    fv = 1.0 / np.tan(np.radians(fov_deg) / 2.0)
    m = np.zeros((4, 4))
    m[0, 0] = fv / aspect
    m[1, 1] = fv
    m[2, 2] = far / (near - far)
    m[2, 3] = (near * far) / (near - far)
    m[3, 2] = -1.0
    return m


_PROJ_MAT = _mat4_perspective(60.0, 16.0 / 9.0, 0.1, 100.0)


def _house_polys(verts):
    """Return a list of Nx3 arrays — one polygon per house face."""
    return [verts[idx] for idx in _HOUSE_FACES]


# ── Transformation helpers ────────────────────────────────────────────────


_CAM_TARGET = np.array([3.0, 1.0, -2.0])


_CAM_UP = np.array([0.0, 1.0, 0.0])


def _mat4_look_at(eye, target, up):
    f = target - eye
    f = f / np.linalg.norm(f)
    r = np.cross(f, up)
    r = r / np.linalg.norm(r)
    u = np.cross(r, f)
    m = np.eye(4)
    m[0, :3], m[1, :3], m[2, :3] = r, u, -f
    m[0, 3] = -r.dot(eye)
    m[1, 3] = -u.dot(eye)
    m[2, 3] = f.dot(eye)
    return m


_VIEW_MAT = _mat4_look_at(_CAM_EYE, _CAM_TARGET, _CAM_UP)


def _add_road(ax, x_range, z_center, width):
    """Draw a road strip at mpl-z = 0.001 (sits just above the ground)."""
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    x0, x1 = x_range
    hw = width / 2.0
    mpl_z = 0.001  # tiny offset above ground
    # mpl axes: (x, y=depth, z=up)
    quad = [
        [x0, z_center - hw, mpl_z],
        [x1, z_center - hw, mpl_z],
        [x1, z_center + hw, mpl_z],
        [x0, z_center + hw, mpl_z],
    ]
    col = Poly3DCollection(
        [quad],
        facecolors=[STYLE["grid"]],
        edgecolors=[STYLE["text_dim"]],
        linewidths=0.5,
        alpha=0.45,
    )
    ax.add_collection3d(col)
    # Centre dashes
    n_dash = 6
    xs = np.linspace(x0 + 0.3, x1 - 0.3, n_dash)
    for xd in xs:
        ax.plot(
            [xd, xd + 0.25],
            [z_center] * 2,
            [mpl_z + 0.002] * 2,
            color=STYLE["warn"],
            lw=1.2,
            alpha=0.7,
        )


def _mat4_translate(tx, ty, tz):
    m = np.eye(4)
    m[0, 3], m[1, 3], m[2, 3] = tx, ty, tz
    return m


def _mat4_scale(s):
    return np.diag([s, s, s, 1.0])


# ── Shared pipeline parameters ────────────────────────────────────────────

_MODEL_MAT = _mat4_translate(3.0, 0.0, -2.0) @ _mat4_rot_y(35.0) @ _mat4_scale(2.0)


def _xf3(verts3, mat4):
    """Transform Nx3 vertices by a 4x4 matrix, return Nx3."""
    n = verts3.shape[0]
    h = np.hstack([verts3, np.ones((n, 1))])
    out = (mat4 @ h.T).T
    return out[:, :3]


# ── 3-D house geometry (local space) ──────────────────────────────────────
#
# Y is up.  The house body is a 1×0.7×0.6 box sitting on the XZ ground
# plane (y = 0).  The roof is a triangular prism peaking at y = 1.0.
#
#   Vertices (x, y, z):
#
#        8─────9          roof peak   (y = 1.0)
#       /|\   /|\
#      / | \ / | \
#     2──+──3  |  |       wall top    (y = 0.7)
#     |  4──|──5  |       (back wall top, same y)
#     | /   | /  /
#     |/    |/  /
#     0─────1  /          ground      (y = 0)
#      \       /
#       6─────7           back ground (y = 0)
#
# Faces reference these indices.

_HOUSE_VERTS = np.array(
    [
        [-0.5, 0.0, -0.3],  # 0  front-left-bottom
        [0.5, 0.0, -0.3],  # 1  front-right-bottom
        [-0.5, 0.7, -0.3],  # 2  front-left-top
        [0.5, 0.7, -0.3],  # 3  front-right-top
        [-0.5, 0.7, 0.3],  # 4  back-left-top
        [0.5, 0.7, 0.3],  # 5  back-right-top
        [-0.5, 0.0, 0.3],  # 6  back-left-bottom
        [0.5, 0.0, 0.3],  # 7  back-right-bottom
        [0.0, 1.0, -0.3],  # 8  front-roof-peak
        [0.0, 1.0, 0.3],  # 9  back-roof-peak
    ],
    dtype=float,
)

# Each face is a list of vertex indices forming a polygon.
# Winding is outward-facing for shading orientation.


# ── Coordinate-system adapter ─────────────────────────────────────────────
#
# GPU convention: Y-up (x right, y up, z toward viewer).
# Matplotlib Axes3D: Z-up (x right, y depth, z up).
# _gpu_to_mpl swaps the Y and Z columns so our Y-up data renders upright.


def _gpu_to_mpl(verts):
    """Swap Y↔Z so GPU Y-up data plots correctly in matplotlib Z-up axes."""
    return verts[:, [0, 2, 1]]


# ── 3-D axis styling helper ───────────────────────────────────────────────


def _xf4(verts3, mat4):
    """Transform Nx3 vertices by a 4x4 matrix, return Nx4 (keep w)."""
    n = verts3.shape[0]
    h = np.hstack([verts3, np.ones((n, 1))])
    return (mat4 @ h.T).T


# ── Shared pipeline parameters ────────────────────────────────────────────


def _coord_pipeline():
    """Push house vertices through the full transform pipeline.

    Returns (world_verts, view_verts, clip4, ndc_verts, screen_x, screen_y).
    All arrays use GPU Y-up convention (apply _gpu_to_mpl before plotting).
    """
    world_verts = _xf3(_HOUSE_VERTS, _MODEL_MAT)
    view_verts = _xf3(world_verts, _VIEW_MAT)
    clip4 = _xf4(view_verts, _PROJ_MAT)
    ndc_verts = clip4[:, :3] / clip4[:, 3:4]
    screen_x = (ndc_verts[:, 0] + 1.0) * 0.5 * _SCR_W
    screen_y = (1.0 - ndc_verts[:, 1]) * 0.5 * _SCR_H
    return world_verts, view_verts, clip4, ndc_verts, screen_x, screen_y


def _scenery_world_verts():
    """Return (ground, road, dashes) arrays in GPU world-space coordinates.

    Matches the yard and road drawn in the world-space / view-space diagrams
    so the same scenery appears consistently across all pipeline stages.
    *dashes* is an (N, 2, 3) array of lane-divider line segments.
    """
    world_verts, _, _, _, _, _ = _coord_pipeline()
    hc = world_verts.mean(axis=0)

    gnd_size = 7.0
    ghs = gnd_size / 2.0
    ground = np.array(
        [
            [hc[0] - ghs, 0.0, hc[2] - ghs],
            [hc[0] + ghs, 0.0, hc[2] - ghs],
            [hc[0] + ghs, 0.0, hc[2] + ghs],
            [hc[0] - ghs, 0.0, hc[2] + ghs],
        ]
    )

    road_x0, road_x1 = -2.0, 10.0
    road_z = hc[2] + 5.0
    road_hw = 1.5 / 2.0
    road = np.array(
        [
            [road_x0, 0.0, road_z - road_hw],
            [road_x1, 0.0, road_z - road_hw],
            [road_x1, 0.0, road_z + road_hw],
            [road_x0, 0.0, road_z + road_hw],
        ]
    )

    # Lane-divider dashes (same as _add_road / view-space diagram)
    n_dash = 6
    dash_xs = np.linspace(road_x0 + 0.3, road_x1 - 0.3, n_dash)
    dashes = np.array(
        [[[xd, 0.001, road_z], [xd + 0.25, 0.001, road_z]] for xd in dash_xs]
    )

    return ground, road, dashes


def _scenery_through_pipeline():
    """Push ground/road/dashes through view → clip → NDC → screen.

    Returns a dict with keys 'ground', 'road', and 'dashes'.  Ground and
    road contain sub-dicts with 'clip4', 'ndc', 'screen_x', 'screen_y'.
    Dashes contains lists of 2-point arrays in each coordinate space.
    """
    ground_w, road_w, dashes_w = _scenery_world_verts()

    result = {}
    for name, world_quad in [("ground", ground_w), ("road", road_w)]:
        view = _xf3(world_quad, _VIEW_MAT)
        clip4 = _xf4(view, _PROJ_MAT)
        ndc = clip4[:, :3] / clip4[:, 3:4]
        sx = (ndc[:, 0] + 1.0) * 0.5 * _SCR_W
        sy = (1.0 - ndc[:, 1]) * 0.5 * _SCR_H
        result[name] = {"clip4": clip4, "ndc": ndc, "screen_x": sx, "screen_y": sy}

    # Project lane-divider dashes through the same pipeline.
    # dashes_w has shape (n_dash, 2, 3) — flatten to (n_dash*2, 3), transform,
    # then reshape back.
    n_dash = dashes_w.shape[0]
    flat_w = dashes_w.reshape(-1, 3)
    flat_view = _xf3(flat_w, _VIEW_MAT)
    flat_clip4 = _xf4(flat_view, _PROJ_MAT)
    flat_ndc = flat_clip4[:, :3] / flat_clip4[:, 3:4]
    flat_sx = (flat_ndc[:, 0] + 1.0) * 0.5 * _SCR_W
    flat_sy = (1.0 - flat_ndc[:, 1]) * 0.5 * _SCR_H
    result["dashes"] = {
        "clip4": flat_clip4.reshape(n_dash, 2, 4),
        "ndc": flat_ndc.reshape(n_dash, 2, 3),
        "screen_x": flat_sx.reshape(n_dash, 2),
        "screen_y": flat_sy.reshape(n_dash, 2),
    }
    return result


# ── 3-D axis styling helper ───────────────────────────────────────────────


def _style_3d(ax, title, xlabel="x", ylabel="z", zlabel="y (up)"):
    """Apply the forge dark theme to a 3-D axes."""
    ax.set_facecolor(STYLE["bg"])
    ax.xaxis.pane.set_facecolor(STYLE["surface"])  # type: ignore[attr-defined]
    ax.yaxis.pane.set_facecolor(STYLE["surface"])  # type: ignore[attr-defined]
    ax.zaxis.pane.set_facecolor(STYLE["surface"])
    ax.xaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax.yaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax.zaxis.pane.set_edgecolor(STYLE["grid"])
    ax.tick_params(colors=STYLE["axis"], labelsize=7)
    ax.set_xlabel(xlabel, color=STYLE["axis"], fontsize=9, labelpad=2)
    ax.set_ylabel(ylabel, color=STYLE["axis"], fontsize=9, labelpad=2)
    ax.set_zlabel(zlabel, color=STYLE["axis"], fontsize=9, labelpad=2)
    ax.set_title(
        title,
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )


def _set_equal_3d(ax, verts, pad=0.5):
    """Force equal aspect on a 3-D axes based on vertex extents."""
    mins = verts.min(axis=0)
    maxs = verts.max(axis=0)
    center = (mins + maxs) / 2.0
    half = (maxs - mins).max() / 2.0 + pad
    ax.set_xlim(center[0] - half, center[0] + half)
    ax.set_ylim(center[1] - half, center[1] + half)
    ax.set_zlim(center[2] - half, center[2] + half)


# ── Diagram 1: Local Space ────────────────────────────────────────────────


def _add_house_3d(ax, verts):
    """Add the shaded 3-D house to *ax* using Poly3DCollection."""
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    polys = _house_polys(verts)
    col = Poly3DCollection(
        polys,
        facecolors=_HOUSE_COLORS,
        edgecolors=STYLE["text_dim"],
        linewidths=0.5,
        alpha=0.85,
    )
    ax.add_collection3d(col)


def _add_scenery_2d(ax, ground_xy, road_xy, dash_segments=None):
    """Draw ground, road, and lane dashes as 2-D elements on *ax*.

    *ground_xy* and *road_xy* are Nx2 arrays of (x, y) for each quad.
    *dash_segments* is an optional list of (start_xy, end_xy) pairs for
    lane-divider stripes drawn in yellow on top of the road.
    """
    ax.add_patch(
        Polygon(
            ground_xy,
            closed=True,
            facecolor=STYLE["accent3"],
            edgecolor=STYLE["grid"],
            linewidth=0.5,
            alpha=0.30,
            zorder=2,
        )
    )
    ax.add_patch(
        Polygon(
            road_xy,
            closed=True,
            facecolor=STYLE["grid"],
            edgecolor=STYLE["text_dim"],
            linewidth=0.5,
            alpha=0.35,
            zorder=2,
        )
    )
    # Lane-divider dashes (yellow centre stripes)
    if dash_segments is not None:
        for seg in dash_segments:
            ax.plot(
                [seg[0, 0], seg[1, 0]],
                [seg[0, 1], seg[1, 1]],
                color=STYLE["warn"],
                lw=1.2,
                alpha=0.7,
                zorder=2,
            )


# ── Coordinate-system adapter ─────────────────────────────────────────────
#
# GPU convention: Y-up (x right, y up, z toward viewer).
# Matplotlib Axes3D: Z-up (x right, y depth, z up).
# _gpu_to_mpl swaps the Y and Z columns so our Y-up data renders upright.


# ── Diagram 2: World Space ────────────────────────────────────────────────


def _add_camera_icon(ax, eye_mpl, target_mpl, size=0.6):
    """Draw a small wireframe camera pyramid in mpl coordinates."""
    fwd = target_mpl - eye_mpl
    fwd = fwd / np.linalg.norm(fwd)
    # Pick a temporary up that isn't parallel to fwd
    tmp_up = np.array([0.0, 0.0, 1.0])
    if abs(np.dot(fwd, tmp_up)) > 0.99:
        tmp_up = np.array([0.0, 1.0, 0.0])
    right = np.cross(fwd, tmp_up)
    right = right / np.linalg.norm(right) * size * 0.5
    up = np.cross(right, fwd)
    up = up / np.linalg.norm(up) * size * 0.5
    tip = eye_mpl
    base_center = eye_mpl + fwd * size
    corners = [
        base_center + right + up,
        base_center - right + up,
        base_center - right - up,
        base_center + right - up,
    ]
    for c in corners:
        ax.plot(
            [tip[0], c[0]],
            [tip[1], c[1]],
            [tip[2], c[2]],
            color=STYLE["warn"],
            lw=1.2,
            alpha=0.9,
        )
    # Base rectangle
    for i in range(4):
        j = (i + 1) % 4
        ax.plot(
            [corners[i][0], corners[j][0]],
            [corners[i][1], corners[j][1]],
            [corners[i][2], corners[j][2]],
            color=STYLE["warn"],
            lw=1.2,
            alpha=0.9,
        )


def _add_ground_plane(ax, center_xz, size, color=STYLE["accent3"]):
    """Draw a flat ground-plane quad at mpl-z = 0 (GPU y = 0)."""
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    cx, cz = center_xz
    hs = size / 2.0
    # mpl axes: (x, y=depth, z=up).  Ground is flat at z = 0.
    quad = [
        [cx - hs, cz - hs, 0],
        [cx + hs, cz - hs, 0],
        [cx + hs, cz + hs, 0],
        [cx - hs, cz + hs, 0],
    ]
    col = Poly3DCollection(
        [quad],
        facecolors=[color],
        edgecolors=[STYLE["grid"]],
        alpha=0.40,
    )
    ax.add_collection3d(col)


# ── Diagram 1: Local Space ────────────────────────────────────────────────


def diagram_coord_local_space():
    """3-D house centred at the origin in its own local coordinate system."""
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

    fig = plt.figure(figsize=(7, 7), facecolor=STYLE["bg"])
    ax: Axes3D = fig.add_subplot(111, projection="3d")  # type: ignore[assignment]

    mpl_verts = _gpu_to_mpl(_HOUSE_VERTS)
    _add_house_3d(ax, mpl_verts)

    # Origin marker (mpl coords: x, z, y)
    ax.scatter(
        [0],
        [0],
        [0],  # type: ignore[arg-type]
        color=STYLE["warn"],
        s=60,
        marker="+",
        linewidths=2,
        zorder=10,
        depthshade=False,
    )
    ax.text(
        0.15,
        -0.15,
        -0.15,
        "origin",
        color=STYLE["warn"],
        fontsize=9,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    _set_equal_3d(ax, mpl_verts, pad=0.6)
    ax.view_init(elev=25, azim=-45)
    _style_3d(ax, "1. Local Space")

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_local_space.png")


# ── Diagram 2: World Space ────────────────────────────────────────────────


def diagram_coord_world_space():
    """House placed in the world scene with yard and road for context."""
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

    fig = plt.figure(figsize=(8, 7), facecolor=STYLE["bg"])
    ax: Axes3D = fig.add_subplot(111, projection="3d")  # type: ignore[assignment]

    world_verts, _, _, _, _, _ = _coord_pipeline()
    mpl_verts = _gpu_to_mpl(world_verts)
    _add_house_3d(ax, mpl_verts)

    # Yard (green ground around the house)
    hc = world_verts.mean(axis=0)  # GPU coords for centre
    _add_ground_plane(ax, (hc[0], hc[2]), 7.0)

    # Road running along X in front of the house
    _add_road(ax, (-2, 10), hc[2] + 5.0, 1.5)

    # World origin (mpl: x, z, y)
    ax.scatter(
        [0],
        [0],
        [0],  # type: ignore[arg-type]
        color=STYLE["warn"],
        s=60,
        marker="+",
        linewidths=2,
        zorder=10,
        depthshade=False,
    )
    ax.text(
        0.3,
        0.6,
        0.0,
        "world origin",
        color=STYLE["warn"],
        fontsize=8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Camera icon at _CAM_EYE, pointing at _CAM_TARGET
    eye_mpl = _gpu_to_mpl(_CAM_EYE.reshape(1, 3)).flatten()
    tgt_mpl = _gpu_to_mpl(_CAM_TARGET.reshape(1, 3)).flatten()
    _add_camera_icon(ax, eye_mpl, tgt_mpl, size=1.0)
    ax.text(
        eye_mpl[0] + 0.4,
        eye_mpl[1] + 0.4,
        eye_mpl[2] + 0.4,
        "camera",
        color=STYLE["warn"],
        fontsize=9,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Include camera position in the bounds calculation
    all_pts = np.vstack([mpl_verts, eye_mpl.reshape(1, 3)])
    _set_equal_3d(ax, all_pts, pad=3.0)
    ax.view_init(elev=45, azim=-50)
    _style_3d(ax, "2. World Space")

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_world_space.png")


# ── Diagram 3: View / Camera Space ───────────────────────────────────────


# ── Diagram 3: View / Camera Space ───────────────────────────────────────


def diagram_coord_view_space():
    """Whole scene re-expressed relative to the camera at the origin."""
    from mpl_toolkits.mplot3d import Axes3D  # noqa: F401
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    fig = plt.figure(figsize=(8, 7), facecolor=STYLE["bg"])
    ax: Axes3D = fig.add_subplot(111, projection="3d")  # type: ignore[assignment]

    # --- Transform house into view space ---
    world_verts, view_verts, _, _, _, _ = _coord_pipeline()
    mpl_house = _gpu_to_mpl(view_verts)
    _add_house_3d(ax, mpl_house)

    # --- Reconstruct the same ground plane used in the world-space diagram,
    #     then transform it into view space ---
    hc = world_verts.mean(axis=0)  # house centre in GPU world coords
    gnd_size = 7.0
    ghs = gnd_size / 2.0
    # Ground quad corners in GPU world space (y = 0 is the ground)
    ground_world = np.array(
        [
            [hc[0] - ghs, 0.0, hc[2] - ghs],
            [hc[0] + ghs, 0.0, hc[2] - ghs],
            [hc[0] + ghs, 0.0, hc[2] + ghs],
            [hc[0] - ghs, 0.0, hc[2] + ghs],
        ]
    )
    ground_view = _xf3(ground_world, _VIEW_MAT)
    ground_mpl = _gpu_to_mpl(ground_view)
    col_gnd = Poly3DCollection(
        [ground_mpl.tolist()],
        facecolors=[STYLE["accent3"]],
        edgecolors=[STYLE["grid"]],
        alpha=0.40,
    )
    ax.add_collection3d(col_gnd)

    # --- Road (same as world-space diagram, transformed to view space) ---
    road_x0, road_x1 = -2.0, 10.0
    road_z = hc[2] + 5.0  # GPU z centre of road
    road_hw = 1.5 / 2.0
    road_world = np.array(
        [
            [road_x0, 0.0, road_z - road_hw],
            [road_x1, 0.0, road_z - road_hw],
            [road_x1, 0.0, road_z + road_hw],
            [road_x0, 0.0, road_z + road_hw],
        ]
    )
    road_view = _xf3(road_world, _VIEW_MAT)
    road_mpl = _gpu_to_mpl(road_view)
    col_road = Poly3DCollection(
        [road_mpl.tolist()],
        facecolors=[STYLE["grid"]],
        edgecolors=[STYLE["text_dim"]],
        linewidths=0.5,
        alpha=0.45,
    )
    ax.add_collection3d(col_road)

    # Road centre dashes
    n_dash = 6
    dash_xs_world = np.linspace(road_x0 + 0.3, road_x1 - 0.3, n_dash)
    for xd in dash_xs_world:
        seg_world = np.array([[xd, 0.001, road_z], [xd + 0.25, 0.001, road_z]])
        seg_view = _xf3(seg_world, _VIEW_MAT)
        seg_mpl = _gpu_to_mpl(seg_view)
        ax.plot(
            seg_mpl[:, 0],
            seg_mpl[:, 1],
            seg_mpl[:, 2],
            color=STYLE["warn"],
            lw=1.2,
            alpha=0.7,
        )

    # --- World origin transformed into view space ---
    origin_view = _xf3(np.array([[0.0, 0.0, 0.0]]), _VIEW_MAT)
    origin_mpl = _gpu_to_mpl(origin_view).flatten()
    ax.scatter(
        [origin_mpl[0]],
        [origin_mpl[1]],
        [origin_mpl[2]],  # type: ignore[arg-type]
        color=STYLE["accent2"],
        s=60,
        marker="+",
        linewidths=2,
        zorder=10,
        depthshade=False,
    )
    ax.text(
        origin_mpl[0] + 0.3,
        origin_mpl[1] + 0.6,
        origin_mpl[2],
        "world origin",
        color=STYLE["accent2"],
        fontsize=8,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Camera at origin (this IS view space, so camera sits at [0,0,0]) ---
    ax.scatter(
        [0],
        [0],
        [0],  # type: ignore[arg-type]
        color=STYLE["warn"],
        s=80,
        marker="^",
        linewidths=1.5,
        zorder=10,
        depthshade=False,
    )
    ax.text(
        0.3,
        0.0,
        0.3,
        "camera\n(origin)",
        color=STYLE["warn"],
        fontsize=9,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # --- Frame the whole scene (house + ground + road + camera + origin) ---
    all_pts = np.vstack(
        [mpl_house, ground_mpl, road_mpl, origin_mpl.reshape(1, 3), [[0, 0, 0]]]
    )
    _set_equal_3d(ax, all_pts, pad=3.0)
    # Position the viewpoint roughly behind and above the camera origin,
    # looking toward the scene (camera looks down -Z → mpl -Y).
    ax.view_init(elev=25, azim=80)
    _style_3d(
        ax,
        "3. View / Camera Space",
        xlabel="x (right)",
        ylabel="z (depth)",
        zlabel="y (up)",
    )

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_view_space.png")


# ── Diagram 4: Clip Space ────────────────────────────────────────────────


# ── Diagram 4: Clip Space ────────────────────────────────────────────────


def diagram_coord_clip_space():
    """House shown from the camera's perspective in clip-space coordinates.

    Uses a 2D view (clip x vs clip y) with scenery.  The w annotation
    highlights that the homogeneous w component is no longer 1 after
    projection — the key property that distinguishes clip space from
    the earlier spaces.
    """
    fig = plt.figure(figsize=(9, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])

    _, _, clip4, _, _, _ = _coord_pipeline()
    clip_w = clip4[:, 3]

    # --- Scenery (ground + road + lane dashes) projected into clip space ---
    scenery = _scenery_through_pipeline()
    ground_clip4 = scenery["ground"]["clip4"]
    road_clip4 = scenery["road"]["clip4"]
    dash_clip4 = scenery["dashes"]["clip4"]  # (n_dash, 2, 4)
    dash_segs_clip = [np.column_stack([seg[:, 0], seg[:, 1]]) for seg in dash_clip4]
    _add_scenery_2d(
        ax,
        np.column_stack([ground_clip4[:, 0], ground_clip4[:, 1]]),
        np.column_stack([road_clip4[:, 0], road_clip4[:, 1]]),
        dash_segments=dash_segs_clip,
    )

    # --- House faces (painter's algorithm using clip z / w for depth) ---
    clip_depth = clip4[:, 2] / clip_w
    face_depths = []
    for face_idx, idx_list in enumerate(_HOUSE_FACES):
        avg_z = np.mean(clip_depth[idx_list])
        face_depths.append((avg_z, face_idx, idx_list))
    face_depths.sort(key=lambda t: t[0], reverse=True)

    for draw_order, (_, face_idx, idx_list) in enumerate(face_depths):
        fx = clip4[idx_list, 0]
        fy = clip4[idx_list, 1]
        poly = np.column_stack([fx, fy])
        ax.add_patch(
            Polygon(
                poly,
                closed=True,
                facecolor=_HOUSE_COLORS[face_idx],
                edgecolor=STYLE["text_dim"],
                linewidth=0.6,
                alpha=0.85,
                zorder=3 + draw_order,
            )
        )

    # Annotate w at the roof peak (highest clip y)
    peak_idx = int(np.argmax(clip4[:, 1]))
    w_val = clip_w[peak_idx]
    ax.annotate(
        f"w = {w_val:.1f}",
        xy=(clip4[peak_idx, 0], clip4[peak_idx, 1]),
        xytext=(clip4[peak_idx, 0] + 1.2, clip4[peak_idx, 1] + 1.0),
        color=STYLE["warn"],
        fontsize=11,
        fontweight="bold",
        arrowprops={"arrowstyle": "->", "color": STYLE["warn"], "lw": 1.2},
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Note about clip-space boundaries
    ax.text(
        0.02,
        0.02,
        "visible when  |x|, |y| \u2264 w",
        color=STYLE["text_dim"],
        fontsize=8,
        transform=ax.transAxes,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Auto-scale with some padding around the house
    all_x = np.concatenate([clip4[:, 0], ground_clip4[:, 0]])
    all_y = np.concatenate([clip4[:, 1], ground_clip4[:, 1]])
    pad = 2.0
    ax.set_xlim(all_x.min() - pad, all_x.max() + pad)
    ax.set_ylim(all_y.min() - pad, all_y.max() + pad)
    ax.set_aspect("equal")
    ax.tick_params(colors=STYLE["axis"], labelsize=8)
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    ax.grid(True, color=STYLE["grid"], linewidth=0.5, alpha=0.3)
    ax.set_xlabel("clip x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("clip y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "4. Clip Space",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_clip_space.png")


# ── Diagram 5: NDC ───────────────────────────────────────────────────────


# ── Diagram 5: NDC ───────────────────────────────────────────────────────


def diagram_coord_ndc():
    """House shown from the camera's perspective in NDC x-y space.

    Uses a 2D front-facing view so learners see what the camera sees,
    with coordinates normalised to [-1, 1].  The NDC boundary rectangle
    makes the visible region obvious and bridges naturally into the
    screen-space diagram that follows.
    """
    fig = plt.figure(figsize=(9, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])

    _, _, _, ndc_verts, _, _ = _coord_pipeline()

    # NDC boundary rectangle: x ∈ [-1, 1], y ∈ [-1, 1]
    ax.add_patch(
        Rectangle(
            (-1, -1),
            2,
            2,
            linewidth=2,
            edgecolor=STYLE["warn"],
            facecolor=STYLE["surface"],
            alpha=0.25,
            zorder=1,
        )
    )

    # --- Scenery (ground + road + lane dashes) projected into NDC ---
    scenery = _scenery_through_pipeline()
    ground_ndc = scenery["ground"]["ndc"]
    road_ndc = scenery["road"]["ndc"]
    dash_ndc = scenery["dashes"]["ndc"]  # (n_dash, 2, 3)
    dash_segs_ndc = [np.column_stack([seg[:, 0], seg[:, 1]]) for seg in dash_ndc]
    _add_scenery_2d(
        ax,
        np.column_stack([ground_ndc[:, 0], ground_ndc[:, 1]]),
        np.column_stack([road_ndc[:, 0], road_ndc[:, 1]]),
        dash_segments=dash_segs_ndc,
    )

    # Depth for painter's algorithm (NDC z — higher = farther)
    ndc_z = ndc_verts[:, 2]

    # Sort faces back-to-front so nearer faces draw on top
    face_depths = []
    for face_idx, idx_list in enumerate(_HOUSE_FACES):
        avg_z = np.mean(ndc_z[idx_list])
        face_depths.append((avg_z, face_idx, idx_list))
    face_depths.sort(key=lambda t: t[0], reverse=True)  # farthest first

    for draw_order, (_, face_idx, idx_list) in enumerate(face_depths):
        fx = ndc_verts[idx_list, 0]
        fy = ndc_verts[idx_list, 1]
        poly = np.column_stack([fx, fy])
        ax.add_patch(
            Polygon(
                poly,
                closed=True,
                facecolor=_HOUSE_COLORS[face_idx],
                edgecolor=STYLE["text_dim"],
                linewidth=0.6,
                alpha=0.85,
                zorder=3 + draw_order,
            )
        )

    # Boundary label
    ax.text(
        0.0,
        -1.0 - 0.08,
        "visible region  [-1, 1] \u00d7 [-1, 1]",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Z-range annotation
    ax.text(
        1.0,
        1.0 + 0.06,
        "z \u2208 [0, 1]  (depth)",
        color=STYLE["text_dim"],
        fontsize=8,
        ha="right",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlim(-1.35, 1.35)
    ax.set_ylim(-1.35, 1.35)
    ax.set_aspect("equal")
    ax.tick_params(colors=STYLE["axis"], labelsize=8)
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    ax.grid(True, color=STYLE["grid"], linewidth=0.5, alpha=0.3)
    ax.set_xlabel("ndc x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("ndc y", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "5. Normalized Device Coordinates",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_ndc.png")


# ── Diagram 6: Screen Space ──────────────────────────────────────────────


# ── Diagram 6: Screen Space ──────────────────────────────────────────────


def diagram_coord_screen_space():
    """House mapped to final pixel coordinates on a 1920 × 1080 screen."""
    fig = plt.figure(figsize=(9, 5.5), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    ax.set_facecolor(STYLE["bg"])

    _, _, clip4, ndc_verts, screen_x, screen_y = _coord_pipeline()

    # Per-vertex depth in view space (GPU z) for painter's algorithm
    screen_z = ndc_verts[:, 2]

    # Screen boundary (drawn before scenery so it sits behind everything)
    ax.add_patch(
        Rectangle(
            (0, 0),
            _SCR_W,
            _SCR_H,
            linewidth=2,
            edgecolor=STYLE["text_dim"],
            facecolor=STYLE["surface"],
            alpha=0.25,
            zorder=1,
        )
    )

    # --- Scenery (ground + road + lane dashes) projected into screen space ---
    scenery = _scenery_through_pipeline()
    dash_sx = scenery["dashes"]["screen_x"]  # (n_dash, 2)
    dash_sy = scenery["dashes"]["screen_y"]  # (n_dash, 2)
    dash_segs_scr = [
        np.column_stack([dash_sx[i], dash_sy[i]]) for i in range(dash_sx.shape[0])
    ]
    _add_scenery_2d(
        ax,
        np.column_stack([scenery["ground"]["screen_x"], scenery["ground"]["screen_y"]]),
        np.column_stack([scenery["road"]["screen_x"], scenery["road"]["screen_y"]]),
        dash_segments=dash_segs_scr,
    )

    # Sort faces back-to-front (painter's algorithm) so nearer faces
    # draw on top of farther ones.
    face_depths = []
    for face_idx, idx_list in enumerate(_HOUSE_FACES):
        avg_z = np.mean(screen_z[idx_list])
        face_depths.append((avg_z, face_idx, idx_list))
    face_depths.sort(key=lambda t: t[0], reverse=True)  # farthest first

    for draw_order, (_, face_idx, idx_list) in enumerate(face_depths):
        fx = screen_x[idx_list]
        fy = screen_y[idx_list]
        poly = np.column_stack([fx, fy])
        ax.add_patch(
            Polygon(
                poly,
                closed=True,
                facecolor=_HOUSE_COLORS[face_idx],
                edgecolor=STYLE["text_dim"],
                linewidth=0.6,
                alpha=0.85,
                zorder=3 + draw_order,
            )
        )

    ax.text(
        _SCR_W / 2,
        _SCR_H + 40,
        "1920 \u00d7 1080",
        color=STYLE["text_dim"],
        fontsize=10,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_xlim(-80, 2000)
    ax.set_ylim(_SCR_H + 80, -60)
    ax.set_aspect("equal")
    ax.tick_params(colors=STYLE["axis"], labelsize=8)
    for spine in ax.spines.values():
        spine.set_color(STYLE["grid"])
        spine.set_linewidth(0.5)
    ax.grid(True, color=STYLE["grid"], linewidth=0.5, alpha=0.3)
    ax.set_xlabel("pixel x", color=STYLE["axis"], fontsize=10)
    ax.set_ylabel("pixel y  (0 = top)", color=STYLE["axis"], fontsize=10)
    ax.set_title(
        "6. Screen Space",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )

    fig.tight_layout()
    save(fig, _COORD_LESSON, "coord_screen_space.png")


# ---------------------------------------------------------------------------
# math/15-bezier-curves
# ---------------------------------------------------------------------------
