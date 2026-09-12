---
hide:
  - toc
---

# PocketKrKr

<p align="center">
    <img src="resources/logo.png" alt="PocketKrKr" width="120">
</p>

**PocketKrKr** is a mobile-oriented KiriKiri2 runtime built with a C++ engine, a Flutter shell, and a stable C ABI bridge. It targets iOS and Android, with macOS as a development target and Linux for engine verification.

## Features

<div class="grid cards" markdown>

- :material-gamepad-square-outline: **Visual novel runtime**

    Provides TJS2, KAG, XP3, Layer, PSB/M2 animation, and common extension support.

- :material-lightning-bolt-outline: **Multi-platform rendering**

    Uses ANGLE with Metal or Vulkan, with IOSurface, SurfaceTexture, and RGBA readback paths.

- :material-cellphone: **Mobile first**

    iOS and Android are the primary targets; macOS is used for development and debugging.

</div>

## Platforms

| Platform | Graphics | Texture path | Engine form |
|---|---|---|---|
| iOS | Metal | IOSurface | Static library linked into Runner |
| Android | Vulkan | SurfaceTexture | Self-contained `libengine_api.so` |
| macOS | Metal | IOSurface | `libengine_api.dylib` |
| Linux | Vulkan | Host verification | No application package |

## Quick start

```bash
./build.sh ios release
./build.sh android debug
./build.sh macos debug
```

See the [build documentation](dev/build.md) for the complete toolchain and artifact details.

## Documentation

- [Getting started](dev/getting-started.md)
- [Architecture](dev/architecture.md)
- [Source map](dev/source-map.md)
- [Key references](dev/key-references.md)
- [Build and toolchain](dev/build.md)
- [Development conventions](dev/conventions.md)
- [Compatibility testing](dev/compatibility.md)
- [Rendering diagnosis](dev/rendering-diagnosis.md)
- [Optimization roadmap](dev/optimization-roadmap.md)
- [Known issues](dev/todo.md)
- [Downloads](download.md)
- [Plugins and extensions](plugins.md)
- [Supported games](support_games.md)

## References

Format, protocol, and behavior references are collected in [compatibility and references](dev/krkrz-compat.md).

## License

This project is distributed under GPL-3.0. See [LICENSE](../LICENSE).
