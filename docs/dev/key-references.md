# 关键引用

路径以仓库根目录为基准。模块编译边界以各级 `CMakeLists.txt` 和目标源列表为准。

## 构建与配置

| 路径 | 作用 |
|---|---|
| `CMakeLists.txt` | 根构建编排、工具链和模块入口 |
| `CMakePresets.json` | iOS、Android、macOS、Linux 预设 |
| `build.sh` | 平台构建统一入口 |
| `build/build_ios.sh` | iOS 静态库、依赖合并和 Flutter 构建 |
| `build/build_android.sh` | Android `.so`、JNI 和 APK 构建 |
| `vcpkg.json` | 依赖清单 |
| `vcpkg-configuration.json` | registry 和 overlay 配置 |
| `vcpkg/triplets/` | 平台 ABI 与编译选项 |

## C ABI 与 Flutter

- `bridge/engine_api/include/engine_api.h`：生命周期、启动、帧、输入、渲染目标和统计接口。
- `bridge/engine_api/src/engine_api.cpp`：C ABI 实现、运行时状态和主循环。
- `bridge/engine_api/src/engine_api_android_jni.cpp`：Android Surface/JNI 胶水。
- `bridge/flutter_engine_bridge/lib/src/ffi/engine_bindings.dart`：Dart FFI 类型绑定。
- `bridge/flutter_engine_bridge/lib/src/ffi/engine_ffi.dart`：动态库加载与调用封装。
- `apps/flutter_app/lib/engine/engine_bridge.dart`：Flutter 侧生命周期封装。
- `apps/flutter_app/lib/widgets/engine_surface.dart`：纹理显示、尺寸同步和输入。

## 引擎核心

- `cpp/core/tjs2/`：TJS2 VM、字节码、内置对象和异常。
- `cpp/core/base/`：存储、归档、事件、消息和 KAG。
- `cpp/core/environ/EngineBootstrap.*`：子系统初始化顺序。
- `cpp/core/environ/EngineLoop.*`：定时器、事件和每帧执行。
- `cpp/core/visual/LayerIntf.*`：图层接口和绘制提交。
- `cpp/core/visual/LayerBitmapImpl.*`：位图、字体和绘制资源。
- `cpp/core/visual/RenderManager*`：渲染目标与图形后端。
- `cpp/core/visual/tvpgl.cpp`：像素混合标量基准。
- `cpp/core/movie/ffmpeg/`：视频解码和播放调度。
- `cpp/core/plugin/`：插件宿主和 ncbind。

## 插件

- `cpp/plugins/psbfile/`：PSB 归档、图像、运动数据和元数据。
- `cpp/plugins/psdfile/`：PSD 图层和资源。
- `cpp/plugins/motionplayer/`：PSB/M2 时间轴和动态节点。
- `cpp/plugins/layerex_draw/`：扩展绘制。
- `cpp/plugins/zcompat/`：兼容名称映射和桩。
- `cpp/plugins/cubism/`：可选 Live2D SDK 接入。

## 修改前确认

1. 先确认目标属于 C ABI、Flutter、核心引擎、插件还是脚本兼容层。
2. 修改公共 C ABI 时同步检查 Dart binding 和版本约束。
3. 修改渲染或生命周期时检查 context、线程、所有权和重启路径。
4. 修改 SIMD 时以 `tvpgl.cpp` 的标量函数为基准，并运行逐像素对比。
