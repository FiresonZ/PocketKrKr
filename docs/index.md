---
hide:
  - toc
---

# 口袋里的吉里吉里2

**PocketKrKr** 是一个基于 [Flutter](https://flutter.dev) 与 [ANGLE](https://github.com/google/angle)（Metal / Vulkan）重构的
[KiriKiri2（吉里吉里2）](https://zh.wikipedia.org/wiki/%E5%90%89%E9%87%8C%E5%90%89%E9%87%8C2) 视觉小说引擎运行环境，
**专注移动端：iOS + Android**（macOS 保留为 Apple 开发/调试目标）。

它完全兼容原版游戏脚本，通过 ANGLE 离屏渲染 + IOSurface / SurfaceTexture **零拷贝**纹理共享实现硬件加速，
并在渲染性能与脚本执行效率上做了大量优化。

## 特性

<div class="grid cards" markdown>

- :material-gamepad-square-outline: **原生兼容**

    内置高精度 TJS2 解析器，完全兼容原版 KiriKiri2 游戏脚本；支持 PSB 动画、Live2D Cubism、PSD 素材与 LayerEx 扩展。

- :material-lightning-bolt-outline: **零拷贝渲染**

    ANGLE 离屏渲染（iOS/macOS 用 Metal 后端，Android 用 Vulkan 后端），经 IOSurface / SurfaceTexture 直达 Flutter 原生纹理，低功耗告别卡顿。

- :material-cellphone: **移动优先**

    iOS 与 Android 为主目标；无 Mac 也能在 Windows 上构建 Android APK，iOS 打包走 GitHub Actions 的 macOS runner。

</div>

## 架构

```
C++ 引擎 (cpp/core, TJS2) ──engine_api C ABI──> Dart FFI (flutter_engine_bridge)
        │ ANGLE EGL/GLES2 离屏渲染                     │ Flutter Texture
        └─ iOS/macOS: IOSurface ──┐
        └─ Android:  SurfaceTexture ─┴──────────────────┘ 显示
```

采用 **「C++ 引擎 + Flutter 壳」** 架构：C++ 引擎离屏渲染到 IOSurface（iOS/macOS）或 SurfaceTexture（Android），
Flutter 以原生纹理零拷贝显示，UI 完全由 Flutter 构建。桥接层 `bridge/engine_api` 提供稳定 C ABI，
Dart 优先走 FFI，MethodChannel 为兜底。

## 平台状态

| 平台 | 状态 | 图形后端 | 纹理共享 | 引擎形态 |
|------|------|----------|----------|----------|
| iOS | 主目标，开发中 | Metal | IOSurface | 静态库链接进 Runner |
| Android | 主目标，开发中 | Vulkan | SurfaceTexture | `libengine_api.so` 打包进 APK |
| macOS | 开发目标 | Metal | IOSurface | dylib 打包进 Frameworks |

## 快速开始

```bash
./build.sh ios release     # 构建 iOS（需 macOS + Xcode，或走 CI）
./build.sh android debug   # 构建 Android APK（任意主机）
./build.sh macos debug     # 构建 macOS（开发）
```

## 文档地图

- :material-school-outline: **新手**：[小白开发指引](dev/for-beginners.md) · [开发者入门](dev/developers-guide.md)
- :material-sitemap-outline: **架构与源码**：[架构](dev/architecture.md) · [源代码结构地图](dev/source-map.md) · [关键引用](dev/key-references.md)
- :material-hammer-wrench-outline: **构建与约定**：[构建](dev/build.md) · [约定与陷阱](dev/conventions.md)
- :material-debug-step-over: **排查与优化**：[渲染/黑屏诊断](dev/rendering-diagnosis.md) · [兼容性](dev/compatibility.md) · [性能优化](dev/perf-optimization.md)
- :material-format-list-checks: **协作队列**：[待办 / 已知问题](dev/todo.md)

## 致谢

本项目基于 [KrKr2-Next](https://github.com/reAAAq/KrKr2-Next) 二次开发而成，感谢所有上游作者与贡献者。
本项目以 [GPL-3.0](https://github.com/FiresonZ/KrKr2-Next-Mobile/blob/main/LICENSE) 协议开源。