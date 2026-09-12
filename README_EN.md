<p align="center">
  <img src="https://raw.githubusercontent.com/FiresonZ/PocketKrKr/main/docs/resources/logo.png" alt="PocketKrKr" width="96">
  <h1 align="center">PocketKrKr</h1>
  <p align="center">A KiriKiri2 runtime for mobile platforms</p>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/status-In%20Development-orange" alt="Status">
  <img src="https://img.shields.io/badge/platform-iOS%20%7C%20Android%20%7C%20macOS-blue" alt="Platform">
  <img src="https://img.shields.io/badge/framework-Flutter-02569B" alt="Flutter">
  <img src="https://img.shields.io/badge/graphics-ANGLE(Metal%2FVulkan)-red" alt="Graphics">
  <img src="https://img.shields.io/badge/license-GPL--3.0-blue" alt="License">
</p>

**Language / 语言**: English | [中文](README.md)

PocketKrKr combines a C++ KiriKiri2 engine, a stable C ABI bridge, and a Flutter shell. iOS and Android are the primary targets; macOS is a development target and Linux is used for engine verification.

## Architecture

```text
C++ engine (TJS2) -> C ABI -> Dart FFI / MethodChannel -> Flutter texture
       ANGLE offscreen rendering -> IOSurface or SurfaceTexture
```

The engine uses ANGLE with Metal on Apple platforms and Vulkan on Android. Native texture sharing is preferred, with an RGBA readback path available as a fallback.

## Platforms

| Platform | Backend | Texture path | Engine form |
|---|---|---|---|
| iOS | Metal | IOSurface | Static library linked into Runner |
| Android | Vulkan | SurfaceTexture | Self-contained `libengine_api.so` |
| macOS | Metal | IOSurface | `libengine_api.dylib` |
| Linux | Vulkan | Host verification | No application package |

## Build

```bash
./build.sh ios release
./build.sh android debug
./build.sh macos debug
```

See [build documentation](docs/dev/build.md) for prerequisites, artifacts, CI, and signing.

## Documentation

- [Getting started](docs/dev/getting-started.md)
- [Architecture](docs/dev/architecture.md)
- [Source map](docs/dev/source-map.md)
- [Build](docs/dev/build.md)
- [Development conventions](docs/dev/conventions.md)
- [Compatibility testing](docs/dev/compatibility.md)
- [Rendering diagnosis](docs/dev/rendering-diagnosis.md)
- [Optimization roadmap](docs/dev/optimization-roadmap.md)
- [Known issues](docs/dev/todo.md)
- [Downloads](docs/download.md)

## License

GPL-3.0. See [LICENSE](LICENSE).
