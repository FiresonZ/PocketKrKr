# Getting Started

PocketKrKr is a mobile KiriKiri2 runtime environment composed of a C++ engine, a C ABI bridge layer, and a Flutter shell.

## Directory Layout

- `cpp/core/`: scripts, storage, layers, fonts, audio, video, and lifecycle.
- `cpp/plugins/`: PSB, PSD, motionplayer, LayerEx, and other extensions.
- `bridge/`: C ABI, Dart FFI, MethodChannel, and native texture bridging.
- `apps/flutter_app/`: pages, game library, settings, and engine display components.

See [Source Map](source-map.md) for detailed responsibilities and [Architecture](architecture.md) for the data flow.

## Build

| Target | Command | Requirements |
|---|---|---|
| iOS | `./build.sh ios debug` | macOS, Xcode, Flutter, vcpkg |
| Android | `./build.sh android debug` | NDK, JDK 17, Flutter, vcpkg |
| macOS | `./build.sh macos debug` | macOS, Xcode, Flutter, vcpkg |
| Linux verification | `cmake --preset "Linux Debug Config" && cmake --build --preset "Linux Debug Build"` | CMake, Ninja, vcpkg |

See [Build](build.md) for the complete toolchain, artifacts, and CI details.

## Run a Game

The Flutter shell supports game directories and XP3 resource packages. After importing a game, select it; the shell creates an engine instance and opens the startup script. The engine handles script execution, resource reading, and frame-by-frame rendering.

The engine prioritizes native texture sharing for output and falls back to RGBA readback when unavailable. See the [Download Page](../download.md) in the project root for system requirements and installation instructions.

## Modify Code

1. Read [Conventions](conventions.md) and [Todo](todo.md) first.
2. Choose `cpp/core/`, `cpp/plugins/`, `bridge/`, or a Flutter page according to the change type.
3. Preserve the C ABI, platform boundaries, and resource ownership.
4. Run the corresponding platform build and tests; changes to rendering, fonts, caches, audio, and lifecycle must be regressed on the target platform.

Common entry points:

| Task | Location |
|---|---|
| Layers, fonts, pixel blending | `cpp/core/visual/` |
| Scripts and KAG behavior | `cpp/core/tjs2/`, `cpp/core/base/KAGParser.*` |
| PSB/M2 animation | `cpp/plugins/psbfile/`, `cpp/plugins/motionplayer/` |
| C ABI and engine lifecycle | `bridge/engine_api/` |
| Flutter textures and input | `apps/flutter_app/lib/widgets/engine_surface.dart`, `bridge/flutter_engine_bridge/` |
| Dependencies and platform toolchains | `vcpkg.json`, `vcpkg/ports/`, `vcpkg/triplets/` |

## Diagnosis

Use [Rendering Diagnosis](rendering-diagnosis.md) for black screens, frozen frames, and video issues. Use [Optimization Roadmap](optimization-roadmap.md) for performance and structural optimization.
