# Todo and Known Issues

This document records only items that still require handling or verification. Historical investigations, personal test logs, internal commit IDs, and temporary analysis files are not stored here.

## High Priority

### Z-Compatible Rendering Path

- **Status**: Some Z games depend on a dedicated main drawing device or special compositing path, and the current mobile implementation does not guarantee correct display for all main DrawBuffer scenarios.
- **Solution**: Map the target bindings among the main window, LayerManager, DrawDevice, and RenderManager; record the lifecycle and context generation for each drawing target; verify target textures, FBOs, and update counts before and after GPU compositing.
- **Acceptance**: Every startup, continuous redraw, transition, and return operation produces valid frames; the source texture is not solid black; opening different games consecutively does not contaminate drawing targets.

### Video Frame Compositing

- **Status**: The FFmpeg decoding pipeline is available, but the complete path for submitting video frames to scene textures still needs to be finalized.
- **Solution**: Implement a unified frame clock, frame-dropping strategy, pixel-format conversion, and target Layer updates; clarify ownership among the video layer, ordinary Layers, and the Flutter texture.
- **Acceptance**: The first video frame, continuous playback, pause, seeking, end, and repeated playback all work correctly; there are no thread or texture leaks.

### Prerendered Fonts and Option Boxes

- **Status**: The baseline, drawing area, and clipping rules for prerendered glyphs and runtime fonts may still be inconsistent.
- **Solution**: Record the glyph origin, baseline, drawing rectangle, and target Layer coordinates separately; bind prerendered font metrics to glyph bitmaps and avoid deriving layout from the ascent of another font; add regional regression coverage for option boxes.
- **Acceptance**: Option text remains inside the box at different font sizes, full-width characters, mixed scripts, and scale factors; dialogue text does not regress.

## Medium Priority

### Layer Effects API

- **Status**: Some extended effect interfaces have only registration names or compatibility stubs and lack complete pixel implementations.
- **Solution**: First define the input and output semantics for alpha, brightness, mosaic, and masks, then reuse existing CPU/GPU pixel operations; provide a scalar baseline and boundary cases for each effect.
- **Acceptance**: Method existence, transparency, edge pixels, empty Layers, and repeated calls remain stable.

### Motion / Emote Player

- **Status**: Basic PSB resources and some timelines are supported; complex meshes, particles, child motions, and physics behavior are still incomplete.
- **Solution**: Split timeline evaluation, parent-child transforms, mesh evaluation, and render submission into independent stages; use the same matrix convention for all coordinates and an explicit clock for child players; retain the static scalar path as a regression baseline.
- **Acceptance**: Node visibility, keyframe interpolation, rotation pivots, clipping, and the final non-looping frame remain stable across continuous frames.

### Font Error Fallback

- **Status**: When a glyph cannot be obtained, the current glyph should be skipped and drawing should continue; lifecycle checks for invalid font objects, font switching, and restart scenarios still need to be added.
- **Solution**: Maintain validity state and generation for font objects; pair initialization, face creation, caching, and release in the font library lifecycle; use an empty glyph only as the final fallback so font-loading errors are not hidden.
- **Acceptance**: Missing glyphs do not block scripts; font switching and engine restart do not access invalid objects; logs distinguish missing glyphs from invalid font objects.

## Low Priority

### SIMD Blending Formulas

- **Status**: Some PS blending currently uses the scalar implementation to ensure consistency with scalar results.
- **Solution**: Treat the scalar function as the specification and reproduce packing, truncation, and cross-channel arithmetic with per-pixel `u32` lanes; add byte-by-byte comparisons using random pixels, boundary values, and alpha combinations.
- **Acceptance**: Enable SIMD dispatch only after SIMD and scalar results match pixel by pixel; automatically fall back to scalar when they differ.

### Flutter Frame Buffers and Size Changes

- **Status**: The CPU readback and texture paths contain duplicate allocations, copies, and asynchronous size requests.
- **Solution**: Introduce size-request version numbers and discard stale requests; reuse descriptor structures and pixel buffers; retain stride, frame serial, and one retry mechanism.
- **Acceptance**: Rotation, window changes, and rapid render-path switching do not cause stale sizes to overwrite current sizes, old and new frames to mix, or resource leaks.

### Resource and Script Caches

- **Status**: Resource caches, script caches, and game-list persistence still have opportunities to optimize allocation and write frequency.
- **Solution**: Use capacity and hit-rate metrics to determine LRU parameters; coalesce writes for non-critical metadata; preserve save ordering, session deduplication, and crash-recovery semantics.
- **Acceptance**: Cache hit rate improves, peak memory does not increase, and crash recovery and save compatibility remain unchanged.

## Verification Rules

1. Establish the corresponding scalar or old-path baseline before modifying rendering, font, cache, or lifecycle code.
2. Run Linux core validation first, then build for the target platform; target-platform regression is required when textures, fonts, audio, or threads are involved.
3. Diagnostic logs should record only necessary state and counts, with high-frequency probes disabled by default.
4. Remove each resolved item from this document and retain only one brief status record.
