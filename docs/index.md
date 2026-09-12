---
hide:
  - toc
---

# PocketKrKr

<p align="center">
    <img src="resources/logo.png" alt="PocketKrKr" width="120">
</p>

**PocketKrKr** 是面向移动端的 KiriKiri2 运行环境，采用 C++ 引擎、Flutter 壳和稳定的 C ABI 桥接层。支持 iOS、Android 和 macOS 开发目标，Linux 用于引擎验证。

## 特性

<div class="grid cards" markdown>

- :material-gamepad-square-outline: **视觉小说运行环境**

    提供 TJS2、KAG、XP3、Layer、PSB/M2 动画和常用扩展能力。

- :material-lightning-bolt-outline: **多平台渲染**

    通过 ANGLE 使用 Metal 或 Vulkan，并支持 IOSurface、SurfaceTexture 和 RGBA 回读路径。

- :material-cellphone: **移动优先**

    iOS 和 Android 是主要目标，macOS 用于开发与调试。

</div>

## 平台

| 平台 | 图形后端 | 纹理路径 | 引擎形态 |
|---|---|---|---|
| iOS | Metal | IOSurface | 静态库链接进 Runner |
| Android | Vulkan | SurfaceTexture | 自包含 `libengine_api.so` |
| macOS | Metal | IOSurface | `libengine_api.dylib` |
| Linux | Vulkan | 宿主验证 | 不提供应用包 |

## 快速开始

```bash
./build.sh ios release
./build.sh android debug
./build.sh macos debug
```

完整工具链和产物说明见 [构建文档](dev/build.md)。

## 文档

- [入门指南](dev/getting-started.md)
- [架构](dev/architecture.md)
- [源码地图](dev/source-map.md)
- [关键引用](dev/key-references.md)
- [构建](dev/build.md)
- [开发约定](dev/conventions.md)
- [兼容性测试](dev/compatibility.md)
- [渲染诊断](dev/rendering-diagnosis.md)
- [待优化方案](dev/optimization-roadmap.md)
- [待办](dev/todo.md)
- [下载](download.md)
- [插件与扩展](plugins.md)
- [支持的游戏](support_games.md)

## 参考资料

格式、协议和行为对照资料统一见 [兼容性与参考资料](dev/krkrz-compat.md)。

## 许可证

本项目使用 GPL-3.0，详见 [LICENSE](../LICENSE)。
