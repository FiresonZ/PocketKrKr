# AetherKiri 对照审计

## 目的

本文记录 PocketKrKr 与 AetherKiri 的同源代码对照结果，用于筛选可借鉴的行为、测试和诊断方法。AetherKiri 的 Godot 产品壳、多运行时 Provider 和商业功能不属于 PocketKrKr 的直接缺口；只有经过本项目架构、平台和兼容性验证的项目才进入实现计划。

参考仓库：<https://github.com/AetherKiri/AetherKiri>

审计范围：AetherKiri 当前公开仓库的核心目录、构建入口、插件、测试、诊断工具和同源 C++ 渲染/字体实现；PocketKrKr 对应的 `cpp/`、`bridge/`、`apps/`、`build/` 和 `docs/`。

## 状态定义

- **已有**：PocketKrKr 已具备等价能力，不需要移植。
- **部分**：已有基础实现，但缺少验证、边界处理或统一入口。
- **候选**：AetherKiri 的实现有明确参考价值，但尚未证明适合本项目。
- **不适用**：属于 AetherKiri 的 Godot、多运行时或产品层架构。
- **风险**：不能直接复制，必须先建立基准和目标平台回归。

## 总体结论

1. 两个项目共享 KiriKiri2 核心和大量 `cpp/core`、`cpp/plugins` 代码，但当前文件已产生较大分叉，不能按文件整体替换。
2. 本次选项文字问题暴露出的字体基线与顶部裁剪保护，AetherKiri 已抽成独立辅助逻辑并配有单元测试；PocketKrKr 原先缺少这层保护，已在 `LayerIntf.cpp` 中补入等价的本地边界处理。
3. AetherKiri 的字体、插件和脚本兼容测试比 PocketKrKr 更系统，适合借鉴测试组织方式，不适合直接复制其测试框架或 Godot 依赖。
4. AetherKiri 的诊断系统将 profile、事件、标记窗口、平台证据和 artifact bundle 统一起来；PocketKrKr 已有统一探针开关和文件日志，但还没有同等的 profile 和证据包契约。
5. AetherKiri 的 GPU 文字批处理、Godot Native 纹理路径和运行时 Provider 属于高耦合改造，不能因为对方存在就判定 PocketKrKr 缺失或应当移植。

## 对照矩阵

