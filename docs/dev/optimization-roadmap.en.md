# Optimization Roadmap

This document records identified optimization items that should not be modified directly without benchmarks and target-platform regression testing. Each item includes the problem, solution, and acceptance criteria.

## Rendering and Images

### GPU Layer Compositing

- **Problem**: Layer blending is currently performed mainly on the CPU, and is only uploaded or submitted to the graphics target afterward, resulting in high pixel transfer costs.
- **Solution**: First measure the blending modes, layer counts, resolutions, and frame times; move ordinary alpha, additive, and commonly used filters that can be expressed as shaders to the GPU; retain the CPU scalar path as a fallback and compare pixels mode by mode.
- **Acceptance**: The visual result matches the scalar path, memory bandwidth and frame time decrease, and low-end devices do not experience frame drops or worsening heat.

### IOSurface and SurfaceTexture Buffer Reuse

- **Problem**: The readback path may cause duplicate allocations and copies, and asynchronous request contention may occur when the size changes.
- **Solution**: Maintain the size, stride, frame serial, and context generation for each render target; use double-buffered or pooled buffers; discard stale size requests through an incrementing version number.
- **Acceptance**: No stale frame is displayed during rotation, background recovery, rapid window resizing, or render-path switching, and there are no resource leaks.

### Texture Cache

- **Problem**: The texture cache capacity, hit rate, and invalidation strategy after context recreation need to be quantified.
- **Solution**: Add statistics for cache hit rate, eviction count, and recreation count; bind GPU objects to the context generation; use an LRU based on a memory budget rather than a fixed item count.
- **Acceptance**: Invalid GPU objects are not reused after context recreation, peak memory stays within the budget, and common scene load times do not increase.

## Script and Data Processing

### TJS2 Hot Paths

- **Problem**: The interpreter has frequent overhead in property lookup, Variant conversion, and function calls.
- **Solution**: First use sampling analysis to locate hotspots, then evaluate computed-goto, property-slot caching, and reducing temporary Variants; every optimization must preserve exception, scope, and dynamic-property semantics.
- **Acceptance**: The script benchmark suite produces consistent results, long-running execution has no object leaks, and the interpreter builds with every compiler.

### PSB and Archive Decoding

- **Problem**: Resource parsing and image decoding may reread data, reconvert data, or consume excessive temporary memory.
- **Solution**: Cache parsed metadata by resource key; use streaming decoding and reusable temporary buffers; separate compressed-format detection from pixel-format conversion to avoid duplicate work caused by falling back from an incorrect format.
- **Acceptance**: The same resource is parsed only once, decoded results match the baseline files, and peak memory and load time show measurable improvement.

### Flutter FFI Frame Interface

- **Problem**: Frequent frame descriptions, pixel readback, and Dart image creation may trigger duplicate allocations and copies.
- **Solution**: Reuse FFI descriptor structures and native buffers; use the frame serial to avoid duplicate reads; retain one retry on size changes and remove copies only after buffer ownership is explicit.
- **Acceptance**: Allocation count decreases in CPU readback mode, and stride, size changes, dispose, and exception paths have no leaks.

## Fonts and Text

### Font Cache and Glyph Fallback

- **Problem**: The lifecycles and metric sources of font faces, sizes, glyphs, and prerendered bitmaps differ, which may cause misalignment or access to invalid objects.
- **Solution**: Establish explicit cache keys from font face, size, style, and character set; bind glyph metrics to the bitmap source; record separate states for missing glyphs, invalid font faces, and cache eviction; use an empty glyph only as the final fallback.
- **Acceptance**: Prerendered and runtime font baselines match, missing glyphs do not block scripts, and restart and font switching do not access released objects.

## Audio and Video

### Video Frame Scheduling

- **Problem**: Decoding, audio-video clocks, frame dropping, and scene compositing need a unified timeline.
- **Solution**: Establish a monotonic media clock; select frames by presentation timestamps; define queue cleanup rules for pause, seeking, end, and looping; update the target Layer across explicit thread boundaries.
- **Acceptance**: Video playback remains continuous without backlog, audio and video stay synchronized, pause and seeking show no stale frames, and repeated playback does not leak threads or textures.

### Audio Mixing

- **Problem**: Audio is usually not the main bottleneck, but multichannel, variable-speed, and restart scenarios require a stable buffering strategy.
- **Solution**: Record mix-queue depth and underrun count; optimize buffer reuse or SIMD only when data shows it is necessary; keep volume, looping, and stop semantics unchanged.
- **Acceptance**: There are no pops, dropouts, or residual playback after restart, and CPU usage does not increase.

## Code Structure

### Header Dependencies

- **Problem**: Large headers and repeated includes increase compilation time and expand the coupling surface.
- **Solution**: First use the compilation database to measure include dependencies, then gradually reduce them with forward declarations, private implementations, and module boundaries; handle one target at a time.
- **Acceptance**: Builds pass on all platforms, the public ABI remains unchanged, and incremental compilation time decreases.

### Platform Code Boundaries

- **Problem**: Some shared directory names carry historical platform meanings, making accidental deletion or migration more likely.
- **Solution**: Prefer documenting the actual sharing boundaries; if renaming is needed, update CMake, include, and script references first, then verify builds by platform.
- **Acceptance**: iOS, Android, macOS, and Linux validation targets all pass, and platform-guard behavior remains unchanged.

## General Acceptance Principles

1. Establish a baseline for the old implementation before implementing an individual optimization.
2. Correctness takes priority over throughput; pixel, script, audio-video, and save-data behavior must be regressable.
3. Lifecycle optimizations must cover creation, restart, exceptions, background recovery, and destruction.
4. Diagnostic probes are disabled by default; use sampling or counters for high-frequency metrics to avoid changing the timing of the code under test.
