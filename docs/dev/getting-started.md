# PocketKrKr — 程序员入门指南

> 面向**刚接触本项目的开发者**：怎么理解项目、代码在哪、怎么跑起来、怎么装游戏、
> 怎么改代码与调试。适合"小白"，也适合会 C++ 的开发者 30 分钟上手。
> 深入内容见 [architecture.md](architecture.md)（架构）、[source-map.md](source-map.md)（源码地图）、
> [key-references.md](key-references.md)（关键符号）、[build.md](build.md)（构建）、
> [conventions.md](conventions.md)（约定与陷阱，**改代码前必读**）。

---

## 1. 项目是什么

一句话：**PocketKrKr = 在手机（iOS/Android）上运行"吉里吉里2"游戏引擎的现代运行环境。**

- **引擎**（C++，`cpp/core/`）：运行游戏脚本（TJS2）、解码图片/音频/视频、图层合成，输出逐帧像素。
- **壳**（Flutter，`apps/flutter_app/`）：窗口、界面、文件选择、游戏库管理，把引擎帧显示到屏幕。
- **桥**（`bridge/engine_api/`，C ABI + FFI）：引擎与壳之间的管道。

```
游戏目录/XP3 → Flutter 壳 → C++ 引擎解析脚本并渲染 → 纹理共享 → 屏幕
```

吉里吉里2（KiriKiri2 / TVP）是日式视觉小说最常用的引擎，很多老游戏用它编写。目标：让这些
PC 游戏能直接搬到手机上跑，并保持与原版脚本 100% 兼容。

## 2. 目录地图

```
apps/flutter_app/              Flutter 壳（Dart）。lib/pages 页面、lib/engine 引擎封装
bridge/engine_api/             桥核心 C ABI：include/engine_api.h 接口清单，src/engine_api.cpp 实现
bridge/flutter_engine_bridge/  Flutter 平台插件：Dart FFI + iOS/macOS Swift / Android Kotlin 纹理
cpp/core/                      引擎核心（最重要）
  tjs2/       TJS2 脚本引擎（bison 生成 parser）
  base/       存储/归档/事件/消息（XP3、7z、zip 解包）
  visual/     渲染：图层合成、图像编解码、字体、SIMD 像素混合
  environ/    平台抽象 + 主循环（iOS/macOS 在 environ/apple/）
  sound/      音频（OpenAL）
  movie/      视频（FFmpeg）
  plugin/     插件框架（ncbind 把 C++ 类暴露给 TJS）
cpp/plugins/  TJS 插件：PSB/PSD/Live2D(layerex/motionplayer/fstat…) 等
build.sh      一键构建（ios / android / macos）
vcpkg.json    依赖管理（manifest + overlay ports/triplets）
docs/         对外文档（GitHub Pages + dev/ 开发文档）
.github/workflows/   CI（iOS 打 IPA、Android 打 APK、Linux 引擎验证）
```

**按需改哪：**
- 改**界面** → `apps/flutter_app/lib/`
- 改**引擎逻辑/兼容性** → `cpp/core/`
- 处理**依赖装不上** → `vcpkg.json` + `vcpkg/triplets/` + `vcpkg/ports/`

## 3. 引擎内置功能

- **脚本与存档**：TJS2 脚本引擎（`cpp/core/tjs2`）；KAG 标签解析（`cpp/core/base`，`KAGParser`）；
  XP3 解包支持目录散装 + 同目录多 xp3 自动挂载（`TVPAutoMountProjectXP3Archives`）。
- **渲染**：图层（Layer）系统；图像解码 PNG/JPEG/BMP/TLG(TLG5/6)/PSB；OpenGL 离屏合成（ANGLE）。
- **音频/视频**：WAV/OGG/Opus 解码；`krmovie`（ffmpeg）解码链路就绪，画面合成 Present 待办
  （见 [todo.md](todo.md)，仅播 OP/影片相关）。
- **内置插件**（`cpp/plugins`）：`psb`/`psd`/`layerex`/`motionplayer`/`fstat`/`cubism`(Live2D，
  按 SDK 存在性编译)/`drawDeviceD2DCompat`/`krkrgles`。
- **尚未内置（Z 时代缺口）**：`drawdeviceD3DZ`/`kztouch`/`k2compat`/`kagexopt`/`multiimage`/
  `squirrel` 会提示 `Failed`，属"与 Z 持平"目标，见 [todo.md](todo.md)。

## 4. 构建与运行

