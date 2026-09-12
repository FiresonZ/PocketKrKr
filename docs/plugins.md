# 插件与扩展

PocketKrKr 的插件分为三类：核心内建能力、随引擎编译的 C++ 插件、游戏侧加载的脚本扩展。名称能够被 `Plugins.link` 找到，不等于相关功能全部实现。

## 当前插件目录

| 目录或文件 | 作用 | 状态 |
|---|---|---|
| `cpp/plugins/psbfile/` | PSB 资源与动画解析 | 内建 |
| `cpp/plugins/psdfile/` | PSD 资源解析 | 内建 |
| `cpp/plugins/motionplayer/` | M2/PSB 动画播放 | 内建，复杂功能持续完善 |
| `cpp/plugins/layerex_draw/` | 扩展绘制 | 内建 |
| `cpp/plugins/fstat/` | 文件状态查询 | 内建 |
| `cpp/plugins/cubism/` | Live2D 接入 | 可选 SDK |
| `cpp/plugins/zcompat/` | 兼容名称映射和桩 | 部分能力待实现 |
| `cpp/plugins/*.cpp` | 各类 TJS 扩展 | 以 CMake 和注册表为准 |

## 实现边界

- 核心能力应放在 `cpp/core/`，例如 Layer、渲染、存储、字体和视频基础设施。
- 可复用的 TJS 扩展放在 `cpp/plugins/`，通过 ncbind 注册接口。
- 纯脚本兼容层保持在脚本资源边界，不把脚本状态混入 C++ 全局生命周期。
- 桌面专属绘制设备不能直接视为移动端实现；移动端应明确使用的图形后端和目标纹理路径。
- 可选 SDK 缺失时保持自动禁用，不能让可选功能阻断核心构建。

## 外部资料

历史插件名称、格式资料和外部仓库入口统一见 [兼容性与参考资料](dev/krkrz-compat.md)。该页面只作为资料入口和能力边界说明，不保证外部插件可直接加载。
