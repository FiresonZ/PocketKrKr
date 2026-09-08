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

## drawdeviceD3DZ 深挖（2026-09-08）

> 对 Kirikiroid2 `RenderManager_ogl.cpp` 与本项目同文件逐环节比对（子代理 + 人工复核），
> 结论与定位如下，防止返工。

### 结论（重要）
- **黑屏根因不在 `RenderManager_ogl.cpp` 内部合成算法**：本文件与 Kirikiroid2 逐环节
  一一对应、逻辑等价（`SetRenderTarget/_RestoreGLStatues/InitGL/GetTempTexture2D/CopyTexture/
  CreateTexture2D/OperateRect/OperateTriangles/OperatePerspective/Stencil` 全匹配）。
- **场景一次绘制入口不在本文件**：合成入口在 `cpp/core/visual/LayerBitmapIntf.cpp`
  （`TVPGetRenderManager()->OperateRect`）、`LayerIntf.cpp`、`impl/PassThroughDrawDevice.cpp`；
  参考(Kirikiroid2)对应在它自己的 `LayerImpl.cpp`/`BitmapLayerTreeOwner.cpp`。
- 因此 **drawdeviceD3DZ 在移动端不存在可照搬的独立"插件文件"**，它是渲染管线如何让
  Z 主 `DrawBuffer` 被实际画入 primary texture 的问题，属 core/visual 能力，非插件。

### 待实机复核的 3 处候选（按性价比排序）
| # | 候选 | 现状 | 真机验证动作 |
|---|---|---|---|
| C | 渲染管理器注册/链接 | **代码已核实正确**：`EngineBootstrap.cpp:61` 在 EGL 就绪后、首次 `TVPGetRenderManager()` 前调 `TVPForceRegisterOpenGLRenderManager()`；宏在 `RenderManager.h:319-326` | 首帧打点确认 `TVPGetRenderManager()` 非空且为 OpenGL 管理器 |
| A | `krkr::gl` 包装 vs `cocos2d::GL` 语义等价（viewport/FBO bind/blend cache/attribute enable） | 本项目相对参考新增的重写层，最可能画错/画到失效 FBO | 开 `enable_render_probe` 抓 `SourceSample`/`PostBlit`，看主 DrawBuffer 是否被写入纹理 |
| B | Renderer-recreated/FBO 重建 | `5fd30da` 已增强重建 `_FBO`/`_stencil_FBO` 并复位状态（二次打开），系正确修复 | 两游戏不杀进程二次打开复核渲染 |

### 首选下一步
真机开 `enable_render_probe=true` 抓二次打开黑屏日志：确认主 `DrawBuffer` 的 `OperateRect`/
合成是否真的被触发。若 `SourceSample`/draw 计数缺失（现状据 2026-09-08 日志正是如此，
且日志仍存在重复问题见 `engine_api.cpp` 4e47e97 修复），则先到 `LayerBitmapIntf.cpp`/
`PassThroughDrawDevice.cpp` 定位"主 DrawBuffer 从未被合成"的调用缺失一环，再据候选 A/B 修正。

## 探针实测结论（2026-09-08，先开 IINCHO-Re.co 后开 千恋万花）

> 开 `KRKR_RENDER_PROBE` 抓 `pocketkrkr_engine(8).log`（修复日志重复前）。长 3347 行，
> 两次 `engine_open_game` 各一段。关键在 `FlutterWindowLayer::UpdateDrawBuffer/SourceSample/PostBlit`。

### 证据
| 项目 | 第一游戏 IINCHO-Re.co（正常） | 第二游戏 千恋万花（黑屏） |
|---|---|---|
| UpdateDrawBuffer | `nativeTex=83 srcTex=83 blitTex=84 1280x720 layers=69 draw=15`，持续 | `nativeTex=3 srcTex=3 blitTex=7 1920x1080 layers=215 draw=5`，仅 1 帧 |
| SourceSample | `nonBlack=25/25` 全程健康，颜色随时间渐变 | `nonBlack=0/25 avg=(0,0,0,255)`（全黑） |
| PostBlit center | 非黑 | `(0,0,0,255)`（黑） |
| 渲染频率 | 持续 ~20 采样/秒 | 首帧后 **2.8s 无任何 frame 采样**（探针每 5 帧必打）→ present/渲染循环停摆 |
| 插件 | — | `k2compat/kztouch/krmovie/kagexopt/menu/yuzuex/lzfs/multiimage/win32ole/motionplayer_nod3d/PackinOne/extNagano/krkrsteam` 全 **Failed** |
| 启动脚本 | 正常 | `startup.tjs` 正常跑完，`layers=215`，无崩溃 |

### 结论（纠正方向，重要）
- **这次黑屏是 runtime-restart（二次打开）专属问题，不是 drawdeviceD3DZ 的 Z 引擎根本缺口。**
  依据：千恋万花作为**第一个游戏**已被真机验证能正常出画面；探针也证明第一游戏合成+blit
  全链路正常，仅重启后的第二游戏出问题。
- **黑屏现象两异常**：
  1. 第二游戏 `draw=5`（引擎发起了合成）但 `SourceSample` 全黑 → **合成没画进 blit 源纹理
     srcTex=3**。→ 对应候选 A（`krkr::gl` FBO/state 在重启后的语义错配）。
  2. 首帧后 2.8s 无任何 frame → **游戏逻辑/定时器或 present 循环重启后未恢复**（与"第二次
     打开日志比第一次少"观察吻合）。