### 环境
| 目标 | 需要 |
|------|------|
| iOS / macOS | macOS + Xcode |
| Android | Windows/macOS/Linux + `ANDROID_NDK_HOME` |
| Linux / Windows 引擎验证 | 用 CMake 预设（无 App 产物，最快） |

### 一键构建
```bash
./build.sh ios release     # iOS → 无签名 IPA（侧载测试）
./build.sh android debug   # Android → APK
./build.sh macos debug     # macOS → App（开发最快）
JOBS=16 ./build.sh ios release   # 多核加速
```

### 手动 / 平台预设（CI 用、够快）
```bash
cmake --preset "Linux Debug Config" && cmake --build --preset "Linux Debug Build"
```
> 💡 只想快速验证"能不能跑/黑不黑"，优先用 Linux 或 Windows MinGW 预设编译引擎，
> 比 iOS/Android 快得多。完整构建说明见 [build.md](build.md)。

## 5. 装一款游戏

在 `apps/flutter_app/lib/pages/home_page.dart`：
1. **导入**：选一个**游戏目录**（需含 `startup.tjs` 或 `.xp3`），或一个 **`.xp3` 文件**。
2. **目录 ⇄ 单包互转**：目录可"打包为 XP3"（RAW 聚合不压缩），`.xp3` 可"解包 XP3"，互为逆操作。
3. 点进游戏，引擎启动并渲染。

## 6. 渲染原理：一帧怎么画出来的

```
每帧：
1. TJS 脚本改图层（背景/立绘/文字/位置）
2. 引擎把图层合成到一块内存纹理（主 DrawBuffer）
3. BasicDrawDevice::Show() 交给窗口
4. FlutterWindowLayer::UpdateDrawBuffer() blit 到 IOSurface/纹理 → Flutter 显示
```
桥的调用链（Dart→C++）：`FlutterEngineBridge.engineTick(deltaMs)` →
`engine_tick(handle, deltaMs)` → 引擎主循环 `Application->Run()` →
`engineGetFrameRenderedFlag()` → `notifyFrameAvailable(textureId)`。

- **零拷贝**：移动端走 GPU 直通（`GetNativeGLTextureId()` 非 0），省去 CPU 来回搬像素。
- **黑屏判断**：未被合成过的 DrawBuffer 初始是**不透明黑（0xFF000000）**——若源纹理始终
  `(0,0,0,255)` 且 draw 计数不涨，是"合成没发生"，多为绘图设备/插件未接上，而非普通渲染 bug。
  详见 [rendering-diagnosis.md](rendering-diagnosis.md)。

## 7. 改代码与调试

**姿势**：
1. 先读 [README.md](README.md)（索引）→ [conventions.md](conventions.md)（约定，**最重要**）。
2. 尤其注意：平台守卫别删、`win32/` 是共享实现不能删、SIMD 以标量为准、改代码同步更新文档。
3. 改动请标注理由 + 手动 `git commit`（提交信息写清"为什么"）。
4. 待办先看 [todo.md](todo.md)，避免重复。

**常见入口**：
| 想做什么 | 改哪 |
|---------|------|
| 修混合/滤镜 | `cpp/core/visual/simd/` 或 `tvpgl.cpp`（标量参考） |
| 修脚本引擎行为 | `cpp/core/tjs2/` |
| 加 TJS 可调用的类/函数 | `cpp/core/plugin/`（ncbind）或 `cpp/plugins/` |
| 改 iOS 平台行为 | `cpp/core/environ/apple/ios/platform.mm` |
| 改 Flutter UI | `apps/flutter_app/lib/pages/` |
| 加桥接 API | `engine_api.h` + `engine_api.cpp` + `bridge/flutter_engine_bridge/lib/src/ffi/engine_bindings.dart` |

**调试三板斧**：
1. **日志**：C++ 用 `spdlog`，Dart 用 `debugPrint`；启动日志可用 `engineDrainStartupLogs` 拉取。
2. **开关**：`tvpgl_simd_init.cpp` 里 `TVPGL_SIMD_Init()` 决定是否启用 SIMD——想对比标量 vs SIMD
   就注释掉注册行。
3. **真机**：`./build.sh ios debug` 后用 Xcode 打开 `apps/flutter_app/ios/Runner.xcworkspace` 跑真机。

## 8. 进阶路线

1. 读 [architecture.md](architecture.md)（架构图 + 渲染数据流）。
2. 读 [key-references.md](key-references.md)（关键文件/符号）。
3. 读 [conventions.md](conventions.md)（尤其 SIMD 审计记录，已知缺陷）。
4. 从一个小 bug 练手（如 SIMD 审计里 SubBlend 公式错误）。