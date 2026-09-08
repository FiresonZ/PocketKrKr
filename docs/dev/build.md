# 构建与工具链

> iOS 构建需在 **macOS** 上进行（需要 Xcode + Apple 工具链 + vcpkg）。
> **Android 构建可在 Windows / macOS / Linux 上进行**（需要 NDK + vcpkg）。
> 根 CMake 设置 `CMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake`，依赖由 vcpkg manifest 安装。

## 前置要求

### iOS / macOS（macOS 主机）

- macOS（Apple Silicon 或 Intel）+ Xcode

- CMake ≥3.28、Ninja、ccache（可选）

- bison（TJS2 parser 生成，Homebrew：`/opt/homebrew/opt/bison`）

- **autoconf / automake / autoconf-archive / libtool / gettext / pkg-config**
  （vcpkg 交叉编译 iOS 时需构建宿主工具：gperf→glib、libexif 等 autotools 端口；
  GNU libtool 提供 `libtool.m4` 供 aclocal 使用）

  ```bash
  brew install cmake ninja bison autoconf automake autoconf-archive libtool gettext pkg-config
  ```

  > ⚠️ Homebrew 的 GNU libtool 会覆盖 PATH 里的 `libtool`。
  > `build_ios.sh` 已改为显式调用系统 `/usr/bin/libtool` 做静态库合并（`-static`），勿改回裸 `libtool`。

### Android（任意主机）

- Android NDK（r25+ 推荐），设置环境变量：
  ```bash
  export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/<版本>"   # Windows: set ANDROID_NDK_HOME=...
  ```
  > `build_android.sh` 会自动从 `ANDROID_NDK_HOME` / `ANDROID_NDK_ROOT` / `$ANDROID_HOME/ndk/*`（取最新）定位 NDK。

- JDK 17（AGP 8 要求，Android Studio 自带）

- CMake ≥3.28、Ninja

- Flutter SDK（`flutter` 在 PATH，或放到 `.devtools/flutter`）——`flutter doctor --android-licenses` 需通过

- 网络（vcpkg 首次会 clone 依赖）

## 快速开始

```bash
# 一键构建 iOS（默认 debug，需 macOS）
./build.sh ios debug

# 或 release
./build.sh ios release

# Android APK（Windows / macOS / Linux 均可）
./build.sh android debug

# macOS
./build.sh macos debug
```

## iOS 构建步骤（build/build\_ios.sh 内部）

1. 定位 Flutter SDK（`.devtools/flutter` 或 PATH）。
2. 定位/自举 vcpkg（`.devtools/vcpkg`，`bootstrap-vcpkg.sh`）。
3. CMake 配置 `iOS <Debug|Release> Config` 预设 → 构建 `iOS <Debug|Release> Build`。

   - 输出：`out/ios/<type>/bridge/engine_api/libengine_api.a`（静态库）

   - 依赖装到 `out/ios/<type>/vcpkg_installed/arm64-ios/`（triplet `arm64-ios`）
4. **静态库合并**：

   - 工程库（排除 `cpp/plugins` 顶层，保留深层子库如 psdparse 的独有 `.o`）→ `libengine_project.a`

   - vcpkg 三方库（排除 libpng/libjpeg/libwebpdecoder/libharfbuzz-subset 冗余子集）→ `libengine_vendors.a`

   - 二者写入 `bridge/flutter_engine_bridge/ios/Libs/`（该目录 `*.a` 已 gitignore）
5. `flutter pub get` + `flutter build ios --<mode> --no-codesign`

   - 产物：`apps/flutter_app/build/ios/iphoneos/Runner.app`
6. 真机运行：Xcode 打开 `apps/flutter_app/ios/Runner.xcworkspace`。

## macOS 构建步骤（build/build\_macos.sh 内部）

1. CMake `MacOS <Debug|Release> Config` → 构建 → `out/macos/<type>/bridge/engine_api/libengine_api.dylib`。
2. `flutter build macos --<mode>` → `build/macos/Build/Products/<Debug|Release>/Runner.app`。
3. 把 dylib 拷入 `Contents/Frameworks/`，`install_name_tool` 设 `@executable_path/../Frameworks/`，ad-hoc 重签。

## Android 构建步骤（build/build\_android.sh 内部）

