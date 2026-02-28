"""CLI entry point for forge_diagrams package.

Invoke as:  python scripts/forge_diagrams --lesson math/01
"""

# Bootstrap: when run as `python scripts/forge_diagrams` (directory path),
# re-execute through runpy so the package machinery resolves relative imports
# correctly and without DeprecationWarning.
if __name__ == "__main__" and not __package__:
    import os
    import runpy
    import sys

    _scripts_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    if _scripts_dir not in sys.path:
        sys.path.insert(0, _scripts_dir)
    runpy.run_module("forge_diagrams", run_name="__main__", alter_sys=True)
    raise SystemExit(0)  # unreachable — run_module already calls sys.exit()

import argparse
import sys

from .engine_diagrams import (
    diagram_alpha_blending,
    diagram_barycentric_coords,
    diagram_bounding_box,
    diagram_call_stack,
    diagram_debugger_workflow,
    diagram_edge_functions,
    diagram_gpu_upload_pipeline,
    diagram_indexed_quad,
    diagram_pointer_arithmetic,
    diagram_rasterization_pipeline,
    diagram_stack_vs_heap,
    diagram_stepping_modes,
    diagram_struct_padding,
    diagram_vertex_memory_layout,
)
from .gpu_diagrams import (
    diagram_aabb_sorting,
    diagram_arvo_method,
    diagram_aspect_ratio,
    diagram_atmosphere_layers,
    diagram_blend_modes,
    diagram_blinn_phong_vectors,
    diagram_bloom_pipeline,
    diagram_brightness_threshold,
    diagram_cascade_ortho_projections,
    diagram_cascaded_shadow_maps,
    diagram_composite_modes,
    diagram_cube_face_layout,
    diagram_density_profiles,
    diagram_depth_reconstruction,
    diagram_downsample_13tap,
    diagram_exposure_effect,
    diagram_filtering_comparison,
    diagram_fog_blending,
    diagram_fog_falloff_curves,
    diagram_fog_scene_layout,
    diagram_fullscreen_triangle,
    diagram_gbuffer_mrt,
    diagram_hdr_pipeline,
    diagram_hemisphere_sampling,
    diagram_karis_averaging,
    diagram_kernel_distribution,
    diagram_ldr_clipping,
    diagram_lengyel_tangent_basis,
    diagram_linear_vs_hardware_depth,
    diagram_mip_chain_flow,
    diagram_noise_and_blur,
    diagram_normal_transformation,
    diagram_occlusion_test,
    diagram_pcf_kernel,
    diagram_peter_panning,
    diagram_phase_functions,
    diagram_range_check,
    diagram_ray_march,
    diagram_ray_sphere_intersection,
    diagram_reflection_mapping,
    diagram_scattering_geometry,
    diagram_shadow_lookup,
    diagram_sky_render_pipeline,
    diagram_specular_comparison,
    diagram_ssao_render_pipeline,
    diagram_sun_limb_darkening,
    diagram_sun_transmittance,
    diagram_tangent_space,
    diagram_tbn_construction,
    diagram_tent_filter,
    diagram_time_of_day_colors,
    diagram_tone_map_comparison,
    diagram_undersampling,
    diagram_unit_circle,
    diagram_uv_mapping,
)
from .math_diagrams import (
    diagram_arc_length,
    diagram_avalanche_matrix,
    diagram_bernstein_basis,
    diagram_bilinear_interpolation,
    diagram_camera_basis_vectors,
    diagram_cie_chromaticity,
    diagram_continuity,
    diagram_control_point_influence,
    diagram_convex_hull,
    diagram_coord_clip_space,
    diagram_coord_local_space,
    diagram_coord_ndc,
    diagram_coord_screen_space,
    diagram_coord_view_space,
    diagram_coord_world_space,
    diagram_cubic_tangent_vectors,
    diagram_de_casteljau_cubic,
    diagram_de_casteljau_quadratic,
    diagram_discrepancy_convergence,
    diagram_distribution_histogram,
    diagram_dithering_comparison,
    diagram_domain_warping,
    diagram_dot_product,
    diagram_fade_curves,
    diagram_fbm_octaves,
    diagram_frustum,
    diagram_gamma_perception,
    diagram_gradient_noise_concept,
    diagram_hash_pipeline,
    diagram_lacunarity_persistence,
    diagram_lerp_foundation,
    diagram_matrix_basis_vectors,
    diagram_mip_chain,
    diagram_noise_comparison,
    diagram_perlin_vs_simplex_grid,
    diagram_pixel_footprint,
    diagram_power_spectrum,
    diagram_quadratic_vs_cubic,
    diagram_radical_inverse,
    diagram_sampling_comparison,
    diagram_similar_triangles,
    diagram_tone_mapping_curves,
    diagram_trilinear_interpolation,
    diagram_vector_addition,
    diagram_view_transform,
    diagram_white_noise_comparison,
)
from .ui_diagrams import (
    diagram_antialiasing_comparison,
    diagram_baseline_metrics,
    diagram_button_draw_data,
    diagram_character_deletion,
    diagram_character_insertion,
    diagram_checkbox_anatomy,
    diagram_checkbox_vs_button,
    diagram_clip_rect_operation,
    diagram_collapse_toggle,
    diagram_contour_reconstruction,
    diagram_cursor_positioning,
    diagram_declare_then_draw,
    diagram_deferred_draw_pipeline,
    diagram_drag_mechanics,
    diagram_drag_outside_bounds,
    diagram_endianness,
    diagram_focus_state_machine,
    diagram_glyph_anatomy,
    diagram_hit_testing,
    diagram_horizontal_vs_vertical,
    diagram_hot_active_state_machine,
    diagram_input_routing_overlap,
    diagram_keyboard_input_flow,
    diagram_layout_cursor_model,
    diagram_layout_next_sequence,
    diagram_layout_stack_visualization,
    diagram_line_breaking,
    diagram_mouse_wheel_and_drag,
    diagram_nested_layout,
    diagram_padding_and_spacing,
    diagram_padding_bleed,
    diagram_panel_anatomy,
    diagram_panel_with_scroll_sequence,
    diagram_pen_advance,
    diagram_quad_vertex_layout,
    diagram_scanline_crossings,
    diagram_scroll_offset_model,
    diagram_scrollbar_proportions,
    diagram_shelf_packing,
    diagram_slider_anatomy,
    diagram_slider_state_colors,
    diagram_slider_value_mapping,
    diagram_text_input_anatomy,
    diagram_ttf_file_structure,
    diagram_uv_coordinates,
    diagram_uv_remap_on_clip,
    diagram_widget_interaction_comparison,
    diagram_winding_direction,
    diagram_window_anatomy,
    diagram_window_state_persistence,
    diagram_window_vs_panel_comparison,
    diagram_z_order_model,
)

