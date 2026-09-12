# Architecture

## Overview

```
┌───────────────────────── apps/flutter_app (Dart/Flutter) ─────────────────────────┐
│  pages (home/game/settings/…)  ·  widgets/engine_surface.dart  ·  engine/bridge   │
└──────────────┬───────────────────────────────────────┬────────────────────────────┘
               │ Dart FFI (DynamicLibrary)             │ MethodChannel（兜底）
┌──────────────▼───────────────────┐   ┌───────────────▼─────────────────────────────┐
│ bridge/flutter_engine_bridge     │   │ iOS: FlutterEngineBridgePlugin.swift        │
│  lib/src/ffi/engine_ffi.dart     │   │  - EngineHostTexture (RGBA 上传, 兼容)       │
│  lib/src/ffi/engine_bindings.dart│   │  - EngineIOSurfaceTexture (零拷贝)           │
└──────────────┬───────────────────┘   └───────────────┬─────────────────────────────┘
               │ C ABI                                │ IOSurfaceID
┌──────────────▼───────────────────────────────────────▼─────────────────────────────┐
│ bridge/engine_api  (engine_api.cpp, C ABI)                                          │
│  engine_create / engine_tick / engine_open_game / engine_set_render_target_iosurface│
│  engine_read_frame_rgba / engine_send_input / engine_get_memory_stats …             │
└──────────────┬──────────────────────────────────────────────────────────────────────┘
               │ 链接 krkr2core + krkr2plugin
┌──────────────▼──────────────────────────────────────────────────────────────────────┐
│ cpp/core  (C++17 引擎)                                                               │
│  tjs2(脚本VM)  base(存储/归档/事件)  environ(平台/主循环)  visual(渲染/字体)          │
│  sound(音频)  movie(ffmpeg)  plugin(插件框架)  utils(线程/定时器)  extension          │
└──────────────┬──────────────────────────────────────────────────────────────────────┘
               │ ANGLE EGL/GLES2 离屏渲染 → IOSurface（零拷贝）
               ▼
          Flutter Texture 显示
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

| 能力                        | 实现                                             |
| ------------------------- | ---------------------------------------------- |
| iOS 平台层（路径/弹窗/内存/退出…）     | `cpp/core/environ/apple/ios/platform.mm`       |
| macOS 平台层                 | `cpp/core/environ/apple/macos/platform.mm`     |
| SDL/系统细节                  | `cpp/core/environ/sdl/tvpsdl.cpp`              |
| UI 桩（Flutter 接管 UI 后的空实现） | `cpp/core/environ/stubs/ui_stubs.cpp`          |
| 系统控制（事件分发/内存治理）           | `cpp/core/environ/win32/SystemControl.cpp`（共享） |
| 线程/定时器/剪贴板等               | `cpp/core/utils/win32/*`（共享）                   |
| 音频设备实现                    | `cpp/core/sound/win32/*`（共享）                   |

## Current GPU Compositing Pipeline (整理记录，勿大改)

> 本段为「现状整理 + 检查」记录。GPU 管线是方向性大改造，**暂不深入改动**，
> 待真机基准后再定方案（见 [perf-optimization.md](perf-optimization.md)）。

- **当前管线**：图层合成主要由 CPU 完成——`cpp/core/visual/` 的图层树在软件层用
  `tvpgl.cpp` / `simd/` 的混合函数把多层合成到中间缓冲；随后通过 ANGLE（EGL/GLES2）
  作为**最终绘制**（离屏到 IOSurface，零拷贝给 Flutter）。

- **因此**：混合计算（alpha/PS 混合等）目前主要吃 CPU/NEON，GPU 只负责"画上去"。

- **"全 GPU 合成"方向**：把图层混合搬进 GL shader，减少 CPU 像素搬运——收益不确定，
  需真机（帧率/功耗/发热）基准验证。

- **检查要点**：改渲染相关代码前先确认走的是哪个路径——
  `iosurface_attached`（零拷贝）还是 `engine_read_frame_rgba`（回读）；
  以及 SIMD 是否启用（见 conventions.md 第 9 节，已知公式缺陷会影响合成结果）。
