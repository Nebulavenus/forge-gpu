"""Diagram functions for audio lessons."""

from .lesson_03 import (
    diagram_mixer_signal_chain,
    diagram_peak_hold_behavior,
    diagram_soft_clipping,
)
from .lesson_04 import (
    diagram_attenuation_curves,
    diagram_doppler_effect,
    diagram_spatial_setup_flow,
    diagram_stereo_pan,
)
from .lesson_05 import (
    diagram_adaptive_layers,
    diagram_crossfade_curves,
    diagram_streaming_architecture,
)

__all__: list[str] = [
    "diagram_mixer_signal_chain",
    "diagram_soft_clipping",
    "diagram_peak_hold_behavior",
    "diagram_attenuation_curves",
    "diagram_stereo_pan",
    "diagram_doppler_effect",
    "diagram_spatial_setup_flow",
    "diagram_streaming_architecture",
    "diagram_crossfade_curves",
    "diagram_adaptive_layers",
]
