# Performance and Refactoring

Detailed implementation plans are consolidated in [optimization-roadmap.md](optimization-roadmap.md). This document retains the evaluation principles to avoid duplicating the todo and architecture documents.

## Priorities

| Priority | Direction | Main Benefit | Risk |
|---|---|---|---|
| High | Render targets and GPU resource lifecycles | Reduce black screens, stale textures, and context recreation issues | High |
| High | Video frame submission and target Layer compositing | Complete the video display pipeline | High |
| Medium | Flutter FFI buffer reuse | Reduce allocations and copies in the readback path | Medium |
| Medium | Font cache and prerendered metrics | Improve text layout and font-switching stability | Medium |
| Medium | TJS2 hot paths | Reduce CPU usage in script-intensive scenes | High |
| Low | Restore SIMD PS blending | Improve throughput for specific blending modes | High, requires per-pixel verification |
| Low | Reduce header dependencies | Shorten incremental compilation time | Medium |

## Evaluation Rules

- Measure first, then optimize; do not change hotspot code without baseline data.
- Any pixel, script, audio-video, save-data, or lifecycle optimization must retain the old path as a regression baseline.
- Make local, reversible changes first, then consider cross-module refactoring.
- Rendering and asynchronous code must cover creation, restart, background recovery, size changes, exceptions, and destruction paths.
- Use frame time, peak memory, power consumption, and compatibility results on target platforms as the final criteria; do not substitute a single host-machine result.
