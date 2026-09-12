# 源码地图

## 顶层结构

```text
apps/flutter_app/                  Flutter 壳、页面、游戏管理和本地化
bridge/engine_api/                 C ABI、引擎生命周期、帧接口和 JNI
bridge/flutter_engine_bridge/      Dart FFI、MethodChannel、IOSurface、SurfaceTexture
cpp/core/                          TJS2、存储、归档、渲染、字体、音频、视频和主循环
cpp/plugins/                       PSB、PSD、motionplayer、LayerEx 和其他 TJS 插件
build.sh、build/                   iOS、Android、macOS 构建脚本
CMakeLists.txt、CMakePresets.json  CMake 目标和平台预设
vcpkg.json、vcpkg/                 依赖、端口和平台 triplet
docs/                              用户文档与开发文档
```

## 桥接层

### `bridge/engine_api/`

稳定的 C ABI 入口。`include/engine_api.h` 定义生命周期、启动、帧读取、输入、渲染目标和内存统计接口；`src/engine_api.cpp` 管理引擎实例、主循环和状态；Android JNI 文件负责 Surface 与 native window 的连接。

### `bridge/flutter_engine_bridge/`

Flutter 插件和 Dart 封装。`lib/src/ffi/` 绑定 C ABI，平台目录负责原生纹理，MethodChannel 作为 FFI 不可用时的后备通道。

## 引擎核心

| 目录 | 职责 |
|---|---|
| `cpp/core/tjs2/` | TJS2 词法、编译、字节码加载、解释执行和内置对象 |
| `cpp/core/base/` | 存储、XP3/ZIP/7z/TAR、事件、消息和 KAG 解析 |
| `cpp/core/environ/` | 引擎引导、主循环、系统对象、配置和平台抽象 |
| `cpp/core/visual/` | Bitmap、Layer、字体、图像解码、转场、渲染和 SIMD |
| `cpp/core/sound/` | Wave、CDDA、MIDI、解码、混音和音频效果 |
| `cpp/core/movie/` | FFmpeg 解封装、解码、播放时钟和视频层接口 |
| `cpp/core/plugin/` | 插件加载、生命周期和 ncbind 宿主 |
| `cpp/core/utils/` | 线程、定时器、编码、数学、容器和调试工具 |

重点入口：

- 渲染：`cpp/core/visual/LayerIntf.cpp`、`LayerBitmapImpl.cpp`、`RenderManager*.cpp`。
- 标量像素基准：`cpp/core/visual/tvpgl.cpp`。
- 字体：`FontSystem.*`、`FreeType*`、`PrerenderedFont.*`、`CharacterData.*`。
- 生命周期：`cpp/core/environ/EngineBootstrap.*`、`EngineLoop.*`、`Application.*`。
- 存储与脚本：`StorageIntf.*`、`StorageImpl.*`、`ScriptMgnIntf.*`、`KAGParser.*`。

`win32` 目录在音频、线程、定时器和系统控制中包含跨平台共享实现，不能按目录名称删除。

## 插件

| 目录 | 职责 |
|---|---|
| `cpp/plugins/psbfile/` | PSB 归档、图像资源、运动数据和元数据 |
| `cpp/plugins/psdfile/` | PSD 图层和资源解析 |
| `cpp/plugins/motionplayer/` | PSB/M2 节点、时间轴和动态资源播放 |
| `cpp/plugins/layerex_draw/` | 扩展路径和绘制操作 |
| `cpp/plugins/fstat/` | 文件状态查询 |
| `cpp/plugins/cubism/` | 可选 Live2D SDK 接入 |
| `cpp/plugins/zcompat/` | Z 相关名称映射和兼容桩 |

插件目标和源文件边界以各级 `CMakeLists.txt` 为准。新增 TJS 能力时，优先确认是核心 API、插件 API 还是脚本兼容层。

## Flutter 前端

- `lib/main.dart`：应用入口、初始化和路由。
- `lib/engine/`：引擎状态机与桥接适配。
- `lib/widgets/engine_surface.dart`：纹理显示、尺寸同步、输入和帧更新。
- `lib/pages/`：首页、游戏页、设置和元数据页面。
- `lib/services/`：游戏库、封面、元数据和统计服务。
- `lib/l10n/`：中文、英文和日文资源。

页面不应直接操作 C ABI；引擎相关调用集中在 `engine/` 和桥接插件中。

## 修改路径

- 修改引擎行为：`cpp/core/`。
- 增加脚本可调用能力：`cpp/core/plugin/` 或 `cpp/plugins/`。
- 修改平台与生命周期：`cpp/core/environ/`、`bridge/engine_api/`。
- 修改纹理与输入：`bridge/flutter_engine_bridge/`、`engine_surface.dart`。
- 修改页面和游戏管理：`apps/flutter_app/lib/pages/`、`services/`。
- 修改构建依赖：`vcpkg.json`、`vcpkg/ports/`、`vcpkg/triplets/`。
