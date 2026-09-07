# AGENTS.md — AI Agent 快速上手指南

> 给**无上下文的 AI Agent** 的首屏指令。改代码前必读（指到详细文档，别全塞在这）：
> [docs/dev/README.md](docs/dev/README.md)（索引）→ [docs/dev/conventions.md](docs/dev/conventions.md)（约定，最重要）→ [docs/dev/key-references.md](docs/dev/key-references.md)（符号索引）。

## 项目一句话

**PocketKrKr** = [KiriKiri2（吉里吉里2）](https://zh.wikipedia.org/wiki/%E5%90%89%E9%87%8C%E5%90%89%E9%87%8C2) 视觉小说引擎的现代化运行环境，**面向移动端 iOS + Android**（macOS 为 Apple 开发目标，Linux 仅 CI 宿主验证）。

- **架构**：C++ 引擎（TVP/TJS2）离屏渲染（ANGLE：iOS/macOS=Metal、Android=Vulkan）→ IOSurface/SurfaceTexture 零拷贝 → Flutter 纹理显示；Dart 优先 FFI，MethodChannel 兜底。
- 主页 <https://github.com/FiresonZ/PocketKrKr> · 上游（基于 [KrKr2-Next](https://github.com/reAAAq/KrKr2-Next) 二次开发）· 文档体系 `docs/dev/` 专为 AI 设计，改代码务必与文档同步。

## 仓库结构速览

```
apps/flutter_app/     Flutter 壳（页面/引擎封装/平台目录）
bridge/engine_api/    C ABI 引擎桥（engine_create/tick/destroy…）
bridge/flutter_engine_bridge/  Flutter 插件（IOSurface/SurfaceTexture + FFI）
cpp/core/             C++ 引擎核心（tjs2/base/environ/sound/visual/movie…）
cpp/plugins/          TJS 插件（psb/psd/layerex/motionplayer/fstat/cubism…）
build.sh + build/*.sh + CMakePresets.json   构建入口/预设
vcpkg.json + vcpkg/triplets/  arm64-ios / arm64-android 依赖
docs/  docs/dev/     GitHub Pages 落地页 + AI Agent 开发文档
.github/workflows/    iOS/Android 打包 + Linux 引擎验证
```

## 构建命令

```bash
./build.sh ios release     # iOS（macOS/Xcode 或 CI）
./build.sh android debug   # Android APK（需 ANDROID_NDK_HOME）
./build.sh macos debug     # macOS（开发）
cmake --preset "Linux Debug Config" && cmake --build --preset "Linux Debug Build"  # CI 宿主验证
```

## 硬性约定（改前必读，详见 conventions.md）

1. **平台守卫是惰性的，别删**：`#if __ANDROID__`/`#ifdef _WIN32`/`#if __linux__` 等在 Apple 构建不编译，是潜在复用代码，剥离是高危重构（§2）。
2. **`win32/` 是跨平台共享实现**（音频/线程/系统控制），不是 Windows 专属，绝不能删（§1）。
3. **SIMD（Highway）公式以 [tvpgl.cpp](cpp/core/visual/tvpgl.cpp) 的 `*_c` 标量为准**；SubBlend/ScreenBlend_o/AdditiveAlphaBlend/PS alpha 已修，待逐像素验证（§9）。
4. **Live2D（cubism）按 SDK 磁盘存在与否条件编译**，CI 上自动禁用；缺库是正常状态，不是 bug（§4）。
5. **vcpkg 的 angle 分平台**：Apple 用 `metal` feature，Android/Linux 用 `vulkan`，不可混用。
6. **Android 引擎是自包含 `libengine_api.so`**：插件源码经 `target_sources(PUBLIC)`→`INTERFACE_SOURCES` 直接编进 .so，**普通链接** `krkr2core+krkr2plugin`；**别再 `--whole-archive`**，否则 ld.lld 重复符号；JNI 在 `bridge/engine_api/src/engine_api_android_jni.cpp`。
7. `*.md` 不再被 gitignore，新增 md 正常 `git add`；`build/` 已反忽略。
8. 改 vcpkg 依赖 → CI 缓存 key 变 + 首次全量重编（半小时级）属正常。
9. **待办在 [docs/dev/todo.md](docs/dev/todo.md)**，动手前先看是否已有条目。

## Git 协作规则（务必遵守）

1. **只改 `codex` 分支**，绝不动 `main`。
2. **手动 `git add` + `git commit`**，commit message 写明改动文件及原因。
3. **绝不 `push`**、**不建 PR**、**不 `amend`**、**不 `rebase`**、**不 `force push`**、**不改任何历史**（历史提交只增不改）。
4. 完成后把提交留给用户手动推送/合并。

## 当前状态（2026-09，详见 todo.md）

| 平台/模块 | 状态 |
|---|---|
| iOS 构建 + CI 打包 | ✅ 可出无签名 IPA（nosign.ipa 供 AltStore/Sideloadly）；已出测试版骨架，准备预发布 |
| iOS 黑屏诊断 | 🔬 探针已加，真机日志排除视频后根因转向 **Z 插件兼容**（缺 drawdeviceD3DZ/kztouch/k2compat 等，主 DrawBuffer 从未被合成、源纹理保持初始黑） |
| Android 构建链路 | ✅ APK 可出、真机不再闪退/不转圈；SDL Java 层 + 上游 JNI 平台层已补齐（`a2d8d75`），引擎日志写公共存储 `PocketKrKrLogs`（`c312272`），已进入真机日志筛查游戏兼容性 |
| vcpkg meson × Android | ⚠️ glib 等 meson 端口与 arm64 错配；修法见 `vcpkg/ports/glib/portfile.cmake` |
| Linux 引擎验证 CI | ✅ 绿灯 |
| SIMD 公式 | ⚠️ 23 处 SIMD≠标量，已回退标量保正确，待逐模式修到位级一致放回 |

## 建议的下一步

1. 重跑 Linux engine_verify CI，确认回退后 ctest 全绿。
2. 读 iOS 真机日志，按 `BlackScreen` 探针结论决定是否先做 krmovie Present（见 todo.md）。
3. Android 已进入日志筛查游戏兼容性阶段；iOS 出 nosign IPA 真机实测；核心待办转向 **Z 插件兼容黑屏**（见 todo「已解决 / 进行中」）。
4. 逐模式修 SIMD 到位级一致（PsApplyAlpha 舍入序 + SubBlend_o/ScreenBlend alpha + Overlay/HardLight 分支），tests 验证后放回。
5. 真机问题修复后进入游戏兼容性测试（[docs/dev/compatibility.md](docs/dev/compatibility.md)）。