# Development Documentation

This directory provides PocketKrKr architecture, build, source code, and compatibility materials. Content is based on the current code state; historical troubleshooting processes are not stored here.

## Reading Order

1. [Getting Started](getting-started.md): Quickly understand the project, build, and run it.
2. [Architecture](architecture.md): View the engine, bridge, and rendering data flow.
3. [Source Map](source-map.md): Locate module responsibilities by directory.
4. [Key References](key-references.md): Find major files and C ABI symbols.
5. [Build](build.md): View platform toolchains, artifacts, and CI.
6. [Conventions](conventions.md): Must-read before modifying platform, lifecycle, linking, Live2D, or SIMD code.
7. [Compatibility](compatibility.md): Run game compatibility regressions.
8. [Rendering Diagnosis](rendering-diagnosis.md): Troubleshoot black screens, frozen frames, and display pipeline issues.
9. [Optimization Roadmap](optimization-roadmap.md): View performance, stability, and code structure optimization plans.
10. [Todo](todo.md): View current unfinished items.

## Project Boundaries

```text
apps/flutter_app/                  Flutter shell, pages, and user interface
bridge/engine_api/                 C ABI, lifecycle, and frame interface
bridge/flutter_engine_bridge/      Dart FFI, MethodChannel, and native texture bridge
cpp/core/                          TJS2, storage, rendering, audio, video, and lifecycle
cpp/plugins/                       PSB, PSD, motionplayer, LayerEx, and other plugins
build.sh、build/                   Platform build entry points
vcpkg.json、vcpkg/                 Dependencies, ports, and triplets
docs/                              User and development documentation
```

## Platform Forms

| Platform | Engine artifact | Graphics path |
|---|---|---|
| iOS | Static library linked into Runner | ANGLE Metal, IOSurface, with RGBA readback fallback |
| Android | Self-contained `libengine_api.so` | ANGLE Vulkan, SurfaceTexture, with RGBA readback fallback |
| macOS | `libengine_api.dylib` | ANGLE Metal, IOSurface |
| Linux | Host verification build | CI verification, no application package |

Android plugin sources are propagated into the engine shared library through target sources, so normal linking is sufficient; do not use `--whole-archive`.

## References

External implementations, format materials, and behavior comparison entry points are collected in [krkrz-compat.md](krkrz-compat.md). Reference materials are only for understanding protocols and behavior; they do not mean that PocketKrKr has the corresponding capabilities, nor do they directly copy external code.
