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
apps/flutter_app/                  Flutter 壳和用户界面
bridge/engine_api/                 C ABI、生命周期和帧接口
bridge/flutter_engine_bridge/      Dart FFI、MethodChannel、原生纹理桥接
cpp/core/                          TJS2、存储、渲染、音频、视频和生命周期
cpp/plugins/                       PSB、PSD、motionplayer、LayerEx 等插件
build.sh、build/                   平台构建入口
vcpkg.json、vcpkg/                 依赖、端口和 triplet
docs/                              用户文档和开发文档
```

## Platform Forms

| 平台 | 引擎产物 | 图形路径 |
|---|---|---|
| iOS | 静态库，链接进 Runner | ANGLE Metal、IOSurface，保留 RGBA 回读兜底 |
| Android | 自包含 `libengine_api.so` | ANGLE Vulkan、SurfaceTexture，保留 RGBA 回读兜底 |
| macOS | `libengine_api.dylib` | ANGLE Metal、IOSurface |
| Linux | 宿主验证构建 | CI 验证，不提供应用包 |

Android 的插件源码通过目标源传播进引擎共享库，使用普通链接即可；不要使用 `--whole-archive`。

## References

External implementations, format materials, and behavior comparison entry points are collected in [krkrz-compat.md](krkrz-compat.md). Reference materials are only for understanding protocols and behavior; they do not mean that PocketKrKr has the corresponding capabilities, nor do they directly copy external code.
