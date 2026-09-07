# PocketKrKr — 源代码结构地图（CPP + Flutter 速查）

> 目标读者：想**学习本项目并参与开发**的个人开发者。
> 用法：先看下方整体树抓住骨架，再按模块深入；每段"树 + 说明"告诉你**这个目录/文件负责什么、该怎么读**。
> 建议搭配：[architecture.md](architecture.md)（数据流/桥接）、[key-references.md](key-references.md)（关键符号）、
> [for-beginners.md](for-beginners.md)（小白入门）、[build.md](build.md)（构建）。
> 树中 `//` 后的文字是对该目录/文件职责的速记。

## 0. 顶层一览

```
PocketKrKr/            (= 仓库根目录)
├── apps/flutter_app/         // Flutter 壳应用（UI、游戏管理、本地化）
│   └── lib/                  //   Dart 前端源码（main + pages/services/...）
├── bridge/
│   ├── engine_api/           // C ABI 引擎桥（engine_create/tick/destroy/...）
│   └── flutter_engine_bridge// Flutter 平台插件（IOSurface/SurfaceTexture + FFI）
├── cpp/
│   ├── core/                 // C++ 引擎核心（TVP/TJS2）
│   ├── plugins/              // TJS 插件（psb/psd/layerex/motionplayer/fstat/...）
│   └── external/             // 内置三方（libbpg、minizip）
├── build.sh + build/*.sh     // 一键构建
├── CMakeLists.txt+CMakePresets.json // MacOS/iOS/Android/Linux 预设
├── vcpkg.json + vcpkg/       // 依赖与 triplet
└── docs/                     // 文档（GitHub Pages + dev/ 开发文档）
```

---

## 1. bridge/ — C ABI 桥接层（C++ 与 Dart 的交界）

```
bridge/
└── Flutter 壳  ──Dart FFI / MethodChannel──>  engine_api(C ABI)  ──>  TVP 引擎
```

### 1.1 `engine_api/`
稳定 C ABI，是 Dart 侧能调用的**唯一入口**。头部定义了版本号 `ENGINE_API_VERSION`，保证 ABI 兼容。

```
engine_api/include/engine_api.h        // C 头：引擎生命周期/输入/帧读取/内存统计的函数原型
engine_api/include/engine_options.h    // 引擎启动参数结构（窗口大小、存档路径、是否开调试等）
engine_api/src/engine_api.cpp          // C ABI 实现：engine_create/tick/destroy、ReadFrameRgba 回读、
                                       //   输入转发、探针上报（黑屏诊断）等
engine_api/src/engine_api_android_jni.cpp // Android JNI 胶水：Kotlin 驱动 surface 初始化 & 生命周期
```

> 阅读入口：`engine_api.h` 的函数签名（约十几个函数）→ 逐个到 `.cpp` 看实现。改动接口时 **必须同步 bump 版本号**。

### 1.2 `flutter_engine_bridge/`
Flutter 平台插件包（plugin），把引擎的零拷贝纹理接到 Flutter，同时暴露 FFI 绑定。

```
lib/src/ffi/engine_bindings.dart      // FFI 绑定：engine_api.h 的 Dart 声明（指针/结构体/回调）
lib/src/ffi/engine_ffi.dart           // 加载 dylib/.so/RN，封装调用（FFI 优先路径）
lib/flutter_engine_bridge.dart        // 对外主类：给 Flutter 壳用的高层 API
lib/flutter_engine_bridge_platform_interface.dart // 平台通道抽象
lib/flutter_engine_bridge_method_channel.dart     // MethodChannel 兜底实现
ios/Classes/FlutterEngineBridgePlugin.swift  // iOS/macOS：IOSurface 纹理注册给 Flutter
macos/Classes/FlutterEngineBridgePlugin.swift  // 同上（macOS 版本）
android/.../FlutterEngineBridgePlugin.kt      // Android：SurfaceTexture(JNI) 纹理注册
```

> 阅读顺序：`flutter_engine_bridge.dart`（对外）→ `engine_ffi.dart`（加载/调用）→ 平台 `.swift/.kt`（纹理）。

---

## 2. cpp/core/ — C++ 引擎核心（最重要的部分）

