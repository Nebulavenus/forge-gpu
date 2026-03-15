"""Diagrams for math/18 — Scalar Field Gradients."""

import matplotlib.pyplot as plt
import numpy as np
from mpl_toolkits.mplot3d import Axes3D  # noqa: F401

from .._common import STYLE, save, setup_axes

_LESSON = "math/18-scalar-field-gradients"


# ---------------------------------------------------------------------------
# Diagram: Gradient quiver plot on contour field
# ---------------------------------------------------------------------------


def diagram_gradient_quiver():
    """Gradient vectors overlaid on contour plot of sin(x)*cos(y)."""
    res = 300
    extent = 3.0
    x = np.linspace(-extent, extent, res)
    y = np.linspace(-extent, extent, res)
    X, Y = np.meshgrid(x, y)
    Z = np.sin(X) * np.cos(Y)

    # Analytic gradient: (cos(x)*cos(y), -sin(x)*sin(y))
    qx = np.linspace(-extent, extent, 12)
    qy = np.linspace(-extent, extent, 12)
    QX, QY = np.meshgrid(qx, qy)
    GX = np.cos(QX) * np.cos(QY)
    GY = -np.sin(QX) * np.sin(QY)

    fig = plt.figure(figsize=(7, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-extent, extent), ylim=(-extent, extent), grid=False)

    # Filled contours
    levels = np.linspace(-1.0, 1.0, 40)
    cf = ax.contourf(X, Y, Z, levels=levels, cmap="RdBu_r", extend="both")

    # Contour lines
    ax.contour(
        X,
        Y,
        Z,
        levels=np.linspace(-0.8, 0.8, 9),
        colors=[STYLE["text_dim"]],
        linewidths=0.5,
        alpha=0.6,
    )

    # Gradient arrows
    ax.quiver(
        QX,
        QY,
        GX,
        GY,
        color=STYLE["accent1"],
        scale=20,
        width=0.004,
        headwidth=4,
        alpha=0.9,
    )

    ax.set_title(
        r"$\nabla f$ on $f(x,y) = \sin x \cdot \cos y$",
        color=STYLE["text"],
        fontsize=13,
        pad=10,
    )
    ax.set_xlabel("x", color=STYLE["axis"])
    ax.set_ylabel("y", color=STYLE["axis"])

    cbar = fig.colorbar(cf, ax=ax, shrink=0.8)
    cbar.ax.tick_params(colors=STYLE["axis"])
    cbar.set_label("f(x, y)", color=STYLE["axis"])

    fig.tight_layout()
    save(fig, _LESSON, "gradient_quiver.png")


# ---------------------------------------------------------------------------
# Diagram: Height map surface with normal vectors
# ---------------------------------------------------------------------------


def diagram_heightmap_normals():
    """3D surface plot with normal vectors at grid points."""
    size = 20
    extent = 2.0
    x = np.linspace(-extent, extent, size)
    z = np.linspace(-extent, extent, size)
    X, Z = np.meshgrid(x, z)
    H = 2.0 * np.exp(-(X**2 + Z**2) / 2.0)

    fig = plt.figure(figsize=(8, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111, projection="3d", facecolor=STYLE["bg"])

    # Surface
    ax.plot_surface(
        X,
        Z,
        H,
        cmap="terrain",
        alpha=0.7,
        edgecolor="none",
        antialiased=True,
    )

    # Normal vectors at sparse grid points
    spacing = 2.0 * extent / (size - 1)
    step = 4
    for i in range(0, size, step):
        for j in range(0, size, step):
            px, pz = X[i, j], Z[i, j]
            ph = H[i, j]
            # Match forge_heightmap_normal(): central differences in interior,
            # forward/backward differences on the boundary.
            jl = max(j - 1, 0)
            jr = min(j + 1, size - 1)
            il = max(i - 1, 0)
            ir = min(i + 1, size - 1)
            dx = (H[i, jr] - H[i, jl]) / ((jr - jl) * spacing) if jr != jl else 0.0
            dz = (H[ir, j] - H[il, j]) / ((ir - il) * spacing) if ir != il else 0.0
            n = np.array([-dx, 1.0, -dz])
            n = n / np.linalg.norm(n)
            scale = 0.4
            ax.quiver(
                px,
                pz,
                ph,
                n[0] * scale,
                n[2] * scale,
                n[1] * scale,
                color=STYLE["accent2"],
                linewidth=1.5,
                arrow_length_ratio=0.3,
            )

    ax.set_xlabel("x", color=STYLE["axis"])
    ax.set_ylabel("z", color=STYLE["axis"])
    ax.set_zlabel("height", color=STYLE["axis"])
    ax.set_title(
        "Height Map Surface Normals",
        color=STYLE["text"],
        fontsize=13,
        pad=10,
    )
    ax.tick_params(colors=STYLE["axis"])
    ax.xaxis.pane.fill = False  # type: ignore[attr-defined]
    ax.yaxis.pane.fill = False  # type: ignore[attr-defined]
    ax.zaxis.pane.fill = False  # type: ignore[attr-defined]
    ax.xaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax.yaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]
    ax.zaxis.pane.set_edgecolor(STYLE["grid"])  # type: ignore[attr-defined]

    fig.tight_layout()
    save(fig, _LESSON, "heightmap_normals.png")


