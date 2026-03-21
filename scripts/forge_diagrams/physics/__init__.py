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
    "diagram_force_at_point",
    "diagram_force_accumulator_lifecycle",
    "diagram_drag_terminal_velocity",
    "diagram_gyroscopic_stability",
    "diagram_friction_decomposition",
    "diagram_coulomb_friction_cone",
    "diagram_contact_normal_tangent",
    "diagram_box_plane_contacts",
    "diagram_iterative_solver_convergence",
    "diagram_impulse_resolution",
    "diagram_sap_algorithm",
    "diagram_axis_selection",
    "diagram_temporal_coherence",
    "diagram_clipping_pipeline",
    "diagram_reference_incident_faces",
    "diagram_contact_reduction",
    "diagram_manifold_cache",
    "diagram_accumulated_vs_per_iteration",
    "diagram_friction_cone",
    "diagram_solver_pipeline",
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
from .lesson_05 import (
    diagram_drag_terminal_velocity,
    diagram_force_accumulator_lifecycle,
    diagram_force_at_point,
    diagram_friction_decomposition,
    diagram_gyroscopic_stability,
)
from .lesson_06 import (
    diagram_box_plane_contacts,
    diagram_contact_normal_tangent,
    diagram_coulomb_friction_cone,
    diagram_impulse_resolution,
    diagram_iterative_solver_convergence,
)
from .lesson_08 import (
    diagram_axis_selection,
    diagram_sap_algorithm,
    diagram_temporal_coherence,
)
from .lesson_11 import (
    diagram_clipping_pipeline,
    diagram_contact_reduction,
    diagram_manifold_cache,
    diagram_reference_incident_faces,
)
from .lesson_12 import (
    diagram_accumulated_vs_per_iteration,
    diagram_friction_cone,
    diagram_solver_pipeline,
)
