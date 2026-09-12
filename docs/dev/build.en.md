# Build and Toolchain

> iOS builds must be performed on **macOS** (Xcode + Apple toolchain + vcpkg required).
> **Android builds can be performed on Windows / macOS / Linux** (NDK + vcpkg required).
> The root CMake sets `CMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`; dependencies are installed by the vcpkg manifest.

## Prerequisites

The documentation site is built with MkDocs Material and `mkdocs-static-i18n`. Install the pinned versions before the first build:

```bash
pip install mkdocs-material==9.7.1 mkdocs-static-i18n==1.3.1
```

### iOS / macOS (macOS Host)

- macOS (Apple Silicon or Intel) + Xcode

- CMake ≥3.28, Ninja, ccache (optional)

- bison (TJS2 parser generation, Homebrew: `/opt/homebrew/opt/bison`)

- **autoconf / automake / autoconf-archive / libtool / gettext / pkg-config**
  (When vcpkg cross-compiles for iOS, host tools must be built: gperf→glib, libexif, and other autotools ports;
  GNU libtool provides `libtool.m4` for aclocal.)

  ```bash
  brew install cmake ninja bison autoconf automake autoconf-archive libtool gettext pkg-config
  ```

  > ⚠️ Homebrew's GNU libtool overrides `libtool` in PATH.
  > `build_ios.sh` now explicitly invokes the system `/usr/bin/libtool` for static library merging (`-static`); do not change it back to bare `libtool`.

### Android (Any Host)

- Android NDK (r25+ recommended), set the environment variable:
  ```bash
  export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/<version>"   # Windows: set ANDROID_NDK_HOME=...
  ```
  > `build_android.sh` automatically locates the NDK from `ANDROID_NDK_HOME` / `ANDROID_NDK_ROOT` / `$ANDROID_HOME/ndk/*` (newest version).

- JDK 17 (required by AGP 8, included with Android Studio)

- CMake ≥3.28, Ninja

- Flutter SDK (`flutter` in PATH, or placed in `.devtools/flutter`) — `flutter doctor --android-licenses` must pass

- Network access (vcpkg clones dependencies on the first run)

## Quick Start

```bash
# Build iOS in one step (debug by default; requires macOS)
./build.sh ios debug

# Or release
./build.sh ios release

# Android APK (Windows, macOS, or Linux)
./build.sh android debug

# macOS
./build.sh macos debug
```

## iOS Build Steps (Inside build/build\_ios.sh)

1. Locate the Flutter SDK (`.devtools/flutter` or PATH).
2. Locate/bootstrap vcpkg (`.devtools/vcpkg`, `bootstrap-vcpkg.sh`).
3. Configure CMake with the `iOS <Debug|Release> Config` preset → build with `iOS <Debug|Release> Build`.

   - Output: `out/ios/<type>/bridge/engine_api/libengine_api.a` (static library)

   - Dependencies installed to `out/ios/<type>/vcpkg_installed/arm64-ios/` (triplet `arm64-ios`)
