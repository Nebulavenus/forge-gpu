"""Diagram functions for assets lessons."""

__all__ = [
    "diagram_texture_block_compression",
    "diagram_texture_format_comparison",
    "diagram_mesh_processing_pipeline",
    "diagram_lod_simplification",
    "diagram_parametric_sphere",
    "diagram_seam_duplication",
    "diagram_smooth_vs_flat_normals",
    "diagram_struct_of_arrays",
]

from .lesson_02 import (
    diagram_texture_block_compression,
    diagram_texture_format_comparison,
)
from .lesson_03 import diagram_lod_simplification, diagram_mesh_processing_pipeline
from .lesson_04 import (
    diagram_parametric_sphere,
    diagram_seam_duplication,
    diagram_smooth_vs_flat_normals,
    diagram_struct_of_arrays,
)