```
cpp/core/
├── base/     // 引擎基础：存档/字节流/字符集/KAG/事件/脚本/存储接口
├── tjs2/     // TJS2 脚本语言虚拟机（解释器/编译器/字节码/Core 对象）
├── environ/  // 应用生命周期、主循环、平台抽象、全局配置
├── visual/   // 渲染：Bitmap/Layer/字体/图像编解码 + GL/OGL/SIMD
├── sound/    // 音频：Wave/CD/MIDI/解码/效果
├── movie/    // 视频（ffmpeg 播放）
├── plugin/   // 插件宿主：ncbind 双向绑定
└── utils/    // 底层工具：容器/线程/编码/数学/RNG/定时器
```

### 2.1 `base/` — 引擎基础设施
存放"引擎执行需要但非渲染"的通用能力，多为 `Intf`(接口) + `Impl`(平台实现) 成对出现。

```
base/impl/                    // 各平台的实现（iOS/Android/Windows 共用一套源，靠宏守卫）
  EventImpl.*                 // 事件系统（原生键盘/触摸 → TVP 事件）
  NativeEventQueue.*          // 原生事件队列
  MsgImpl.*                   // 弹窗/Message 实现
  FileSelector.*              // 文件选择框
  ScriptMgnImpl.*             // 脚本管理（脚本块缓存、全局作用域）
  StorageImpl.*               // 磁盘存储（存档/读取文件流）
  SysInitImpl.* / SystemImpl.*// 引擎初始化与系统对象实现
base/StorageIntf.*            // 存储接口：Open/Exist/GetFileInfo 等（读存档/配包）
base/ScriptMgnIntf.*          // 脚本引擎接口：Eval/EvalExpression（TJS2 宿入口）
base/SystemIntf.*             // 系统层接口：延迟暂停/ExitingHandler/崩溃转储
base/EventIntf.* / MsgIntf.*  // 事件/消息接口
base/XP3Archive.* / ZIPArchive.* / 7zArchive.* / TARArchive.*
                              // 归档读取器：XP3（本体素材包）/ZIP/7z/TAR
base/KAGParser.*              // KAG（KAG3 脚本标签）解析
base/BinaryStream.* / TextStream.* / CharacterSet.*
                              // 字节流/文本流/字符集转换（GBK/JIS/UTF-8）
base/UtilStreams.*            // 工具流（包装/过滤流）
base/UserEvent.h              // 用户事件定义
```

### 2.2 `tjs2/` — TJS2 脚本语言虚拟机
让游戏脚本（`.tjs`/`.ks`）能跑起来的核心。包含词法→语法→字节码→解释执行全链路。

```
tjs2/bison/*.y                // 语法：tjs.y（TJS 主语法）、tjsdate.y（日期）、tjspp.y（预处理）
tjs2/script/                  // 生成世界地图表 + 停用词表（词法辅助）
  tjs.cpp / tjs.h             // 主入口：VM 从句柄启动、字符串/脚本注册
  tjsLex.*                    // 词法分析（lexer）
  tjsInterCodeGen.*           // 中间代码生成（字节码结构）
  tjsInterCodeExec.*          // 字节码解释执行（大跳转表 VM）
  tjsCompileControl.*         // 编译控制（优化开关、即时编译配置）
  tjsByteCodeLoader.*         // 字节码加载/反序列化（运行已编译脚本）
  tjsScriptBlock.*            // 脚本块（源码→代码块的编译单元）
  tjsScriptCache.*            // 脚本编译缓存
S 内置对象：
  tjsObject.*                 // 对象/原型链（TJS 一切皆派生）
  tjsObjectExtendable.*       // 可扩展对象
  tjsArray.* / tjsConstArrayData.* / tjsDictionary.*  // Array/ConstArray/Dictionary
  tjsVariant.* / tjsVariantString.* // Variant 万能值 & 字符串
  tjsString.*                 // 字符串（UTF-16、写时复制/全局映射）
  tjsGlobalStringMap.*        // 全局字符串驻留表（省内存）
  tjsFunction(经 Native/Interface)  // 见 tjsNative.* 原生方法绑定
  tjsMath.* / tjsRandomGenerator.*  // 数学 & 随机（MT19937：tjsMT19937ar-cok）
  tjsDate.* / tjsDateParser.* // 日期与解析
  tjsRegExp.*                 // 正则
  tjsException.* / tjsError.* // 异常/错误
  tjsDebug.* / tjsDisassemble.* // 调试器 & 反汇编（看字节码用）
  tjsBinarySerializer.*       // 二进制序列化（跨会话对象持久化）
  tjsNative.*                 // 原生(native)方法/类注册路径
  tjsInterface.* / tjsNamespace.*  // interface 与命名空间
  tjsConfig.*                 // VM 配置
  tjsOctPack.*                // 位打包工具
```