- **插件大量 Failed 不是本次黑屏主因**：它们缺但游戏能作为第一游戏跑起来，故 Z 插件兼容属
  独立问题（见本文档清单），与本次二次打开黑屏脱钩。

### 下一步（替换原 drawdeviceD3DZ 首选）
不再先做 drawdeviceD3DZ。转向重启状态排查：
1. 定位第二游戏 `draw` 的合成为何没进 `srcTex`（`krkr::gl` FBO bind / FlutterWindowLayer
   `blitSrcTexture` 在重启后的解析）→ 候选 A。
2. 定位第二游戏渲染循环为何停摆：`EngineLoop` tick 调度 / 连续处理器 / present 未随二次
   `StartApplication` 重新拉起。

## 重启复位链静态审计（2026-09-08）

> 背景：上游 KrKr2-Next master 的 `engine_destroy` 仍报 "runtime restart is not supported yet"
> （即上游不热重启），我们这份"对照 PR#12"的重启复位链是全 fork 自写的、无上游可对照。
> 因此改为审计我们自身复位链完整性，找出漏掉的进程级静态。

### 结论
复位链（`engine_api.cpp engine_destroy` 内）对多数子系统已彻底：脚本/存储/窗口/字体/位图/插件/
EGL context（Destroy+重建）/OpenGL 共享 `_FBO` 重建（`OnRendererRecreated`）均已清或重建。

剩余 4 处进程级静态漏项，但**只有个别对 Android 二次黑屏成立**：

| # | 漏项 | 位置 | 是否解释 Android 二次黑屏 |
|---|---|---|---|
| 1 | `tTVPAtExit` 一次性注册失效，二次退出静态清理不执行，跨代累积污染 | `base/SysInitIntf.cpp:114-160` | 结构性；单次重启影响弱 |
| 2 | `TVPDrawSceneOnce` 的 `static lastTick` 不复位 | `environ/EngineLoop.cpp:62-75` | ❌ 不适用：`engine_tick` 恒 `interval=0` 调用，合成照常发生（探针 draw 增长为证） |
| 3 | `EGLContextManager::Destroy()` 漏调 `DestroyIOSurfaceResources()` 复位 IOSurface 字段 | `visual/ogl/krkr_egl_context.cpp:224` | ❌ 仅 iOS/macOS；Android 走 NativeWindow 不受影响 |
| 4 | `TVPIsSoftwareRenderManager` 的 `static bool ret` | `visual/RenderManager.cpp:4966` | 低危 |

### 诚实结论（重要）
静态审计**未能为 Android "第二游戏 draw=5 但 blit 源全黑 + 首帧后停摆"给出决定性单线根因**。
#2/#3 与 Android 现象对不上。Android 高概率根因仍在：重启后 `FlutterWindowLayer` 的
`blitSrcTexture` 与引擎实际主 DrawBuffer 未同步（此前候选 A）。要定死需一次探针日志区分
"合成画错地方" vs "渲染循环停摆"。

### 可低风险顺手修的（正确性卫生，非 Android 根因）
- #2：显式复位 `TVPDrawSceneOnce` 的 `lastTick`（保护正 interval 内部路径）。
- #3：`EGLContextManager::Destroy()` 内补 `DestroyIOSurfaceResources()`（iOS 干净重置）。
- #4：失效 `static bool ret`。

## 二次实测补充（2026-09-08，真机）

**新证据**：第一个游戏成功跑完后，第二个不同游戏黑屏；但**不杀进程再次点开第一个游戏，仍能正常跑**。

**判定（修正方向，重要）**：
- 全局重启 teardown（EGL context 重建 + FBO 重建 + FlutterWindowLayer blit 路径）**基本健康**——
  若全局渲染状态脏，重开同一个游戏也会黑，但实测不会。
- 黑屏是**游戏/图层资源特定**的：同游戏复开正常、换游戏才黑 → 不同游戏走了不同的主层渲染路径。
  IINCHO 用标准 primary layer 能出画面；千恋万花（Yuzusoft/Z 引擎）可能走 Z 型主 DrawBuffer，
  引擎未给其合成 → 黑屏。这与"千恋万花作为第一游戏其实也只是开了 logo、未真正渲染"一致。
- 因此**候选 A（krkr::gl 全局状态）权重下调**；问题重新指向"Z 主层 DrawBuffer 未走标准合成"——
  即 drawdeviceD3DZ 兼容（又回到 P0 渲染管线，但性质是"不同游戏主层路径"，非"重启残留"）。

**待 RTProbe 定死**：`FlutterWindowLayer::RTProbe blitSrcTex X fboAttachedTex Z [SAME]/[DIFF]`
- 对黑屏游戏若为 `DIFF` → 其主层画进了别的纹理（非 blit 源），属游戏特定主层/渲染目标设定。
- 对黑屏游戏若为 `SAME` 但仍黑 → 引擎对它的主层画了黑/没画，指向该游戏主层用了未接入的钻取路径。

## 自检 / 验收

- 目标游戏（魔女的夜宴/sabbat_kr）启动后：主 `DrawBuffer` 被合成、源纹理非全黑、draw 计数增长。
- `plugin/` 目录缺失插件从"Failed"变为可 `Plugins.link`。
- 视频 OP 可播放（krmovie Present）。