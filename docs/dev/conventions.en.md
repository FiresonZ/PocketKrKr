# Development Conventions

## Platforms and Directories

- Some files in `cpp/core/sound/win32/`, `cpp/core/utils/win32/`, and `cpp/core/environ/win32/` are shared cross-platform implementations. They must not be deleted or changed into Windows-only implementations based on the directory names.
- Platform guards in shared source files may carry platform paths that are not yet enabled. Before removing them, confirm the CMake source lists and build results for all target platforms.
- The Live2D SDK is an optional dependency. When the SDK is absent, it should remain automatically disabled; a missing SDK must not be treated as a core build error.
- `*.md` files are normal version-controlled files; build outputs and platform artifacts are handled according to the existing ignore rules.

## Live2D Cubism SDK (Optional)

The Live2D SDK is not managed through vcpkg, and related commercial SDK files are not committed to the repository. To enable Live2D, place the following contents from the official SDK package into `cpp/plugins/cubism/`:

```text
cpp/plugins/cubism/
├── Core/
│   ├── include/Live2DCubismCore.h        Header retained in the repository
│   └── lib/
│       ├── ios/Release-iphoneos/libLive2DCubismCore.a
│       └── macos/
│           ├── arm64/libLive2DCubismCore.a
│           └── x86_64/libLive2DCubismCore.a
└── Framework/                            Cubism SDK Framework source
```

- `Framework/` should contain the Cubism Framework C++ source code; the current CMake configuration uses its OpenGL ES 2 renderer.
- iOS uses the static library under `Core/lib/ios/Release-iphoneos/`; macOS uses the static library under the directory matching its architecture.
- If configuration detects both `Framework/*.cpp` and the target platform's `libLive2DCubismCore.a`, CMake compiles `krkrlive2d.cpp` and defines `KRKR2_LIVE2D`.
- If either part is missing, CMake keeps an empty `CubismFramework` interface and disables the Live2D plugin; core builds for other platforms can continue.
- `Core/lib/` and `Framework/` are already included in `.gitignore`; no ignore-rule changes are needed after obtaining the SDK again.

Verification: after confirming that the directories and static libraries above exist, reconfigure the target platform. The configuration output should no longer contain `Live2D Cubism SDK not found`, and `krkrlive2d.cpp` should appear in the build target.

## Build and Linking

- Apple uses ANGLE's Metal feature; Android and Linux use the Vulkan feature. Do not mix them across platforms.
- iOS artifacts are static libraries, macOS artifacts are dynamic libraries, and Android artifacts are self-contained `libengine_api.so`.
- Android plugin source is propagated into the shared library through CMake target sources, using normal linking of `krkr2core` and `krkr2plugin`; do not use `--whole-archive`.
- Android JNI Java package names, native method names, and C++ declarations must be updated together.
- After modifying the vcpkg manifest, overlay port, or triplet, re-verify the dependency cache and ABI for the corresponding platform.

## Lifecycle

- Engine restart must handle scripts, storage, plugins, fonts, audio, windows, the renderer, and GPU resources separately.
- Process-level singletons must not mistake one-time process-exit cleanup for engine-cycle cleanup; when an explicit reset is needed, define independent initialization and reset interfaces.
- After EGL context recreation, do not reuse textures, FBOs, shaders, or extension state from the old context; all GPU objects must be bound to the current context generation.
- Ownership must be explicit for asynchronous startup, destruction, and frame callbacks; old engine instances must not retain callbacks or resources.

## Rendering and SIMD

- The scalar implementation in `cpp/core/visual/tvpgl.cpp` is the correctness baseline for pixel blending.
- Before modifying SIMD formulas, compare scalar and SIMD results pixel by pixel, covering alpha, boundary values, overflow, and negative-value paths.
- PS blending that has not completed bit-level verification remains on the scalar fallback; SIMD dispatch may be registered again only after verification passes.
- Coordinate systems for render targets, FBOs, textures, and Layers must be explicit at interfaces; scaling, offset, or rotation must not be applied repeatedly at different layers.
- Diagnostic probes uniformly use `KRKR_RENDER_PROBE` and are disabled by default; do not add module-level probe switches that default to enabled.
- High-frequency logs must be sampled, rate-limited, deduplicated, or limited to state edges; diagnostic-only scans must not run when the macro is disabled.
- Probes must not change rendering results, event ordering, thread scheduling, resource lifetime, or normal performance; logs must not contain complete user text, personal paths, or device-private data.
- When adding or changing a probe, update the [probe inventory](probes.md) and verify that switching probes ON/OFF reconfigures the build directory.

## Code and Documentation

- New code follows existing module boundaries and error-handling conventions and does not introduce libraries that are not already used by the project.
- Comments explain only non-obvious constraints, algorithms, and lifecycles; complex logic may use short Chinese-English bilingual comments, avoiding repetition of the code itself.
- Documentation describes current facts, constraints, and acceptance criteria. It does not record personal devices, log file names, temporary directories, internal commit IDs, or iterative trial-and-error processes.
- External materials serve only as protocol and behavior entry points. Documentation must not describe an implementation as directly copied or treat a reference implementation as a capability statement for this project.
- After completing a change, run at least the corresponding formatting check, type check, build, or test; rendering and lifecycle changes must cover target-platform regression testing.