> 学习建议：跑一个"Hello"脚本，跟随 `tjs.cpp` → lex → 编译 → `tjsInterCodeExec` 执行，是理解 TJS2 最快路径。

### 2.3 `environ/` — 生命周期、主循环与平台
引擎"从开机到跑起来"的骨架，以及跨平台抽象层。

```
environ/EngineBootstrap.*     // 引擎引导：加载配置、初始化子系统顺序
environ/EngineLoop.*          // 主循环：每帧执行脚本定时器/事件/渲染 tick
environ/MainScene.*           // 主场景（舞台）宿主
environ/Application.*         // App 应用对象
environ/GlobalConfigManager.* // 全局配置（ini 读写）
environ/IndividualConfigManager.* // 每游戏配置
environ/LocaleConfigManager.* // 多语言配置
environ/DumpSend.cpp          // 崩溃转储发送
environ/DetectCPU.*           // CPU 特性检测（决定 SIMD 派发）
environ/XP3ArchiveRepack.*    // XP3 重打包工具
environ/Platform.h            // 平台抽象头
environ/android/platform_android.cpp // Android 平台实现
environ/apple/ios/platform.mm        // iOS 平台实现（Objective-C++）
environ/apple/macos/platform.mm      // macOS 平台实现
environ/sdl/tvpsdl.cpp        // SDL 平台实现（CI/Linux 宿主、SDL 窗口）
environ/stubs/platform_linux.cpp    // Linux CI 宿主验证用 stub
environ/stubs/ui_stubs.cpp    // 无窗口 stub：探针上报（UpdateDrawBuffer → draw 状态）
environ/win32/                // Win32 共享实现（音频/线程/系统控制，跨平台复用，勿删）
environ/typedefine.h / cpu_types.h / vkdefine.h / combase.h  // 基础类型/平台宏
```

> `ui_stubs.cpp` 里能看到黑屏探针（`SourceSample/PostBlit/draw/BlackScreen` 上报），排查黑屏从这里入手。

### 2.4 `visual/` — 渲染（最复杂、坑最多）
Bitmap/图层/文字/图像加载/OpenGL 渲染/SIMD 混合，全都在这。

```
visual/BitmapIntf.*           // 位图接口（Surface/图层位图）
visual/LayerIntf.*            // 图层接口（场景里一层一层的东西）
visual/LayerManager.* / LayerTreeOwnerImpl.* // 图层树管理
visual/BasicDrawDevice.*      // 基础绘制设备（CPU 软绘）
visual/DrawDevice.*           // 绘制设备抽象（软/GL 选择）
visual/LayerBitmapImpl.* / LayerImpl.*  // 图层/位图实现
visual/ImageFunction.*        // 图像处理函数（缩放/混合）
visual/Load*.cpp              // 图像解码：LoadPNG/WEBP/JPEG/TLG/BPG/JXR/AMV/PVRv3
visual/tvpgl.cpp / tvpgl.h    // 像素级混合函数（对照 SIMD 的标准标量实现！）
visual/argb.*                 // ARGB 像素格式与操作
visual/FontSystem.* / FreeType* / NativeFreeTypeFace.* / GDIFontRasterizer.*
                              // 字体系统：FreeType 光栅化 + 系统字体
visual/CharacterData.*        // 字符数据（字形）
visual/PrerenderedFont.*      // 预渲染字体（提速）
visual/RenderManager.* / RenderManager_software.h  // 渲染管理（合成/提交）
visual/VideoOvlImpl.* / VideoOvlIntf.*  // 视频覆盖层
visual/WindowImpl.* / WindowIntf.*      // 窗口抽象
visual/TransIntf.*            // 转场
visual/SaveTLG5/6.*           // TLG（KiriKiri 专有位图）写出
visual/gl/                    // GL 标量像素函数：blend_function/blend_functor_c 等
visual/ogl/                   // OpenGL 后端：krkr_egl_context(EGL+ANGLE)、krkr_gl、纹理、
                              //   etcpak/astc/pvrtc 压缩纹理、图像打包 imagepacker
visual/simd/                  // Highway SIMD 像素混合：tvpgl_simd_*（alpha/arithmetic/blur/
                              //   const/convert/copy/misc/premul/ps_blend）tvpgl_simd_init 派发
```

