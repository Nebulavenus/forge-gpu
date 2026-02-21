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

from .gpu_diagrams import (
    diagram_aabb_sorting,
    diagram_arvo_method,
    diagram_blend_modes,
    diagram_blinn_phong_vectors,
    diagram_cascade_ortho_projections,
    diagram_cascaded_shadow_maps,
    diagram_filtering_comparison,
    diagram_fullscreen_triangle,
    diagram_lengyel_tangent_basis,
    diagram_normal_transformation,
    diagram_pcf_kernel,
    diagram_peter_panning,
    diagram_reflection_mapping,
    diagram_specular_comparison,
    diagram_tangent_space,
    diagram_undersampling,
    diagram_uv_mapping,
)
from .math_diagrams import (
    diagram_avalanche_matrix,
    diagram_bilinear_interpolation,
    diagram_camera_basis_vectors,
    diagram_cie_chromaticity,
    diagram_distribution_histogram,
    diagram_domain_warping,
    diagram_dot_product,
    diagram_fade_curves,
    diagram_fbm_octaves,
    diagram_frustum,
    diagram_gamma_perception,
    diagram_gradient_noise_concept,
    diagram_hash_pipeline,
    diagram_lacunarity_persistence,
    diagram_matrix_basis_vectors,
    diagram_mip_chain,
    diagram_noise_comparison,
    diagram_perlin_vs_simplex_grid,
    diagram_pixel_footprint,
    diagram_similar_triangles,
    diagram_tone_mapping_curves,
    diagram_trilinear_interpolation,
    diagram_vector_addition,
    diagram_view_transform,
    diagram_white_noise_comparison,
)

# ---------------------------------------------------------------------------
# Diagram registry
# ---------------------------------------------------------------------------

DIAGRAMS = {
    "math/01": [
        ("vector_addition.png", diagram_vector_addition),
        ("dot_product.png", diagram_dot_product),
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
}

# Full lesson directory names for display
LESSON_NAMES = {
    "math/01": "math/01-vectors",
    "math/03": "math/03-bilinear-interpolation",
    "math/04": "math/04-mipmaps-and-lod",
    "math/05": "math/05-matrices",
    "math/06": "math/06-projections",
    "math/09": "math/09-view-matrix",
    "math/10": "math/10-anisotropy",
    "math/11": "math/11-color-spaces",
    "math/12": "math/12-hash-functions",
    "math/13": "math/13-gradient-noise",
    "gpu/04": "gpu/04-textures-and-samplers",
    "gpu/10": "gpu/10-basic-lighting",
    "gpu/11": "gpu/11-compute-shaders",
    "gpu/12": "gpu/12-shader-grid",
    "gpu/14": "gpu/14-environment-mapping",
    "gpu/15": "gpu/15-cascaded-shadow-maps",
    "gpu/16": "gpu/16-blending",
    "gpu/17": "gpu/17-normal-maps",
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
