# AetherKiri Comparison Audit

## Purpose

This document records a source and behavior comparison between PocketKrKr and AetherKiri. It is used to select reusable behavior, tests, and diagnostic practices. AetherKiri's Godot product shell, multi-runtime provider, and commercial features are not direct PocketKrKr gaps; an item enters implementation only after validation against this project's architecture, platforms, and compatibility requirements.

Reference repository: <https://github.com/AetherKiri/AetherKiri>

Scope: AetherKiri's current public core directories, build entry points, plugins, tests, diagnostic tools, and shared C++ rendering/font implementation, compared with PocketKrKr's `cpp/`, `bridge/`, `apps/`, `build/`, and `docs/`.

## Status Definitions

- **Existing**: PocketKrKr already has equivalent behavior; no port is needed.
- **Partial**: The basic capability exists, but verification, boundary handling, or a unified entry point is missing.
- **Candidate**: AetherKiri provides a useful reference, but suitability for PocketKrKr is not yet proven.
- **Not applicable**: Belongs to AetherKiri's Godot, multi-runtime, or product architecture.
- **Risk**: Must not be copied directly; establish a baseline and target-platform regression first.

## Overall Findings

1. The projects share KiriKiri2 core code and many `cpp/core` and `cpp/plugins` modules, but the current files have substantially diverged and must not be replaced wholesale.
2. The option-text issue exposed a missing generic font-baseline and top-clipping guard. AetherKiri already isolates this behavior and tests it; PocketKrKr now has an equivalent local boundary fix in `LayerIntf.cpp`.
3. AetherKiri has more systematic font, plugin, and script-compatibility tests. Its test organization is useful, but its Godot test framework should not be copied into PocketKrKr.
4. AetherKiri unifies diagnostic profiles, events, marker windows, platform evidence, and artifact bundles. PocketKrKr has a unified probe switch and file logs, but not the same profile and evidence-bundle contract.
5. AetherKiri's GPU text batching, Godot Native texture path, and runtime Provider are high-coupling changes. Their presence alone does not mean PocketKrKr is missing an implementation or should adopt it.

## Comparison Matrix

| Category | AetherKiri reference | PocketKrKr status | Assessment | Priority | Recommendation |
|---|---|---|---|---|---|
| Font baseline | `cpp/core/visual/FontBaseline.h`; baseline and glyph-top helpers | Font, TPF, FreeType, and text probes exist; drawText top-overflow guard was previously missing | Partial; equivalent local logic is now present | P0 | Regress options, dialogue, full-width, missing-glyph, mixed text, and shadows; add minimal unit tests later |
| Prerendered fonts | Shared baseline contract for TPF metrics and runtime glyphs | TPF, FreeType fallback, mapping, and missing-glyph fallback exist | Partial | P1 | Make Origin, Inc, ascent, clip, and scaling a testable contract; do not replace the implementation wholesale |
| Font mapping cache | Avoids clearing the global glyph cache for identical face/storage mappings | PocketKrKr already has the same direction of repeated-mapping protection | Existing | P2 | Check cache invalidation during restart, font changes, and context recreation; do not port again |
| Font unit tests | `font-baseline.cpp`, `font-compat.cpp` | Compatibility rules exist in docs, but equivalent isolated tests are missing | Candidate | P1 | Add platform-independent baseline, TPF, fallback, and KAG font API tests |
| GPU text submission | Pending text draws, batch textures, and GPU path | Existing Layer/Bitmap/RenderManager path and text probes | Risk candidate | P2 | Establish CPU/target-platform baselines and pixel comparisons before evaluating a new path |
| Layer clipping | Corrects the glyph origin against the clip top before drawText | Previously missing; now present in `LayerIntf.cpp` | Fixed, pending verification | P0 | Use the next Android log to verify requested/effective y and final stretch clipping |
| `operateStretch` | TJS width/height conversion to right/bottom coordinates, followed by Layer StretchBlt | Parameter semantics match; no parsing difference found | Existing | P1 | Continue testing negative destination coordinates, source-rect clip adjustment, and zero sizes |
| Pixel/texture path | More complete GPU texture self-tests and Godot texture tests | Scalar pixel baseline, RenderManager, and Flutter texture probes exist | Partial | P1 | Add texture-size changes, context-generation, alpha, and readback-path tests |
| Plugin gap audit | `tools/plugin_gap_audit.py` classifies real/compat-stub/empty-stub modules | Plugin directories and compatibility stubs exist without one unified gap report | Candidate | P1 | Scan registry, CMake targets, and stubs; produce a report without auto-implementing plugins |
| Plugin registration tests | Registry, script-compatibility, and plugin behavior tests | Relies mainly on target-platform and game regressions | Candidate | P1 | Test key names, argument counts, return types, and missing-plugin behavior |
| TJS/KAG compatibility tests | Targeted tests for compiled scripts, CP932, AffineSource, and missing members | TJS2 tools and script-analysis workflow exist, but the fixture matrix is small | Partial | Add minimal fixtures for dynamic properties, Variant conversion, exceptions, and encoding fallback |
| Diagnostic profiles | `tools/diagnose.py`, profile catalog, bounded JSONL, marker windows, ZIP bundles | Unified `KRKR_RENDER_PROBE` switch, file logs, and multiple probes | Partial | First define profile semantics and log fields; consider bundles only while keeping default-off, low-impact probes |
| Diagnostic sessions | Issue markers, platform evidence, screenshots, and summary contract | Rotating file sink and probes, without one evidence contract | Candidate | Design a lightweight log bundle without importing Godot UI dependencies |
| Runtime lifecycle | Provider lifecycle, restart, and multi-runtime boundary tests | EngineBootstrap, EngineLoop, C ABI, and restart rules exist | Partial | Add ownership tests for create, async open, stop, restart, context recreation, and dispose |
| Resource and video | Video smoke, RangeFS, runtime frame evidence, and media tests | FFmpeg decode path exists; video display composition remains a documented gap | Partial | Borrow fixture and frame-clock testing ideas, not the Godot playback layer |
| Godot Native renderer | Godot-owned RenderingDevice, GPU Bridge, and Debug CPU backends | Flutter, ANGLE, IOSurface/SurfaceTexture, and RGBA fallback | Not applicable | None | Borrow the backend capability matrix and fallback validation ideas only |
| Runtime Provider ABI | Unified KiriRuntime, ONS, Siglus, and C Runtime providers | PocketKrKr targets a single KiriKiri engine and Flutter bridge | Not applicable | None | Do not introduce a multi-runtime abstraction |
| Web/ONS/Siglus | AetherKiri product and runtime extensions | Outside PocketKrKr's current boundary | Not applicable | None | Exclude from the PocketKrKr compatibility roadmap |
| Development-tool cache | Linux setup, Godot/vcpkg cache, and diagnostic tooling | Build scripts, vcpkg, and TJS2 tooling exist, with more scattered environment guidance | Partial | P2 | Add tool-availability checks and a Linux verification entry without importing the Godot toolchain |