# ---------------------------------------------------------------------------
# Diagram registry
# ---------------------------------------------------------------------------

DIAGRAMS = {
    "engine/04": [
        ("stack_vs_heap.png", diagram_stack_vs_heap),
        ("vertex_memory_layout.png", diagram_vertex_memory_layout),
        ("pointer_arithmetic.png", diagram_pointer_arithmetic),
        ("struct_padding.png", diagram_struct_padding),
        ("gpu_upload_pipeline.png", diagram_gpu_upload_pipeline),
    ],
    "engine/07": [
        ("debugger_workflow.png", diagram_debugger_workflow),
        ("stepping_modes.png", diagram_stepping_modes),
        ("call_stack.png", diagram_call_stack),
    ],
    "engine/10": [
        ("rasterization_pipeline.png", diagram_rasterization_pipeline),
        ("edge_functions.png", diagram_edge_functions),
        ("barycentric_coords.png", diagram_barycentric_coords),
        ("bounding_box.png", diagram_bounding_box),
        ("alpha_blending.png", diagram_alpha_blending),
        ("indexed_quad.png", diagram_indexed_quad),
    ],
    "math/01": [
        ("vector_addition.png", diagram_vector_addition),
        ("dot_product.png", diagram_dot_product),
    ],
    "math/02": [
        ("coord_local_space.png", diagram_coord_local_space),
        ("coord_world_space.png", diagram_coord_world_space),
        ("coord_view_space.png", diagram_coord_view_space),
        ("coord_clip_space.png", diagram_coord_clip_space),
        ("coord_ndc.png", diagram_coord_ndc),
        ("coord_screen_space.png", diagram_coord_screen_space),
    ],
    "math/03": [
        ("bilinear_interpolation.png", diagram_bilinear_interpolation),
    ],
    "math/04": [
        ("mip_chain.png", diagram_mip_chain),
        ("trilinear_interpolation.png", diagram_trilinear_interpolation),
    ],
    "math/05": [
        ("matrix_basis_vectors.png", diagram_matrix_basis_vectors),
    ],
    "math/06": [
        ("frustum.png", diagram_frustum),
        ("similar_triangles.png", diagram_similar_triangles),
    ],
    "math/09": [
        ("view_transform.png", diagram_view_transform),
        ("camera_basis_vectors.png", diagram_camera_basis_vectors),
    ],
    "math/10": [
        ("pixel_footprint.png", diagram_pixel_footprint),
    ],
    "math/11": [
        ("cie_chromaticity.png", diagram_cie_chromaticity),
        ("gamma_perception.png", diagram_gamma_perception),
        ("tone_mapping_curves.png", diagram_tone_mapping_curves),
    ],
    "math/12": [
        ("white_noise_comparison.png", diagram_white_noise_comparison),
        ("avalanche_matrix.png", diagram_avalanche_matrix),
        ("distribution_histogram.png", diagram_distribution_histogram),
        ("hash_pipeline.png", diagram_hash_pipeline),
    ],
    "math/13": [
        ("gradient_noise_concept.png", diagram_gradient_noise_concept),
        ("fade_curves.png", diagram_fade_curves),
        ("perlin_vs_simplex_grid.png", diagram_perlin_vs_simplex_grid),
        ("noise_comparison.png", diagram_noise_comparison),
        ("fbm_octaves.png", diagram_fbm_octaves),
        ("lacunarity_persistence.png", diagram_lacunarity_persistence),
        ("domain_warping.png", diagram_domain_warping),
    ],
    "math/14": [
        ("sampling_comparison.png", diagram_sampling_comparison),
        ("dithering_comparison.png", diagram_dithering_comparison),
        ("power_spectrum.png", diagram_power_spectrum),
        ("discrepancy_convergence.png", diagram_discrepancy_convergence),
        ("radical_inverse.png", diagram_radical_inverse),
    ],
    "math/15": [
        ("lerp_foundation.png", diagram_lerp_foundation),
        ("quadratic_vs_cubic.png", diagram_quadratic_vs_cubic),
        ("de_casteljau_quadratic.png", diagram_de_casteljau_quadratic),
        ("de_casteljau_cubic.png", diagram_de_casteljau_cubic),
        ("control_point_influence.png", diagram_control_point_influence),
        ("cubic_tangent_vectors.png", diagram_cubic_tangent_vectors),
        ("convex_hull.png", diagram_convex_hull),
        ("continuity.png", diagram_continuity),
        ("arc_length.png", diagram_arc_length),
        ("bernstein_basis.png", diagram_bernstein_basis),
    ],
    "gpu/03": [
        ("unit_circle.png", diagram_unit_circle),
        ("aspect_ratio.png", diagram_aspect_ratio),
    ],
    "gpu/04": [
        ("uv_mapping.png", diagram_uv_mapping),
        ("filtering_comparison.png", diagram_filtering_comparison),
    ],
    "gpu/10": [
        ("blinn_phong_vectors.png", diagram_blinn_phong_vectors),
        ("specular_comparison.png", diagram_specular_comparison),
        ("normal_transformation.png", diagram_normal_transformation),
    ],
    "gpu/11": [
        ("fullscreen_triangle.png", diagram_fullscreen_triangle),
    ],
    "gpu/12": [
        ("undersampling.png", diagram_undersampling),
    ],
    "gpu/14": [
        ("reflection_mapping.png", diagram_reflection_mapping),
    ],
    "gpu/15": [
        ("cascaded_shadow_maps.png", diagram_cascaded_shadow_maps),
        ("cascade_ortho_projections.png", diagram_cascade_ortho_projections),
        ("pcf_kernel.png", diagram_pcf_kernel),
        ("peter_panning.png", diagram_peter_panning),
    ],
    "gpu/16": [
        ("blend_modes.png", diagram_blend_modes),
        ("aabb_sorting.png", diagram_aabb_sorting),
        ("arvo_method.png", diagram_arvo_method),
    ],
    "gpu/17": [
        ("tangent_space.png", diagram_tangent_space),
        ("lengyel_tangent_basis.png", diagram_lengyel_tangent_basis),
    ],
    "gpu/20": [
        ("fog_falloff_curves.png", diagram_fog_falloff_curves),
        ("fog_blending.png", diagram_fog_blending),
        ("fog_scene_layout.png", diagram_fog_scene_layout),
    ],
    "gpu/21": [
        ("hdr_pipeline.png", diagram_hdr_pipeline),
        ("ldr_clipping.png", diagram_ldr_clipping),
        ("tone_map_comparison.png", diagram_tone_map_comparison),
        ("exposure_effect.png", diagram_exposure_effect),
    ],
    "gpu/22": [
        ("bloom_pipeline.png", diagram_bloom_pipeline),
        ("downsample_13tap.png", diagram_downsample_13tap),
        ("tent_filter.png", diagram_tent_filter),
        ("karis_averaging.png", diagram_karis_averaging),
        ("mip_chain_flow.png", diagram_mip_chain_flow),
        ("brightness_threshold.png", diagram_brightness_threshold),
    ],
    "gpu/23": [
        ("cube_face_layout.png", diagram_cube_face_layout),
        ("linear_vs_hardware_depth.png", diagram_linear_vs_hardware_depth),
        ("shadow_lookup.png", diagram_shadow_lookup),
    ],
    "gpu/26": [
        ("atmosphere_layers.png", diagram_atmosphere_layers),
        ("density_profiles.png", diagram_density_profiles),
        ("ray_sphere_intersection.png", diagram_ray_sphere_intersection),
        ("scattering_geometry.png", diagram_scattering_geometry),
        ("ray_march_diagram.png", diagram_ray_march),
        ("phase_functions.png", diagram_phase_functions),
        ("sun_transmittance.png", diagram_sun_transmittance),
        ("sun_limb_darkening.png", diagram_sun_limb_darkening),
        ("time_of_day_colors.png", diagram_time_of_day_colors),
        ("render_pipeline.png", diagram_sky_render_pipeline),
    ],
    "gpu/27": [
        ("ssao_render_pipeline.png", diagram_ssao_render_pipeline),
        ("gbuffer_mrt.png", diagram_gbuffer_mrt),
        ("hemisphere_sampling.png", diagram_hemisphere_sampling),
        ("kernel_distribution.png", diagram_kernel_distribution),
        ("depth_reconstruction.png", diagram_depth_reconstruction),
        ("tbn_construction.png", diagram_tbn_construction),
        ("occlusion_test.png", diagram_occlusion_test),
        ("range_check.png", diagram_range_check),
        ("noise_and_blur.png", diagram_noise_and_blur),
        ("composite_modes.png", diagram_composite_modes),
    ],
    "ui/01": [
        ("ttf_file_structure.png", diagram_ttf_file_structure),
        ("glyph_anatomy.png", diagram_glyph_anatomy),
        ("endianness.png", diagram_endianness),
    ],
    "ui/02": [
        ("contour_reconstruction.png", diagram_contour_reconstruction),
        ("scanline_crossings.png", diagram_scanline_crossings),
        ("winding_direction.png", diagram_winding_direction),
        ("antialiasing_comparison.png", diagram_antialiasing_comparison),
    ],
    "ui/03": [
        ("shelf_packing.png", diagram_shelf_packing),
        ("padding_bleed.png", diagram_padding_bleed),
        ("uv_coordinates.png", diagram_uv_coordinates),
    ],
    "ui/04": [
        ("pen_advance.png", diagram_pen_advance),
        ("baseline_metrics.png", diagram_baseline_metrics),
        ("quad_vertex_layout.png", diagram_quad_vertex_layout),
        ("line_breaking.png", diagram_line_breaking),
    ],
    "ui/05": [
        ("hot_active_state_machine.png", diagram_hot_active_state_machine),
        ("declare_then_draw.png", diagram_declare_then_draw),
        ("button_draw_data.png", diagram_button_draw_data),
        ("hit_testing.png", diagram_hit_testing),
    ],
    "ui/06": [
        ("checkbox_anatomy.png", diagram_checkbox_anatomy),
        ("checkbox_vs_button.png", diagram_checkbox_vs_button),
        ("slider_anatomy.png", diagram_slider_anatomy),
        ("slider_value_mapping.png", diagram_slider_value_mapping),
        ("drag_outside_bounds.png", diagram_drag_outside_bounds),
        ("slider_state_colors.png", diagram_slider_state_colors),
        ("widget_interaction_comparison.png", diagram_widget_interaction_comparison),
    ],
    "ui/07": [
        ("focus_state_machine.png", diagram_focus_state_machine),
        ("text_input_anatomy.png", diagram_text_input_anatomy),
        ("cursor_positioning.png", diagram_cursor_positioning),
        ("character_insertion.png", diagram_character_insertion),
        ("character_deletion.png", diagram_character_deletion),
        ("keyboard_input_flow.png", diagram_keyboard_input_flow),
    ],
    "ui/08": [
        ("layout_cursor_model.png", diagram_layout_cursor_model),
        ("horizontal_vs_vertical.png", diagram_horizontal_vs_vertical),
        ("nested_layout.png", diagram_nested_layout),
        ("layout_stack_visualization.png", diagram_layout_stack_visualization),
        ("padding_and_spacing.png", diagram_padding_and_spacing),
        ("layout_next_sequence.png", diagram_layout_next_sequence),
    ],
    "ui/09": [
        ("panel_anatomy.png", diagram_panel_anatomy),
        ("clip_rect_operation.png", diagram_clip_rect_operation),
        ("scroll_offset_model.png", diagram_scroll_offset_model),
        ("scrollbar_proportions.png", diagram_scrollbar_proportions),
        ("uv_remap_on_clip.png", diagram_uv_remap_on_clip),
        ("panel_with_scroll_sequence.png", diagram_panel_with_scroll_sequence),
        ("mouse_wheel_and_drag.png", diagram_mouse_wheel_and_drag),
    ],
    "ui/10": [
        ("window_anatomy.png", diagram_window_anatomy),
        ("z_order_model.png", diagram_z_order_model),
        ("drag_mechanics.png", diagram_drag_mechanics),
        ("deferred_draw_pipeline.png", diagram_deferred_draw_pipeline),
        ("input_routing_overlap.png", diagram_input_routing_overlap),
        ("collapse_toggle.png", diagram_collapse_toggle),
        ("window_state_persistence.png", diagram_window_state_persistence),
        ("window_vs_panel_comparison.png", diagram_window_vs_panel_comparison),
    ],
}

