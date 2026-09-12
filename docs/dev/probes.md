# 诊断探针清单

本文登记源码中的诊断探针。所有诊断探针统一由 CMake 选项 `ENABLE_RENDER_PROBE` 控制，并通过编译宏 `KRKR_RENDER_PROBE` 实现；默认值为 `OFF`。关闭时，诊断专用日志和扫描不应执行。

## 使用方式

工作流中的 `enable_render_probe` 是全局开关，不只控制黑屏诊断。开启后会同时启用渲染、字体、预渲染字体、PSB 解析和 MotionPlayer 诊断探针。

```bash
cmake -DENABLE_RENDER_PROBE=ON <其他参数>
```

Android、iOS 和 macOS 构建脚本会在环境变量 `ENABLE_RENDER_PROBE` 明确存在时显式传递 `ON` 或 `OFF`，确保切换状态时重新配置 CMake。诊断完成后应使用 `OFF` 重新配置并构建。

## 探针类型

| 类型和前缀 | 作用 | 位置 | 频率与成本 |
|---|---|---|---|
| `StorageExec`、`StartupProbe`、`EngineState` | 定位脚本启动、存储执行和引擎状态变化 | `cpp/core/base/ScriptMgnIntf.cpp` | 启动或状态边沿；低成本 |
| `RequestUpdate` | 判断图层变化是否请求窗口更新 | `cpp/core/visual/WindowIntf.cpp` | 受限频率；低成本 |
| `DeliverWinUpdate`、`ContinuousProbe` | 判断窗口更新事件是否投递、处理和持续运行 | `cpp/core/base/EventIntf.cpp` | 受限频率；低成本 |
| `TimerProbe` | 检查计时器回调是否持续触发 | `cpp/core/utils/win32/TVPTimer.cpp` | 受限频率；低成本 |
| `BasicShow` | 记录基础绘制设备的异常显示路径 | `cpp/core/visual/impl/BasicDrawDevice.cpp` | 异常路径；低成本 |
| `RTProbe`、`UpdateDrawBuffer` | 检查渲染目标、绘制缓冲和图层合成状态 | `cpp/core/environ/stubs/ui_stubs.cpp` | 受限频率；低到中成本 |
| `SourceSample` | 采样引擎源纹理，判断是否写入有效像素 | `cpp/core/environ/stubs/ui_stubs.cpp` | 采样触发时；有像素读取成本 |
| `BlackScreen` | 汇总连续黑屏时的绘制与目标状态 | `cpp/core/environ/stubs/ui_stubs.cpp` | 黑屏状态触发；中成本 |
| `IOSurfacePixelSample` | 判断共享 IOSurface 是否收到有效像素 | `bridge/engine_api/src/engine_api.cpp` 及 Apple 桥接路径 | 采样触发时；有回读成本 |
| `engine_tick` | 判断桥接帧调用是否进入并返回 | `bridge/engine_api/src/engine_api.cpp` | 帧级诊断；开启时有日志成本 |
| `[FontProbe]` | 对比请求字体、实际字体和 ascent 等度量，定位字体基线或字号缩放异常 | `cpp/core/visual/FreeTypeFontRasterizer.cpp` | 字体应用时；低成本 |
| `[TpfMap]` | 确认字体 face/字号映射到的 `.tpf` 路径 | `cpp/core/visual/impl/LayerBitmapImpl.cpp` | 映射时；低成本 |
| `[TpfProbe] HIT/MISS` | 确认字符是否命中预渲染字体以及缺字回退信息 | `cpp/core/visual/impl/LayerBitmapImpl.cpp` | 按 face、字号、字符和结果去重；低成本 |
| `[TextProbe] single/multi` | 对比文本目标矩形、绘制位置、ascent 偏移和预渲染字体路径 | `cpp/core/visual/impl/LayerBitmapImpl.cpp` | 文本绘制时；受控日志成本，不执行额外像素扫描 |
| `PSB frameColor` | 检查 PSB 帧颜色字段是否存在及其四角颜色解析结果 | `cpp/plugins/psbfile/PSBMedia.cpp` | 目标 PSB 帧解析时；低成本 |
| `loadProbe` | 对比 MotionPlayer 资源的实际加载尺寸和 PSB 缓存元数据 | `cpp/plugins/motionplayer/Player.h` | 目标 m2logo 线条节点加载时；低成本 |
| `m2foldProbe` | 检查 M2 折叠部件的最终角点包围盒、锚点和变换结果 | `cpp/plugins/motionplayer/Player.h` | 目标图标且达到折叠阶段；有几何计算和日志成本 |
| `drawAnimatedTree stencil` | 记录受控的 stencil 离屏层准备和合成结果 | `cpp/plugins/motionplayer/Player.h` | stencil 合成时；低到中成本 |
| `applyStencilComposite: RGB-rotation alpha recovered` | 记录蒙版从 RGB 恢复 alpha 的异常兼容路径 | `cpp/plugins/motionplayer/Player.h` | 仅发生时；低成本 |

## 已移除探针

以下日志会按节点或按帧输出大量内容，且没有稳定的长期诊断价值，已删除：MotionPlayer 的 `param`、`cp`、`mesh`、`motionDt`、`strclip`、`viewportClip`、`rawColor` 以及颜色处理中的逐像素统计。对应的参数化动画、网格变形、裁剪、颜色处理仍然是正常功能代码。

## 维护规则

- 新增或修改诊断探针必须使用 `KRKR_RENDER_PROBE`，不能另设模块级默认开启的日志开关。
- 探针默认关闭，不能改变渲染结果、事件顺序、线程调度、资源生命周期或错误处理。
- 高频日志必须采样、限频、去重或只记录状态边沿；逐像素扫描只能在明确诊断需要时启用。
- 日志不得记录完整用户文本、个人路径、设备隐私或完整游戏资源内容；路径和文本应限制为必要的标识。
- 探针前缀、触发条件、源码位置和性能成本必须同步更新本文及英文版清单。
- 关闭探针时要重新配置构建目录，确认编译命令不再包含 `KRKR_RENDER_PROBE`。