# ---------------------------------------------------------------------------
# Diagram: Gradient descent path on contour plot
# ---------------------------------------------------------------------------


def diagram_gradient_descent_path():
    """Contour plot with gradient descent path showing convergence."""
    res = 300
    x = np.linspace(-2, 12, res)
    y = np.linspace(-4, 12, res)
    X, Y = np.meshgrid(x, y)
    Z = (X - 2) ** 2 + (Y + 1) ** 2

    # Run gradient descent — matches C constants GD_START_X/Y, GD_STEP, GD_ITERATIONS
    px, py = 10.0, 10.0
    step = 0.1
    gd_iterations = 30
    path_x, path_y = [px], [py]
    for _ in range(gd_iterations):
        gx = 2.0 * (px - 2.0)
        gy = 2.0 * (py + 1.0)
        px -= step * gx
        py -= step * gy
        path_x.append(px)
        path_y.append(py)

    fig = plt.figure(figsize=(7, 6), facecolor=STYLE["bg"])
    ax = fig.add_subplot(111)
    setup_axes(ax, xlim=(-2, 12), ylim=(-4, 12), grid=False)

    # Filled contours
    levels = np.concatenate([np.arange(0, 20, 2), np.arange(20, 160, 20)])
    cf = ax.contourf(X, Y, Z, levels=levels, cmap="viridis", extend="max")

    # Contour lines
    ax.contour(
        X,
        Y,
        Z,
        levels=levels,
        colors=[STYLE["text_dim"]],
        linewidths=0.4,
        alpha=0.5,
    )

    # Descent path
    ax.plot(
        path_x,
        path_y,
        color=STYLE["accent2"],
        linewidth=2,
        zorder=5,
    )
    ax.plot(
        path_x,
        path_y,
        "o",
        color=STYLE["accent2"],
        markersize=3,
        zorder=6,
    )

    # Start and end markers
    ax.plot(
        path_x[0],
        path_y[0],
        "o",
        color=STYLE["warn"],
        markersize=10,
        zorder=7,
        label="Start (10, 10)",
    )
    ax.plot(
        2,
        -1,
        "*",
        color=STYLE["accent3"],
        markersize=15,
        zorder=7,
        label="Minimum (2, -1)",
    )

    ax.set_title(
        r"Gradient Descent on $(x-2)^2 + (y+1)^2$",
        color=STYLE["text"],
        fontsize=13,
        pad=10,
    )
    ax.set_xlabel("x", color=STYLE["axis"])
    ax.set_ylabel("y", color=STYLE["axis"])
    ax.legend(
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
        fontsize=9,
    )

    cbar = fig.colorbar(cf, ax=ax, shrink=0.8)
    cbar.ax.tick_params(colors=STYLE["axis"])
    cbar.set_label("f(x, y)", color=STYLE["axis"])

    fig.tight_layout()
    save(fig, _LESSON, "gradient_descent_path.png")


# ---------------------------------------------------------------------------
# Diagram: Laplacian classification
# ---------------------------------------------------------------------------


def diagram_laplacian_classification():
    """2x2 grid showing Laplacian classification: min, max, saddle, general."""
    res = 100
    extent = 2.0
    x = np.linspace(-extent, extent, res)
    y = np.linspace(-extent, extent, res)
    X, Y = np.meshgrid(x, y)

    fields = [
        (r"$f = x^2 + y^2$ (minimum)", X**2 + Y**2, r"$\nabla^2 f = 4 > 0$"),
        (
            r"$f = -(x^2 + y^2)$ (maximum)",
            -(X**2 + Y**2),
            r"$\nabla^2 f = -4 < 0$",
        ),
        (r"$f = x^2 - y^2$ (saddle)", X**2 - Y**2, r"$\nabla^2 f = 0$"),
        (
            r"$f = \cos x \cdot \cos y$",
            np.cos(X) * np.cos(Y),
            r"$\nabla^2 f = -2f$",
        ),
    ]

    fig, axes = plt.subplots(2, 2, figsize=(10, 9), facecolor=STYLE["bg"])

    for ax, (title, Z, lap_text) in zip(axes.flat, fields, strict=True):
        setup_axes(
            ax,
            xlim=(-extent, extent),
            ylim=(-extent, extent),
            grid=False,
        )
        ax.contourf(X, Y, Z, levels=30, cmap="RdBu_r")
        ax.contour(
            X,
            Y,
            Z,
            levels=10,
            colors=[STYLE["text_dim"]],
            linewidths=0.5,
            alpha=0.5,
        )
        ax.set_title(title, color=STYLE["text"], fontsize=11, pad=6)
        ax.text(
            0.05,
            0.05,
            lap_text,
            transform=ax.transAxes,
            color=STYLE["warn"],
            fontsize=10,
            bbox={
                "facecolor": STYLE["bg"],
                "alpha": 0.8,
                "edgecolor": STYLE["grid"],
            },
        )

        # Mark origin
        ax.plot(0, 0, "+", color=STYLE["accent1"], markersize=10, markeredgewidth=2)

    fig.suptitle(
        "Laplacian Classification of Critical Points",
        color=STYLE["text"],
        fontsize=14,
        y=0.98,
    )
    fig.tight_layout(rect=[0, 0, 1, 0.95])
    save(fig, _LESSON, "laplacian_classification.png")
