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

### 二次实测补充②（2026-09-08，engine(9).log，旧版无 RTProbe）

**序列**：Kemomusu(1st)｜正常(SourceSample 25/25 持续) → IINCHO(2nd)｜**无帧** → Kemomusu(3rd)｜正常(25/25 持续)。

**决定性证据（修正方向②，重要）**：
- 2nd 段 `StartApp 完成(923)` 到 destroy(945) 共 5.5s：仅 1 次 `engine_tick auto-attached`，
  **其间无任何 UpdateDrawBuffer/SourceSample/帧**；那次 engine_tick 调 `Application->Run()` 也
  **未触发任何 blit**。
- 1st/3rd 段（相同游戏 Kemomusu）都持续出帧。
- 结论：**二次打开(open#2)是"帧驱动/主窗口绘制没恢复"，既非合成画错目标，也非 drawdeviceD3DZ 主层，
  更非具体游戏**（IINCHO+Tick 平时能跑，作为 2nd 打开照样无帧）。且呈位置性：open#2 失败、open#3 成功。

**已加判别探针**（`engine_api.cpp` engine_tick，KRKR_RENDER_PROBE，每 15 tick）：
`Application::Run enter / return` paired with 原有 `UpdateDrawBuffer`。
- 2nd 段看到 enter/return 却无 UpdateDrawBuffer → `Application->Run()` 走不下（主窗口/绘制未恢复）。
- 2nd 段连 enter 都没有 → host 停止调用 engine_tick。
（此判别探针连同 RTProbe + 3 处复位卫生修复见 `d29009f`。）

### 二次实测补充③（2026-09-08，engine(10).log，带 RTProbe + Application::Run 探针的 release 包）

**序列**：Kemomusu(1st) 正常 → IINCHO(2nd) 黑 → Kemomusu(3rd)（同题）。
**决定性证据（彻底定性）**：
- 1st/3rd：`RTProbe` **每帧** `blitSrcTex=3 engineCurFbo=1 fboAttachedTex=3 [SAME]`，
  `Application::Run` 每 tick 进出，合成进同一纹理、持续出帧。
- 2nd（IINCHO，5.4s）：`Application::Run enter/return` **持续每 tick 都在**
  （tick=15…600），但 **RTProbe/UpdateDrawBuffer 只出现 1 帧（行1050 `blitSrcTex=81 [SAME]`）**，
  之后到 destroy 再无 blit。
- 结论：**host 一直在调 engine_tick、Application::Run 一直在跑**；不是"合成画错目标"、
  也不是"host 停摆"。回收链是 **`Application::Run → SystemWatchTimerTimer → DeliverEvents → 
  tTVPWinUpdateEvent → Window::UpdateContent → DrawDevice->Update()+Show → UpdateDrawBuffer`**，
  二次打开从第 1 帧起**不再有窗口重绘（win update event/Show 不再触发）** → 画面停在第 1 帧黑块。

**因此根因在框架重绘调度，不在 GL/合成/目标/ host 帧回调**。候选集中于：
- `tTVPAtExit` 一次性注册失效（审计#1）→ 二次打开定时器/连续处理器未恢复，游戏不再请求重绘；
- 或二次打开的 `TVPSystemControl`/`TVPTimer`/主窗口 update 事件投递未恢复。

**下一步判别**：在 `tTVPWinUpdateEvent::Deliver`/`TVPWinUpdateEventQueue` 或 `TVPTimer::ProgressAllTimer`
打点，确认 2nd 段是否还有窗口重绘事件被投递/分发，从而区分"框架没投递重绘"vs"投递了但 Show 没 blit"。

### 候选排查结论（2026-09-08，静态核对 + 已加 DeliverWinUpdate 探针）

- 已**排除**：`TVPSystemControl`（OnExit 时 delete、二次 open 时 `new` 重建，Application.cpp:865/412）；
  `TVPEventInvoked`（每 tick 经 `_TVPDeliverAllEvents→TVPEventReceived()`（EventIntf.cpp:481/46）复位，
  不易卡死）；`TVPContinuousHandlerCallLimitThread`/`TVPTimer` 由 End/BeginContinuousEvent 切换。
- 确定性结论：二次打开从第 1 帧起**不再有窗口重绘（win update 事件投递/交付）**，回收链
  `RequestUpdate→TVPPostWindowUpdate→TVPInvokeEvents→(Run 每tick)DeliverEvents→TVPDeliverWindowUpdateEvents
  →UpdateContent→Show→UpdateDrawBuffer` 在 2nd 段断在 blit 之前。
- **已加探针**（`EventIntf.cpp` TVPDeliverWindowUpdateEvents，KRKR_RENDER_PROBE）：
  `DeliverWinUpdate: queue=N -> UpdateContent`。下一轮 release+probe 日志即可判：
  - 2nd 段 `DeliverWinUpdate` 持续打印、但无 RTProbe/blit → **投递→Show 段坏**（Show guard/Managers 空）；
  - 2nd 段 `DeliverWinUpdate` 根本不打印 → **游戏/脚本未请求重绘**（RequestUpdate 未发生，指向脚本/连续处理器未恢复）。

### 二次黑屏探针矩阵（一次 release+probe 日志即可定死整条链）

| 链路节点 | 探针 | 打印内容 | 判定 |
|---|---|---|---|
| 游戏请求重绘 | `RequestUpdate`（WindowIntf.cpp） | `RequestUpdate: repaint requested (cum=N)`，每 60 次 | 有=游戏在画；无=脚本/连续处理器没恢复 |
| 重绘事件投递 | `TVPDeliverWindowUpdateEvents`（EventIntf.cpp） | `DeliverWinUpdate: queue=N -> UpdateContent`（queue 非空） | 有=投递发生 |
| Show 被调但没 blit | `tTVPBasicDrawDevice::Show`（BasicDrawDevice.cpp） | `BasicShow: skip (buf=null \| form/Managers)` | 有=Show 守卫生效（Managers 空等） |
| blit 是否到纹理 | `RTProbe`（ui_stubs.cpp） | `blitSrcTex=… engineCurFbo=… [SAME/DIFF]` | 有 blit 则每帧；`SAME/DIFF` 定合成目标 |
| 引擎帧驱动 | `Application::Run enter/return`（engine_api.cpp） | 每 15 tick | 每 tick 有=引擎在跑 |

组合判：
- `RequestUpdate` 无 + 全部其他无 → 游戏/脚本未推进（脚本引擎/连续处理器重启问题）。
- `RequestUpdate`/`DeliverWinUpdate` 有 + `BasicShow: skip` 打法 → Show 段（主层 Manager 空）。
- `DeliverWinUpdate` 无但 `RequestUpdate` 有 → 投递链（DeliverEvents）坏。

### 决定性实测结论（2026-09-08，engine(11).log，完整探针矩阵）

**序列**：Kemomusu(1st) 健康 → IINCHO(2nd) 黑屏 → Kemomusu(3rd)。
**各段探针**：
| 段 | Application::Run | RequestUpdate | DeliverWinUpdate | RTProbe/blit |
|---|---|---|---|---|
| 1st Kemomusu | 每 tick | `cum=1/61/121…` 持续 | 持续 queue=1 | 每帧 SAME |
| 2nd IINCHO | 每 tick | **0 次** | 仅 1 次(queue=1) | 仅 1 帧 SAME |
| 3rd Kemomusu | 每 tick | 持续 | 持续 | 每帧 |

**板钉结论**：断点在最上游——**第二游戏从第 1 帧起不再调用 `RequestUpdate`**（不再请求重绘）。
引擎投递/Show/blit 机制全程健康（DeliverWinUpdate 工作过、RTProbe [SAME]），`Application::Run`
每 tick 都跑。是**游戏侧每帧驱动（主层连续更新/脚本/定时器）没恢复**，与"第二游戏日志更少"吻合。

**收窄到候选①强化**：`RequestUpdate` 由主层更新触发；二次打开其连续/每帧驱动不转。
下一步直接从"主层如何触发 RequestUpdate + 连续重绘/TJS 定时器在二次打开的复位"入手，
而不再查渲染/投递链路（已证明健康）。

### 渲染侧彻底排除 + 根因定性（2026-09-08）

- `RequestUpdate` 唯一触发：主层内容变化 → `iTVPLayerManager`/`tTVPDrawDevice::NotifyLayerImageChange`
  → `Window->RequestUpdate()`（DrawDevice.cpp:302-307）。第二游戏 `RequestUpdate=0` ⇒ **主层从未产生
  新内容 ⇒ 游戏脚本没在画**。
- 整条显示链已逐环节证明健康：`Application::Run` 每tick 在跑、`TVPSystemControl` 重建、
  `TVPEventInvoked` 每tick 复位、`DeliverWinUpdate` 工作过、`RTProbe [SAME]`、`Show` 无 skip。
  加上第 3 段同游戏复开正常 ⇒ 渲染与重启渲染机制无误。
- **根因定性**：二次打开（open#2）的**游戏侧每帧驱动（TJS 脚本推进/定时器/连续处理/主层连续重绘）
  未转**，脚本起了一次（startup.tjs 跑完、建 layers）后便不再产出画面 → 黑屏定格。与"第二游戏日志
  更少"吻合。属脚本引擎/连续处理器重启问题，非渲染谱系。
- `TVPResetScriptEngineForRestart`（ScriptMgnIntf.cpp:591）仅清 guard 标志，疑似不是此处；
  最可能是一次性 `tTVPAtExit` 注册失效（`TVPDestroyContinuousHandlerVector`/timer 线程）致二次打开的
  连续/定时驱动不建立（候选①强化）。

### tTVPAtExit 一次性注册失效——核实 + 安全修复（2026-09-08）

**核实结论：部分成立——"二次退出不跑 at-exit 清理"属实（清理卫生缺口），但它不是黑屏根因。**
- 二次退出（open#2 的 teardown）确实不会再跑进程启动时那批 at-exit 清理（清理卫生缺口）。
- 它**并不阻止第二游戏建立每帧驱动**，因为这些驱动都是懒重建/静态向量，二次 `StartApplication`
  会重新建立：
  - `TVPTimerThread`：`Add()→Init()` 懒创建（TimerImpl.cpp:307-333），游戏#2 首个 TJS 定时器即重建。
  - `TVPContinuousHandlerCallLimitThread`：`TVPBeginContinuousEvent` 里 `if(!...)` 懒创建（EventImpl.cpp:234）。
  - `TVPContinuousHandlerVector` / `TVPContinuousEventVector`：静态 vector，`TVPAddContinuousHandler` 重填。
- 结论：at-exit 一次性注册失效不会让 game#2 的每帧驱动"建立不起来"；真根因仍需从脚本驱动侧实测确认。

**但"清理卫生缺口"曾误判为需修——实测重放会崩，已回退（终判：at-exit 是一次性进程 teardown，原设计正确）**：
- 曾尝试把 `TVPAtExitInfos` 改为持久清单、每次 `engine_destroy` 重放全部 at-exit handler（含把
  `TVPDestroyLogObjects` 复位 init 标志）——**真机第 2 个游戏退出时在 CLEANUP handler 上 SIGSEGV 闪退**
  （二次 `engine_destroy` 的 `TVPCauseAtExit` 重放，`handler[15]=FreeAllocator` 之前那个 pri=10000 崩溃）。
- 根因定性：这些 at-exit handler 是**按进程周期一次性**设计的（进程启动注册一次、进程末清理），
  不是"置空+下次 Init 重建"的按引擎周期 teardown；跨游戏重放会触碰已按需重建的单例，导致
  double-free/use-after-free。多 handler 都长这样，逐个修是打地鼠且高风险。
- 终判：**回退该修复**（`SysInitIntf`/`DebugIntf` 恢复原样）。框架单例本就跨游戏安全复用
  （timer/连续事件懒重建），不漏 teardown 不致命；原设计对 runtime-restart 是正确模型。
- 教训：runtime-restart 下，at-exit 只该在"真进程退出前"跑一次；引擎间复用应全走 lazy-init/显式 lifecycle，别重放 at-exit。

### 已加脚本驱动侧判别探针（KRKR_RENDER_PROBE，与现有探针一次日志定死）

| 探针 | 位置 | 打印 | 判 |
|---|---|---|---|
| `ContinuousProbe` | `EventIntf.cpp` TVPDeliverContinuousEvent | `eventVec / handlerVec` 大小，每 30 次 | 两向量空且不涨=连续驱动没建立 |
| `TimerProbe` | `TVPTimer.cpp` ProgressAllTimer（FireNext 计数） | `cumulativeFired / delta`，每 30 次 | delta=0= TJS 定时器没推进 |
| `KAGLoadScenario` | `KAGParser.cpp` LoadScenario | `name / curStorage / hitRewind / curLine`，每次加载 | hitRewind=1 且 curLine>0=二次命中 Rewind 续跑（宏前奏不复重） |
| `KAGTag` | `KAGParser.cpp` _GetNextTag（tagname 取出处） | `storage.line tag=名`，每 256 条 tag | 二次打开首条即 [linemode] 且 line=1 → 宏前奏确实没跑；序列正常 → 宏注册本身失效 |
| （已有）`RTProbe`/`RequestUpdate`/`Run`/`DeliverWinUpdate` | 见上 | — | 渲染链已证明健康 |

配合判：
- game#2 段 `TimerProbe delta=0` + `ContinuousProbe` 两向量空 → **脚本每帧驱动未建立**（即使脚本起过一次），
  治脚本/连续重启；若向量非空但 delta=0 → 注册了但调度不上，治 EngineLoop/tick 调度口。
- game#2 段 `TimerProbe delta≈4` 在转 + `[TVP Console] エラーが発生しました` → **游戏脚本/KAG 自身报错**停画，非重启状态 bug。

### 换游戏黑屏的实测定性（pocketkrkr_engine(12).log + IINCHO 单开 log13，2026-09-08）——重启残留已证实

**决定性对比（Kemomusu→IINCHO 重启 vs IINCHO 单开全新）：**
- **IINCHO 单开（全新进程，engine(13).log）**：渲染链全程健康——`RequestUpdate` 增长、`DeliverWinUpdate`
  持续、`UpdateDrawBuffer layers=69 draw=15`、`PostBlit err=0` 且颜色逐帧变化（245,252→200,239→147,224…，
  即在播内容）。**全程无 `[linemode]` 报错、无 `first.ks : [linemode]` 字样**。其 `first.ks` 先走前奏
  `@eval KAGLoadScript('debug.dtjs')` → `@call ks_system.ks` → `@call cmdMacro.ks`（注册大量 `[macro ...]`），
  之后 `[linemode]` 已被宏/标签覆盖定义，正常运行。
- **Kemomusu→IINCHO 重启（engine(12).log）**：KAG 模块表与单开**完全一致**（都无 LineMode.tjs，模块表不是差异）；
  `first.ks` 却**跳过前奏**，直接命中 `[linemode]` → `タグ/マクロ "linemode" は存在しません` → 画停黑屏。

⇒ **用户"重启后脚本/宏加载未 reset、仍是残留状态"的方向成立**：二次启动时 first.ks 的宏定义前奏
（KAGLoadScript/`@call cmdMacro.ks`）没执行/失效，导致 `[linemode]` 未注册。这不是插件列表（已否决），
而是 **KAG 场景宏/脚本的加载态在 in-process 重启下残留**。修复方向：定位并复位 KAG 场景/宏加载的
进程级残留（宏注册表/场景位置/已加载脚本缓存），使二次打开时 first.ks 前奏完整重跑。

**reset 链缺口核对（2026-09-08）：`TVPScenarioCache` 是唯一未纳入 reset 的进程级全局。**
- engine_destroy 的 reset-for-restart 覆盖各子系统：存储/插件/类安装/渲染/字体/窗口/trans/drawscene/
  extension 等均有 `TVPResetXXXForRestart()`。
- 但 KAGParser.cpp 文件作用域的 `tTVPScenarioCache TVPScenarioCache(8)`（KAGParser.cpp:249）**只在
  `KAGParser::Clear()` 内部经 `TVPClearScnearioCache()` 清空**，该 Clear 由脚本侧 `kag.Clear()` 触发；
  engine 生命周期结束的 reset-for-restart **没有清它**。
- 若二次启动的 KAG 沿用了首游戏遗留的场景缓存对象（两个游戏 first.ks 名不同本应不命中，但 cmdMacro/
  KAGLoadScript 等共享名若在缓存中会命中首游戏残留），则前奏/宏注册场景可能被错误复用。
- 候选修复：新增 `TVPClearScnearioCache()` 到 reset-for-restart 链（或在 teardown 时清一次），
  与其余子系统一致。需评估脚本侧是否已自行 Clear（多数 KAG 启动会 new KAGParser，实例内各自独立，
  全局缓存却是共享的）。

### KAG 探针首投实测（engine(1).log，2026-09-08，IINCHO→Kemomusu→IINCHO 三连）

**结论前置：宿命证据是 `curLine=510789971` 垃圾值 —— 二次打开 first.ks 时 KAGParser 的 CurLine 未初始化。**
- 第 1 次 IINCHO（正常）：`first.ks curLine=0/0` 干净启动，full KAG 宏加载链（cmdMacro→cmdEffects→
  usr_macro→wesave… 大量 `KAGTag: macro/endmacro/eval`），RequestUpdate++ → DeliverWinUpdate → PostBlit
  全程健康，进 title。
- 第 2 次 Kemomusu：`first.ks curLine=510789971/0` —— **CurLine 是垃圾地址/未初始化值**。此后
  KAGTag 宏加载输出骤减（仅 4077-4085 每条文件一两行），无 RequestUpdate/DeliverWinUpdate/PostBlit，
  脚本几乎不推进。start 后即 destroy。
- 第 3 次 IINCHO：`first.ks curLine=0/0` 干净，但 startup 后**无后续 KAG 加载链**，start → 立即 destroy，
  无任何渲染/宏加载。
- 相较旧 log12 的 `[linemode]` 报错：本次三连都**没打印 linemode / KAG 标签错误**，说明问题已从
  "特定标签缺失"退化为"二次启动后 KAG 场景/脚本整体不振"；且第三次再开同游戏也不再提振。
- **判读**：`curLine` 垃圾值指向 KAGParser 场景对象在重启复用时未正确重建（复用已释放内存/状态）。
  `TVPScenarioCache` 全局缓存未随 restart 清理是首要嫌疑（见上节），其缓存项引用旧消息释放的
  Scenario，二次 `LoadScenario` 命中后 CurLine/指针指向失效内存。

### 换游戏黑屏的实测定性（历史版本保留供追溯）——第 1 版结论已被证伪

**第 1 版（已废弃）：误判为 "IINCHO 本身 KAG 不兼容"**。依据：game1=Kemomusu 渲染全健康，
game2=IINCHO 在 `first.ks` 第 1 行 `[linemode]` 抛 **`タグ/マクロ "linemode" は存在しません`** 后画停。
但用户实测确认 **IINCHO 是能正常游玩的正经游戏** ⇒ 该 KAG 错误只在"作为第二游戏重启打开"时才出现，
**它就是 runtime-restart 残留的症状**，不是游戏本身不兼容。
- 现象复核：全新启动的 IINCHO，`[linemode]` 标签是在的；作为第二游戏启动时该标签不见了
  （KAG 标签/宏注册没在二次启动时重新建立 → "不存在"）。这正是"换游戏才黑、同游戏复开正常"的本质：
  重启后脚本/KAG 全局状态残留了第一个游戏（Kemomusu）的东西，没有正确清空/重建。
- 时间线证据（与重启无关的判据在此不成立）：错误发生在二次 `StartApplication` 的 KAG 场景解析期，
  而引擎 tick / 定时器 / 连续 handler 仍照常运转——说明**驱动没停，是 KAG 层注册态坏了**。
- 结论修正：黑屏真根因仍指向**脚本引擎/连续处理器/全局脚本态的 runtime-restart 复位缺失**
  （候选①方向复活）。
- **日志里最具体的重启签名（`[linemode]` 由 KAG 的 LineMode 模块定义）：**
  - game1（Kemomusu）KAG 模块表**加载了 `LineMode.tjs`+`LineModeEx.tjs`**（本 log 行 233-234，
    在 DefaultMover 之后、MainWindow 之前）→ `[linemode]` 有定义，场景正常画。
  - game2（IINCHO）KAG 模块表**从 DefaultMover 直接到 MainWindow，全程未加载 LineMode.tjs**
    （行 1307-1308）→ `first.ks` 第 1 行 `[linemode]` 未定义报错 → 画停 → 黑屏。
  - 若 IINCHO 全新启动能正常游玩（KAG 里本应有 LineMode/`[linemode]`），则"作为第二游戏时
    LineMode 没被加载"就是重启残留的具体表现——KAG 模块/脚本加载链在二次启动少加载了模块。
- **复核实验（无需改代码即可定死）**：抓一份 IINCHO 作为**第一游戏、全新进程**的日志，对比
  其 KAG 模块表是否包含 `LineMode.tjs`：
  - 全新 IINCHO 含 LineMode 但二次打开不含 ⇒ 重启下 KAG 模块/脚本加载丢失，修复目标锁定该加载链。
  - 全新 IINCHO 也不含 LineMode 但能玩 ⇒ `[linemode]` 由别处（宏/override）定义，二次打开该处失效，
    继续追宏/override 的加载。
- `[linemode]` 未在本项目 C++ 源码定义（仅脚本/KAG 侧注册），引擎源码仅一处无关 MultilineMode。

**"reset 后还是第一个游戏的插件列表"假设——已被日志否决：**
- game2 的 `tvpLoadPlugins` 用的是 **IINCHO 自己的插件列表**（extrans/layerExBTOA/layerExImage/wuvorbis/
  KAGParser/menu/xpzdec），不是 Kemomusu 的（AlphaMovie/KAGParserEx/getSample/layerExDraw/psbfile/psd/windowEx）。
- game2 挂载的也是自己的 8 个 xp3（iincho-re.co），路径/归档无 Kemomusu 残留 ⇒ 存储未泄漏。
- reset 链本身完整：`TVPResetPluginSystemForRestart`（清自动加载计数/模块状态/ncb 注册表）、
  `TVPResetExtensionClassInstallStateForRestart`（清全局类安装态）、`TVPResetStorageImplForRestart` 等均已实现。
- 故"插件列表 stale"不成立；真正差异仍集中在**game2 自己的 KAG 模块表缺 LineMode.tjs**（见上），
  待全新 IINCHO 日志定它是包配置如此还是重启加载丢失。

记录此日志文件名供后续对照：`.uploads/29c5fff9-...-pocketkrkr_engine(12).log`

### 决定性 A/B：换游戏黑屏是 restart 专属（engine(14)/(15).log，2026-09-08）
> 用两份"同一进程多开"日志把换游戏黑屏定性为**纯 restart 残留**，并排除三个假说。

**取证（log15 = IINCHO→Kemomusu→IINCHO 同一进程）：**
- game1 IINCHO(白)：`Config.tjs`(→118) → `KAG System スクリプトを読み込んでいます`(120) → 全 KAG 模块 →
  `KAGMainWindow コンストラクタ`(156) → `RequestUpdate`(153) → 渲染。**但全程无 `kag:` 对象**（KAGParser.dll Failed）。
- game3 IINCHO(黑)：插件 → 建窗 → `Startup script ended`(3950) → **无 Config/KAG-System 模块加载块** → 永不 RequestUpdate → 黑。
- log14 game3 Kemomusu(白, 也 restart)：**同样跳过 Config/KAG-System 块**（无 LineMode 重载，`Member "kag" does not exist` L1233），
  但 L1239 重建 `kag:` 对象、L1264 `UpdateDrawBuffer layers=207 draw=12` → 照样渲染。

**结论：**
1. 黑屏 = restart 后游戏 startup 的"KAG System 模块加载块"被跳过，KAGMainWindow/层从未建立 → 永不 RequestUpdate。
2. 该"跳过"是重启共性；Kemomusu 因能重建 KAG 解析器对象照常渲染，IINCHO 依赖该块致致命。
3. **排除假说**：① KAGParser.dll 缺失 —— IINCHO 首次也 Failed 却正常；② 全局 `kag` 缺失 —— IINCHO 首次就无它却正常；
   ③ TVPScenarioCache 残留 —— 重启 IINCHO 连 `first.ks` 也未加载，清缓存不会重新触发被跳过的 boot 块。
4. 门控状态来源：Global 每次新建(tTJS `new`)、原生类每次重注册，均排除；**剩余为某跨 restart 未复位的
   C++ 进程级状态** —— 触发点尚未从日志单钉死，待加"KAG boot 每步探针/抓首个抛错点"定位。

**记录对比日志**：`.uploads/0f0103b2-...-engine(14).log`（Kemomusu→IINCHO→Kemomusu）、
`.uploads/77a31787-...-engine(15).log`（IINCHO→Kemomusu→IINCHO）。

### 与上游 PR#12 的复位差异补口（2026-09-08）
> 逐条对照 `reAAAq/KrKr2-Next#12` 后，我们的复位序列基本同位，仅缺 OpenGL 渲染器复位；已移植。

- **曾移植（85684a8/86f2391，后因回归回退 →1fd6206/109704f）**：
  `TVPResetOpenGLRenderManagerForRestart()` + `TVPResetOpenGLGlobalsForRestart()`（清扩展/格式/GL 函数指针/
  shader 缓存）+ `krkr::gl::ClearRendererRecreatedCallbacks()`。
  **回退原因**：`ClearRendererRecreatedCallbacks()` 清掉了 `5fd30da` 依赖的、供 EGL context 重建后
  `FireRendererRecreated()` 触发的"重建共享 _FBO/_stencil_FBO + shader"回调；而我们复用渲染器单例
  （`TVPResetRenderManagerForRestart` 明言绝不 delete），PR#12 的设计是重建实例、二者生命周期不同 →
  清回调致二次打开乱屏复现。且 OGL 复位也**未修好换游戏黑屏**。故整体回退，保留 5fd30da 机制。
  **未移植**：PR#12 的异步启动状态机（`EnsureEngineRuntimeInitialized`/`engine_get_startup_state`/
  `engine_drain_startup_logs`）——体量大属架构改造，暂缓。

### StartupProbe 首测 conclusive（engine(16).log，2026-09-09）
- 三次启动（Kemomusu→IINCHO→Kemomusu）`StartupProbe` **均为 `completed without throwing`**。
- 首开 Kemomusu：`AppConfig/Config.tjs` + `KAG System スクリプト` 全链加载。
- 非首开（IINCHO、重开 Kemomusu）：**不加载 Config/KAG-System boot 块**，且**无任何文件打开失败/抛错**。
- ⇒ **排除**"异常吞掉/fallback"假设。黑屏是游戏 startup.tjs 里**静默条件分支**：restart 后读到某跨游戏
  残留状态即提前返回、跳 KAG boot。Kemomusu 无 boot 也能渲染，IINCHO 依赖 boot 致黑。门控状态待进一步定位。
- **误判澄清（engine(2).log）**：IINCHO(黑)启动段出现 `RequestUpdate: repaint requested (cum=181)` 一度
  怀疑跨 restart 重绘残留；查证 `cpp/core/visual/WindowIntf.cpp` 的 RequestUpdate 中 cum 是
  KRKR_RENDER_PROBE 下**静态局部变量 s_reqUpd**（不随 restart 重置）的纯计数，非引擎残留状态，属探针伪影。
- **推进**：引擎侧已排除（异常/fallback/漏复位/重绘残留），改加 StorageExec 探针（TVPExecuteStorage 低频
  记录 startup 链实际执行的文件），直接对比 restart 时从哪个脚本断掉/跳过 KAG boot。

### StartupProbe 二次确认 + 执行链定性（engine(3)(1).log，2026-09-09，Kemomusu→IINCHO→Kemomusu 探针包）
- `StartupProbe` 三次启动**均为 `completed without throwing`** → gate 再次排除异常/fallback，是 startup.tjs 静默分支。
- **渲染侧铁证**：IINCHO(第2个游戏) startup 后**仅一次全黑 blit**，此后 ~2.6s 运行期 `RequestUpdate=0`、
  `DeliverWinUpdate=0`；而 C++ tick/Timer 健康（TimerProbe 56→94 持续触发）→ 黑屏 = 游戏脚本从不请求重绘，
  tick/定时器/事件线程未停。
- **资产/KAG 标记**：Kemomusu 两次启动都在 startup 窗口内 `kag:`（motionplayer ResourceManager）+ 加载
  `ezsave.PIMG`/`bgmtitle.PIMG`；IINCHO **完全不加载任何 psb、无 motionplayer 初始化** → boot 链从未推进到
  场景资产加载。
- **cmdArgGen 澄清（非 gate）**：
  - entry 值 Kemomusu=2、IINCHO=1 的差来自 **patch.tjs 是否存在**：Kemomusu 有 patch.tjs（exec#1，其中
    `System.exeCommandLine=` 一次 +1）；IINCHO 无 patch.tjs → entry=1。与 restart 残留无关。
  - IINCHO startup 期间 1→12（11 次 `System.exeCommandLine=`）= 其 config 链（config/gameConfig.tjs 等）
    自身行为，首开同样会发生，非门控。
- **IINCHO 启动链采样**（exec#121/#161）：config/gameConfig.tjs → `Plugins.link`（KAGParser.dll / menu.dll
  Failed 被脚本吞掉继续）→ 建窗 → exDefaultMover.tjs → 完成。KAGParser.dll 加载失败属插件兼容清单
  （P0 drawdeviceD3DZ 之外的 KAGParser 类插件），但首开也会失败，不是 restart gate。
- **重开 Kemomusu（第3会话）正常渲染** → restart 残留并不破坏 Kemomusu；gate 是 IINCHO 特有的静默分支。
- **下一步**：缺 IINCHO**首开**（工作态）基线。StorageExec 探针已改全量记录（exec# 累计序号），
  用同一探针包跑「IINCHO 首开」与「Kemomusu→IINCHO」，diff 两条 StorageExec 链即可钉死分支断点脚本。

### 链 diff 定论 + [TVP Console] 日志系统 restart bug（engine(19)/engine(20).log，2026-09-09）
- **engine(19)=IINCHO 首开（工作态），engine(20)=Kemomusu→IINCHO（黑屏）**。全量 StorageExec 链 diff：
  - 两条 IINCHO 链 **逐条完全相同**（各 67 个脚本：patch→startup→ks_system/exLayer→config/gameConfig→
    system/Initialize.tjs→Config..exSE→savesc/savesu→AfterInit→gmPlugin..exButtonLayer）。
  - ⇒ **推翻"跳过 KAG boot"假说**：黑屏会话也完整加载了 KAG System。
- **首开会话 boot 尾段（可见输出）**：exButtonLayer 后 → sys_load/sys_save 等 11 个系统图层加载 →
  AfterInit 完成 → `Scenario loaded : first.ks` → `処理を開始します` → first.ks 逐 tag 处理（@call ks_system.ks
  等）→ `ContinuousProbe: eventVec=0 handlerVec=2`（KAG 更新循环以 `System.addContinuousHandler` 注册）→
  RequestUpdate → 渲染。
- **黑屏会话**：同样 67 脚本后 **无任何可见 boot 尾段输出、无 first.ks、无 ContinuousProbe（=连续事件投递从未发生，
  `TVPProcessContinuousHandlerEventFlag` 未置位/无 handler 注册）、无 RequestUpdate**。StartupProbe 仍 completed。
- **关键新发现——日志系统 restart bug（已修）**：`DebugIntf.cpp` 的 `TVPDestroyLogObjects`（tTVPAtExit 注册，
  每次 TVPSystemUninit 触发）删除 `TVPLogDeque`/`TVPImportantLogs` 后**未复位 `TVPLogObjectsInitialized`**，
  导致 restart 后 `TVPEnsureLogObjects` 提前返回、`TVPAddLog` 静默丢弃 → 全部 `[TVP Console]` 输出（含 KAG 场景/
  错误信息）在 restart 会话永久消失。修复：销毁时复位标志。**此 bug 同时解释了此前"KAG boot 被跳过"的误判——
  场景处理日志在 restart 后本就不可见**。
- **残余问题**：修复日志后黑屏成因仍待一次复测——若 boot 尾段实际执行（图像加载/AfterInit/first.ks 只是日志被吞），
  黑屏=连续事件投递/重绘未建立；若尾段真未执行，则看恢复后的 KAG 报错。

### 根因钉死：TVPScenarioCache 跨游戏同名场景碰撞（engine(21).log，2026-09-09，日志修复后复测）
- 日志修复后黑屏会话可见完整 boot 尾段：sys_* 图层加载 → AfterInit(232ms) → `Scenario loaded : first.ks` →
  **`first.ks : [linemode]` → `エラーが発生しました / タグ/マクロ "linemode" は存在しません`（行 1）**。
- **首开会话（engine(19)）first.ks 从 `@eval`/`@nextskip` 开始处理，无任何错误** ⇒ 同一 `kag.loadScenario("first.ks")`
  两次加载到了**不同的 first.ks 内容**：
  - 首开加载 IINCHO 自己的 first.ks（KAG3 stock 结构，@eval 开头）→ 正常。
  - restart 后加载到 **Kemomusu 的 first.ks**（`[linemode]` 开头，KAGParserEx 系）→ IINCHO 的 KAG3 无 linemode → 报错 → boot 中止 → 黑屏。
- **机制**：`TVPGetScenario`（KAGParser.cpp:266）以**场景短名**（如 "first.ks"）为 key 存进程级
  `TVPScenarioCache`（文件作用域 static，KAGParser.cpp:249）；restart 不随 TJS VM 销毁、`TVPClearScnearioCache`
  仅被 `KAGParser::Clear()`（脚本调 kag.clear()）触发。前一个游戏（Kemomusu）加载 first.ks 后缓存命中即跨游戏复用。
- **排除项**：auto-path 表无污染（IINCHO 会话重建后 Total 3084 = IINCHO 自身 8 个 xp3 文件数，恰为
  116+323+502+83+101+480+17+1462）；兄弟 xp3 扫描两者均 "No sibling"（xp3 在各游戏目录内，父目录无裸 xp3）。
- **修复**：`TVPResetRuntimeForRestart`（engine_destroy 复位链）中调用 `TVPClearScnearioCache()`，
  换游戏/复开前清空跨游戏残留的场景缓存。同游戏复开仅损失一次场景解压。

### PR#12 剩余差异全量扫描（2026-09-08/09）
- ✅ **已对齐且保留**：全部 `TVPReset*ForRestart` + `TVPUnregisterInternalPluginsForRestart` +
  `TVPResetPluginSystemForRestart` + `ncbAutoRegister::ResetModuleStateForRestart` + FontImpl release；
  EngineLoop 析构复位（6521e4f）。
- ⭕ **回退**：OGL 复位/GL 函数指针/ClearRendererRecreatedCallbacks（见上，回归同游戏复开乱屏）。
- 🟡 **暂缓**：LogSetup.cpp/h（异步机日志辅助，不移植异步机为死代码）。
- ⚪ **不适用/无需**：
  - StorageImpl `_tjs_normalize_nfc` GetLen —— 我们的 Apple 路径已用 `static_cast<CFIndex>(name.GetLen())`，殊途同归。
  - cubism/krkrlive2d（+188/-16 / +14/-3）—— R4→R5 SDK 迁移，我们仍用 R4 API，直接挪编译不过；与 restart 无关。
  - platforms/{linux,windows}/main.cpp 桌面端 spdlog —— Android 走自有路径。
  - CMakeLists/vcpkg libarchive/.gitignore/pubspec.lock —— 构建依赖/杂项。

## 自检 / 验收

- 目标游戏（魔女的夜宴/sabbat_kr）启动后：主 `DrawBuffer` 被合成、源纹理非全黑、draw 计数增长。
- `plugin/` 目录缺失插件从"Failed"变为可 `Plugins.link`。
- 视频 OP 可播放（krmovie Present）。