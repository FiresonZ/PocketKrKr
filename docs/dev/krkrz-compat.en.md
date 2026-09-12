# Compatibility and References

This document retains only the current compatibility boundaries, capability gaps, and unified reference entry points. External materials are used to understand formats, interfaces, and behavior; they do not mean that PocketKrKr has implemented the corresponding functionality, nor are external code sections copied directly.

## Current Capability Matrix

| Capability | Current status | Main locations | Next step |
|---|---|---|---|
| TJS2 and basic KAG runtime | Supported | `cpp/core/tjs2/`, `cpp/core/base/` | Complete boundary behavior through compatibility regression |
| XP3, ZIP, 7z, TAR resource reading | Supported | `cpp/core/base/` | Add encryption and exceptional-path tests |
| PNG, JPEG, WEBP, TLG, PSB, and other images | Some formats supported | `cpp/core/visual/`, `cpp/plugins/psbfile/` | Unify format detection and error fallback |
| Layers and conventional transitions | Supported | `cpp/core/visual/` | Complete extension effect boundary cases |
| Basic PSB/M2 animation | Some timelines and nodes supported | `cpp/plugins/psbfile/`, `cpp/plugins/motionplayer/` | Improve meshes, sub-motions, particles, and physics |
| Live2D Cubism | Optional | `cpp/plugins/cubism/` | Build and regress when the SDK is present |
| Z-specific main compositing path | Partially supported | `cpp/core/visual/`, `cpp/plugins/zcompat/` | Check the main DrawBuffer, target texture, and update chain |
| Video display compositing | Decoding available, display path pending consolidation | `cpp/core/movie/ffmpeg/`, `cpp/core/visual/` | Complete frame clock, Present, and target Layer updates |
| Layer Alpha/Mosaic extensions | Some interfaces are compatibility stubs | `cpp/plugins/`, `cpp/core/visual/` | Define pixel semantics first, then implement |
| Optional scripting plugins such as Squirrel | Not fully supported | `cpp/plugins/zcompat/` | Clarify runtime dependencies before implementing as needed |

## Compatibility Boundaries

- The compatibility goal is to keep the behavior of TJS2, KAG, Layer, resource reading, and common plugin interfaces as consistent as possible; not all desktop extensions are guaranteed to work.
- Mobile platforms do not use desktop D3D9 drawing devices. What needs to be verified is the compositing relationship among the mobile RenderManager, LayerManager, DrawDevice, and Flutter textures.
- C++ plugins are compiled into platform artifacts through CMake targets; pure TJS compatibility layers should preserve the script loading boundary and must not mix in the C++ lifecycle.
- Optional SDKs, desktop-only interfaces, and plugins of unknown origin must be clearly marked as optional or unsupported. “Loaded” cannot stand in for “functionality implemented.”

## Unified Reference Entry Points

- KIRIKIRI Z engine and plugin materials: <https://github.com/krkrz/krkrz>
- KIRIKIRI 2 plugin materials: <https://github.com/krkrz/krkr2>
- Z tools and extension materials: <https://github.com/krkrz/krkrz_dev>
- Android port and platform behavior materials: <https://github.com/zeas2/Kirikiroid2>
- KIRIKIRI 2 compatibility script materials: <https://github.com/krkrz/Krkr2Compat>
- SDL runtime and M2 player materials: <https://github.com/krkrsdl3/krkrsdl3>
- TJS2 bytecode tools: <https://github.com/crate-1556/tjs2-decompiler>

## Rules for Using Reference Materials

1. First confirm whether the target behavior belongs to the engine, plugin, script, or platform bridge layer.
2. Extract only protocols, data structures, and behavioral constraints, then reimplement them in this project according to its existing architecture.
3. Do not bring desktop-only dependencies into mobile platforms; do not add files from external repositories directly to this project.
4. Every implementation must have a scalar baseline, a target-platform build, and compatibility regression.
5. Reference materials cannot prove that a feature is currently supported; status is determined by code and verification results.
