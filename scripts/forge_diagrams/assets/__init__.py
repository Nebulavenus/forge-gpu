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
    "diagram_bundle_layout",
    "diagram_random_vs_sequential",
    "diagram_dependency_graph",
    "diagram_material_data_flow",
    "diagram_fmesh_v2_layout",
    "diagram_scene_hierarchy",
    "diagram_morph_pipeline",
    "diagram_morph_binary_layout",
    "diagram_settings_merge",
    "diagram_settings_data_flow",
    "diagram_guillotine_packing",
    "diagram_atlas_uv_transform",
    "diagram_mipmap_bleeding",
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
from .lesson_05 import (
    diagram_bundle_layout,
    diagram_dependency_graph,
    diagram_random_vs_sequential,
)
from .lesson_07 import diagram_fmesh_v2_layout, diagram_material_data_flow
from .lesson_09 import diagram_scene_hierarchy
from .lesson_13 import diagram_morph_binary_layout, diagram_morph_pipeline
from .lesson_16 import diagram_settings_data_flow, diagram_settings_merge
from .lesson_17 import (
    diagram_atlas_uv_transform,
    diagram_guillotine_packing,
    diagram_mipmap_bleeding,
)