| 分类 | AetherKiri 参考 | PocketKrKr 当前状态 | 判断 | 优先级 | 建议 |
|---|---|---|---|---|---|
| 字体基线 | `cpp/core/visual/FontBaseline.h`；字体基线与字形顶部边界辅助函数 | 已有字体、TPF、FreeType 和文字探针；此前缺少 drawText 顶部越界保护 | 部分，当前选项修复已补入等价本地逻辑 | P0 | 用选项、对话、全角、缺字和阴影回归验证；后续补最小单元测试 |
| 预渲染字体 | TPF 字形度量与运行时字形共享基线约定 | 已支持 TPF、FreeType fallback、字体映射和缺字回退 | 部分 | P1 | 将 `Origin`、`Inc`、ascent、clip 和缩放统一成可测试契约，不替换整套实现 |
| 字体映射缓存 | 重复映射相同 face/storage 时避免清空全局字形缓存 | PocketKrKr 已有相同方向的重复映射保护 | 已有 | P2 | 检查重启、换字体和 context 重建时的缓存失效，不再重复移植 |
| 字体单元测试 | `font-baseline.cpp`、`font-compat.cpp` | 当前 docs 有回归要求，但缺少对应独立测试目录 | 候选 | P1 | 增加不依赖 Android 的基线、TPF、fallback 和 KAG 字体 API 测试 |
| GPU 文字提交 | AetherKiri 有 pending text draw、批量纹理和 GPU 路径 | PocketKrKr 使用现有 Layer/Bitmap/RenderManager 路径，已有文字探针 | 风险候选 | P2 | 先做 CPU/目标平台基准和逐像素对比，禁止直接替换绘制路径 |
| 图层裁剪 | drawText 前按字形包围盒修正 ClipTop | 原先缺失，已补入 `LayerIntf.cpp` | 已修复待验证 | P0 | 用最新 Android 日志确认 requestedY/effectiveY 和最终 stretch 不再产生异常裁剪 |
| operateStretch | TJS 宽高转右下坐标，Layer 再执行 StretchBlt | 参数语义已对齐，未发现解析差异 | 已有 | P1 | 继续验证负目标坐标、ClipRect 源矩形同步和零尺寸边界 |
| 像素/纹理路径 | AetherKiri 有更完整的 GPU texture self-test 和 Godot texture tests | PocketKrKr 有标量混合基准、RenderManager 和 Flutter 纹理探针 | 部分 | P1 | 补充纹理尺寸变化、context generation、透明度和回读路径的自动验证 |
| 插件缺口审计 | `tools/plugin_gap_audit.py` 按 real/compat_stub/empty_stub 分类 | PocketKrKr 有插件目录和兼容桩，但没有统一缺口报告工具 | 候选 | P1 | 先扫描注册表、CMake 目标和桩实现，输出缺口清单，不自动实现插件 |
| 插件注册测试 | registry、script-compatibility、插件行为测试 | PocketKrKr 主要依赖目标平台和游戏回归，独立覆盖不足 | 候选 | P1 | 为关键插件注册名、参数数量、返回类型和缺失插件行为补测试 |
| TJS/KAG 兼容测试 | 编译脚本、CP932、AffineSource、缺失成员等针对性测试 | 已有 TJS2 工具和脚本分析流程，测试矩阵较少 | 部分 | P1 | 建立最小脚本 fixture，覆盖动态属性、Variant 转换、异常和编码回退 |
| 诊断 profile | `tools/diagnose.py`、profile catalog、bounded JSONL、marker window、ZIP | 已有 `KRKR_RENDER_PROBE` 全局开关、文件日志和多个探针 | 部分 | P1 | 先统一 profile 语义和日志字段，再考虑证据包；保持默认关闭和低扰动 |
| 诊断会话 | AetherKiri 有 issue marker、平台证据、截图和 summary contract | PocketKrKr 有 rotating file sink 和探针日志，没有统一事件契约。 | 候选 | P2 | 设计轻量日志 bundle 规范，不引入 Godot UI 依赖 |
| 运行时生命周期 | AetherKiri 有 provider 生命周期、重启和多 runtime 边界测试 | PocketKrKr 有 EngineBootstrap、EngineLoop、C ABI 和重启规则 | 部分 | P1 | 对照创建、异步打开、停止、重启、context 重建和 dispose 的所有权测试 |
| 资源与视频 | AetherKiri 有视频 smoke、RangeFS、运行时帧证据和媒体测试 | PocketKrKr 有 FFmpeg 解码链路，视频显示合成仍列为缺口 | 部分 | P1 | 借鉴 fixture 和帧时钟测试，不移植 Godot 播放层 |
| Godot Native renderer | Godot-owned RenderingDevice、GPU Bridge、Debug CPU 三后端 | PocketKrKr 使用 Flutter、ANGLE、IOSurface/SurfaceTexture 和 RGBA 回退 | 不适用 | 无 | 只借鉴后端能力矩阵和回退验证思想 |
| Runtime Provider ABI | KiriRuntime、ONS、Siglus、C Runtime 统一 Provider | PocketKrKr 目标是单一 KiriKiri 引擎和 Flutter bridge | 不适用 | 无 | 不引入多运行时抽象 |
| Web/ONS/Siglus | AetherKiri 的产品和运行时扩展 | PocketKrKr 当前边界不包含这些运行时 | 不适用 | 无 | 不列入 PocketKrKr 兼容路线 |
| 开发工具缓存 | Linux setup、Godot/vcpkg cache、诊断工具 | PocketKrKr 有构建脚本、vcpkg 和 TJS2 工具，但环境依赖说明较分散 | 部分 | P2 | 补充工具可用性检查和 Linux 验证入口，不引入 Godot 工具链 |

