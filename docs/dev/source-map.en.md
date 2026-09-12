# Source Map

## Top-Level Structure

```text
apps/flutter_app/                  Flutter shell, pages, game management, and localization
bridge/engine_api/                 C ABI, engine lifecycle, frame interface, and JNI
bridge/flutter_engine_bridge/      Dart FFI, MethodChannel, IOSurface, SurfaceTexture
cpp/core/                          TJS2, storage, archives, rendering, fonts, audio, video, and main loop
cpp/plugins/                       PSB, PSD, motionplayer, LayerEx, and other TJS plugins
build.sh、build/                   iOS, Android, and macOS build scripts
CMakeLists.txt、CMakePresets.json  CMake targets and platform presets
vcpkg.json、vcpkg/                 Dependencies, ports, and platform triplets
docs/                              User documentation and development documentation
```

## Bridge Layer

### `bridge/engine_api/`

Stable C ABI entry points. `include/engine_api.h` defines lifecycle, startup, frame reading, input, render target, and memory statistics interfaces; `src/engine_api.cpp` manages engine instances, the main loop, and state; Android JNI files connect the Surface and native window.

### `bridge/flutter_engine_bridge/`

Flutter plugin and Dart wrapper. `lib/src/ffi/` binds the C ABI, platform directories manage native textures, and MethodChannel serves as a fallback channel when FFI is unavailable.

## Engine Core

| Directory | Responsibility |
|---|---|
| `cpp/core/tjs2/` | TJS2 lexical analysis, compilation, bytecode loading, interpretation, and built-in objects |
| `cpp/core/base/` | Storage, XP3/ZIP/7z/TAR, events, messages, and KAG parsing |
| `cpp/core/environ/` | Engine bootstrap, main loop, system objects, configuration, and platform abstraction |
| `cpp/core/visual/` | Bitmap, Layer, fonts, image decoding, transitions, rendering, and SIMD |
| `cpp/core/sound/` | Wave, CDDA, MIDI, decoding, mixing, and audio effects |
| `cpp/core/movie/` | FFmpeg demuxing, decoding, playback clock, and video layer interface |
| `cpp/core/plugin/` | Plugin loading, lifecycle, and ncbind host |
| `cpp/core/utils/` | Threads, timers, encoding, mathematics, containers, and debugging tools |

Key entry points:

- Rendering: `cpp/core/visual/LayerIntf.cpp`, `LayerBitmapImpl.cpp`, `RenderManager*.cpp`.
- Scalar pixel baseline: `cpp/core/visual/tvpgl.cpp`.
- Fonts: `FontSystem.*`, `FreeType*`, `PrerenderedFont.*`, `CharacterData.*`.
- Lifecycle: `cpp/core/environ/EngineBootstrap.*`, `EngineLoop.*`, `Application.*`.
- Storage and scripts: `StorageIntf.*`, `StorageImpl.*`, `ScriptMgnIntf.*`, `KAGParser.*`.

The `win32` directories contain cross-platform shared implementations for audio, threads, timers, and system control, and must not be deleted based on their directory names.

## Plugins

| Directory | Responsibility |
|---|---|
| `cpp/plugins/psbfile/` | PSB archives, image resources, motion data, and metadata |
| `cpp/plugins/psdfile/` | PSD layer and resource parsing |
| `cpp/plugins/motionplayer/` | PSB/M2 nodes, timelines, and dynamic resource playback |
| `cpp/plugins/layerex_draw/` | Extended paths and drawing operations |
| `cpp/plugins/fstat/` | File status queries |
| `cpp/plugins/cubism/` | Optional Live2D SDK integration |
| `cpp/plugins/zcompat/` | Z-related name mapping and compatibility stubs |

Plugin target and source-file boundaries are defined by the `CMakeLists.txt` files at each level. When adding TJS capabilities, first determine whether they belong to the core API, plugin API, or script compatibility layer.

## Flutter Frontend

- `lib/main.dart`: Application entry point, initialization, and routing.
- `lib/engine/`: Engine state machine and bridge adapter.
- `lib/widgets/engine_surface.dart`: Texture display, size synchronization, input, and frame updates.
- `lib/pages/`: Home, game, settings, and metadata pages.
- `lib/services/`: Game library, cover, metadata, and statistics services.
- `lib/l10n/`: Chinese, English, and Japanese resources.

Pages should not operate on the C ABI directly; engine-related calls are centralized in `engine/` and the bridge plugin.

## Modification Paths

- Modify engine behavior: `cpp/core/`.
- Add script-callable capabilities: `cpp/core/plugin/` or `cpp/plugins/`.
- Modify platform and lifecycle: `cpp/core/environ/`, `bridge/engine_api/`.
- Modify textures and input: `bridge/flutter_engine_bridge/`, `engine_surface.dart`.
- Modify pages and game management: `apps/flutter_app/lib/pages/`, `services/`.
- Modify build dependencies: `vcpkg.json`, `vcpkg/ports/`, `vcpkg/triplets/`.
