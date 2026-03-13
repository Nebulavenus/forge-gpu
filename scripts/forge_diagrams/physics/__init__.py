"""Diagram functions for physics lessons."""

__all__ = [
    "diagram_force_accumulator_pattern",
    "diagram_sphere_plane_collision",
    "diagram_symplectic_vs_explicit_euler",
    "diagram_hookes_law",
    "diagram_damped_spring_comparison",
    "diagram_spring_damping_components",
    "diagram_distance_constraint_projection",
    "diagram_constraint_mass_weighting",
    "diagram_gauss_seidel_convergence",
    "diagram_cloth_topology",
    "diagram_spring_vs_constraint",
]

from .lesson_01 import (
    diagram_force_accumulator_pattern,
    diagram_sphere_plane_collision,
    diagram_symplectic_vs_explicit_euler,
)
from .lesson_02 import (
    diagram_cloth_topology,
    diagram_constraint_mass_weighting,
    diagram_damped_spring_comparison,
    diagram_distance_constraint_projection,
    diagram_gauss_seidel_convergence,
    diagram_hookes_law,
    diagram_spring_damping_components,
    diagram_spring_vs_constraint,
)
