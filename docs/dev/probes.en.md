# Diagnostic Probe Inventory

This page inventories diagnostic probes in the source tree. All diagnostic probes use the CMake option `ENABLE_RENDER_PROBE` and the compile definition `KRKR_RENDER_PROBE`; the default is `OFF`. When disabled, diagnostic-only logging and scans must not run.

## Usage

The workflow input `enable_render_probe` is a global switch. It enables rendering, font, prerendered-font, PSB parsing, and MotionPlayer diagnostics together.

```bash
cmake -DENABLE_RENDER_PROBE=ON <other arguments>
```

The Android, iOS, and macOS build scripts explicitly pass `ON` or `OFF` whenever `ENABLE_RENDER_PROBE` is present in the environment. This forces CMake to reconfigure when the state changes. Reconfigure and build with `OFF` after diagnosis is complete.

## Probe Types

| Type and prefix | Purpose | Location | Frequency and cost |
|---|---|---|---|
| `StorageExec`, `StartupProbe`, `EngineState` | Locate script startup, storage execution, and engine-state transitions | `cpp/core/base/ScriptMgnIntf.cpp` | Startup or state edges; low cost |
| `RequestUpdate` | Determine whether layer changes request a window update | `cpp/core/visual/WindowIntf.cpp` | Rate-limited; low cost |
| `DeliverWinUpdate`, `ContinuousProbe` | Determine whether window-update events are delivered, handled, and continued | `cpp/core/base/EventIntf.cpp` | Rate-limited; low cost |
| `TimerProbe` | Check whether timer callbacks continue to fire | `cpp/core/utils/win32/TVPTimer.cpp` | Rate-limited; low cost |
| `BasicShow` | Record exceptional display paths in the basic drawing device | `cpp/core/visual/impl/BasicDrawDevice.cpp` | Exception paths; low cost |
| `RTProbe`, `UpdateDrawBuffer` | Check render targets, draw buffers, and layer compositing state | `cpp/core/environ/stubs/ui_stubs.cpp` | Rate-limited; low to medium cost |
| `SourceSample` | Sample the engine source texture and determine whether valid pixels were written | `cpp/core/environ/stubs/ui_stubs.cpp` | On sampling events; pixel-read cost |
| `BlackScreen` | Summarize draw and target state during continuous black screens | `cpp/core/environ/stubs/ui_stubs.cpp` | Black-screen transitions; medium cost |
| `IOSurfacePixelSample` | Determine whether the shared IOSurface received valid pixels | `bridge/engine_api/src/engine_api.cpp` and Apple bridge paths | On sampling events; readback cost |
| `engine_tick` | Determine whether a bridge frame call entered and returned | `bridge/engine_api/src/engine_api.cpp` | Frame-level diagnosis; logging cost when enabled |
| `[FontProbe]` | Compare requested and actual font metrics, including ascent, to diagnose baseline or size-scaling issues | `cpp/core/visual/FreeTypeFontRasterizer.cpp` | On font application; low cost |
| `[TpfMap]` | Confirm which `.tpf` path is mapped for a face and size | `cpp/core/visual/impl/LayerBitmapImpl.cpp` | On mapping; low cost |
| `[TpfProbe] HIT/MISS` | Confirm prerendered-font hits and missing-glyph fallback details | `cpp/core/visual/impl/LayerBitmapImpl.cpp` | Deduplicated by face, size, character, and result; low cost |
| `[TextProbe] single/multi` | Compare text rectangles, draw positions, ascent offsets, and prerendered-font paths | `cpp/core/visual/impl/LayerBitmapImpl.cpp` | On text drawing; controlled logging cost, no extra pixel scan |
| `PSB frameColor` | Check whether PSB frame color fields exist and how four-corner colors were parsed | `cpp/plugins/psbfile/PSBMedia.cpp` | On target PSB frame parsing; low cost |
| `loadProbe` | Compare actual MotionPlayer resource dimensions with PSB cached metadata | `cpp/plugins/motionplayer/Player.h` | When target m2logo line nodes load; low cost |
| `m2foldProbe` | Check folded M2 parts using final corner bounds, anchors, and transform results | `cpp/plugins/motionplayer/Player.h` | Target icons during the fold stage; geometry and logging cost |
| `drawAnimatedTree stencil` | Record controlled stencil scratch-layer preparation and compositing results | `cpp/plugins/motionplayer/Player.h` | During stencil compositing; low to medium cost |
| `applyStencilComposite: RGB-rotation alpha recovered` | Record the exceptional compatibility path that recovers mask alpha from RGB | `cpp/plugins/motionplayer/Player.h` | Only when it occurs; low cost |

## Removed Probes

The following logs produced large amounts of per-node or per-frame output without stable long-term diagnostic value and were removed: MotionPlayer `param`, `cp`, `mesh`, `motionDt`, `strclip`, `viewportClip`, `rawColor`, and per-pixel statistics in color processing. The corresponding parameterized animation, mesh deformation, clipping, and color-processing paths remain normal functionality.

## Maintenance Rules

- New or modified diagnostic probes must use `KRKR_RENDER_PROBE`; do not add module-level switches that default to logging.
- Probes are disabled by default and must not change rendering results, event ordering, thread scheduling, resource lifetime, or error handling.
- High-frequency logs must be sampled, rate-limited, deduplicated, or limited to state edges; per-pixel scans may run only when explicitly needed for diagnosis.
- Logs must not contain complete user text, personal paths, device-private data, or complete game resources; paths and text must be limited to necessary identifiers.
- Probe prefixes, trigger conditions, source locations, and performance costs must be updated in this inventory and its Chinese counterpart.
- When probes are disabled, reconfigure the build directory and confirm that compile commands no longer contain `KRKR_RENDER_PROBE`.