1. 定位 NDK（`ANDROID_NDK_HOME` / `ANDROID_NDK_ROOT` / `$ANDROID_HOME/ndk/*` 最新版）。
2. 定位 Flutter SDK（`.devtools/flutter` 或 PATH）。
3. 定位/自举 vcpkg（`.devtools/vcpkg`，`bootstrap-vcpkg.sh`）。
4. CMake 配置 `Android <Debug|Release> Config` 预设 → 构建 `Android <Debug|Release> Build`。

   - 输出：`out/android/<type>/bridge/engine_api/libengine_api.so`（**自包含共享库**）

   - 依赖装到 `out/android/<type>/vcpkg_installed/arm64-android/`（triplet `arm64-android`）

   - `engine_api` 自包含打包：插件子库 `target_sources(PUBLIC)` 源码经 `INTERFACE_SOURCES`
     直接编进 .so，`engine_api` 普通链接 `krkr2core + krkr2plugin`（**不用 `--whole-archive`**，
     否则 psbfile/motionplayer 对象重复触发 ld.lld 重复符号）；
     JNI 胶水 `engine_api_android_jni.cpp` 提供 `krkr_GetNativeWindow` 及 Kotlin 可调用的
     `nativeSetSurface` / `nativeDetachSurface`。
5. 拷贝 `libengine_api.so` → `apps/flutter_app/android/app/src/main/jniLibs/arm64-v8a/`（已 gitignore）。
6. `flutter pub get` + `flutter build apk --<mode>`。

   - 产物：`apps/flutter_app/build/app/outputs/flutter-apk/app-<debug|release>.apk`
7. 真机运行：`flutter run -d <device>`（或 `adb install` APK）。

### Android 渲染路径

- **GPU 零拷贝（首选）**：Kotlin 插件 `createSurfaceTexture` 创建 `SurfaceTexture` → JNI
  `nativeSetSurface` 把 `ANativeWindow` 交给引擎 → `engine_tick` 自动挂载 EGL WindowSurface
  （ANGLE Vulkan 后端）→ `eglSwapBuffers` 直接把帧交给 Flutter。
- **CPU 回读（兜底）**：`engineReadFrameRgba` → Dart → `updateTextureRgba` → FlutterTexture 上传。
- 若 SurfaceTexture 创建失败，`engine_surface.dart` 会自动降级到回读路径。

## 产物路径速查

| 目标    | 引擎库                                                               | App 产物                                                                       |
| ----- | ----------------------------------------------------------------- | ---------------------------------------------------------------------------- |
| iOS   | `out/ios/{debug,release}/bridge/engine_api/libengine_api.a`       | `apps/flutter_app/build/ios/iphoneos/Runner.app`                             |
| Android | `out/android/{debug,release}/bridge/engine_api/libengine_api.so` | `apps/flutter_app/build/app/outputs/flutter-apk/app-{debug,release}.apk`      |
| macOS | `out/macos/{debug,release}/bridge/engine_api/libengine_api.dylib` | `apps/flutter_app/build/macos/Build/Products/{Debug,Release}/Runner.app` |

## 常用操作

```bash
# 清理某平台产物
./build.sh --clean ios
./build.sh --clean android
./build.sh --clean macos

# 并行数
JOBS=16 ./build.sh ios release
```

## CI 打包（GitHub Actions）

- **iOS**：`.github/workflows/ios_package.yml`（手动触发或 `v*` 标签），运行于 `macos-15`。

- **Android**：`.github/workflows/android_package.yml`（手动触发或 `v*` 标签），运行于 `ubuntu-22.04`：
  setup Flutter + JDK 17 + 自动安装 NDK（`sdkmanager "ndk;27.0.12077973"`）+ `./build.sh android <type>`。

- **引擎核心验证（Linux）**：`.github/workflows/engine_verify.yml`（push/PR 自动触发），
  运行于 `ubuntu-22.04`：以宿主构建（`Linux Debug` 预设，`x64-linux` triplet）编译引擎核心 +
  tools，跑 ctest（tests/ 目录，目前为空）+ `tools/xp3 --help` 冒烟。
  这是最快的反馈闭环（5-10 分钟），后续 SIMD 逐像素比对等测试挂这里。

- vcpkg 二进制缓存：`~/.cache/vcpkg`（key 基于 `vcpkg.json`/`vcpkg-configuration.json`/`vcpkg/**`），
  通过环境变量 `VCPKG_BINARY_SOURCES=files,<path>,readwrite` 启用。

- iOS 产物：未签名 `Runner.app` 的 zip（`ditto` 打包）；Android 产物：APK。均保留 14 天。

- iOS 真机安装需自行用 Apple 开发者证书签名；Android 可直接安装 APK。

### 版本号与发布（GitHub Actions）

- **版本号规则（X.Y.Z 三段式）**：一个版本号同时控制四处，需保持一致——
  - Git tag / Release：`ios-vX.Y.Z`（iOS）、`android-vX.Y.Z`（Android），**分平台各自 Release**；
  - 原生 app 版本：iOS `CFBundleShortVersionString`、Android `versionName`（`--build-name`）；
  - 原生构建号：Android `versionCode`、iOS `CFBundleVersion`（`--build-number`），自动取
    `major*10000 + minor*100 + patch`（如 `0.1.4` → `104`）；
  - 软件内版本显示：设置 → 版本，副标题显示该版本号（`--dart-define=APP_VERSION` 注入）。
