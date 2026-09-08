# KIRIKIRI Z（krkrz）插件兼容移植清单

> 目标：让 PocketKrKr 能运行 **KIRIKIRI Z**（魔女的夜宴等 Z 引擎游戏），核心是
> **先治黑屏**（Z 主层 drawdevice 未接入），再逐个恢复 Z 常用插件能力，目标对齐
> krkrz 闭源版与 Kirikiroid2（安卓端完整实现）。
>
> 关联待办：[todo.md](todo.md) #2（黑屏根因）、#2b（krmovie Present）。

## 参考源（在线仓库，作移植蓝本，勿直接 commit 进仓库）

> 本环境无本地 `/tmp/krkrz-ref` 副本，参考以在线仓库为准（可用 `git clone --depth=1` 拉取）。

| 仓库 | 在线地址 | 用途 |
|---|---|---|
| krkrz/krkrz | https://github.com/krkrz/krkrz | KIRIKIRI Z 引擎核心；`movie/win32/krmovie.cpp` |
| krkrz/krkr2 | https://github.com/krkrz/krkr2 | kwiki 全量 win32 插件源码；`src/plugins/win32/*`（drawdeviceZ_D3D9、squirrel、drawdeviceD3D 等） |
| krkrz/krkrz_dev | https://github.com/krkrz/krkrz_dev | Z 工具 + win32 插件（menu、fftgraph、theora…） |
| zeas2/Kirikiroid2 | https://github.com/zeas2/Kirikiroid2 | **安卓端完整移植（最强参考）**：`src/core/visual/ogl`（RenderManager_ogl=GL drawdevice）、`src/core/movie/krmovie.cpp`+`ffmpeg/`、`src/core/visual/win32/DrawDevice.h`/`BasicDrawDevice.*`/`PassThroughDrawDevice.*`、`src/plugins/InternalPlugins.cpp` |
| krkrz/Krkr2Compat | https://github.com/krkrz/Krkr2Compat | `k2compat/k2compat.tjs`：krkr2→Z 兼容层，纯 TJS 脚本 |

## 移植总原则

1. **插件 = TJS Native Class（ncb）**：用 `tvpRegisterClass`/`ncb`/`simplebinder` 实现，
   源码加入 `cpp/plugins/`，经 `target_sources(PUBLIC)` 编进 `libengine_api.so`
   （Android），与现有插件一致。
2. **不搬 D3D9 后端**：drawdevice 的 Windows 插件依赖 D3D9/ANGLE-D3D，移动端应参考
   Kirikiroid2 的 **OpenGL（RenderManager_ogl）**实现合成逻辑，而不是复制 D3D9 代码。
3. **驱动的动态加载**：Z 游戏通过 `Plugins.link()`/`loadPlugin` 触发，PluginManager 需能
   命中这些插件名（缺则 `Failed`）。可先在内部注册表给插件挂名，再实现功能。
4. **静态内嵌 vs TJS 脚本**：C++ 才编进 .so；纯 TJS（如 k2compat）不编引擎，而是放进
   游戏可访问的脚本路径或随包内置。

## 插件清单（按对"跑通 Z 游戏"的关键性排序）

优先级：**P0** 关键链路（决定能不能出画面/回放）→ **P1** 常用功能 → **P2** 冷门/来源待定。

| 插件 | 功能 | 来源（参考码） | 依赖/注意 | 策略 | 优先级 |
|---|---|---|---|---|---|
| drawdeviceD3DZ（drawdeviceZ_D3D9） | Z 主画面 D3D draw device；**黑屏根因** | krkr2 `src/plugins/win32/drawdeviceZ_D3D9`；移动端真形态 Kirikiroid2 `src/core/visual/RenderManager_ogl.cpp` + `win32/DrawDevice.h`/`BasicDrawDevice.cpp` | 强依赖 D3D9/合成链路；需 GL 化 | P0：对照 Kirikiroid2 OGL RenderManager 与我们的 `RenderManager`/`BasicDrawDevice` 逐环节定位主 DrawBuffer 未合成原因并补齐 | **P0** |
| krmovie | 视频回放（解码→合成）；性能优于纯软件 | Kirikiroid2 `src/core/movie/krmovie.cpp` + `ffmpeg/KRMovie*.{h,cpp}`；krkrz `movie/win32/krmovie.cpp` | 我们已 ffmpeg 解码，缺 Present/overlay 合成 | P0：把 Kirikiroid2 的 krmovie 帧→Layer 合成路径移植进 `cpp/core/movie/` | **P0** |
| squirrels plugin（squirrel） | 内嵌 **Squirrel** 脚本语言（很多 Z 游戏系统/存档逻辑用它） | krkr2 `src/plugins/win32/squirrel` | 需接入 tjs2 扩展：暴露 `Squirrel` 类/宿主调用 | P1 | 1 |
| k2compat | krkr2→Z 兼容函数/类兼容（`Window.innerSunken` 等） | TJS：Krkr2Compat `data/k2compat/*.tjs` | 纯 TJS，非 C++；需打进游戏脚本可加载路径 | P1：内置脚本 + 让 `Plugins`/`Scripts` 能 include | 1 |
| drawdeviceD3D | krkr2 的 D3D drawdevice（Z 之前的常规 draw device） | krkr2 `src/plugins/win32/drawdeviceD3D` | D3D9；移动端参考 OGL | P1：比 drawdeviceZ_D3D9 更通用，促成同链路 | 1 |
| PassThroughDrawDevice | 透传 draw device（部分补丁/特效需要） | Kirikiroid2 `src/core/visual/win32/PassThroughDrawDevice.*`；uyjulian/PassThroughDrawDevice | | P1 | 1 |
| menu | MenuItem / window.menu(dll) | krkrz_dev `src/plugins/win32/menu` | 我们已有 win32dialog / 对话框 | P1 | 1 |
| kztouch | Z 触摸/触摸控件 | 来源待确认（独立/第三方，未在三仓库钓出） | | P2：先定位源码再移植 | 2 |
| kagexopt | KAG 系统扩展（KAGEX 优化，`kagexopt`） | 独立第三方（keepcreating/wtnbgo 等） | | P2：定位源码仓库 | 2 |
| multiimage | 多图/多图层纹理（PSD 相关） | 来源待确认（gist 无来源，可能是私有） | | P2：先确认有无开源 | 2 |
| PackinOne | 打包/资源插件 | 作者/开源未知（gist 评论区质疑），**疑似闭源** | | P2：无开源则跳过，记录兼容缺口 | 2 |
| fftgraph | 频谱可视化画层 | krkrz_dev `src/plugins/win32/fftgraph` + Cirikiroid2 | 我们已 `fftgraph.cpp` | 已有可跑，可选升级 | 低 |