> ⚠️ 高亮：`tvpgl.cpp` 的 `*_c` **标量**实现是正确基准；`simd/*` 曾检出 23 处与标量不一致，
> 高危模式已指回标量。**改混合公式前先比对标量**（见 conventions.md）。

### 2.5 `sound/` — 音频
```
sound/WaveIntf.* / SoundBufferBaseIntf.* / CDDAIntf.* / MIDIIntf.*  // 接口（波形段/CD/MIDI）
sound/WaveLoopManager.*     // 波形循环管理
sound/WaveSegmentQueue.*    // 波形段队列（流式/叠加）
sound/WaveFormatConverter.* // 格式转换（+ SSE 版）
sound/FFWaveDecoder.* / VorbisWaveDecoder.*  // WAV / Vorbis 解码
sound/PhaseVocoderDSP.* / PhaseVocoderFilter.*  // 变调不变速（相位声码器）
sound/MathAlgorithms.*      // 数学算法
sound/xmmlib.cp             // 播放内核（MCI/DSound 时代遗留的共享混合内核）
sound/win32/*               // 平台音频：tvpsnd 设备、WaveImpl、Mixer
```

### 2.6 `movie/ffmpeg/` — 视频播放（媒体框架）
```
movie/ffmpeg/ 是重构的播放框架（源自 Kodi 风格分层）：
  Demux*                 // 解封装（DemuxFFmpeg）
  VideoPlayer* / VideoPlayerVideo / VideoPlayerAudio  // 播放器与音视频线程
  VideoRenderer* / VideoReferenceClock* / VideoCodec / AudioCodec  // 渲染/时钟/编解码
  KRMoviePlayer / KRMovieLayer  // KiriKiri 电影层 API 接入
  BitstreamStats / CodecUtils / Thread / Timer / MessageQueue  // 基座工具
  krffmpeg.*             // 入口：把播放器注册成 TVP 电影引擎
```

### 2.7 `plugin/` — 插件宿主与 ncbind
```
plugin/ncbind.cpp / ncbind.hpp  // ncbind：用模板让 C++ 函数/类自动暴露给 TJS
plugin/PluginImpl.* / PluginIntf.*  // 插件加载（.tjs / .dll 插件）与生命周期
plugin/ncb_foreach.inc        // ncbind 遍历辅助
```

### 2.8 `utils/` — 底层工具
```
utils/ObjectList.h / Defer(common)   // 容器/RAII
utils/ThreadIntf.* / TimerIntf.* / TickCount.* // 线程/定时器/毫秒时钟
utils/ClipboardIntf.* / DebugIntf.* / PadIntf.*// 剪贴板/调试输出/外设
utils/CharacterSet / encoding/iconv   // 字符集（gbk2unicode/jis2unicode）
utils/MathAlgorithms_Default.* / RealFFT_Default.*  // 数学/FFT 软实现
utils/Random.* / md5.c  // 随机/哈希
utils/VelocityTracker.* // 手势速度跟踪
utils/win32/            // 平台共享实现（ThreadImpl/TimerImpl/Clipboard 等）
```

---

## 3. cpp/plugins/ — TJS 脚本插件（给游戏加能力）
每个插件编译后由脚本 `Plugins.link` 加载，向 TJS 暴露类/函数。