# Full lesson directory names for display
LESSON_NAMES = {
    "engine/04": "engine/04-pointers-and-memory",
    "engine/07": "engine/07-using-a-debugger",
    "engine/10": "engine/10-cpu-rasterization",
    "math/01": "math/01-vectors",
    "math/02": "math/02-coordinate-spaces",
    "math/03": "math/03-bilinear-interpolation",
    "math/04": "math/04-mipmaps-and-lod",
    "math/05": "math/05-matrices",
    "math/06": "math/06-projections",
    "math/09": "math/09-view-matrix",
    "math/10": "math/10-anisotropy",
    "math/11": "math/11-color-spaces",
    "math/12": "math/12-hash-functions",
    "math/13": "math/13-gradient-noise",
    "math/14": "math/14-blue-noise-sequences",
    "math/15": "math/15-bezier-curves",
    "gpu/03": "gpu/03-uniforms-and-motion",
    "gpu/04": "gpu/04-textures-and-samplers",
    "gpu/10": "gpu/10-basic-lighting",
    "gpu/11": "gpu/11-compute-shaders",
    "gpu/12": "gpu/12-shader-grid",
    "gpu/14": "gpu/14-environment-mapping",
    "gpu/15": "gpu/15-cascaded-shadow-maps",
    "gpu/16": "gpu/16-blending",
    "gpu/17": "gpu/17-normal-maps",
    "gpu/20": "gpu/20-linear-fog",
    "gpu/21": "gpu/21-hdr-tone-mapping",
    "gpu/22": "gpu/22-bloom",
    "gpu/23": "gpu/23-point-light-shadows",
    "gpu/26": "gpu/26-procedural-sky",
    "gpu/27": "gpu/27-ssao",
    "ui/01": "ui/01-ttf-parsing",
    "ui/02": "ui/02-glyph-rasterization",
    "ui/03": "ui/03-font-atlas",
    "ui/04": "ui/04-text-layout",
    "ui/05": "ui/05-immediate-mode-basics",
    "ui/06": "ui/06-checkboxes-and-sliders",
    "ui/07": "ui/07-text-input",
    "ui/08": "ui/08-layout",
    "ui/09": "ui/09-panels-and-scrolling",
    "ui/10": "ui/10-windows",
}