## 建议推进顺序

1. **黑屏定位（P0 起点）**：深入对比 `Kirikiroid2/src/core/visual/RenderManager_ogl.cpp` +
   `BasicDrawDevice.cpp` 与我们的 `cpp/core/visual/RenderManager.*`，找出 Z 主层
   `DrawBuffer` 从未被合成的原因，先达成" Z 游戏能出画面"。
2. **krmovie Present（P0）**：移植 Kirikiroid2 的视频帧→Layer 合成，替代现有 stub。
3. **squirrel + k2compat（P1）**：恢复 Z 游戏脚本/系统逻辑。
4. 逐一消化 P1 其余（menu、PassThroughDrawDevice、drawdeviceD3D…），P2 待源码确认。

## 差距分析（2026-09-08，实测于 Kirikiroid2 全量 clone）

> 背景：我们已 `git clone --depth=1` zeas2/Kirikiroid2（本地 `/tmp/Kirikiroid2`），
> 想逐个确认缺哪些插件。

### 关键认知

1. **drawdeviceD3DZ 在移动端不是插件**：Kirikiroid2 用 `src/core/visual/RenderManager_ogl.cpp`
   + `win32/DrawDevice.*`/`BasicDrawDevice.*` 作为**核心渲染管理器**直接合成 Z 主 DrawBuffer，
   不走 `plugin/*.dll` 的 ncb 通道。魔女的夜宴等 Z 游戏黑屏根因在此，属 **core/visual 引擎能力**，
   不是"写个插件"能解决 → P0 优先走渲染管线，勿当插件实现。
2. **同源 fork → 插件大面积重合**：我们与 Kirikiroid2 同源，其 `src/plugins/*` 的 ncb 插件
   （extrans/wuvorbis/wuflac/wuopus、layerEx 全家、AlphaMovie、psb/psd、xp3filter、fftgraph、
   drawDeviceD2Dm、krkrsteam、krkrgles、json、fstat…）我们 `_internal_plugins` 基本都已注册。
   真要补的插件很少。

### 实测缺口（对照 Kirikiroid2 全仓库 `grep -rin`，区分"插件"与"引擎"）

| 插件 | 态况 | 缺的是插件还是引擎 | 决策 |
|---|---|---|---|
| drawdeviceD3DZ | Z 主画面，**黑屏根因** | **引擎**（RenderManager_ogl） | P0，对核心渲染链路 |
| krmovie Present | 视频回放 | **引擎**（`movie/krmovie.cpp`） | P0 |
| squirrel | 很多 Z 存档/系统脚本 | 插件（krkr2 `squirrel`） | P1 |
| k2compat | krkr2→Z 兼容函数 | 脚本/TJS（Krkr2Compat） | P1，内置脚本 |
| menu / drawdeviceD3D / PassThroughDrawDevice | 常用 | 插件 | P1 |
| **PackinOne** | 千恋万花实载 Failed；**Kirikiroid2 全仓库无此实现** | 插件（疑似闭源） | **C 级，跳过**：下载已解包游戏可跑；仅整包加密需解包 |
| **extNagano** | 千恋万花实载 Failed；**Kirikiroid2 全仓库无此实现** | 插件（独立冷门扩展） | **C 级，跳过**：基干无它照跑 |
| kztouch / kagexopt / multiimage | 冷门，来源待确认 | 插件 | P2，等源码 |

### 结论
- 真缺且急用的插件只有 **squirrel**（P1）；PackinOne/extNagano 多为下载解包版可跳过，
  只做"挂名不报 Failed"或直接跳过，等有真实游戏卡在它们头上再补。
- 当前最大阻塞仍是 P0 两个**引擎能力**（drawdeviceZ 主 buffer 合成 + krmovie Present），
  不靠写插件解决。

## 自检 / 验收

- 目标游戏（魔女的夜宴/sabbat_kr）启动后：主 `DrawBuffer` 被合成、源纹理非全黑、draw 计数增长。
- `plugin/` 目录缺失插件从"Failed"变为可 `Plugins.link`。
- 视频 OP 可播放（krmovie Present）。