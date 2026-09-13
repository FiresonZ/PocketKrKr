# Plugin Compatibility Inventory

## Status Definitions

- **Real implementation**: Compiled into the current CMake target with executable behavior; coverage still needs interface and game regression.
- **Compatibility adapter**: Registers names or required APIs so scripts continue to run; this does not mean full desktop-plugin behavior.
- **Optional implementation**: Enters the product only when the SDK, platform, or external dependency exists.
- **Disabled**: Source or subdirectory exists, but the default CMake plugin target does not compile it.
- **Needs audit**: The real call contract must be confirmed before deciding whether to implement or keep a stub.

## Current Plugin Matrix

| Module/script name | Status | Local entry | Current capability and boundary | Implement? | Acceptance focus |
|---|---|---|---|---|---|
| `motionplayer.dll` | Real implementation | `cpp/plugins/motionplayer/` | PSB/M2 layers, timelines, submotions, and mobile capture compatibility; complex Emote/physics remain incomplete | Yes, by gap | Timeline, skip/sync, submotion, looping, clipping, resource lifetime |
| `psbfile.dll` | Real implementation | `cpp/plugins/psbfile/` | PSB reading, images, and part of motion metadata | Yes, by format gap | PSB versions, WebP/raw fallback, metadata, corrupt resources |
| `psdfile.dll` | Real implementation | `cpp/plugins/psdfile/` | PSD/PBD resources and layer loading | Yes, by format gap | Layer order, alpha, clipping, colors, malformed files |
| `layerex_draw.dll` | Real implementation | `cpp/plugins/layerex_draw/` | Extended paths, brushes, and draw methods | Yes, by API gap | Alpha, empty layers, coordinates, scaling, scalar pixel baseline |
| `kagparserex.dll` | Real implementation | `cpp/plugins/kagparserex/` | KAG extension parsing and script API | Yes, by fixture | Tag parameters, exceptions, encodings, dynamic properties |
| `fstat.dll` | Real implementation | `cpp/plugins/fstat/` | File status queries | Low priority | Files, virtual archive paths, failure returns |
| `json.dll` | Disabled | `cpp/plugins/json/`; commented in `CMakeLists.txt` | Source and script notes exist, but the default target does not compile it | Decide by game call | Registration, JSON types, exceptions, thread boundary |
| `steam.dll` | Disabled | `cpp/plugins/steam/`; commented in `CMakeLists.txt` | Steam-specific capability is not part of the mobile default artifact | Usually no; keep a stub | Missing optional Steam APIs must not stop startup |
| `DrawDeviceForSteam.dll` | Disabled | `cpp/plugins/DrawDeviceForSteam/` | Desktop/Steam DrawDevice adapter | Do not port to mobile | Scripts must not incorrectly enter the desktop path |
| `krkrlive2d.dll` | Optional | `cpp/plugins/krkrlive2d.cpp`, `cpp/plugins/cubism/` | Enabled when the SDK exists; otherwise the SDK path is disabled safely | Yes where SDK exists | Missing SDK, model load, continuous animation, context recreation |
| `krkrgles.dll` | Real/platform adapter | `cpp/plugins/krkrgles.cpp` | GLES/mobile APIs and Live2D/texture bridge entry | Only proven calls | EGL context, texture ownership, backend, destruction |
| `textrender.dll` | Real implementation | `cpp/plugins/textrender.cpp` | Independent text rendering and logical-size scaling | Yes, boundary tests | Framebuffer scaling, font size, alpha, target Layer |
| `layerExMovie.dll` | Real implementation/audit needed | `cpp/plugins/layerExMovie.cpp`, `cpp/core/movie/ffmpeg/` | Video-layer API and FFmpeg connection | Yes | First frame, pause, seek, end, repeat, A/V clock |
| `alphamovie.dll` | Real/audit needed | `cpp/plugins/alphamovie.cpp` | Alpha-movie registration and compatibility API | By real call | Video alpha, pixel format, missing decoder |
| `layerExImage.dll` | Real implementation | `cpp/plugins/layerExImage.cpp` | Image extension methods | By call coverage | Scaling, filters, formats, malformed resources |
| `layerExRaster.dll` | Real implementation | `cpp/plugins/layerExRaster.cpp` | Raster extension methods | By call coverage | Empty input, boundaries, alpha, performance |
| `layerExBTOA.dll` | Real implementation | `cpp/plugins/layerExBTOA.cpp` | Blue-channel-to-alpha and related operations | By call coverage | Channel semantics, alpha, scalar pixel result |
| `layerExPerspective.dll` / `perspective.dll` | Real implementation | `cpp/plugins/layerExPerspective.cpp` | Perspective/geometric extension | By call coverage | Matrix, clipping, boundaries, alpha |
| `layerExAreaAverage.dll` | Real implementation | `cpp/plugins/layerExAreaAverage.cpp` | Area averaging | Low priority | Empty regions, boundaries, scalar pixels |
| `layerExLongExposure.dll` | Real implementation | `cpp/plugins/layerExLongExposure.cpp` | Long-exposure/accumulation effect | Low priority | Frame accumulation, cleanup, alpha, memory |
| `extrans.dll` | Real implementation | `cpp/plugins/extrans.cpp`, `extrans_precise/` | Transition extensions and precise variants | Yes, by effect | Start/end frames, time, alpha, clipping, scalar/SIMD parity |
| `xp3filter.dll` | Real implementation | `cpp/plugins/xp3filter.cpp` | XP3 filtering and extension callbacks | By game need | Archive filter, callback lifetime, exceptions |
| `addFont.dll` | Real implementation | `cpp/plugins/addFont.cpp` | Script dynamic-font registration | Yes, font regression | Registration, release, restart, fallback |
| `getSample.dll` | Real implementation | `cpp/plugins/getSample.cpp` | WaveSoundBuffer sample API | Low priority | Audio buffer, bounds, no-device fallback |
| `fftgraph.dll` | Compatibility adapter | `cpp/plugins/fftgraph.cpp` | Registers script function, mostly empty/simplified compatibility | By real call | Whether scripts read the result; missing capability must not stop startup |
| `win32dialog.dll` | Compatibility adapter | `cpp/plugins/win32dialog.cpp` | Script/mobile replacement for desktop dialogs | Usually no desktop UI | Return values, cancel paths, headless environment |
| `drawDeviceD2Dm.dll` | Compatibility adapter | `cpp/plugins/drawDeviceD2DCompat.cpp` | Desktop D2D API compatibility | Do not port to Android/iOS | Script probing must not fail or switch to desktop rendering |
| `wfBasicEffect.dll` | Compatibility adapter | `cpp/plugins/wfBasicEffectCompat.cpp` | Windows effect API names/compatibility | By game call | Parameters, returns, empty behavior |
| `wfTypicalDSP.dll` | Compatibility adapter | `cpp/plugins/wfTypicalDSPCompat.cpp` | Windows DSP API compatibility | By game call | Audio calls do not crash; missing capability is explicit |
| `zcompat` | Compatibility adapter | `cpp/plugins/zcompat/` | Z names, scripts, and plugin compatibility | Yes, by real call | DrawBuffer, plugin names, Squirrel/extension boundary |
| `kirikiroid2` compatibility | Compatibility adapter | `cpp/plugins/kirikiroid2.cpp`, `motionplayer/main.cpp` | Mobile script and D3DAdaptor/SeparateLayerAdaptor compatibility | By call contract | captureCanvas, canvasCaptureEnabled, property writes, lifetime |
| `windowEx.dll` | Real/compatibility | `cpp/plugins/windowEx.cpp` | Window extensions and mobile replacements | By call coverage | Input, window properties, no-desktop-window paths |
| `varfile.dll` | Real implementation | `cpp/plugins/varfile.cpp` | Variable-file read/write | By save regression | Encoding, atomic write, recovery |
| `saveStruct.dll` | Real implementation | `cpp/plugins/saveStruct.cpp` | Save-structure helpers | Yes | Versioning, corrupt saves, ordering |
| `getabout.dll` | Real/compatibility | `cpp/plugins/getabout.cpp` | About/environment information | Low priority | Mobile fields and privacy boundary |
| `wutcwf.dll` | Needs audit | `cpp/plugins/wutcwf.cpp` | Legacy-game compatibility API | By real call | Call contract and exceptions |

