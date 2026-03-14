"""Diagram functions for math lessons."""

__all__ = [
    "diagram_vector_addition",
    "diagram_dot_product",
    "diagram_coord_local_space",
    "diagram_coord_world_space",
    "diagram_coord_view_space",
    "diagram_coord_clip_space",
    "diagram_coord_ndc",
    "diagram_coord_screen_space",
    "diagram_bilinear_interpolation",
    "diagram_aliasing_problem",
    "diagram_lod_walkthrough",
    "diagram_mip_chain",
    "diagram_trilinear_interpolation",
    "diagram_matrix_basis_vectors",
    "diagram_frustum",
    "diagram_similar_triangles",
    "diagram_view_transform",
    "diagram_camera_basis_vectors",
    "diagram_pixel_footprint",
    "diagram_cie_chromaticity",
    "diagram_gamma_perception",
    "diagram_tone_mapping_curves",
    "diagram_white_noise_comparison",
    "diagram_avalanche_matrix",
    "diagram_distribution_histogram",
    "diagram_hash_pipeline",
    "diagram_gradient_noise_concept",
    "diagram_fade_curves",
    "diagram_perlin_vs_simplex_grid",
    "diagram_noise_comparison",
    "diagram_fbm_octaves",
    "diagram_lacunarity_persistence",
    "diagram_domain_warping",
    "diagram_sampling_comparison",
    "diagram_dithering_comparison",
    "diagram_power_spectrum",
    "diagram_discrepancy_convergence",
    "diagram_radical_inverse",
    "diagram_lerp_foundation",
    "diagram_quadratic_vs_cubic",
    "diagram_de_casteljau_quadratic",
    "diagram_de_casteljau_cubic",
    "diagram_control_point_influence",
    "diagram_cubic_tangent_vectors",
    "diagram_convex_hull",
    "diagram_continuity",
    "diagram_arc_length",
    "diagram_bernstein_basis",
    "diagram_histogram_comparison",
    "diagram_pdf_curves",
    "diagram_integration_area",
    "diagram_histogram_to_density",
    "diagram_sdf_distance_field",
    "diagram_csg_operations",
    "diagram_smooth_blend",
    "diagram_gradient_field",
    "diagram_marching_squares",
]

from .lesson_01 import diagram_dot_product, diagram_vector_addition
from .lesson_02 import (
    diagram_coord_clip_space,
    diagram_coord_local_space,
    diagram_coord_ndc,
    diagram_coord_screen_space,
    diagram_coord_view_space,
    diagram_coord_world_space,
)
from .lesson_03 import diagram_bilinear_interpolation
from .lesson_04 import (
    diagram_aliasing_problem,
    diagram_lod_walkthrough,
    diagram_mip_chain,
    diagram_trilinear_interpolation,
)
from .lesson_05 import diagram_matrix_basis_vectors
from .lesson_06 import diagram_frustum, diagram_similar_triangles
from .lesson_09 import diagram_camera_basis_vectors, diagram_view_transform
from .lesson_10 import diagram_pixel_footprint
from .lesson_11 import (
    diagram_cie_chromaticity,
    diagram_gamma_perception,
    diagram_tone_mapping_curves,
)
from .lesson_12 import (
    diagram_avalanche_matrix,
    diagram_distribution_histogram,
    diagram_hash_pipeline,
    diagram_white_noise_comparison,
)
from .lesson_13 import (
    diagram_domain_warping,
    diagram_fade_curves,
    diagram_fbm_octaves,
    diagram_gradient_noise_concept,
    diagram_lacunarity_persistence,
    diagram_noise_comparison,
    diagram_perlin_vs_simplex_grid,
)
from .lesson_14 import (
    diagram_discrepancy_convergence,
    diagram_dithering_comparison,
    diagram_power_spectrum,
    diagram_radical_inverse,
    diagram_sampling_comparison,
)
from .lesson_15 import (
    diagram_arc_length,
    diagram_bernstein_basis,
    diagram_continuity,
    diagram_control_point_influence,
    diagram_convex_hull,
    diagram_cubic_tangent_vectors,
    diagram_de_casteljau_cubic,
    diagram_de_casteljau_quadratic,
    diagram_lerp_foundation,
    diagram_quadratic_vs_cubic,
)
from .lesson_16 import (
    diagram_histogram_comparison,
    diagram_histogram_to_density,
    diagram_integration_area,
    diagram_pdf_curves,
)
from .lesson_17 import (
    diagram_csg_operations,
    diagram_gradient_field,
    diagram_marching_squares,
    diagram_sdf_distance_field,
    diagram_smooth_blend,
)