4. **Merge static libraries**:

   - Project libraries (excluding the top level of `cpp/plugins`, while retaining unique `.o` files from deep sublibraries such as psdparse) → `libengine_project.a`

   - vcpkg third-party libraries (excluding redundant subsets libpng/libjpeg/libwebpdecoder/libharfbuzz-subset) → `libengine_vendors.a`

   - Write both to `bridge/flutter_engine_bridge/ios/Libs/` (the directory's `*.a` files are already gitignored)
5. `flutter pub get` + `flutter build ios --<mode> --no-codesign`

   - Artifact: `apps/flutter_app/build/ios/iphoneos/Runner.app`
6. Run on a physical device: open `apps/flutter_app/ios/Runner.xcworkspace` with Xcode.

## macOS Build Steps (Inside build/build\_macos.sh)

1. CMake `MacOS <Debug|Release> Config` → build → `out/macos/<type>/bridge/engine_api/libengine_api.dylib`.
2. `flutter build macos --<mode>` → `build/macos/Build/Products/<Debug|Release>/Runner.app`.
3. Copy the dylib into `Contents/Frameworks/`, set `@executable_path/../Frameworks/` with `install_name_tool`, and perform an ad-hoc re-sign.

## Android Build Steps (Inside build/build\_android.sh)

1. Locate the NDK (latest version from `ANDROID_NDK_HOME` / `ANDROID_NDK_ROOT` / `$ANDROID_HOME/ndk/*`).
2. Locate the Flutter SDK (`.devtools/flutter` or PATH).
3. Locate/bootstrap vcpkg (`.devtools/vcpkg`, `bootstrap-vcpkg.sh`).
4. Configure CMake with the `Android <Debug|Release> Config` preset → build with `Android <Debug|Release> Build`.

   - Output: `out/android/<type>/bridge/engine_api/libengine_api.so` (**self-contained shared library**)

   - Dependencies installed to `out/android/<type>/vcpkg_installed/arm64-android/` (triplet `arm64-android`)

   - `engine_api` self-contained packaging: plugin sublibrary `target_sources(PUBLIC)` source is compiled directly into the .so through `INTERFACE_SOURCES`; `engine_api` normally links `krkr2core + krkr2plugin` (**without `--whole-archive`**, otherwise duplicate symbols from psbfile/motionplayer objects trigger ld.lld duplicate-symbol errors);
     JNI glue `engine_api_android_jni.cpp` provides `krkr_GetNativeWindow` and the Kotlin-callable `nativeSetSurface` / `nativeDetachSurface`.
5. Copy `libengine_api.so` → `apps/flutter_app/android/app/src/main/jniLibs/arm64-v8a/` (already gitignored).
6. `flutter pub get` + `flutter build apk --<mode>`.

   - Artifact: `apps/flutter_app/build/app/outputs/flutter-apk/app-<debug|release>.apk`
7. Run on a physical device: `flutter run -d <device>` (or `adb install` the APK).

### Android Rendering Paths

- **GPU zero-copy (preferred)**: Kotlin plugin `createSurfaceTexture` creates a `SurfaceTexture` → JNI `nativeSetSurface` passes the `ANativeWindow` to the engine → `engine_tick` automatically attaches an EGL WindowSurface (ANGLE Vulkan backend) → `eglSwapBuffers` passes frames directly to Flutter.
- **CPU readback (fallback)**: `engineReadFrameRgba` → Dart → `updateTextureRgba` → upload to FlutterTexture.
- If SurfaceTexture creation fails, `engine_surface.dart` automatically falls back to the readback path.

## Artifact Path Quick Reference

| Target    | Engine Library                                                               | App Artifact                                                                       |
| ----- | ----------------------------------------------------------------- | ---------------------------------------------------------------------------- |
| iOS   | `out/ios/{debug,release}/bridge/engine_api/libengine_api.a`       | `apps/flutter_app/build/ios/iphoneos/Runner.app`                             |
| Android | `out/android/{debug,release}/bridge/engine_api/libengine_api.so` | `apps/flutter_app/build/app/outputs/flutter-apk/app-{debug,release}.apk`      |
| macOS | `out/macos/{debug,release}/bridge/engine_api/libengine_api.dylib` | `apps/flutter_app/build/macos/Build/Products/{Debug,Release}/Runner.app` |

## Common Operations

```bash
# Clean artifacts for a platform
./build.sh --clean ios
./build.sh --clean android
./build.sh --clean macos

# Parallel job count
JOBS=16 ./build.sh ios release
```

## CI Packaging (GitHub Actions)

- **iOS**: `.github/workflows/ios_package.yml` (manual trigger or `v*` tag), running on `macos-15`.

- **Android**: `.github/workflows/android_package.yml` (manual trigger or `v*` tag), running on `ubuntu-22.04`:
  set up Flutter + JDK 17 + automatically install the NDK (`sdkmanager "ndk;27.0.12077973"`) + `./build.sh android <type>`.

- **Engine core verification (Linux)**: `.github/workflows/engine_verify.yml` uses the `Linux Debug` preset and `x64-linux` triplet, builds the core and tools, and runs available tests. Pixel-by-pixel SIMD comparison tests should be integrated into this workflow.

- vcpkg binary cache: `~/.cache/vcpkg` (key based on `vcpkg.json`/`vcpkg-configuration.json`/`vcpkg/**`), enabled through the environment variable `VCPKG_BINARY_SOURCES=files,<path>,readwrite`.

- iOS artifact: zip of the unsigned `Runner.app` (packaged with `ditto`); Android artifact: APK. Both are retained for 14 days.

- iOS physical-device installation requires signing with an Apple developer certificate; Android APKs can be installed directly.

### Versioning and Release (GitHub Actions)

- **Version rule (X.Y.Z three-part format)**: one version controls four places, which must remain consistent:
  - Git tag / Release: `ios-vX.Y.Z` (iOS), `android-vX.Y.Z` (Android), with **separate Releases for each platform**;
  - Native app version: iOS `CFBundleShortVersionString`, Android `versionName` (`--build-name`);
  - Native build number: Android `versionCode`, iOS `CFBundleVersion` (`--build-number`), automatically calculated as
    `major*10000 + minor*100 + patch` (for example, `0.1.4` → `104`);
  - In-app version display: Settings → Version, with the subtitle displaying the version number (injected via `--dart-define=APP_VERSION`).
- **Manual release (recommended)**:
  1. Actions → the corresponding packaging workflow → Run workflow;
  2. Set `build_type=release`, fill in **`Release version`** (`X.Y.Z`), and check **`Publish Release`**;
  3. After completion, automatically create tag `ios-vX.Y.Z` / `android-vX.Y.Z` → create the corresponding GitHub Release → attach artifacts.
- **Note**: For testing without publishing, do not check **Publish Release** (artifacts are still retained for 14 days); rerunning with an existing tag will not create a duplicate Release, but will add or overwrite artifacts; even if published, `debug` is a debug package, so use `release` for formal releases.

### Android Stable Signing (Optional for Upgrade Consistency)

Android overwrite installation requires the **same signature**; by default, release uses a temporary debug signature, and each fresh CI runner generates a different keystore → **signatures differ each time, so users cannot upgrade in place and must uninstall/reinstall**. An **optional stable keystore** is provided:

- **How it works**: After GitHub Secrets are configured, CI decodes the keystore and writes the `KEYSTORE_PATH/KEYSTORE_PASSWORD/KEY_ALIAS/KEY_PASSWORD` environment variables. When `android/app/build.gradle` detects these variables (and the keystore file exists), it signs the release with the **fixed keystore**; when not configured, it falls back to the temporary debug signature (current behavior).
- **Configure Secrets** (repository Settings → Secrets and variables → Actions): `ANDROID_KEYSTORE_BASE64`, `ANDROID_KEYSTORE_PASSWORD`, `ANDROID_KEY_ALIAS`, `ANDROID_KEY_PASSWORD`.
- **Generate the keystore + base64** (once locally, store securely; losing it prevents updating old packages):
  ```bash
  keytool -genkeypair -v -keystore release.jks -alias release -keyalg RSA \
    -keysize 2048 -validity 10000 -storepass <pw> -keypass <pw> -dname "CN=PocketKrKr"
  base64 -w0 release.jks        # The complete output is ANDROID_KEYSTORE_BASE64
  ```
- **No such issue on iOS**: The iOS artifact is an unsigned IPA, sideload-signed by the user with **their own Apple ID**; as long as the bundle ID (`org.pocketkrkr.app`) remains unchanged and the same Apple ID is used for re-signing each time, it can be upgraded in place without CI signing keys.
- **Local builds**: These variables are not required for installation; for consistent signing, export the four environment variables above and run `./build.sh android release`.

## Common Issues

- **bison not found**: After Homebrew installation, the path is already listed in `HINTS` (`tjs2/CMakeLists.txt`).

- **vcpkg hangs**: The first installation of `arm64-ios` dependencies takes a long time; `--jobs` controls parallelism.

- **Symbol not found (iOS)**: Confirm that the libtool merge step in `build_ios.sh` succeeded, `ios/Libs/*.a` was updated, and Runner links these libraries (podspec configuration).

- **Static-library duplicate symbols**: Usually caused by redundant third-party libraries not being excluded (libpng/libjpeg/libwebpdecoder, etc.); check the exclusion list in the merge script.

- **Unexpected vcpkg cache path**: If the exported target references another build type or working directory, delete the invalid binary cache for the corresponding triplet and reconfigure; also confirm that the cache key includes the manifest, overlay port, triplet, and build type. Do not modify third-party source code in the download directory.
