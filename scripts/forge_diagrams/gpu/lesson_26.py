"""Diagrams for gpu/26."""

import matplotlib.patheffects as pe
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.patches import Circle, Rectangle

from .._common import STYLE, save, setup_axes

# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — atmosphere_layers.png
# ---------------------------------------------------------------------------


def diagram_atmosphere_layers():
    """Planet cross-section with R_ground, R_atmo, density profiles."""
    fig, ax = plt.subplots(figsize=(10, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-1.3, 1.3), ylim=(-0.3, 1.5), grid=False)

    R_GROUND = 1.0
    R_ATMO = R_GROUND + 0.4  # scaled for visual clarity

    # Draw planet surface
    theta = np.linspace(0, np.pi, 200)
    ax.fill_between(
        R_GROUND * np.cos(theta),
        R_GROUND * np.sin(theta),
        0,
        color="#2d5016",
        alpha=0.6,
    )
    ax.plot(
        R_GROUND * np.cos(theta),
        R_GROUND * np.sin(theta),
        color=STYLE["accent3"],
        lw=2,
        label="Ground (6360 km)",
    )

    # Draw atmosphere boundary
    ax.plot(
        R_ATMO * np.cos(theta),
        R_ATMO * np.sin(theta),
        color=STYLE["accent1"],
        lw=2,
        ls="--",
        label="Atmosphere top (6460 km)",
    )

    # Rayleigh layer (dense near surface, fades)
    for i in range(8):
        r = R_GROUND + i * 0.05
        alpha = 0.3 * np.exp(-i / 2.0)
        ax.plot(
            r * np.cos(theta),
            r * np.sin(theta),
            color=STYLE["accent1"],
            lw=0.5,
            alpha=alpha,
        )

    # Mie layer (concentrated near surface)
    for i in range(3):
        r = R_GROUND + i * 0.02
        alpha = 0.4 * np.exp(-i / 0.5)
        ax.plot(
            r * np.cos(theta),
            r * np.sin(theta),
            color=STYLE["accent2"],
            lw=1.0,
            alpha=alpha,
        )

    # Ozone layer (band around 25 km ~ 0.1 in our scale)
    ozone_r = R_GROUND + 0.1
    ax.plot(
        ozone_r * np.cos(theta),
        ozone_r * np.sin(theta),
        color=STYLE["accent4"],
        lw=2,
        alpha=0.8,
        label="Ozone layer (~25 km)",
    )

    # Radial markers
    ax.annotate(
        "",
        xy=(0, R_ATMO + 0.02),
        xytext=(0, 0),
        arrowprops={"arrowstyle": "<->", "color": STYLE["text_dim"], "lw": 1.5},
    )
    ax.text(
        0.05,
        (R_GROUND + R_ATMO) / 2 + 0.05,
        "100 km\natmosphere",
        color=STYLE["text_dim"],
        fontsize=9,
        ha="left",
    )

    ax.legend(
        loc="upper right",
        fontsize=9,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    ax.set_title(
        "Atmosphere Layers \u2014 Planet Cross-Section",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "atmosphere_layers.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — density_profiles.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — density_profiles.png
# ---------------------------------------------------------------------------


def diagram_density_profiles():
    """Rayleigh, Mie, ozone density vs altitude (0-100 km)."""
    fig, ax = plt.subplots(figsize=(10, 6), facecolor=STYLE["bg"])
    setup_axes(ax, grid=True, aspect=None)

    alt = np.linspace(0, 100, 500)

    # Rayleigh: exp(-h / 8)
    rho_rayleigh = np.exp(-alt / 8.0)

    # Mie: exp(-h / 1.2)
    rho_mie = np.exp(-alt / 1.2)

    # Ozone: tent centered at 25 km, width 15 km
    rho_ozone = np.maximum(0, 1.0 - np.abs(alt - 25.0) / 15.0)

    ax.plot(
        alt, rho_rayleigh, color=STYLE["accent1"], lw=2.5, label="Rayleigh (H=8 km)"
    )
    ax.plot(alt, rho_mie, color=STYLE["accent2"], lw=2.5, label="Mie (H=1.2 km)")
    ax.plot(
        alt, rho_ozone, color=STYLE["accent4"], lw=2.5, label="Ozone (tent @ 25 km)"
    )

    ax.set_xlabel("Altitude (km)", color=STYLE["text"], fontsize=11)
    ax.set_ylabel("Relative Density", color=STYLE["text"], fontsize=11)
    ax.set_xlim(0, 100)
    ax.set_ylim(0, 1.1)
    ax.legend(
        loc="upper right",
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    ax.set_title(
        "Density Profiles \u2014 Scattering Species vs Altitude",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "density_profiles.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — ray_sphere_intersection.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — ray_sphere_intersection.png
# ---------------------------------------------------------------------------


def diagram_ray_sphere_intersection():
    """View ray through atmosphere, t_near/t_far, ground hit."""
    fig, ax = plt.subplots(figsize=(10, 8), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-1.5, 1.5), ylim=(-0.2, 1.6), grid=False)

    # Planet and atmosphere circles (upper half)
    theta = np.linspace(0, np.pi, 200)
    R_G = 0.8
    R_A = 1.2

    ax.fill_between(
        R_G * np.cos(theta), R_G * np.sin(theta), 0, color="#2d5016", alpha=0.4
    )
    ax.plot(R_G * np.cos(theta), R_G * np.sin(theta), color=STYLE["accent3"], lw=2)
    ax.plot(
        R_A * np.cos(theta), R_A * np.sin(theta), color=STYLE["accent1"], lw=2, ls="--"
    )

    # Camera position (inside atmosphere)
    cam = np.array([-0.3, R_G + 0.05])
    ax.plot(*cam, "o", color=STYLE["warn"], markersize=10, zorder=10)
    ax.text(
        cam[0] - 0.15,
        cam[1] + 0.08,
        "Camera",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
    )

    # View ray direction
    ray_dir = np.array([0.7, 0.5])
    ray_dir = ray_dir / np.linalg.norm(ray_dir)

    # Find intersections with atmosphere sphere
    # Parametric: |cam + t*dir|^2 = R_A^2
    a = np.dot(ray_dir, ray_dir)
    b = 2 * np.dot(cam, ray_dir)
    c_coeff = np.dot(cam, cam) - R_A**2
    disc = b**2 - 4 * a * c_coeff
    if disc < 0:
        t_far = 2.0  # fallback: draw ray to a safe max distance
    else:
        t_near = (-b - np.sqrt(disc)) / (2 * a)  # noqa: F841
        t_far = (-b + np.sqrt(disc)) / (2 * a)

    # Draw ray
    ray_end = cam + ray_dir * t_far
    ax.annotate(
        "",
        xy=tuple(ray_end),  # type: ignore[arg-type]
        xytext=tuple(cam),  # type: ignore[arg-type]
        arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 2.5},
    )

    # Mark t_far
    ax.plot(*ray_end, "s", color=STYLE["accent2"], markersize=8, zorder=10)
    ax.text(
        ray_end[0] + 0.05,
        ray_end[1] + 0.05,
        "$t_{far}$",
        color=STYLE["accent2"],
        fontsize=12,
        fontweight="bold",
    )

    # Labels
    ax.text(0.9, R_G + 0.05, "R_ground", color=STYLE["accent3"], fontsize=10)
    ax.text(1.0, R_A + 0.05, "R_atmo", color=STYLE["accent1"], fontsize=10)

    ax.set_title(
        "Ray-Sphere Intersection \u2014 View Ray Through Atmosphere",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "ray_sphere_intersection.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — scattering_geometry.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — scattering_geometry.png
# ---------------------------------------------------------------------------


def diagram_scattering_geometry():
    """Single ray march step: sun direction, scatter angle theta, vectors."""
    fig, ax = plt.subplots(figsize=(10, 6), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.8, 3.2), ylim=(-0.2, 2.6), grid=False)

    # Sample point P — center of the geometry
    P = np.array([1.0, 0.7])
    ax.plot(*P, "o", color=STYLE["warn"], markersize=12, zorder=10)
    ax.text(
        P[0],
        P[1] - 0.22,
        "P",
        color=STYLE["warn"],
        fontsize=14,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # View ray — incoming from camera (upper-left), arriving at P.
    # Angled so the label has space above the arrow, away from θ.
    view_dir = np.array([-0.9, -0.15])
    view_dir = view_dir / np.linalg.norm(view_dir)
    view_len = 1.1
    ax.annotate(
        "",
        xy=tuple(P),  # type: ignore[arg-type]
        xytext=tuple(P - view_dir * view_len),  # type: ignore[arg-type]
        arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 2.5},
    )
    # Label above the incoming arrow, well clear of θ
    view_mid = P - view_dir * 0.65
    ax.text(
        view_mid[0] - 0.3,
        view_mid[1] + 0.2,
        "View ray",
        color=STYLE["accent1"],
        fontsize=11,
        fontweight="bold",
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Sun direction — steep upward to separate from the horizontal scatter
    sun_dir = np.array([0.35, 0.94])
    sun_dir = sun_dir / np.linalg.norm(sun_dir)
    sun_len = 1.2
    ax.annotate(
        "",
        xy=tuple(P + sun_dir * sun_len),  # type: ignore[arg-type]
        xytext=tuple(P),  # type: ignore[arg-type]
        arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 2.5},
    )
    sun_label_pos = P + sun_dir * 0.9
    ax.text(
        sun_label_pos[0] + 0.2,
        sun_label_pos[1],
        "To Sun",
        color=STYLE["accent2"],
        fontsize=11,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Scattered direction (= negative view_dir, toward the camera)
    scatter_dir = -view_dir
    scatter_len = 1.0
    ax.annotate(
        "",
        xy=tuple(P + scatter_dir * scatter_len),  # type: ignore[arg-type]
        xytext=tuple(P),  # type: ignore[arg-type]
        arrowprops={"arrowstyle": "->", "color": STYLE["accent4"], "lw": 2, "ls": "--"},
    )
    scatter_label_pos = P + scatter_dir * 0.75
    ax.text(
        scatter_label_pos[0] + 0.15,
        scatter_label_pos[1] - 0.22,
        "Scattered\nto eye",
        color=STYLE["accent4"],
        fontsize=10,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Angle θ arc — between sun direction and scattered direction
    angle_sun = np.arctan2(sun_dir[1], sun_dir[0])
    angle_scatter = np.arctan2(scatter_dir[1], scatter_dir[0])
    arc_t = np.linspace(
        min(angle_scatter, angle_sun), max(angle_scatter, angle_sun), 50
    )
    arc_r = 0.5
    ax.plot(
        P[0] + arc_r * np.cos(arc_t),
        P[1] + arc_r * np.sin(arc_t),
        color=STYLE["warn"],
        lw=2,
    )
    # θ label at the midpoint of the arc, pushed outward
    mid_angle = (angle_sun + angle_scatter) / 2
    ax.text(
        P[0] + arc_r * 1.5 * np.cos(mid_angle),
        P[1] + arc_r * 1.5 * np.sin(mid_angle),
        "\u03b8",
        color=STYLE["warn"],
        fontsize=16,
        fontweight="bold",
        ha="center",
        va="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_title(
        "Scattering Geometry \u2014 Phase Angle \u03b8",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "scattering_geometry.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — ray_march_diagram.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — ray_march_diagram.png
# ---------------------------------------------------------------------------


def diagram_ray_march():
    """Full march along view ray with sample points and transmittance."""
    fig, ax = plt.subplots(figsize=(12, 5), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 10.5), ylim=(-1.5, 3.0), grid=False)

    N = 8  # number of sample points
    x_start, x_end = 0.5, 9.5

    # Draw the view ray
    ax.annotate(
        "",
        xy=(x_end + 0.3, 0),
        xytext=(x_start - 0.3, 0),
        arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 2},
    )
    ax.text(
        x_start - 0.4,
        0.3,
        "Camera",
        color=STYLE["warn"],
        fontsize=10,
        fontweight="bold",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )
    ax.text(
        x_end + 0.1,
        0.3,
        "Atmo\nedge",
        color=STYLE["accent1"],
        fontsize=9,
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Sample points along ray
    xs = np.linspace(x_start, x_end, N)
    for i, x in enumerate(xs):
        # Transmittance decreases along the ray
        t = np.exp(-i * 0.3)
        color = STYLE["accent2"]
        alpha = 0.3 + 0.7 * t
        ax.plot(x, 0, "o", color=color, markersize=8, alpha=alpha, zorder=5)

        # Sun direction arrow at each sample
        ax.annotate(
            "",
            xy=(x, 1.2),
            xytext=(x, 0.2),
            arrowprops={
                "arrowstyle": "->",
                "color": STYLE["warn"],
                "lw": 1,
                "alpha": 0.5,
            },
        )
        ax.text(
            x, -0.5, f"$P_{{{i}}}$", color=STYLE["text_dim"], fontsize=9, ha="center"
        )

    # Step size markers
    for i in range(N - 1):
        mid = (xs[i] + xs[i + 1]) / 2
        ax.annotate(
            "",
            xy=(xs[i + 1], -0.9),
            xytext=(xs[i], -0.9),
            arrowprops={"arrowstyle": "<->", "color": STYLE["text_dim"], "lw": 1},
        )
        if i == 0:
            ax.text(
                mid, -1.2, "\u0394s", color=STYLE["text_dim"], fontsize=10, ha="center"
            )

    # Transmittance bars — draw bars first, then label above them
    for i, x in enumerate(xs):
        t = np.exp(-i * 0.3)
        ax.barh(
            1.7,
            t * 0.8,
            left=x - 0.4,
            height=0.3,
            color=STYLE["warn"],
            alpha=0.3 + 0.5 * t,
        )

    # Label placed above the bar row, away from bars
    ax.text(
        (x_start + x_end) / 2,
        2.2,
        "Sun transmittance at each sample (decreasing along ray)",
        color=STYLE["warn"],
        fontsize=9,
        ha="center",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    ax.set_title(
        "Ray March \u2014 Accumulating Inscattered Light Along the View Ray",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "ray_march_diagram.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — phase_functions.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — phase_functions.png
# ---------------------------------------------------------------------------


def diagram_phase_functions():
    """Rayleigh vs Mie (g=0.8) angular plots, 0-180 degrees."""
    fig = plt.figure(figsize=(12, 5), facecolor=STYLE["bg"])
    ax1 = fig.add_subplot(121)
    ax2 = fig.add_subplot(122, polar=True, facecolor=STYLE["bg"])

    theta = np.linspace(0, np.pi, 500)
    cos_theta = np.cos(theta)
    theta_deg = np.degrees(theta)

    # Rayleigh: (3/16pi)(1 + cos^2 theta)
    rayleigh = (3.0 / (16.0 * np.pi)) * (1.0 + cos_theta**2)

    # Henyey-Greenstein: (1-g^2) / (4pi * (1+g^2-2g*cos)^1.5)
    g = 0.8
    mie = (1 - g**2) / (4 * np.pi * (1 + g**2 - 2 * g * cos_theta) ** 1.5)

    # Cartesian plot
    setup_axes(ax1, aspect=None)
    ax1.plot(theta_deg, rayleigh, color=STYLE["accent1"], lw=2.5, label="Rayleigh")
    ax1.plot(theta_deg, mie, color=STYLE["accent2"], lw=2.5, label="Mie (g=0.8)")
    ax1.set_xlabel(
        "Scattering Angle \u03b8 (degrees)", color=STYLE["text"], fontsize=11
    )
    ax1.set_ylabel("Phase Function Value", color=STYLE["text"], fontsize=11)
    ax1.set_xlim(0, 180)
    ax1.set_yscale("log")
    ax1.set_ylim(1e-3, 1e2)
    ax1.legend(
        fontsize=10,
        facecolor=STYLE["surface"],
        edgecolor=STYLE["grid"],
        labelcolor=STYLE["text"],
    )
    ax1.set_title(
        "Phase Functions (log scale)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
    )

    # Polar plot (ax2 was already created with polar=True above)
    ax2.plot(
        theta, rayleigh / rayleigh.max(), color=STYLE["accent1"], lw=2, label="Rayleigh"
    )
    ax2.plot(theta, mie / mie.max(), color=STYLE["accent2"], lw=2, label="Mie (g=0.8)")
    ax2.set_theta_zero_location("E")  # type: ignore[attr-defined]
    ax2.set_theta_direction(-1)  # type: ignore[attr-defined]
    ax2.tick_params(colors=STYLE["axis"], labelsize=8)
    ax2.set_rlabel_position(135)  # type: ignore[attr-defined]
    ax2.grid(True, color=STYLE["grid"], alpha=0.3)
    ax2.spines["polar"].set_color(STYLE["grid"])
    ax2.set_title(
        "Polar (normalized)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
        pad=20,
    )

    fig.suptitle(
        "Phase Functions \u2014 Rayleigh vs Mie Scattering",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "phase_functions.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — sun_transmittance.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — sun_transmittance.png
# ---------------------------------------------------------------------------


def diagram_sun_transmittance():
    """Inner march from sample to sun, illustrating orange sunsets."""
    fig, ax = plt.subplots(figsize=(10, 7), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-1.5, 1.5), ylim=(-0.3, 1.6), grid=False)

    theta = np.linspace(0, np.pi, 200)
    R_G = 0.8
    R_A = 1.2

    # Planet and atmosphere
    ax.fill_between(
        R_G * np.cos(theta), R_G * np.sin(theta), 0, color="#2d5016", alpha=0.4
    )
    ax.plot(R_G * np.cos(theta), R_G * np.sin(theta), color=STYLE["accent3"], lw=2)
    ax.plot(
        R_A * np.cos(theta), R_A * np.sin(theta), color=STYLE["accent1"], lw=2, ls="--"
    )

    # Noon sample — short path through atmosphere
    P_noon = np.array([0.0, R_G + 0.05])
    sun_noon = np.array([0.0, 1.0])
    noon_end = P_noon + sun_noon * 0.35
    ax.annotate(
        "",
        xy=tuple(noon_end),  # type: ignore[arg-type]
        xytext=tuple(P_noon),  # type: ignore[arg-type]
        arrowprops={"arrowstyle": "->", "color": STYLE["accent1"], "lw": 2.5},
    )
    ax.plot(*P_noon, "o", color=STYLE["accent1"], markersize=8, zorder=10)
    ax.text(
        0.35,
        R_G + 0.42,
        "Noon: short path\nblue sky",
        color=STYLE["accent1"],
        fontsize=10,
        fontweight="bold",
        ha="left",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Sunset sample — long path through atmosphere
    P_sunset = np.array([-0.5, R_G + 0.03])
    sun_sunset = np.array([0.95, 0.3])
    sun_sunset = sun_sunset / np.linalg.norm(sun_sunset)
    sunset_end = P_sunset + sun_sunset * 1.2
    ax.annotate(
        "",
        xy=tuple(sunset_end),  # type: ignore[arg-type]
        xytext=tuple(P_sunset),  # type: ignore[arg-type]
        arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 2.5},
    )
    ax.plot(*P_sunset, "o", color=STYLE["accent2"], markersize=8, zorder=10)
    ax.text(
        -1.1,
        R_G + 0.15,
        "Sunset: long path\norange light",
        color=STYLE["accent2"],
        fontsize=10,
        fontweight="bold",
    )

    # Inner march sample dots along sunset path
    for i in range(6):
        t = 0.15 + i * 0.17
        pt = P_sunset + sun_sunset * t
        ax.plot(*pt, ".", color=STYLE["warn"], markersize=5, alpha=0.7)

    ax.set_title(
        "Sun Transmittance \u2014 Why Sunsets Are Orange",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "sun_transmittance.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — sun_limb_darkening.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — sun_limb_darkening.png
# ---------------------------------------------------------------------------


def diagram_sun_limb_darkening():
    """Brightness profile across sun disc radius."""
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5), facecolor=STYLE["bg"])

    # Left: brightness curve
    setup_axes(ax1, aspect=None)
    r = np.linspace(0, 1, 200)
    cos_limb = np.sqrt(np.maximum(0, 1 - r**2))
    brightness = 1.0 - 0.6 * (1.0 - cos_limb)

    ax1.fill_between(r, brightness, alpha=0.2, color=STYLE["accent2"])
    ax1.plot(r, brightness, color=STYLE["accent2"], lw=2.5)
    ax1.set_xlabel(
        "Normalized Radius (0=center, 1=edge)", color=STYLE["text"], fontsize=11
    )
    ax1.set_ylabel("Brightness", color=STYLE["text"], fontsize=11)
    ax1.set_xlim(0, 1)
    ax1.set_ylim(0, 1.1)
    ax1.set_title(
        "Limb Darkening Profile (u=0.6)",
        color=STYLE["text"],
        fontsize=12,
        fontweight="bold",
    )

    # Right: sun disc visualization
    setup_axes(ax2, xlim=(-1.5, 1.5), ylim=(-1.5, 1.5), grid=False)
    disc_r = np.linspace(0, 1, 100)
    disc_t = np.linspace(0, 2 * np.pi, 100)
    R, T = np.meshgrid(disc_r, disc_t)
    X = R * np.cos(T)
    Y = R * np.sin(T)
    cos_l = np.sqrt(np.maximum(0, 1 - R**2))
    Z = 1.0 - 0.6 * (1.0 - cos_l)
    ax2.pcolormesh(X, Y, Z, cmap="hot", shading="auto", vmin=0.3, vmax=1.0)
    circ = Circle((0, 0), 1.0, fill=False, ec=STYLE["accent2"], lw=2)
    ax2.add_patch(circ)
    ax2.set_title(
        "Sun Disc Appearance", color=STYLE["text"], fontsize=12, fontweight="bold"
    )
    ax2.axis("off")

    fig.suptitle(
        "Sun Limb Darkening \u2014 Center Brighter Than Edge",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        y=1.02,
    )
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "sun_limb_darkening.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — time_of_day_colors.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — time_of_day_colors.png
# ---------------------------------------------------------------------------


def diagram_time_of_day_colors():
    """Sky color strips at 8 sun elevations (twilight to noon)."""
    fig, ax = plt.subplots(figsize=(12, 4), facecolor=STYLE["bg"])
    setup_axes(ax, grid=False, aspect=None)

    # Approximate sky colors for different sun elevations
    # (artistic approximation — real values would need actual ray marching)
    elevations = [-10, -5, 0, 5, 15, 30, 60, 90]
    labels = [
        "-10\u00b0\nNight",
        "-5\u00b0\nTwilight",
        "0\u00b0\nHorizon",
        "5\u00b0\nDawn",
        "15\u00b0\nMorning",
        "30\u00b0\nDay",
        "60\u00b0\nAfternoon",
        "90\u00b0\nNoon",
    ]
    # Zenith colors at each elevation (approximate)
    colors = [
        "#0a0a1a",  # night — dark blue-black
        "#1a1040",  # twilight — deep purple
        "#4a2040",  # horizon — purple-orange
        "#c06030",  # dawn — orange
        "#4080c0",  # morning — blue
        "#5090d0",  # day — bright blue
        "#60a0e0",  # afternoon — sky blue
        "#70b0f0",  # noon — light blue
    ]

    n = len(elevations)
    bar_width = 1.0
    for i in range(n):
        ax.barh(i, bar_width, height=0.8, color=colors[i], left=0)
        ax.text(
            -0.05,
            i,
            labels[i],
            color=STYLE["text"],
            fontsize=9,
            ha="right",
            va="center",
        )
        ax.text(
            bar_width + 0.05,
            i,
            f"Sun: {elevations[i]}\u00b0",
            color=STYLE["text_dim"],
            fontsize=9,
            va="center",
        )

    ax.set_xlim(-0.6, 1.5)
    ax.set_ylim(-0.6, n - 0.4)
    ax.axis("off")
    ax.set_title(
        "Time of Day \u2014 Sky Color vs Sun Elevation",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "time_of_day_colors.png")


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — render_pipeline.png
# ---------------------------------------------------------------------------


# ---------------------------------------------------------------------------
# gpu/26-procedural-sky — render_pipeline.png
# ---------------------------------------------------------------------------


def diagram_sky_render_pipeline():
    """HDR -> bloom downsample/upsample chain -> tonemap flow."""
    fig, ax = plt.subplots(figsize=(14, 5), facecolor=STYLE["bg"])
    setup_axes(ax, xlim=(-0.5, 14), ylim=(-2, 3.5), grid=False)

    box_h = 1.2
    box_w = 2.0

    def draw_box(cx, cy, label, color, w=box_w, h=box_h):
        rect = Rectangle(
            (cx - w / 2, cy - h / 2),
            w,
            h,
            facecolor=STYLE["surface"],
            edgecolor=color,
            lw=2,
            zorder=5,
        )
        ax.add_patch(rect)
        ax.text(
            cx,
            cy,
            label,
            color=color,
            fontsize=9,
            fontweight="bold",
            ha="center",
            va="center",
            zorder=6,
        )

    # Main pipeline boxes
    draw_box(1.0, 2.0, "Sky Pass\n(ray march)", STYLE["accent1"])
    draw_box(4.0, 2.0, "HDR Target\n(R16G16B16A16)", STYLE["warn"])

    # Arrow: sky -> HDR
    ax.annotate(
        "",
        xy=(2.9, 2.0),
        xytext=(2.1, 2.0),
        arrowprops={"arrowstyle": "->", "color": STYLE["text_dim"], "lw": 2},
    )

    # Downsample chain
    ds_labels = [
        "Mip 0\n640\u00d7360",
        "Mip 1\n320\u00d7180",
        "Mip 2\n160\u00d790",
        "Mip 3\n80\u00d745",
        "Mip 4\n40\u00d722",
    ]
    for i, lbl in enumerate(ds_labels):
        x = 6.5 + i * 1.5
        draw_box(x, 0.0, lbl, STYLE["accent2"], w=1.3, h=box_h)
        if i > 0:
            ax.annotate(
                "",
                xy=(x - 0.65, 0.0),
                xytext=(x - 0.85, 0.0),
                arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 1.5},
            )

    # Arrow from HDR to downsample chain
    ax.annotate(
        "",
        xy=(5.85, 0.0),
        xytext=(5.1, 2.0),
        arrowprops={"arrowstyle": "->", "color": STYLE["accent2"], "lw": 2},
    )
    ax.text(
        5.8,
        1.2,
        "Downsample\n(5 passes)",
        color=STYLE["accent2"],
        fontsize=9,
        ha="left",
        path_effects=[pe.withStroke(linewidth=3, foreground=STYLE["bg"])],
    )

    # Upsample arrows (going back)
    for i in range(4):
        x_from = 6.5 + (4 - i) * 1.5
        x_to = 6.5 + (3 - i) * 1.5
        ax.annotate(
            "",
            xy=(x_to + 0.65, -1.2),
            xytext=(x_from - 0.65, -1.2),
            arrowprops={"arrowstyle": "->", "color": STYLE["accent4"], "lw": 1.5},
        )

    ax.text(
        9.5,
        -1.6,
        "Upsample (4 passes, additive blend)",
        color=STYLE["accent4"],
        fontsize=9,
        ha="center",
    )

    # Tonemap box
    draw_box(4.0, -1.2, "Tonemap\n(ACES + bloom)", STYLE["accent3"])
    # Arrow: HDR -> tonemap
    ax.annotate(
        "",
        xy=(4.0, -0.6),
        xytext=(4.0, 1.3),
        arrowprops={"arrowstyle": "->", "color": STYLE["text_dim"], "lw": 2},
    )
    # Arrow: bloom -> tonemap
    ax.annotate(
        "",
        xy=(5.1, -1.2),
        xytext=(5.85, -1.2),
        arrowprops={"arrowstyle": "->", "color": STYLE["accent4"], "lw": 2},
    )

    # Swapchain
    draw_box(1.0, -1.2, "Swapchain\n(sRGB)", STYLE["accent3"])
    ax.annotate(
        "",
        xy=(2.1, -1.2),
        xytext=(2.9, -1.2),
        arrowprops={"arrowstyle": "<-", "color": STYLE["accent3"], "lw": 2},
    )

    ax.set_title(
        "Render Pipeline \u2014 HDR Sky \u2192 Bloom \u2192 Tonemap",
        color=STYLE["text"],
        fontsize=14,
        fontweight="bold",
        pad=14,
    )
    ax.axis("off")
    fig.tight_layout()
    save(fig, "gpu/26-procedural-sky", "render_pipeline.png")


# ---------------------------------------------------------------------------
# gpu/27-ssao — ssao_render_pipeline.png
# ---------------------------------------------------------------------------