## Gaps and Priorities

### Implement or complete first

1. `motionplayer.dll` skip/sync, automatic progress, command-list behavior, and title-animation regression.
2. `layerExMovie.dll`, FFmpeg video composition, and frame-clock behavior.
3. `addFont.dll` plus lifecycle tests for prerendered and runtime fonts.
4. `zcompat` real-game call gaps and the main DrawBuffer path.
5. Independent fixtures for key plugin registration, parameters, and return values.

### Keep as compatibility stubs

- `steam.dll`
- `DrawDeviceForSteam.dll`
- `drawDeviceD2Dm.dll`
- Desktop UI behavior in `win32dialog.dll`
- Desktop-only behavior in `wfBasicEffect.dll` and `wfTypicalDSP.dll`

These only need to satisfy script probing, make optional absence observable, and prevent incorrect desktop-path selection.

## Reference and Implementation Entry Points

| Topic | Entry | Use |
|---|---|---|
| AetherKiri shared implementation | <https://github.com/AetherKiri/AetherKiri/tree/main/cpp/plugins> | Plugin state machines, compatibility adapters, tests, diagnostics |
| AetherKiri plugin-gap audit | <https://github.com/AetherKiri/AetherKiri/blob/main/tools/plugin_gap_audit.py> | Real/compat-stub/empty-stub classification |
| AetherKiri plugin registration tests | <https://github.com/AetherKiri/AetherKiri/tree/main/tests/unit-tests/plugins> | Names, parameters, returns, compatibility-stub fixtures |
| KIRIKIRI Z plugins | <https://github.com/krkrz/krkrz/tree/master/plugins> | Z APIs, plugin contracts, data formats |
| KIRIKIRI 2 plugins | <https://github.com/krkrz/krkr2/tree/master/plugins> | Original KiriKiri2 behavior |
| Kirikiroid2 mobile implementation | <https://github.com/zeas2/Kirikiroid2> | Android/mobile compatibility and D3DAdaptor replacements |
| krkr2-tools | <https://github.com/xiaocongyu66/krkr2-tools> | XP3, TJS, and script-resource analysis |
| TJS2 decompiler | <https://github.com/crate-1556/tjs2-decompiler> | Compiled-script call-chain and compatibility-variable analysis |
| SDL M2 player | <https://github.com/krkrsdl3/krkrsdl3> | M2/PSB timeline and playback comparison |

## Rules

1. “Registered” does not mean “fully implemented”; distinguish real behavior, compatibility adapters, and empty stubs.
2. External URLs are for protocol, behavior, and test-design reference; do not copy files directly.
3. Every candidate plugin must have a real call contract before a minimal behavior and acceptance fixture are defined.
4. After a gap is completed, move it to a verified section or remove the corresponding todo instead of accumulating permanent entries.
5. Desktop-only plugins do not enter Android/iOS artifacts merely because they exist in AetherKiri or Kirikiroid2.
