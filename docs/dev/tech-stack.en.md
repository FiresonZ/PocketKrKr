# Technology Stack

## Languages and Tools

| Area | Choice |
|---|---|
| Engine | C++17 |
| Flutter shell | Dart / Flutter |
| Build | CMake, Ninja, vcpkg |
| Logging | spdlog, fmt |
| Optional acceleration | ccache, Highway |

## Platforms

| Platform | Minimum target | Graphics backend | Artifacts |
|---|---|---|---|
| iOS | iOS/iPadOS 15, arm64 | ANGLE Metal | Static library and unsigned application package |
| Android | API 24, arm64-v8a | ANGLE Vulkan | Self-contained `libengine_api.so` and APK |
| macOS | Apple deployment target | ANGLE Metal | `libengine_api.dylib` and application |
| Linux | CI host verification | Vulkan | Engine verification build, no application |

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
