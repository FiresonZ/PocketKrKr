<p align="center">
  <img src="https://raw.githubusercontent.com/FiresonZ/PocketKrKr/main/docs/resources/logo.png" alt="PocketKrKr" width="96">
  <h1 align="center">PocketKrKr</h1>
  <p align="center">A Next-Generation KiriKiri2 Runtime for Mobile (iOS + Android)</p>
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

**Language / 语言**: [中文](README.md) | English

> 🙏 **PocketKrKr** (<https://github.com/FiresonZ/PocketKrKr>) is built as a
> **secondary development based on [KrKr2-Next](https://github.com/reAAAq/KrKr2-Next)**.
> Thanks to all upstream authors.

> 🤖 **AI-agent collaboration note**: a large portion of this project's code was written by AI agents.
> A new AI agent should start by reading [AGENTS.md](AGENTS.md) (first-screen instructions) and
> [docs/dev/](docs/dev/README.md) (quick-reference docs) before touching any code.

## Overview

**PocketKrKr** is a modern runtime for the [KiriKiri2](https://en.wikipedia.org/wiki/KiriKiri) visual novel engine, **focused on mobile: iOS + Android** (macOS is kept as the Apple development/debug host). It is fully compatible with original game scripts, uses ANGLE (Metal backend on iOS/macOS, Vulkan backend on Android) with zero-copy texture sharing (IOSurface / SurfaceTexture) for hardware-accelerated rendering, and includes numerous optimizations for both rendering performance and script execution.

The project follows a "C++ engine + Flutter shell" architecture: the C++ engine renders offscreen to an IOSurface (iOS/macOS) or SurfaceTexture (Android), Flutter displays it via a native texture with zero-copy transfer, and the UI is fully built with Flutter.

## Architecture

```
C++ Engine (cpp/core, TJS2) ──engine_api C ABI──> Dart FFI (flutter_engine_bridge)
        │ ANGLE EGL/GLES2 offscreen                  │ Flutter Texture
        └─ iOS/macOS: IOSurface ──┐                  │
        └─ Android:  SurfaceTexture ─┴────────────────┘ display
```

> 📖 Full developer/AI-agent docs: **[docs/dev/](docs/dev/README.md)** (tech stack, architecture, key references, build, conventions).

## Platform Support

| Platform | Status | Graphics Backend | Texture Sharing | Engine Binary |
|----------|--------|------------------|-----------------|---------------|
| iOS | 🚧 Primary target, in development | Metal | IOSurface | Static library linked into Runner |
| Android | 🚧 Primary target, in development | Vulkan | SurfaceTexture | `libengine_api.so` packaged into APK |
| macOS | ✅ Development target | Metal | IOSurface | dylib bundled into Frameworks |

## System Requirements

| Platform | OS Version | Architecture | Notes |
|----------|-----------|--------------|-------|
| iOS | iOS / iPadOS 15.0+ | arm64 | any 64-bit device supporting iOS 15 |
| Android | Android 7.0 (API 24)+ | arm64-v8a | Vulkan-capable GPU |
| macOS | macOS (development target) | arm64 | — |

> The engine (static library / vcpkg deps / Flutter) is configured for `arm64` with the
> corresponding minimum OS versions — see `CMakePresets.json`,
> `vcpkg/triplets/arm64-ios.cmake`, `vcpkg/triplets/arm64-android.cmake`.

## Build

```bash
./build.sh ios release     # Build iOS (requires macOS + Xcode, or use CI)
./build.sh android debug   # Build Android APK (any host: Windows / macOS / Linux)
./build.sh macos debug     # Build macOS (development)
```

See [docs/dev/build.md](docs/dev/build.md) and [build.sh](build.sh).

## Getting the Builds / Installation

Online build artifacts are produced by GitHub Actions (iOS is unsigned, Android is an APK),
with optional auto tag + Release + artifact upload. For trigger modes, artifact naming,
on-device installation, and versioning conventions, see **[docs/dev/build.md](docs/dev/build.md)**.
No macOS locally? The Android APK can be built directly on Windows.

## Development Progress

| Module | Status | Notes |
|--------|--------|-------|
| C++ Engine Core Build | ✅ Done | KiriKiri2 core engine compiles (iOS/Android/macOS) |
| ANGLE Rendering Migration | ✅ Mostly Done | EGL/GLES offscreen rendering (Metal / Vulkan backends) |
| engine_api Bridge Layer | ✅ Done | Stable C ABI: startup, main loop, input, memory stats, etc. |
| Flutter Plugin (zero-copy textures) | ✅ Mostly Done | IOSurface + SurfaceTexture + RGBA fallback path |
| Flutter Debug UI | ✅ Mostly Done | FPS control, engine lifecycle, rendering monitor |
| Input Event Forwarding | ✅ Mostly Done | Touch / pointer coordinate mapping and forwarding |
| Android build chain | ✅ Mostly Done | Self-contained `libengine_api.so` (with JNI), APK builds; no more crash/spinner on device; entering log-driven game-compat triage |
| Engine Performance | 🔨 In Progress | SIMD pixel blending (Highway): non-PS blending bit-aligned with scalar; **11 PS blends reverted to scalar** (await u32-lane rewrite), GPU compositing pipeline, etc. |
| Game Compatibility | 🔨 In Progress | Completing the script parser and plugins; target parity with Z's closed-source build |

## Related Docs

- AI-agent first-screen instructions: [AGENTS.md](AGENTS.md)
- Development docs (AI-agent quick reference): [docs/dev/](docs/dev/README.md)
- Project home: <https://github.com/FiresonZ/PocketKrKr>
- Direct upstream (secondary development of KrKr2-Next): <https://github.com/reAAAq/KrKr2-Next>

## License

This project is licensed under the GNU General Public License v3.0 (GPL-3.0). See [LICENSE](./LICENSE) for details.