- **手动发布（推荐）**：
  1. Actions → 对应打包工作流 → Run workflow；
  2. 填 `build_type=release`，填 **`发布版本号`**（`X.Y.Z`），勾选 **`发布 Release`**；
  3. 跑完自动建 tag `ios-vX.Y.Z` / `android-vX.Y.Z` → 建对应 GitHub Release → 挂产物。
- **注意**：只测不发布则不勾「发布 Release」（产物仍保留 14 天）；已存在 tag 再跑不会重复建
  Release，只补充/覆盖产物；`debug` 类型即使发布也是 debug 包，正式发布请用 `release`。

### Android 稳定签名（升级一致性，可选）

Android 覆盖安装要求**同一签名**；默认 release 用 debug 临时签名，每次 CI 全新 runner 生成的
keystore 不同 → **签名每次不一致，用户无法覆盖更新（只能卸载重装）**。为此提供**可选稳定 keystore**：

- **工作方式**：在 GitHub Secrets 配置后，CI 解出 keystore 并写入 `KEYSTORE_PATH/KEYSTORE_PASSWORD/KEY_ALIAS/KEY_PASSWORD` 环境变量，
  `android/app/build.gradle` 检测到这些变量（且 keystore 文件存在）时用**固定 keystore** 给 release 签名；
  未配置则回退 debug 临时签名（现状行为）。
- **配置 Secrets**（仓库 Settings → Secrets and variables → Actions）：
  `ANDROID_KEYSTORE_BASE64`、`ANDROID_KEYSTORE_PASSWORD`、`ANDROID_KEY_ALIAS`、`ANDROID_KEY_PASSWORD`。
- **生成 keystore + base64**（本地一次，妥善保存，丢失则旧包无法更新）：
  ```bash
  keytool -genkeypair -v -keystore release.jks -alias release -keyalg RSA \
    -keysize 2048 -validity 10000 -storepass <pw> -keypass <pw> -dname "CN=PocketKrKr"
  base64 -w0 release.jks        # 全部输出即为 ANDROID_KEYSTORE_BASE64
  ```
- **iOS 无此问题**：iOS 产物是未签名 IPA，由用户用**自己的 Apple ID** 侧载签名；只要 bundle ID
  （`org.pocketkrkr.app`）不变、每次用同一 Apple ID 重签，即可覆盖更新，无需 CI 签名密钥。
- **本地构建**：随便装即可不设这些变量；要签名一致就导出上述 4 个环境变量再 `./build.sh android release`。

## 常见问题

- **找不到 bison**：Homebrew 安装后路径在 `HINTS` 里已列（tjs2/CMakeLists.txt）。

- **vcpkg 卡住**：首次安装 `arm64-ios` 依赖耗时长；`--jobs` 控制并行。

- **符号找不到（iOS）**：确认 `build_ios.sh` 的 libtool 合并步骤成功，`ios/Libs/*.a` 已更新，
  且 Runner 链接了这些库（podspec 配置）。

- **静态库重复符号**：多为冗余三方库未排除（libpng/libjpeg/libwebpdecoder 等），核对合并脚本排除列表。

- **Android debug 编译失败 / release 却正常（2026-09-08 记录，已决定暂缓）**：
  - 现象：`./build.sh android debug` 在 CMake Generate 阶段报
    `Imported target "libgdiplus::libgdiplus" includes non-existent path
    .../out/android/release/vcpkg_installed/arm64-android/include`；但 `release` 构建正常出包。
  - 根因：**vcpkg 二进制缓存按 triplet（`arm64-android`）共享，不按 debug/release 区分**；
    `libgdiplus`（及其 pkg-config 依赖 glib/cairo/fontconfig/freetype）在 **release** 配置下生成
    的导出目标，把 `INTERFACE_INCLUDE_DIRECTORIES` 烤成了绝对 `release/.../vcpkg_installed/.../include`
    路径并写入共享缓存。后续 **debug** 配置 `find_package(libgdiplus)` 复用该缓存 → 引用 release 专属
    绝对路径 → 该目录在 debug 树/前缀下不存在 → Generate 失败。`--clean` 只清
    `out/android/$BUILD_TYPE`（当前构建类型），清不掉这份共享缓存；当前 GitHub Actions 缓存空间也不足。
  - 关联：glib 属 meson 端口，与 Android arm64 的 include 路径错配是本仓库已知旧坑
    （见 AGENTS「vcpkg meson × Android」⚠️，修法参考 `vcpkg/ports/glib/portfile.cmake`）。
  - 临时规避：清掉 vcpkg `arm64-android` 二进制缓存后重编 debug；release 不受影响。
  - **根治（待做，缓存空间不足暂搁置）**：让 vcpkg 缓存 key 区分 debug/release，或把
    `libgdiplus`/glib 的导出 include 路径改为相对/正确前缀，避免绝对路径串流 debug/release。
    （此问题与引擎代码改动无关，属构建基础设施。）