## 细粒度待实现/待删除清单

以下条目用于逐项实现、验证和关闭。每项完成后应从“待实现”移到“已验证”，而不是长期保留为模糊能力描述。

| ID | 链路 | PocketKrKr 当前状态 | AetherKiri 对照 | 验收条件 | 完成后处理 |
|---|---|---|---|---|---|
| AK-FONT-01 | `Layer::drawText` 字形顶部边界 | 已补 `GetFontGlyphDrawRect` + ClipTop 修正 | 有独立基线辅助函数和测试 | 选项、对话、阴影、负 y 不裁切，普通坐标不改变 | 加入字体回归后关闭 |
| AK-FONT-02 | TPF `OriginY` 与 FreeType bearing | 已有两条路径，缺统一测试 | 共享 `ComputeGlyphOriginY` 契约 | 同一 baseline 下 TPF/fallback 字形顶部一致 | 测试通过后关闭 |
| AK-FONT-03 | 预渲染字体 width/height/zoom | 已有 `PreRenderFontEx` 和 Stretch 探针 | AetherKiri 维护更完整字体状态 | 原始字库尺寸、目标字号、目标矩形三者可从日志闭合 | 验证后关闭 |
| AK-FONT-04 | GAL 选项字号兼容 | 当前对空名 `PreRenderFont` 的 32->39 兼容 | 外部实现不提供该游戏特例 | 仅目标路径生效，普通 32px 不变 | 真机验证后移入兼容规则 |
| AK-SYNC-01 | `D3DEmote.tjs stopMovie/sync` | 脚本调用 `skipToSync -> progress(1) -> stop` | 同一脚本链路依赖有效 skip | 提前点击后标题继续进入主界面 | 通过后关闭 bug 条目 |
| AK-SYNC-02 | `Player::skipToSync` | 原先为空；本次已推进非循环时间线末端 | AetherKiri 会结束 one-shot timeline 并清同步状态 | skip 后 `progress(1)` 返回完成，`onSync` 只触发一次 | 加回归后关闭 |
| AK-SYNC-03 | sync wait 状态 | 缺少 `_syncWaiting/_syncActive` | 有 release/skip 时的同步门控 | 连续点击、动画自然结束、跳过后 stop 均不丢 wait | 若脚本实际读取再实现 |
| AK-SYNC-04 | command-list pulse | 当前以 `_stopCommandSent` 一次性 STOP 为主 | AetherKiri 有 command-list pulse | 动画结束和提前点击都能触发一次有效重绘/STOP | 建 fixture 后决定是否实现 |
| AK-SYNC-05 | 自动进度 | 当前依赖脚本/现有主循环调用 `progress` | AetherKiri 有 continuous tick 自动进度注册 | 点击后无脚本额外 tick 依赖，动画仍可完成 | 证明缺失后再移植 |
| AK-SYNC-06 | `releaseSyncWait` | 当前无同名接口 | AetherKiri 显式清理 sync wait/active | 资源切换、stop、destroy 后不会残留等待状态 | 若调用方存在再实现 |
| AK-MOTION-01 | M2 轨道时间单位 | 当前按解析后的毫秒推进 | AetherKiri 也区分解析时间与实时 delta | logo 4s/1.5s 时间线不二次换算 | 已有验证后关闭 |
| AK-MOTION-02 | M2 子运动展开 | 当前有 flat/tree 展开和去重逻辑 | AetherKiri 拆分为节点树模块 | title 子运动只展开一次且父变换传递正确 | 加多层 fixture 后关闭 |
| AK-MOTION-03 | motion captureCanvas | 当前移动端按 no-op/直接合成兼容 | AetherKiri 有更完整 host/backend 分层 | 脚本只调用不取返回值时不阻塞；取返回值的游戏单独识别 | 按调用契约关闭 |
| AK-PLUGIN-01 | 插件注册缺口 | 没有统一 real/stub/empty 报告 | 有 `plugin_gap_audit.py` | 每个注册名有实现状态和参数契约 | 报告纳入 CI 后关闭 |
| AK-TEST-01 | TJS/KAG fixture | 有反编译工具，独立 fixture 较少 | 有 CP932、缺成员、脚本兼容测试 | 动态属性、Variant、异常、编码和 wait 均可独立回归 | 测试进入 CI 后关闭 |
| AK-DIAG-01 | 诊断 profile | 有统一宏和文件日志，无 profile 契约 | 有 baseline/render/script 等 profile | 同一问题能按低/中/高开销复现并导出关联日志 | 证据包完成后关闭 |

