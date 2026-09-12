# 入门指南

PocketKrKr 是面向移动端的 KiriKiri2 运行环境，由 C++ 引擎、C ABI 桥接层和 Flutter 壳组成。

## 目录

- `cpp/core/`：脚本、存储、图层、字体、音频、视频和生命周期。
- `cpp/plugins/`：PSB、PSD、motionplayer、LayerEx 等扩展。
- `bridge/`：C ABI、Dart FFI、MethodChannel 和原生纹理桥接。
- `apps/flutter_app/`：页面、游戏库、设置和引擎显示组件。

详细职责见 [源码地图](source-map.md)，数据流见 [架构](architecture.md)。

## 构建

| 目标 | 命令 | 要求 |
|---|---|---|
| iOS | `./build.sh ios debug` | macOS、Xcode、Flutter、vcpkg |
| Android | `./build.sh android debug` | NDK、JDK 17、Flutter、vcpkg |
| macOS | `./build.sh macos debug` | macOS、Xcode、Flutter、vcpkg |
| Linux 验证 | `cmake --preset "Linux Debug Config" && cmake --build --preset "Linux Debug Build"` | CMake、Ninja、vcpkg |

完整工具链、产物和 CI 说明见 [构建](build.md)。

## 运行游戏

Flutter 壳支持游戏目录和 XP3 资源包。导入后选择游戏，壳层创建引擎实例并打开启动脚本；引擎负责脚本执行、资源读取和逐帧渲染。

引擎输出优先使用原生纹理共享，无法使用时回退到 RGBA 回读。系统要求和安装方式见项目根目录 [下载页面](../download.md)。

## 修改代码

1. 先阅读 [约定](conventions.md) 和 [待办](todo.md)。
2. 根据改动类型选择 `cpp/core/`、`cpp/plugins/`、`bridge/` 或 Flutter 页面。
3. 保持 C ABI、平台边界和资源所有权不变。
4. 运行对应平台的构建和测试；渲染、字体、缓存、音频和生命周期改动必须做目标平台回归。

常用入口：

| 任务 | 位置 |
|---|---|
| 图层、字体、像素混合 | `cpp/core/visual/` |
| 脚本和 KAG 行为 | `cpp/core/tjs2/`、`cpp/core/base/KAGParser.*` |
| PSB/M2 动画 | `cpp/plugins/psbfile/`、`cpp/plugins/motionplayer/` |
| C ABI 与引擎生命周期 | `bridge/engine_api/` |
| Flutter 纹理与输入 | `apps/flutter_app/lib/widgets/engine_surface.dart`、`bridge/flutter_engine_bridge/` |
| 依赖和平台工具链 | `vcpkg.json`、`vcpkg/ports/`、`vcpkg/triplets/` |

## 诊断

黑屏、停帧和视频问题使用 [渲染诊断](rendering-diagnosis.md)。性能和结构优化使用 [待优化方案](optimization-roadmap.md)。