## Staged Plan

### P0: Close the current font issue

- Verify `requestedY/effectiveY` with an Android probe build containing `4cbe1ff`.
- Confirm the option owner uses size `39`, while the internal prerender layer uses `32` only as its source glyph size.
- Compare `PreRenderFontStretchProbe` destination/source rectangles and ClipRect results.
- Confirm option size, horizontal/vertical placement, and bottom clipping on a real device.

### P1: Correctness and compatibility baselines

- Add minimal font-baseline and prerendered-glyph tests.
- Add plugin registration and compatibility-stub behavior tests.
- Add TJS2/KAG fixtures for CP932, dynamic properties, missing glyphs, exceptions, and script font APIs.
- Add video first-frame, pause, seek, end, and repeat-play fixtures.
- Add size-change, context-generation, and ownership tests for Layer/RenderManager/Flutter bridge paths.

### P2: Diagnostic and performance engineering

- Define lightweight profile levels and event fields for the existing file logs.
- Combine probes, runtime state, platform logs, and user markers into an exportable evidence bundle.
- Evaluate AetherKiri's batch text texture path only after scalar and GPU baselines exist.
- Measure cache hit rate, glyph-cache clears, texture recreations, frame cost, and video queue depth.

## Do Not Port Directly

- AetherKiri's Godot GDExtension, Godot Native/GPU Bridge/Debug CPU backends.
- Its multi-runtime Provider ABI, Onscripter, Siglus, Minori, and commercial runtime boundaries.
- Godot UI, diagnostic drawer, Godot screenshot probes, and product settings pages.
- Wholesale replacement of `LayerIntf.cpp` or `LayerBitmapImpl.cpp`. They share ancestry but have diverged through PocketKrKr's Flutter, ANGLE, plugin, lifecycle, and diagnostic work.
- Any font size, coordinate, or layer-name special case inferred from only one game or device log.

## Acceptance Rules

- Every candidate must have a local code location, an external reference location, a behavioral difference, a risk, and an independent acceptance condition.
- Rendering and font work must cover prerendered fonts, runtime fonts, missing glyphs, full-width characters, mixed text, clipping, and scaling.
- Platform work must cover Android, iOS, and macOS target paths plus the RGBA fallback path.
- Diagnostics remain off by default and must not change timing, lifetime, rendering results, or normal performance.
- External source presence is not evidence that PocketKrKr supports the feature; status is determined by code and verification.
