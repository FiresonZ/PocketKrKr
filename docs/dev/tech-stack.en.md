# Technology Stack

## Languages and Tools

| 项目 | 选择 |
|---|---|
| 引擎 | C++17 |
| Flutter 壳 | Dart / Flutter |
| 构建 | CMake、Ninja、vcpkg |
| 日志 | spdlog、fmt |
| 可选加速 | ccache、Highway |

## Platforms

| 平台 | 最低目标 | 图形后端 | 产物 |
|---|---|---|---|
| iOS | iOS/iPadOS 15、arm64 | ANGLE Metal | 静态库和未签名应用包 |
| Android | API 24、arm64-v8a | ANGLE Vulkan | 自包含 `libengine_api.so` 和 APK |
| macOS | Apple 开发目标 | ANGLE Metal | `libengine_api.dylib` 和应用 |
| Linux | CI 宿主验证 | Vulkan | 引擎验证构建，不提供应用 |

## Main Dependencies

- ANGLE: EGL/GLES2 graphics backend.
- FFmpeg: video and audio decoding.
- OpenAL-soft, Vorbis, Opus: audio output and format support.
- FreeType: font rasterization.
- libpng, libjpeg-turbo, libwebp, jxrlib, libbpg: image decoding.
- libarchive, 7zip, minizip, zstd, lz4: archiving and compression.
- SDL2: host system and windowing infrastructure.
- Highway: pixel-verified SIMD paths.
- Cubism: optional Live2D SDK, not managed through vcpkg.

Platform features, triplets, and optional dependencies are defined by `vcpkg.json`, `vcpkg-configuration.json`, and `vcpkg/triplets/`.
