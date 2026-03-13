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
    "diagram_sphere_sphere_collision",
    "diagram_impulse_response",
    "diagram_restitution_comparison",
    "diagram_collision_pipeline",
    "diagram_momentum_conservation",
    "diagram_rigid_body_state",
    "diagram_inertia_tensor",
    "diagram_inertia_shapes",
    "diagram_quaternion_rotation",
    "diagram_angular_velocity",
    "diagram_torque_force_at_point",
    "diagram_world_space_inertia",
    "diagram_integration_flowchart",
    "diagram_precession",
    "diagram_kinetic_energy",
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
from .lesson_03 import (
    diagram_collision_pipeline,
    diagram_impulse_response,
    diagram_momentum_conservation,
    diagram_restitution_comparison,
    diagram_sphere_sphere_collision,
)
from .lesson_04 import (
    diagram_angular_velocity,
    diagram_inertia_shapes,
    diagram_inertia_tensor,
    diagram_integration_flowchart,
    diagram_kinetic_energy,
    diagram_precession,
    diagram_quaternion_rotation,
    diagram_rigid_body_state,
    diagram_torque_force_at_point,
    diagram_world_space_inertia,
)
