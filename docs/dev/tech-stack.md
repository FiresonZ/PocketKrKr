# 技术栈

## 语言与工具

| 项目 | 选择 |
|---|---|
| 引擎 | C++17 |
| Flutter 壳 | Dart / Flutter |
| 构建 | CMake、Ninja、vcpkg |
| 日志 | spdlog、fmt |
| 可选加速 | ccache、Highway |

## 平台

| 平台 | 最低目标 | 图形后端 | 产物 |
|---|---|---|---|
| iOS | iOS/iPadOS 15、arm64 | ANGLE Metal | 静态库和未签名应用包 |
| Android | API 24、arm64-v8a | ANGLE Vulkan | 自包含 `libengine_api.so` 和 APK |
| macOS | Apple 开发目标 | ANGLE Metal | `libengine_api.dylib` 和应用 |
| Linux | CI 宿主验证 | Vulkan | 引擎验证构建，不提供应用 |

## 主要依赖

- ANGLE：EGL/GLES2 图形后端。
- FFmpeg：视频和音频解码。
- OpenAL-soft、Vorbis、Opus：音频输出和格式支持。
- FreeType：字体光栅化。
- libpng、libjpeg-turbo、libwebp、jxrlib、libbpg：图像解码。
- libarchive、7zip、minizip、zstd、lz4：归档和压缩。
- SDL2：宿主系统和窗口基础设施。
- Highway：经过逐像素验证的 SIMD 路径。
- Cubism：可选 Live2D SDK，不通过 vcpkg 管理。

平台 feature、triplet 和可选依赖以 `vcpkg.json`、`vcpkg-configuration.json` 和 `vcpkg/triplets/` 为准。