### 当前新增 Bug：标题动画提前点击卡住

已定位到一个确定缺口：`cpp/plugins/motionplayer/Player.h` 的 `skipToSync()` 原先是空实现，而 `D3DEmote.tjs` 的跳过链路明确调用 `skipToSync()`、`progress(1)`、`stop()`。本次先实现最小时间线跳过语义；仍需真机确认标题动画提前点击后是否正常进入标题界面。

### P0：当前字体问题闭环

- 使用带 `4cbe1ff` 的 Android 探针包验证 `requestedY/effectiveY`。
- 确认选项拥有者使用字号 `39`，内部预渲染层只作为原始字库层使用 `32`。
- 对照 `PreRenderFontStretchProbe`，确认目标矩形、源矩形和 ClipRect 的裁剪结果。
- 真机确认选项字号、水平/垂直位置和底部裁切均恢复。

### P1：正确性和兼容性基线

- 增加字体基线和预渲染字形的最小测试。
- 增加插件注册与兼容桩行为测试。
- 增加 TJS2/KAG fixture：CP932、动态属性、缺字、异常、脚本字体 API。
- 建立视频首帧、暂停、跳转、结束和重复播放 fixture。
- 对 Layer/RenderManager/Flutter bridge 补充尺寸变化、context generation 和资源所有权测试。

### P2：诊断和性能工程化

- 为现有文件日志定义轻量事件字段和 profile 层级。
- 将探针、运行时状态、平台日志和用户标记合并为可导出的证据包。
- 在建立标量和 GPU 基准后，再评估 AetherKiri 的批量文字纹理路径是否值得借鉴。
- 量化缓存命中率、字形缓存清理、纹理重建、帧耗时和视频队列深度。

## 不应直接移植的项目

- AetherKiri 的 Godot GDExtension、Godot Native/GPU Bridge/Debug CPU 后端。
- 多运行时 Provider ABI、ONScripter、Siglus、Minori 和其商业运行时边界。
- AetherKiri 的 Godot UI、诊断抽屉、Godot screenshot probe 和产品设置页面。
- 整体替换 `LayerIntf.cpp` 或 `LayerBitmapImpl.cpp`。两边同源但已有大量分叉，整体替换会覆盖 PocketKrKr 的 Flutter、ANGLE、插件、生命周期和诊断改动。
- 任何只由单个游戏或单台设备日志推导出的字号、坐标和图层名称特判。

## 验收规则

- 每个候选项目必须有本地代码位置、外部参考位置、行为差异、风险和独立验收条件。
- 渲染和字体项目必须同时覆盖预渲染字体、运行时字体、缺字、全角字符、混排、裁剪和缩放。
- 平台项目必须覆盖 Android、iOS、macOS 的目标路径和 RGBA 回退路径。
- 诊断项目默认关闭，不得改变时序、生命周期、渲染结果或正常性能。
- 外部仓库只作为行为参考，不能用其代码存在替代 PocketKrKr 的验证结果。
