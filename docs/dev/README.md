# 开发文档

本目录提供 PocketKrKr 的架构、构建、源码和兼容性资料。内容以当前代码状态为准，历史排查过程不在这里保存。

## 入口

- [文档分类地图](document-map.md)：按主题选择规范文档，避免重复记录。
- [文档写入规则](documentation-rules.md)：规定内容边界、事实状态、双语维护和占位页要求。

## 分类阅读

### 入门与事实

- [入门指南](getting-started.md)：快速了解项目、构建和运行。
- [技术栈](tech-stack.md)：查看主要语言、框架、依赖和运行形态。
- [架构](architecture.md)：查看引擎、桥接和渲染数据流。

### 源码与接口

- [源码地图](source-map.md)：按目录定位模块职责。
- [关键引用](key-references.md)：查找主要文件和 C ABI 符号。
- [API 契约](api-contracts.md)：记录稳定的 C ABI、Dart FFI、TJS 和插件接口。

### 构建与平台

- [构建](build.md)：查看平台工具链、产物、预设和 CI。

### 开发规则

- [开发约定](conventions.md)：修改平台、生命周期、链接、Live2D 或 SIMD 代码前必读。

### 兼容性

- [兼容性](compatibility.md)：执行游戏兼容性回归。
- [插件兼容清单](plugin-compatibility.md)：查看插件状态、缺失能力、兼容桩和参考实现入口。
- [AetherKiri 对照审计](aetherkiri-audit.md)：查看同源实现、测试、诊断和兼容性差异矩阵。
- [KiriKiri Z 兼容参考](krkrz-compat.md)：查看外部实现、格式资料和协议入口。

### 测试与验收

- [测试夹具](test-fixtures.md)：登记可重复准备的游戏、脚本、资源、字体和渲染输入。
- [兼容性](compatibility.md)：记录回归矩阵和验收要求。

### 诊断与问题

- [探针清单](probes.md)：查看统一开关、探针类型、日志前缀和源码位置。
- [渲染诊断](rendering-diagnosis.md)：排查黑屏、停帧和显示链路问题。
- [问题记录](incident-reports.md)：记录已经确认边界的问题及其验证结果。

### 工具与分析

- [开发工具](tools.md)：使用 XP3 解包和 TJS2/TJS2100 反编译分析工具。

### 规划与优化

- [待办](todo.md)：查看当前未完成事项。
- [优化路线](optimization-roadmap.md)：查看已确认的长期优化方向。
- [性能优化](perf-optimization.md)：查看性能评估原则和验收方法。

### 发布与变更

- [发布变更](release-notes.md)：记录版本范围、用户影响、兼容性影响和验证状态。

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
