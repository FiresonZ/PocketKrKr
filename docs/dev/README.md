# 开发文档

本目录提供 PocketKrKr 的架构、构建、源码和兼容性资料。内容以当前代码状态为准，历史排查过程不在这里保存。

## 阅读顺序

1. [入门指南](getting-started.md)：快速了解项目、构建和运行。
2. [架构](architecture.md)：查看引擎、桥接和渲染数据流。
3. [源码地图](source-map.md)：按目录定位模块职责。
4. [关键引用](key-references.md)：查找主要文件和 C ABI 符号。
5. [构建](build.md)：查看平台工具链、产物和 CI。
6. [约定](conventions.md)：修改平台、生命周期、链接和 SIMD 代码前必读。
7. [兼容性](compatibility.md)：执行游戏兼容性回归。
8. [渲染诊断](rendering-diagnosis.md)：排查黑屏、停帧和显示链路问题。
9. [优化路线](optimization-roadmap.md)：查看性能、稳定性和代码结构优化方案。
10. [待办](todo.md)：查看当前未完成事项。

## 项目边界

```text
apps/flutter_app/                  Flutter 壳和用户界面
bridge/engine_api/                 C ABI、生命周期和帧接口
bridge/flutter_engine_bridge/      Dart FFI、MethodChannel、原生纹理桥接
cpp/core/                          TJS2、存储、渲染、音频、视频和生命周期
cpp/plugins/                       PSB、PSD、motionplayer、LayerEx 等插件
build.sh、build/                   平台构建入口
vcpkg.json、vcpkg/                 依赖、端口和 triplet
docs/                              用户文档和开发文档
```

## 平台形态

| 平台 | 引擎产物 | 图形路径 |
|---|---|---|
| iOS | 静态库，链接进 Runner | ANGLE Metal、IOSurface，保留 RGBA 回读兜底 |
| Android | 自包含 `libengine_api.so` | ANGLE Vulkan、SurfaceTexture，保留 RGBA 回读兜底 |
| macOS | `libengine_api.dylib` | ANGLE Metal、IOSurface |
| Linux | 宿主验证构建 | CI 验证，不提供应用包 |

Android 的插件源码通过目标源传播进引擎共享库，使用普通链接即可；不要使用 `--whole-archive`。

## 参考资料

外部实现、格式资料和行为对照入口统一收录在 [krkrz-compat.md](krkrz-compat.md)。参考资料只用于理解协议和行为，不代表 PocketKrKr 已具备对应能力，也不直接复制外部代码。