```
plugins/psbfile/       // PSB（Live2D 专用二进制包）解析：ArchiveType/Image/Motion/...Type + PSBValue
plugins/psdfile/       // PSD 解析：psdparse 子目录分层文件/图层/资源
plugins/layerex_draw/  // LayerEx 扩展绘制：DrawPath(路径)、LayerExDraw(gdi 风格)
plugins/motionplayer/  // 动感播放器（小动画序列）：Player/EmotePlayer/ResourceManager
plugins/fstat/         // 文件状态/统计
plugins/json/          // JSON 读写绑定
plugins/kagparserex/   // KAG 解析扩展
plugins/steam/         // Steam 相关
plugins/DrawDeviceForSteam/ // Steam 绘制设备
plugins/cubism/        // Live2D Cubism（SDK 存在才编，缺库正常）
根目录散文件：           // 大量单文件插件/兼容层
  krkrlive2d.cpp / krkrgles.cpp / alphaMovie / alphamovie
  drawDeviceD2DCompat / textrender / scriptsEx / saveStruct / varfile
  wfBasicEffectCompat / wfTypicalDSPCompat / wutcwf / fftgraph / xp3filter
  extrans / csvParser / dirlist / getSample / getabout / addFont
  windowEx / win32dialog / layerEx*.cpp / simplebinder(simplebinder.hpp)
```

> 想加自定义功能时，最简做法是仿照任一插件写 `main.cpp` + `CMakeLists.txt`，用 ncbind 暴露类。

---

## 4. apps/flutter_app/ — Flutter 前端（UI 全在这）

```
apps/flutter_app/lib/
├── main.dart                    // App 入口：初始化引擎桥、路由、主题
├── engine/
│   ├── engine_bridge.dart       // 引擎状态机：create→open_game→tick 生命周期、加载 flutter_engine_bridge
│   └── flutter_engine_bridge_adapter.dart // 把平台插件适配成立即可用的桥
├── pages/
│   ├── home_page.dart           // 首页：游戏列表、导入
│   ├── game_page.dart           // 游戏页：承载 EngineSurface，运行引擎主循环
│   ├── game_detail_page.dart    // 游戏详情
│   ├── scrape_select_page.dart  // 元数据抓取选择
│   └── settings_page.dart       // 设置（版本、存储、调试开关）
├── widgets/
│   ├── engine_surface.dart      // 把 NativeTexture(Texture) 与手势 → engine_bridge 绑定的组件
│   └── performance_overlay.dart // 性能/FPS 覆盖层（调试）
├── services/
│   ├── game_manager.dart        // 游戏管理：导入 XP3/配包、扫描、启停
│   ├── cover_downloader.dart    // 封面下载
│   ├── game_metadata_scraper.dart // 元数据抓取
│   ├── vndb_client.dart         // VNDB API 客户端
│   └── first_open_analytics.dart// 首次打开统计
├── models/
│   ├── game_info.dart           // 游戏信息模型
│   └── game_metadata_candidate.dart // 抓取候选模型
├── config/                      // stats_base_url 等
├── constants/prefs_keys.dart    // SharedPreferences 键名集中管理
├── utils/xp3_utils.dart         // XP3 工具（校验/读取元数据）
└── l10n/                        // 多语言：app_zh/en/ja.arb + app_localizations*.dart
```

> 阅读顺序：`main.dart` → `engine/engine_bridge.dart`（掌握引擎生命周期）
> → `widgets/engine_surface.dart`（掌握纹理+输入如何接入）→ `pages/`（各页面）。
> 引擎相关只通过 `flutter_engine_bridge` + `engine_bridge.dart` 进出，别在页面里直接碰 FFI。

---

## 5. 给开发者的三条指引

1. **想先跑起来**：跟 [build.md](build.md)（`./build.sh`），iOS 静态库、Android `.so`、macOS dylib 各自成形。
2. **想加一个游戏脚本功能**：改 `cpp/`（引擎能力）→ bump 版本号；
   或写插件放 `cpp/plugins/`，用 ncbind 暴露给 TJS。
3. **想加一个 UI/页面**：全在 `apps/flutter_app/lib/pages` + `widgets`，需要引擎对象就拿 `engine_bridge.dart`。

> 保持同步：本文件随目录结构变动更新；改动平台目录时同步 [conventions.md](conventions.md)。