def match_lesson(query):
    """Match a query like 'math/01', 'math/01-vectors', or '01' to a registry key."""
    q = query.strip().rstrip("/")

    # Exact match
    if q in DIAGRAMS:
        return q

    # Match by full name
    for key, name in LESSON_NAMES.items():
        if q == name:
            return key

    # Match by number suffix (e.g. "01" matches "math/01")
    for key in DIAGRAMS:
        num = key.split("/")[1]
        if q == num:
            return key

    return None


def main():
    parser = argparse.ArgumentParser(
        description="Generate matplotlib diagrams for forge-gpu lessons."
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--lesson", help="Lesson to generate diagrams for (e.g. math/01)"
    )
    group.add_argument("--all", action="store_true", help="Generate all diagrams")
    group.add_argument("--list", action="store_true", help="List available diagrams")
    args = parser.parse_args()

    if args.list:
        print("Available diagrams:")
        for key in sorted(DIAGRAMS.keys()):
            name = LESSON_NAMES.get(key, key)
            diagrams = DIAGRAMS[key]
            print(f"\n  {name}/")
            for filename, _ in diagrams:
                print(f"    {filename}")
        total = sum(len(d) for d in DIAGRAMS.values())
        print(f"\n{total} diagrams total.")
        return 0

    if args.all:
        keys = sorted(DIAGRAMS.keys())
    else:
        key = match_lesson(args.lesson)
        if key is None:
            print(f"No diagrams registered for '{args.lesson}'.")
            print("Use --list to see available diagrams.")
            return 1
        keys = [key]

    total = 0
    for key in keys:
        name = LESSON_NAMES.get(key, key)
        print(f"{name}/")
        for _, func in DIAGRAMS[key]:
            func()
            total += 1

    print(f"\nGenerated {total} diagram(s).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
