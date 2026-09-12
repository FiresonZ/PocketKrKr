<p align="center">
  <img src="https://raw.githubusercontent.com/FiresonZ/PocketKrKr/main/docs/resources/logo.png" alt="PocketKrKr" width="96">
  <h1 align="center">PocketKrKr</h1>
  <p align="center">面向移动端的下一代 KiriKiri2（吉里吉里2）运行环境</p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-In%20Development-orange" alt="Status">
  <img src="https://img.shields.io/badge/platform-iOS%20%7C%20Android%20%7C%20macOS-blue" alt="Platform">
  <img src="https://img.shields.io/badge/engine-KiriKiri2-blue" alt="Engine">
  <img src="https://img.shields.io/badge/framework-Flutter-02569B" alt="Flutter">
  <img src="https://img.shields.io/badge/graphics-ANGLE(Metal%2FVulkan)-red" alt="Graphics">
  <img src="https://img.shields.io/badge/license-GPL--3.0-blue" alt="License">
</p>

---

**语言 / Language**: 中文 | [English](README_EN.md)

PocketKrKr 使用 GPL-3.0 协议发布。开发入口、架构约束和参考资料见 [docs/dev/](docs/dev/README.md)。

## 简介

**PocketKrKr** 是 [KiriKiri2 (吉里吉里2)](https://zh.wikipedia.org/wiki/%E5%90%89%E9%87%8C%E5%90%89%E9%87%8C2) 视觉小说引擎的现代化运行环境，**专注移动端：iOS + Android**（macOS 保留为 Apple 开发/调试目标）。它完全兼容原版游戏脚本，通过 ANGLE（iOS/macOS 用 Metal 后端，Android 用 Vulkan 后端）+ 零拷贝纹理共享（IOSurface / SurfaceTexture）实现硬件加速渲染，并在渲染性能与脚本执行效率上做了大量优化。

项目采用「C++ 引擎 + Flutter 壳」架构：C++ 引擎离屏渲染到 IOSurface（iOS/macOS）或 SurfaceTexture（Android），Flutter 以原生纹理零拷贝显示，UI 完全由 Flutter 构建。

## 架构

```
C++ 引擎 (cpp/core, TJS2) ──engine_api C ABI──> Dart FFI (flutter_engine_bridge)
        │ ANGLE EGL/GLES2 离屏渲染                     │ Flutter Texture
        └─ iOS/macOS: IOSurface ──┐                    │
        └─ Android:  SurfaceTexture ─┴──────────────────┘ 显示
```

> 📖 渲染管线、桥接层等技术细节见 **[docs/dev/](docs/dev/README.md)**（技术栈、架构、关键引用、构建、约定陷阱）。

## 平台支持

| 平台 | 状态 | 图形后端 | 纹理共享 | 引擎形态 |
|------|------|----------|----------|----------|
| iOS | 🚧 主目标，开发中 | Metal | IOSurface | 静态库链接进 Runner |
| Android | 🚧 主目标，开发中 | Vulkan | SurfaceTexture | `libengine_api.so` 打包进 APK |
| macOS | ✅ 开发目标 | Metal | IOSurface | dylib 打包进 Frameworks |

## 系统要求

| 平台 | 系统版本 | 架构 | 备注 |
|------|----------|------|------|
| iOS | iOS / iPadOS 15.0+ | arm64 | 需支持 iOS 15 的 64 位设备 |
| Android | Android 7.0（API 24）+ | arm64-v8a | 需支持 Vulkan 的 GPU |
| macOS | macOS（开发目标） | arm64 | — |

> 引擎（静态库/vcpkg 依赖/Flutter）均按 `arm64`、对应最低系统版本配置，见
> `CMakePresets.json`、`vcpkg/triplets/arm64-ios.cmake`、`vcpkg/triplets/arm64-android.cmake`。

## 构建

```bash
./build.sh ios release     # 构建 iOS（需 macOS/Xcode，或走 CI）
./build.sh android debug   # 构建 Android APK（Windows / macOS / Linux 均可）
./build.sh macos debug     # 构建 macOS（开发）
```

详见 [docs/dev/build.md](docs/dev/build.md) 与 [build.sh](build.sh)。

## 获取/安装

在线构建产物由 GitHub Actions 打包（iOS 未签名、Android 为 APK），并支持自动打 tag +
建 Release 挂产物。具体触发方式、产物命名、真机安装与版本号规范见
**[docs/dev/build.md](docs/dev/build.md)**。本地无 macOS 时，Android APK 可在 Windows 上直接构建。

## 开发进度

| 模块 | 状态 | 说明 |
|------|------|------|
| C++ 引擎核心编译 | ✅ 完成 | KiriKiri2 核心引擎可编译（iOS/Android/macOS） |
| ANGLE 渲染层迁移 | ✅ 基本完成 | EGL/GLES 离屏渲染（Metal / Vulkan 后端），替代旧 Cocos2d-x + GLFW 管线 |
| engine_api 桥接层 | ✅ 完成 | 稳定 C ABI，含启动/主循环/输入/内存统计等 |
| Flutter 插件（零拷贝纹理） | ✅ 基本完成 | IOSurface + SurfaceTexture 零拷贝纹理 + RGBA 兼容路径 |
| Flutter 调试 UI | ✅ 基本完成 | FPS 控制、引擎生命周期、渲染状态监控 |
| 输入事件转发 | ✅ 基本完成 | 触控 / 指针事件坐标映射转发 |
| Android 构建链 | ✅ 基本完成 | 自包含 `libengine_api.so`（含 JNI），APK 可出、真机不再闪退/不转圈；进入日志筛查游戏兼容性阶段 |
| 引擎性能优化 | 🔨 进行中 | SIMD 像素混合（Highway）：非 PS 混合已对齐标量；**11 个 PS 混合回退标量**（待改 u32 lane 再放回）；GPU 合成管线等 |
| 游戏兼容性优化 | 🔨 进行中 | 补全解析引擎、插件，目标与 Z 闭源版兼容持平 |

## 相关文档

- 开发文档：[docs/dev/](docs/dev/README.md)
- 项目主页：<https://github.com/FiresonZ/PocketKrKr>
- 兼容性与参考资料：[docs/dev/krkrz-compat.md](docs/dev/krkrz-compat.md)

## 许可证

本项目基于 GNU General Public License v3.0 (GPL-3.0) 开源，详见 [LICENSE](./LICENSE)。
