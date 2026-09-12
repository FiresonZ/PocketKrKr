# Architecture

## Overview

```
┌───────────────────────── apps/flutter_app (Dart/Flutter) ─────────────────────────┐
│  pages (home/game/settings/…)  ·  widgets/engine_surface.dart  ·  engine/bridge   │
└──────────────┬───────────────────────────────────────┬────────────────────────────┘
               │ Dart FFI (DynamicLibrary)             │ MethodChannel (fallback)
┌──────────────▼───────────────────┐   ┌───────────────▼─────────────────────────────┐
│ bridge/flutter_engine_bridge     │   │ iOS: FlutterEngineBridgePlugin.swift        │
│  lib/src/ffi/engine_ffi.dart     │   │  - EngineHostTexture (RGBA upload, compat.)  │
│  lib/src/ffi/engine_bindings.dart│   │  - EngineIOSurfaceTexture (zero-copy)       │
└──────────────┬───────────────────┘   └───────────────┬─────────────────────────────┘
               │ C ABI                                │ IOSurfaceID
┌──────────────▼───────────────────────────────────────▼─────────────────────────────┐
│ bridge/engine_api  (engine_api.cpp, C ABI)                                          │
│  engine_create / engine_tick / engine_open_game / engine_set_render_target_iosurface│
│  engine_read_frame_rgba / engine_send_input / engine_get_memory_stats …             │
└──────────────┬──────────────────────────────────────────────────────────────────────┘
               │ Links krkr2core + krkr2plugin
┌──────────────▼──────────────────────────────────────────────────────────────────────┐
│ cpp/core  (C++17 engine)                                                             │
│  tjs2(script VM)  base(storage/archive/events)  environ(platform/main loop)          │
│  visual(rendering/fonts)  sound(audio)  movie(ffmpeg)  plugin(plugin framework)      │
│  utils(threads/timers)  extension                                                   │
└──────────────┬──────────────────────────────────────────────────────────────────────┘
               │ ANGLE EGL/GLES2 offscreen rendering → IOSurface (zero-copy)
               ▼
          Flutter Texture display
```

## Rendering Data Flow (iOS/macOS)

1. The engine uses **ANGLE** (Metal backend) to create an EGL Pbuffer Surface for offscreen rendering (GLES2).
2. On iOS/macOS, the Swift plugin `EngineIOSurfaceTexture.createSurface()` creates an **IOSurface-backed CVPixelBuffer** and returns an `IOSurfaceID`.
3. Dart calls `engine_set_render_target_iosurface(iosurfaceId, w, h)`, and the engine renders directly into that IOSurface (bypassing `glReadPixels`, zero-copy).
4. After each `engine_tick`, check `engine_get_frame_rendered_flag`; when a new frame is available, call `notifyFrameAvailable(textureId)` to trigger Flutter redraw.
5. Compatibility path: `engine_read_frame_rgba()` reads pixels and uploads RGBA through `EngineHostTexture`.

> Engine logical resolution and device pixels: set with `engine_set_surface_size(w, h)`.

## Bridge Layer Design

- **C ABI** (`bridge/engine_api/include/engine_api.h`): Stable ABI, `extern "C"`, version `ENGINE_API_VERSION`.

- **iOS**: `engine_api` is compiled as a **static library** `libengine_api.a` and linked into Runner; the export macro `ENGINE_API_EXPORT_SYMBOLS` ensures symbols are visible in the executable (`DynamicLibrary.process()` can find them).

- **macOS**: Compiled as the **dynamic library** `libengine_api.dylib` with `ENGINE_API_BUILD_SHARED`; loaded by Dart FFI at runtime by path.

- **Dart side**: FFI is preferred; `FlutterEngineBridgePlatform` (`plugin\_platform\_interface`) provides the MethodChannel fallback implementation `MethodChannelFlutterEngineBridge`.

- **ANGLE symbol conflicts**: On Apple platforms, `-force_load` is used to force-link all of ANGLE's `libGLESv2/libEGL/libANGLE`, avoiding conflicts with the system OpenGL.framework (see `bridge/engine_api/CMakeLists.txt`).

## Engine Startup Flow

1. Dart `engineCreate(writablePath, cachePath)` (initialize `Application` and load the config manager).
2. `engineOpenGameAsync(gameRootPath, startupScript)` opens the game package (XP3, etc.) on a background thread.
3. Poll `engineGetStartupState` (IDLE→RUNNING→SUCCEEDED/FAILED) and retrieve startup logs with `engineDrainStartupLogs`.
4. `engineSetSurfaceSize` + `engineSetRenderTargetIOSurface` establish the rendering target.
5. The main loop `engineTick(deltaMs)` is driven by Flutter vsync/Timer, and input is forwarded through `engineSendInput`.

## Platform Implementation Locations

| Capability | Implementation |
| ------------------------- | ---------------------------------------------- |
| iOS platform layer (paths/dialogs/memory/exit) | `cpp/core/environ/apple/ios/platform.mm` |
| macOS platform layer | `cpp/core/environ/apple/macos/platform.mm` |
| SDL/system details | `cpp/core/environ/sdl/tvpsdl.cpp` |
| UI stubs (Flutter owns the UI) | `cpp/core/environ/stubs/ui_stubs.cpp` |
| System control (events/memory management) | `cpp/core/environ/win32/SystemControl.cpp` (shared) |
| Threads/timers/clipboard | `cpp/core/utils/win32/*` (shared) |
| Audio device implementation | `cpp/core/sound/win32/*` (shared) |

## Current GPU Compositing Pipeline

> This section records the current state and review points. GPU compositing is a directional redesign and should remain unchanged until physical-device benchmarks are available; see [perf-optimization.md](perf-optimization.md).

- **Current pipeline**: Layer compositing is performed mainly by the CPU. The layer tree in `cpp/core/visual/` uses blending functions from `tvpgl.cpp` / `simd/` to composite layers into an intermediate buffer, then ANGLE (EGL/GLES2) performs the **final draw** offscreen into IOSurface for zero-copy delivery to Flutter.

- **Implication**: Blending work (alpha, PS blending, and related operations) primarily consumes CPU/NEON resources; the GPU currently only displays the result.

- **"Full GPU compositing" direction**: Move layer blending into GL shaders to reduce CPU pixel transfers. The benefit is uncertain and requires physical-device benchmarks covering frame rate, power use, and heat.

- **Review points**: Before changing rendering code, confirm whether the path is `iosurface_attached` (zero-copy) or `engine_read_frame_rgba` (readback), and whether SIMD is enabled. See section 9 of [conventions.md](conventions.md); known formula defects can affect compositing results.
