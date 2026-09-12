# Key References

Paths are relative to the repository root. Module compilation boundaries are defined by `CMakeLists.txt` files at each level and their target source lists.

## Build and Configuration

| Path | Purpose |
|---|---|
| `CMakeLists.txt` | Root build orchestration, toolchain, and module entry points |
| `CMakePresets.json` | iOS, Android, macOS, and Linux presets |
| `build.sh` | Unified platform build entry point |
| `build/build_ios.sh` | iOS static library, dependency merging, and Flutter build |
| `build/build_android.sh` | Android `.so`, JNI, and APK build |
| `vcpkg.json` | Dependency manifest |
| `vcpkg-configuration.json` | Registry and overlay configuration |
| `vcpkg/triplets/` | Platform ABI and compiler options |

## C ABI and Flutter

- `bridge/engine_api/include/engine_api.h`: lifecycle, startup, frame, input, rendering target, and statistics interfaces.
- `bridge/engine_api/src/engine_api.cpp`: C ABI implementation, runtime state, and main loop.
- `bridge/engine_api/src/engine_api_android_jni.cpp`: Android Surface/JNI glue.
- `bridge/flutter_engine_bridge/lib/src/ffi/engine_bindings.dart`: Dart FFI type bindings.
- `bridge/flutter_engine_bridge/lib/src/ffi/engine_ffi.dart`: dynamic library loading and call wrappers.
- `apps/flutter_app/lib/engine/engine_bridge.dart`: Flutter-side lifecycle wrapper.
- `apps/flutter_app/lib/widgets/engine_surface.dart`: texture display, size synchronization, and input.

## Engine Core

- `cpp/core/tjs2/`: TJS2 VM, bytecode, built-in objects, and exceptions.
- `cpp/core/base/`: storage, archives, events, messages, and KAG.
- `cpp/core/environ/EngineBootstrap.*`: subsystem initialization order.
- `cpp/core/environ/EngineLoop.*`: timers, events, and per-frame execution.
- `cpp/core/visual/LayerIntf.*`: layer interfaces and draw submission.
- `cpp/core/visual/LayerBitmapImpl.*`: bitmaps, fonts, and drawing resources.
- `cpp/core/visual/RenderManager*`: rendering targets and graphics backends.
- `cpp/core/visual/tvpgl.cpp`: scalar pixel-blending baseline.
- `cpp/core/movie/ffmpeg/`: video decoding and playback scheduling.
- `cpp/core/plugin/`: plugin host and ncbind.

## Plugins

- `cpp/plugins/psbfile/`: PSB archives, images, motion data, and metadata.
- `cpp/plugins/psdfile/`: PSD layers and resources.
- `cpp/plugins/motionplayer/`: PSB/M2 timelines and dynamic nodes.
- `cpp/plugins/layerex_draw/`: extended drawing.
- `cpp/plugins/zcompat/`: compatibility name mappings and stubs.
- `cpp/plugins/cubism/`: optional Live2D SDK integration.

## Confirm Before Modifying

1. Confirm whether the target belongs to the C ABI, Flutter, core engine, plugin, or script compatibility layer.
2. When modifying the public C ABI, also check Dart bindings and version constraints.
3. When modifying rendering or lifecycle code, check context, threads, ownership, and restart paths.
4. When modifying SIMD, use the scalar functions in `tvpgl.cpp` as the baseline and run pixel-by-pixel comparisons.
