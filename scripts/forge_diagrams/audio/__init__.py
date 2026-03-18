"""Diagram functions for audio lessons."""

from .lesson_03 import (
    diagram_mixer_signal_chain,
    diagram_peak_hold_behavior,
    diagram_soft_clipping,
)

__all__: list[str] = [
    "diagram_mixer_signal_chain",
    "diagram_soft_clipping",
    "diagram_peak_hold_behavior",
]
