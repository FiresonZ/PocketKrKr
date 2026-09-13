# Development Documentation

This directory provides PocketKrKr architecture, build, source code, and compatibility materials. Content is based on the current code state; historical troubleshooting processes are not stored here.

## Entry Points

- [Documentation Map](document-map.en.md): Choose the canonical page by topic and avoid duplicate records.
- [Documentation Rules](documentation-rules.en.md): Define content boundaries, fact states, bilingual maintenance, and placeholder requirements.

## Reading By Category

### Orientation and Facts

- [Getting Started](getting-started.en.md): Quickly understand the project, build, and run it.
- [Technology Stack](tech-stack.en.md): View the main languages, frameworks, dependencies, and runtime forms.
- [Architecture](architecture.en.md): View the engine, bridge, and rendering data flow.

### Source and Interfaces

- [Source Map](source-map.en.md): Locate module responsibilities by directory.
- [Key References](key-references.en.md): Find major files and C ABI symbols.
- [API Contracts](api-contracts.en.md): Record stable C ABI, Dart FFI, TJS, and plugin interfaces.

### Build and Platforms

- [Build](build.en.md): View platform toolchains, artifacts, presets, and CI.

### Development Rules

- [Conventions](conventions.en.md): Must-read before modifying platform, lifecycle, linking, Live2D, or SIMD code.

### Compatibility

- [Compatibility](compatibility.en.md): Run game compatibility regressions.
- [Plugin Compatibility Inventory](plugin-compatibility.en.md): View plugin status, missing capabilities, compatibility adapters, and reference implementations.
- [General References](reference/README.en.md): View de-featured text-baseline and animation-sync contracts.
- [KiriKiri Z Compatibility References](krkrz-compat.en.md): View external implementations, format materials, and protocol entry points.

### Tests and Acceptance

- [Test Fixtures](test-fixtures.en.md): Register reproducible game, script, resource, font, and rendering inputs.
- [Compatibility](compatibility.en.md): Record regression matrices and acceptance requirements.

### Diagnostics and Incidents

- [Probe Inventory](probes.en.md): View the unified switch, probe types, log prefixes, and source locations.
- [Rendering Diagnosis](rendering-diagnosis.en.md): Troubleshoot black screens, frozen frames, and display pipeline issues.
- [Incident Reports](incident-reports.en.md): Record confirmed issue boundaries and verification results.

### Tools and Analysis

- [Development Tools](tools.en.md): Use XP3 extraction and TJS2/TJS2100 decompilation analysis tools.

### Planning and Optimization

- [Todo](todo.en.md): View current unfinished items.
- [Optimization Roadmap](optimization-roadmap.en.md): View confirmed long-term optimization directions.
- [Performance Optimization](perf-optimization.en.md): View performance evaluation principles and acceptance methods.

### Releases and Changes

- [Release Notes](release-notes.en.md): Record version scope, user impact, compatibility impact, and verification status.

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

External implementations, format materials, and behavior comparison entry points are collected in [krkrz-compat.en.md](krkrz-compat.en.md). Reference materials are only for understanding protocols and behavior; they do not mean that PocketKrKr has the corresponding capabilities, nor do they directly copy external code.
