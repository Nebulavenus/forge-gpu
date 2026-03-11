"""Diagram functions for engine lessons."""

__all__ = [
    "diagram_stack_vs_heap",
    "diagram_vertex_memory_layout",
    "diagram_pointer_arithmetic",
    "diagram_struct_padding",
    "diagram_gpu_upload_pipeline",
    "diagram_debugger_workflow",
    "diagram_stepping_modes",
    "diagram_call_stack",
    "diagram_rasterization_pipeline",
    "diagram_edge_functions",
    "diagram_barycentric_coords",
    "diagram_bounding_box",
    "diagram_alpha_blending",
    "diagram_indexed_quad",
    "diagram_three_areas",
    "diagram_worktree_architecture",
    "diagram_submodule_vs_fetchcontent",
    "diagram_bump_allocation",
    "diagram_alignment_padding",
    "diagram_block_chain_growth",
]

from .lesson_04 import (
    diagram_gpu_upload_pipeline,
    diagram_pointer_arithmetic,
    diagram_stack_vs_heap,
    diagram_struct_padding,
    diagram_vertex_memory_layout,
)
from .lesson_07 import (
    diagram_call_stack,
    diagram_debugger_workflow,
    diagram_stepping_modes,
)
from .lesson_10 import (
    diagram_alpha_blending,
    diagram_barycentric_coords,
    diagram_bounding_box,
    diagram_edge_functions,
    diagram_indexed_quad,
    diagram_rasterization_pipeline,
)
from .lesson_11 import (
    diagram_submodule_vs_fetchcontent,
    diagram_three_areas,
    diagram_worktree_architecture,
)
from .lesson_12 import (
    diagram_alignment_padding,
    diagram_block_chain_growth,
    diagram_bump_allocation,
)
