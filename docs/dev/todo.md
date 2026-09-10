# 待办 / 已知问题

> 维护当前未完成或待验证的工程事项，供 AI Agent 与开发者交接。
> 完成某项并验证后删除/移入文末「已解决（极简）」，并同步 AGENTS.md 的「当前状态」。
> 已完成的历史排查过程不在这里归档（如需回溯看 git log / 具体提交注释）。

## 进行中 / 待办（按优先级）

### P0 — §2. Z（krkrz/KIRIKIRI Z）插件兼容：移动端黑屏根因【高优】
> 移植清单/参考源（在线）：见 [krkrz-compat.md](krkrz-compat.md)；Kirikiroid2（安卓完整移植）为最强参考。
- **现象**（魔女的夜宴/sabbat_kr 目录版）：启动正常、XP3 全挂载、脚本/图层照常，但合成源纹理始终
  `(0,0,0,255)` 纯黑、draw 计数卡死不涨 → **主 DrawBuffer 从未被合成**（=初始 0xFF000000）。
- **已排除**：`VideoOverlay total=0` → 不是视频；源纹理=主 LayerManager DrawBuffer → 走的是 Z 专属路径，
  我们未接入。
- **插件缺口**（real 游戏 `plugin/` 全 Failed，引擎无实现/stub）：`drawdeviceD3D/Z`、`kztouch`、`k2compat`、
  `kagexopt`、`multiimage`、`squirrel`、`PackinOne`。
- **已做**：③ 挂名 + 逐个实现（`cpp/plugins/zcompat/`）：
  - 挂名 16 个 Z 插件名（link 不再 Failed）；drawdeviceD3DZ/D3D、kztouch、menu 确认功能已内建核心。
  - **k2compat**：内嵌 Krkr2Compat 纯 TJS 层（`zcompat/k2compat_scripts.cpp`，10 脚本）已生成，
    **但引擎启动(LoadAllModules)时强制执行抛异常 → 三游戏全黑（engine(22)），已回退为挂名**
    （CMakeLists 不再编译该文件）。真实现待改走"游戏运行时显式 Plugins.link(k2compat.dll)"时机再放回。
  - **motionplayer_nod3d → motionplayer.dll 映射**（PluginImpl.cpp `TVPLoadPlugin`），复用已有实现。
- **下一步**：① Windows 跑同游戏二分（引擎 vs 渲染链路）；② 对照 Kirikiroid2
  `src/core/visual/RenderManager_ogl.cpp`+`BasicDrawDevice.cpp` 与我们的 `RenderManager.*`，定位
  DrawBuffer 未合成原因；③ squirrel 移植（krkr2 trunk `src/plugins/win32/squirrel` + Squirrel VM 源码
  可得，待按 k2compat 同法内嵌；multiimage 源码不可得，暂挂名）。
- **缺口核对（2026-09-09）**：核心 API 层已确认不缺 `Pad`/`PassThroughDrawDevice`/`MenuItem`/`KAGParser`
  （核心均注册）；唯一可选缺项 `System.getDisplayMonitors`（K2COMPAT_SPEC_DESKTOPINFO 默认关）。
  真正未收口仍是 P0 引擎能力：Z 主层 DrawBuffer 合成 + krmovie Present。

### P1 — §2a. motionplayer 缺 `Motion.D3DAdaptor` + `captureCanvas`：千恋万花首屏后无法进入【属 motionplayer 兼容】
> `Motion.D3DAdaptor` 是 **krkr2 motionplayer** 成员（Direct3D affine），非 krkrz/Z 专属——实证
> Kirikiroid2_patch(zeas2) 给千恋万花的 patch.tjs 用 `typeof Motion.D3DAdaptor=="undefined"` 兜底，说明
> 真机 krkr2 的 motionplayer.dll 确有 D3DAdaptor。移动端标准做法=undefined+useD3D=0；我们暂用「可 new 空类」
> 让 `new Motion.D3DAdaptor(...)` 走通、由 CPU/GL motion 播 logo。
- 现况：`getD3DAdaptor` 返回 `new tTJSNativeClass("D3DAdaptor")`（`cpp/plugins/motionplayer/main.cpp`）。
- **2026-09-09 实测（engine(23)）**：三游戏(千恋万花) logo 能显示、渲染健康（SourceSample 25/25、PSB 14 图已缓存），
  **点击后走 `yuzulogo` 动画 `drawAffine` 时，`_window.motionWorkLayer.captureCanvas()` 报
  `Member "captureCanvas" does not exist` → 致命脚本错误 → 闪退**。
- `captureCanvas`/`motionWorkLayer` 是**千恋万花 data 自带 `affinesourcemotion.tjs`** 动画辅助 API，
  **krkrz/krkr2/Kirikiroid2/KrKr2-Next 源码全都没有该实现** ⇒ 无参考可抄。
- **2026-09-09 宿主定位（脚本字节码符号表）**：4 个 data 脚本（patch/affinelayer/affinesourcemotion/motion.tjs
  均为编译后 `TJS2100` 字节码）。`captureCanvas` 是 **affinesourcemotion.tjs 里 AffineLayer 派生类的脚本类
  成员**（与 `D3DAdaptor`/`motionWorkLayer`/`motionD3DAdaptor`/`unloadUnusedTextures`/`updateImage`
  同簇）；base `affinelayer.tjs` **不含**它。
- **2026-09-09 根因反转 ×2（真机 engine(24/25/5) 实证）**：
  - ① `b008020`（native Layer captureCanvas no-op）进了 main@7927938 但真机仍 `captureCanvas`
    缺失 → 工作层对象非 native Layer 继承链（换 D3DAdaptor 为正式类 + 类上 no-op 后见下）。
  - ② **`Motion.D3DAdaptor` 不能是 undefined**：engine(5) 两次打开千恋万花均崩在
    `mainwindow.tjs (property getter) motionD3DAdaptor`——它**无条件**取 `Motion.D3DAdaptor`
    （先算 scWidth/2、pxHeight/2 再访问），置 undefined → `Member "D3DAdaptor" does not exist`
    致命。Kirikiroid2 能跑此游戏正说明它**定义**了 D3DAdaptor（krkr2 motionplayer 本来就有该成员）。
- **修复**：`cpp/plugins/motionplayer/main.cpp` `getD3DAdaptor` 返回**内建空类**
  `Create_NC_D3DAdaptor() = new tTJSNativeClass("D3DAdaptor")`（`new` 收任意参创建实例）。
  Layer 原生 `captureCanvas`/`unloadUnusedTextures` no-op（LayerIntf.cpp `b008020`）兜底。
  ⚠️ 迭代史：`815d8c3` 移除→undefined 崩（Member 不存在）；`4166901` NCB 空类无构造→`new`
  崩（"Called method is not implemented"）；`948ee37` classic tjsNative 在自由函数用
  TJS_BEGIN_NATIVE_MEMBERS→**编译失败**（该宏用 `this`）；**`6d36839` 回退内建空类，编译通过**。
  ⚠️ 但 engine(24) 实证：内建空类 + Layer-native captureCanvas 时，`motionWorkLayer.captureCanvas()`
  仍报 Member 缺失 → motionWorkLayer 既非 Layer 也非空 D3DAdaptor 实例，是**脚本对象**。
- 待定（需决策）：captureCanvas 落在脚本对象上，native 无解。两路并行参考：
  ① **脚本 polyfill**：运行时给游戏 AffineLayer 系脚本类注入 `captureCanvas`/`unloadUnusedTextures`
  no-op（需要游戏脚本类名/宿主，读 mainwindow.world 字节码确认）；② **Kirikiroid2 同款**（绝对参考）：
  让 `Motion` 整体不存在 → 游戏 `_motionD3DAdaptor` 不赋值 → CPU 路径，根本不调 captureCanvas
  （但会影响真用 motion 的 Z 游戏，需用户取舍）。
- **2026-09-09 方案②证伪 + Kirikiroid2 实证（最新，方向反转）**：
  - **Kirikiroid2 不是"让 Motion 整体不存在"**——GitHub 仓库源码（zeas2/Kirikiroid2）确实搜不到
    Motion/motionplayer，但那是**源码未同步**；下载官方发布 APK（1.3.9）反查 `libgame.so`，
    **完整存在**：`D3DEmotePlayer/D3DEmoteModule/SeparateLayerAdaptor/D3DAdaptor/useD3D/enableD3D/
    Motion::ResourceManager/setEmotePSBDecryptSeed/setEmotePSBDecryptFunc/captureCanvas/
    unloadUnusedTextures/canvasCaptureEnabled/motionplayer.dll/emoteplayer.dll`。
  - zeas2 官方补丁库 `Kirikiroid2_patch`（github.com/zeas2/Kirikiroid2_patch）含**千恋万花补丁**
    （patch/ゆずソフト/千恋＊万花/patch.tjs）：`Plugins.link("motionplayer.dll")` +
    `typeof Motion.D3DAdaptor` 两分支**都强制 `useD3D=0`**（移动端无 D3D9，走 CPU/GL），
    并 `System.setArgument("-hdresomode","1080")` + 清空 movieQualitySelectMenuItem。
  - ⇒ 结论：**保持 Motion 存在（D3DAdaptor 可 new）+ captureCanvas/unloadUnusedTextures 挂到
    SeparateLayerAdaptor（motionWorkLayer 宿主）+ useD3D=0** 与 Kirikiroid2 行为一致；当前主线正确。
  - **本次改动（待提交）**：`cpp/plugins/motionplayer/main.cpp` SeparateLayerAdaptor 新增
    `captureCanvas`/`unloadUnusedTextures` no-op 注册（转发目标 Layer，兜底清空返回值）。
  - 其余成员（`Motion.Player.varibleKeys`/`Motion.EmotePlayer.setCameraCoord/Rotate/Scale`）：
    Kirikiroid2 APK **也没有**，游戏脚本自带 `typeof==="undefined"` polyfill 兜底，无需实现。
- 附：engine(5) 二次打开千恋万花（崩溃后不杀进程再进）出现 `The object is already invalidated`
  （mainwindow defaultStableHandler）——重启后的残留原生对象被访问，属独立 restart 问题（待查）。
- 待办：① ✅ 根因 ×2 + D3DAdaptor 正式类（captureCanvas/unloadUnusedTextures no-op 双覆盖 +
  SeparateLayerAdaptor no-op，2026-09-09 补）；② 真机复验 yuzulogo 后不再崩；③ 若仍报 missing member，
  读 mainwindow.tjs motionD3DAdaptor getter 完整字节码，逐个补 D3DAdaptor 实例成员。

### P1 — §2b. motionplayer 的 `EmotePlayer`（emoteplayer.dll）未实现【NEW】
- 现况：`cpp/plugins/motionplayer/EmotePlayer.{h,cpp}` 是**空壳 stub**（仅 `_useD3D` 读写，无 emote 物理/播放）。
- 影响：M2-Emote 型作品（如 limelight-lemonade-jam 那类）派 motion 时无真正向量骨骼动画。
- 优先级：**排在 §2（krkrz 黑屏）之后**；与 §2a/§6 独立，无前置依赖。无稳定环境复现，属"有空再做"补齐。

### P1 — §2c. motion 动画播放（M2 时间轴，logo/标题真正动起来）【当前在做】
- 现象（K2 对照）：K2 里首屏 logo、首页标题是**连续播放的动画**；我们目前**静态合成**——把动画所有
  帧的图层一次性画出来（`yuzu_logo` 两帧 y 差 17px 叠出重影 + 中心黑线；`title_bg` 的 `char_move`
  运动角色钉在某一帧）。能出画面但"不还原游戏表现"。
- 目标：按时间推进播放 motion 帧（`progress()` 推时钟 → draw 时只画当前时刻该出现的帧/节点，
  带位置/透明度取值），对齐 krkrsdl3 `emoteengine`（`plugins/emoteplayer/{emotefile,emoterunner}.{cpp,h}`）。
- 现状基础：`psbfile/PSBMedia` 已能解析对象树/图层坐标/motion 字典/`ExtractFrameInfo`（帧级 time/abort）。
- 方案（MVP→完整）：① 帧步进：每 motion 关键帧按 time 取当前帧，只画当前帧引用的图层（先不做节点插值）；
  ② 补透明度/位置插值；③ 完整移植 krkrsdl3 emoteengine（网格/节点/物理/时间轴）为远期。
- 进度：✅ 首屏动起来的主因已修：PSB 归档是**懒加载**的，`loadMotionTracks` 在首帧归档未解析时查
  得 0 条并把空结果永久锁存（`_motionTracksLoaded=true`），此后只走静态 `drawPSBImages`、`drawAnimated`
  永不触发。已新增 `PSBMedia::ensureArchiveLoaded()`（幂等），在 `Player::draw` 里先强制解析归档再
  `cachePSBImages`/`loadMotionTracks`（engine(6) 实证 `Stored 13 tracks for LOGO/yuzulogo` 但读时却是 0）。
- 进度：✅ **时间单位换算（2026-09-11，engine(11) 实证根因）**：PSB `frameList[].time` 是 **60fps 帧数**
  （yuzulogo 尾帧 t=241=4s、m2logo back_white t=91=1.5s、title t=120=2s），而 `progress(delta)` 由脚本以
  **毫秒**推进时钟 → 4 秒的 logo 时间线约 240ms 就播完，所有角色/字母瞬间同时出现（用户："人物没有依次
  出现、时间太短；logo 动画特别乱"）。已改为在 `PSBMedia.cpp` 解析处统一 `time ×1000/60` 转毫秒
  （扁平轨道 `CollectMotionTracksFromMotion` + 节点树 `CollectMotionNodeFrames` + `loopTime` 同换算），
  下游 `progress`/`drawAnimatedTree`/`drawAnimatedFlat` 全用毫秒比较。
- 进度：✅ **容器帧插值（2026-09-11）**：插值条件从"仅 `src/` 图像帧"放宽为"任意有内容帧"
  （含 `src='layout'`/子运动容器帧），logo 字母/柚子汉字的滑入（cx 48→-24→0）与 m2logo 部件淡入
  平滑过渡，不再在关键帧间跳变。
- 进度：✅ **空帧即隐藏（2026-09-11，engine(3) 实证）**：去掉"时间线播完→保持末内容帧"回退，
  空帧在覆盖时段内隐藏该层（含时间线末尾空帧）。此前 yuzulogo 字母/柚子汉字/software 在 t=215f
  后仍被 keep 到 4s，导致"播放前背景有完整静止 yuzulogo"（真实动画 3.58s 字母即消失、只剩白底）。
  节点树 `drawAnimatedTree` 与扁平 `drawAnimatedFlat` 同步修改；稳态 motion（normal/status）
  末尾本来就是内容帧，不受影响。
- 进度：✅ **PSB 帧 `type` 解析（2026-09-11）**：此前只按"有无 content"判断帧可见性，从未读
  PSB 帧的 `type` 字段。参考 libkrkr2 `sub_6926B4 parseFrame`：`type==0` 一律是**不可见帧**
  （图层的隐藏初始态），即使带 content/src——yuzulogo 的 `white`/`logo` 层 t=0 帧带
  `src='src/yuzu/yuzu_logo'` 但 type==0，被旧逻辑当可见帧画出，正是"播放前背景就有完整
  静止 yuzulogo"的根因（用户澄清：不是播放完残留）。已改 `PSBMedia.cpp` 两处帧解析
  （`CollectMotionTracksFromMotion` + `CollectMotionNodeFrames`）读 `type`，`type==0 → visible=false`；
  `PSBMotionFrame` 增 `type` 字段，`Player.h` 帧 dump 日志加 `ty=` 便于真机核对。
- 进度：✅ **M2 文本子树左对齐（2026-09-11，engine(3) 实证）**：m2logo "cheeseware" 字母
  （str_clip/str_locate 子树）按文本笔位在 advance 锚点**左对齐**绘制，不再居中——居中会让
  较宽的 'w'（icon39 比 'e' 宽 30px）向左压到前一个字母，整行字错乱。`_nodeInStrSubtree`
  预计算（祖先链含 str_* 容器）；普通图像节点（yuzu 字母等）保持居中。
- 待真机：① 三游戏 logo + 标题入场时序/速度对照 K2；② m2logo 仍缺 `iconXX` 部分纹理
  （`Unsupported image format (header 19190519)` → 1x1 透明兜底）；③ yuzulogo 语音（脚本驱动）核对；
  ④ m2logo `str_clip`（src='clip'）裁剪未实现——cheeseware 文字无裁剪/擦除效果；
  ⑤ 标题入场→steady 切换处"最后几像素瞬移"：日志确认人物滑入平滑，跳变在游戏切换稳态
  （entrance 终位 vs normal 稳态坐标差）或亚像素截断，需含稳态段的日志复核；
  ⑥ 若标题入场后（2000ms）背景变黑 → 说明游戏依赖 hold 而非切换稳态，届时对非 logo 场景
  恢复"末尾保持"。
- 参考提交/对照清单：见 [krkrz-compat.md](krkrz-compat.md)「krkrsdl3 emoteplayer 对照」。

### — 文本尺寸比 K2 略小 + 选项框文字偏左上【待做，独立于 motion】
- 现象：同款游戏文字 K2 略大一点点；选项框文字在框的左上（K2 里框内正常）。首次上报于 engine(5)，
  早于 motion 改动；且选项界面无 motion 活动（engine(30) 实证 drawPSBImages/drawOnto 未出现）→
  与 motionplayer 无关，是引擎**文本渲染/字形度量或全局文本缩放**问题。
- 已排除：`realLayer geo left=0 top=0 w=1920 h=1080`（合成目标层无原点偏移，engine(30)）；
  letterbox 缩放链路结构正确（ui_stubs.cpp FlutterWindowLayer 等比 letterbox + SetWindowSize 不动虚拟屏）。
- krkrz 源码无 `hdresomode`/全局字号常量 → 字号为每游戏脚本自定（虚拟屏+Font），模拟器不统一。
- 待做：拿 K2 与我们**同画面对照截图**（最好带像素标尺）量字号倍率，区分"全局缩放/度量系数"（可能连带
  修好选项框偏移）vs"字体度量"（改文本渲染 `textrender`/字形）。

### P1/P2 — krmovie Present 未实现（视频帧→场景合成）
- 现状：ffmpeg 解码链路完整（`cpp/core/movie/ffmpeg/`），但 `VideoPresentOverlay::PresentPicture` 及 overlay
  合成到场景/纹理仍是 stub（只打 warn）。
- 备注：**已证实非魔女的夜宴黑屏根因**（`VideoOverlay total=0`）；供真正 OP/影片游戏使用。
- 参考：Kirikiroid2 `src/core/movie/krmovie.cpp`+`ffmpeg/KRMovie*.{h,cpp}`、krkrz `movie/win32/krmovie.cpp`。

### — §3c. runtime-restart 切换【不同】游戏：旧 auto-path 未清→路径污染/黑屏【已修，待真机】
- 现象：reset（进程内）后同款可重开；换不同游戏变黑屏（用户疑为 xp3 挂载）。
- 根因：全局 `TVPAutoPathList` 跨重启**只增不清**——上一游戏归档路径（`TVPAutoMountProjectXP3Archives`/
  `Storages.addAutoPath`）残留搜索表，切新游戏可能命中旧归档。同款因 `TVPBoostAutoMountPaths` 去重而掩盖。
- 修复（提交，`StorageIntf.{h,cpp}`+`StorageImpl.cpp`）：新增 `TVPClearAutoPathListForRestart()`，
  `TVPResetStorageImplForRestart` 调用，reset 链清空累积 auto-path。
- 待真机：两个不同 krkr2 游戏互切均正常渲染。

### — §3d. 二次游玩（不杀进程 restart）二次进入 FBO incomplete → 黑屏【新增，待真机】
- 现象（引擎 engine(4)/run 34311307313）：首次游玩正常；**退出不杀进程再进** → 闪屏 → 主界面
  `FlutterWindowLayer::SourceSample: FBO incomplete 0x8cd6`(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
  持续 → BlackScreen。
- 根因：EGL context 在 restart 时先 Shutdown 再重建（run2 重新 "ANGLE EGL context created"），
  但 run2 的 `blitSrcTex/nativeTex` 与 run1 **同 id(=83)** → 该 GL 纹理/渲染目标对象是进程级残留，
  新 context 下附件失效。`_FBO` 已由 `5fd30da` 在 `FireRendererRecreated` 重建，但**纹理（主
  DrawBuffer/背景等图形缓存 gcache 条目）没有在新 context 里重新上传**，被 continue 复用。
- 修复（提交）：`RenderManager_ogl.cpp` 的 `OnRendererRecreated` 回调内追加 `TVPClearGraphicCache()`，
  让残留 GL id 在下个 context 按需重新上传为有效纹理（对应 Kirikiroid2 式 context-loss 清缓存）。
- 待真机：退出→不杀进程→二次游玩渲染正常；若仍黑，需运行时探针定位 id83 纹理确切宿主
  （LayerManager DrawBuffer vs gcache），再针对性重建。

### — §3e. 换不同游戏（restart）后 BGM 与上一游戏重叠【已修，待真机】
- 现象（engine(8) 实测链路 IINCHO→Kemomusu→IINCHO）：二次打开**不同**游戏后，BGM 疑似叠加了
  上一游戏的（都 StartApplication 正常、无异常）。同游戏复开不明显。
- 根因：`ShutdownWaveSoundBuffers` 是**一次性 `tTVPAtExit`（PRI_PREPARE）**，首次 engine_destroy 的
  `TVPCauseAtExit` 已把 `TVPAtExitInfos` delete 置空；之后每次重启 teardown 的 `TVPCauseAtExit` 提前 return，
  **不再停混音线程 `TVPWaveSoundBufferThread`** → 上一游戏 BGM 继续响、叠进下一游戏（与日志闭包 `4221543`
  同类一次性 at-exit 问题，但声音模块无 restart 特例）。
- 修复（提交，`SysInitIntf.cpp` `TVPResetRuntimeForRestart` + `WaveImpl.cpp`）：新增
  `TVPStopAllWaveSoundsForRestart()`——停掉 `TVPWaveSoundBufferThread` + 释放存活 buffer（`TVPReleaseSoundBuffers`），
  restart 末尾显式调用。
- 待真机：切游戏后 BGM 不再残留（首屏 logo 即干净）。

### — §3b. 快速 skip 消息框黑块【间歇，挂起低优先】
- 现象：快速 skip 时本应透明的消息框偶发整块变黑；再次 skip 未复现。
- 方向：skip 快速帧间混合/预乘路径或遮罩刷新；与 runtime-restart 无关。
- 处置：下次再现记录触发场景 + 抓渲染探针（`enable_render_probe=true`）日志。

### — §4. SIMD 11 个 PS 混合回退标量（保正确）【低优先】
- 现况：Alpha/Add/Sub/Mul/Screen/Lighten/Darken/Diff/Overlay/HardLight/Exclusion 在 `tvpgl_simd_init.cpp`
  **不再注册 SIMD**（回退标量）。原因：逐字节 u16+`OrderedDemote2To` 饱和 vs 标量 32 位打包跨字节借位截断，
  结构性不等（CI 曾 16 处 mismatch）；`8ff8760` 回退后 Linux CI 全绿。
- 放回前提：用 [harness_ps.cpp](https://github.com/FiresonZ/PocketKrKr/blob/main/harness_ps.cpp) 实证位级一致的算法 = u8 混合核心+u32 打包 alpha；
  改写 Highway **u32 lane** 后放回。功能已由标量保证；非 PS 混合已对齐标量。

### P1 — §2d. 真缺"功能实现"的 Z 插件/API 清单【新增，待排队】
> 与挂名(ZCompatStub)区分：这些是**调用会崩/演出缺损的真正功能缺口**（对照 krkrz / Kirikiroid2）。
> 现状核实于 2026-09-10（grep 核心无实现）：
- **Layer alpha 特效三方法**（P1，Kirikiroid2_patch 19 款游戏高频）：`Layer.AlphaColorBlend` /
  `TranslucentColorBlend` / `LuminanceForAlpha` —— 核心 Layer 无实现，调用报 `Member does not exist`。
  方向：仿 layerExAlpha 实现（KrKr2 layerEx）并内建到 LayerIntf.cpp 的 Layer 类 / extrans 挂名处补真体。
- **Layer.AddMosaic**（P1，Patch 库 46 次最高频单项）：`extrans.cpp` 仅 `NCB_MODULE_NAME` 挂名 stub，
  **无方法体**。方向：Mosaic 马赛克绘制实现。
- **PrerenderFont(...) → .tpf 位图字体通道**（P0 关联：选项框字偏小/偏左上/裁切，见下文独立条目）：
  `PreRenderFont` 前缀未接入 `.tpf/.tpr` 预渲染位图字形；`PrerenderedFont` 类能解析 .tpf 结构但未被
  `GetBeingFont`/字体通道启用。KrKr/Kirikiroid2 的该通道正是选项文字上屏正解（K2 正常而我们错位）。
- **krmovie Present**（P0，视频 OP/过场）：ffmpeg 已解码，但"帧→场景叠加显示"仍是 stub——
  引擎能力非插件，独立条目见 §P1/P2。
- **Squirrel 插件**（P1）：`zcompat g_z_squirrel` 纯挂名，无 VM/类；部分 Z 游戏存档/系统脚本依赖。
- 优先级：Alpha 三方法 + AddMosaic（高频、方法缺失、好落地）＞ .tpf 字体通道（连动选项框字）
  ＞ Squirrel ＞ krmovie（引擎改动）。

### P1 — §2e. Yuzusoft 选项框文字偏小/偏左上/只显示上部【当前在做，等日志】
>- 现象（engine(7)，用户实测）：选项框文字比 Kirikiroid2 **小、位置偏到框左上、只显示上半约 2/3**，
>  颜色样式正常；对话/消息框文字正常。
>- 已明确矛盾：对话(`*ヘッダ`/`*システム`)与选项框(`PrerenderFont(スキップ),ＭＳ ゴシック`)**
>  最终都被 `GetBeingFont` fallback 成同一个 `Noto Sans CJK JP`**（日志 `being='Noto Sans CJK JP'`），
>  系统仅注册 `NotoSansCJK-Regular.ttc` 一个 CJK 字体 → 字体名解析造不出对话/选项的差异，
>  根因更可能在绘制区域/基线/裁剪或 `.tpf` 预渲染位图字体通道。
>- 已做：① 剥离 `PrerenderFont(...)` 前缀 + 等宽/全角优先回退（`8ec234c`）；② 绘制入口探针
>  `[TextProbe]`（destRect/x/y/face/height/AscentOfs/prerender 是否非 0，`26ba8de`）；③ 动画修复同批
>  待真机（`1aca7bf`）。
>- **扫描结论（2026-09-11，对照 krkrz / Kirikiroid2 / KrKr2-Next 全字库管线）**：
>  - 我们的 being 字体管线（FontSystem→FreeType→.tpf 解析/Find/Retrieve→TVPGetCharacter→
>    InternalDrawText）与 krkrz/K2 **逐字节同构**，非管线逻辑差异；我们相对上游仅加探针/
>    前缀剥离/等宽回退/restart 复位。
>  - 探针实证（engine(11) 前后）：选项文本 `prerender=0xb4…` **非空**——`.tpf` 已映射并在走
>    预渲染字形；对话 `*システム` 同样非空但**正常**。
>  - **机制定位（高置信）**：`.tpf` 字形按**原始字体**（MS ゴシック，ascent≈0.86×h）预烘焙，
>    定位 `data->OriginY = -pitem->OriginY + aofsy`，而 `aofsy` 取自**回退字体** ascent
>    （Noto CJK≈1.16×h，实测 height=39→aofsy=45）。基线被压低约 0.3×h →
>    字形下移、框底被裁 → 选项文字"偏左上 + 只见上半"。K2 用 DroidSansFallback（ascent≈0.9）
>    偏差小所以正常。对话字符在 `.tpf` 里 MISS → 走栅格化（同字体自洽）所以正常。
>  - 另发现差异（与尺寸无关，未改）：`tTJSNI_BaseLayer::DrawText/DrawGlyph` 我们对 color
>    多做了一次 `TVP_REVRGB`（krkrz 没有），仅影响颜色、用户实测颜色正常。
>- 待真机：用新增 `[TpfMap]`（映射的 .tpf 路径）与 `[TpfProbe]`（每 face/字符 hit-miss +
>  .tpf 内置 W/H/Origin/Inc + aofs）日志确认：① 选项 face 是否 HIT .tpf；② .tpf 内建字号是否
>  与请求 height 一致（排除映射错 .tpf）；③ 若确认命中且 Origin 与 aofsy 差 ~0.3×h → 修
>  `aofsy`（对 .tpf 字形改用其原始字体 ascent，例如按 .tpf 内建 Origin 推导，而非回退字体）。
>- 待做：① 与动画一起构建；② 依据新日志 `[TextProbe]`（destRect 高度 vs 行高；AscentOfs；prerender
>  是否命中 .tpf）与 `[FontProbe]` 度量，定位是绘制区域被裁、基线偏移还是未走位图字体；③ 若指向
>  .tpf 通道 → 按 §2d 实现。

### — §R. krkr2-tools 反编译参考资源清单（2026-09-11 盘点）【参考索引，非待办】
> 位置：宿主机本地 `/tmp/krkr2-tools`（K2 完整移植源码 + libkrkr2.so 反编译重建，**不 commit 进仓库**；
> 若环境清理需重新获取：`git clone` 对应工程 + IDA 反编译产物重建）。配套参考：
> `/tmp/krkrz`（Z 引擎）、`/tmp/Kirikiroid2`（K2 移植）、`/tmp/krkrz_dev`（Z 工具/插件）。
- **`analysis/`（24 份 libkrkr2.so 反编译分析，全 motion/渲染）**：Player_Draw_Full_RenderPath.md、
  PlayerUpdateLayers / player_updateLayers_accum.md、Player_Rendering_Architecture_libkrkr2so.md、
  EmotePlayer_Internal_Implementation.md（contains 支持圆/矩形/凸四边形、setVariable 9 类分发、
  progress 物理步进）、NodeTree_Construction.md、PSB_RL_Decompression_libkrkr2so.md、
  GPU_RenderPath_libkrkr2so.md、Window_DrawDevice_Scaling_libkrkr2so.md 等。
- **`cpp/plugins/motionplayer/`（44 文件，反编译重建的完整 motionplayer/emoteplayer）**：
  PlayerCore / PlayerRender / PlayerRenderItems / PlayerRenderTargets / PlayerDrawDispatch /
  PlayerUpdateLayers / PlayerUpdateGeometry / PlayerUpdateChildMotion / PlayerUpdateAnchor /
  PlayerUpdateParticles / PlayerTimeline / PlayerFrameProgress / PlayerMotionLoad / PlayerResource /
  PlayerLayerQuery / PlayerVariable / NodeTree / MotionNode / D3DAdaptor / D3DEmoteModule /
  SeparateLayerAdaptor / SourceCache / ResourceManager / RuntimeSupport / EmotePlayer / main.cpp。
  注意：其中的 EmotePlayer.cpp 本身也多为 stub（STUB_WARN），真实现靠 analysis 文档 + D3DEmoteModule.h。
- **`cpp/core/movie/ffmpeg/KRMoviePlayer.cpp`**：`VideoPresentOverlay::PresentPicture` 真实实现
  （pts 同步 + 按 pts 跳帧 + YUV 上屏）——krmovie Present（P0）的目标参考；K2 用 cocos2d sprite
  上屏，移植时换成我们自己的纹理路径。
- **`tools/`**：tjsdump（TJS 字节码反汇编）、ksdec（KAG 脚本反编译）、mtndump（motion 转储）、
  motionsim（motion 模拟器 + 轨迹对比，可验证 Player 与真实引擎逐帧一致）、xp3 / xp3pack。
- **目标映射（"空函数能不能用它实现"结论）**：
  - ✅ 能直接照做：**krmovie Present**（P0，当前 stub）；**motionplayer 完整管线补齐**
    （粒子 / mesh 透视 / anchor / 物理等，当前 Player.h 为简化版）；**EmotePlayer**（按 analysis 文档）。
  - ✅ 工具可复用：tjsdump / ksdec / mtndump / motionsim / xp3。
  - ❌ 参考里也没有、需自研：**AddMosaic**（layerExMosaic）、**TranslucentColorBlend / LuminanceForAlpha**
    （layerExColor）、**squirrel**、**multiimage**（todo 已注明"源码不可得"）、**kagexopt**、**kztouch**、
    **drawdeviceD3DZ**（D3D 桌面概念）。
  - ✅ 已有内建能力、挂名正确勿动：wuvorbis / wuopus / wuflac / extrans。

### — §5. KAGEX / KAG 差异兼容（kagexopt 相关）
- 调研并规划对依赖较新 KAG/KAGEX 行为或未登官方插件的游戏做兼容（需求待明确）。

### — §6. multiimage（多图/psd 相关）支持
- 规划 `multiimage` 能力；范围与用例待明确。

### — §8. 去除桌面端残余文件【工程清理，未来执行】
- 目标：专注移动端（iOS/Android，macOS 为 Apple 开发目标）。
- 约定：`win32/` 是跨平台共享实现不能删（conventions §1）；macOS runner 保留。
- 待清理：`platforms/windows`、`platforms/linux/main.cpp`、纯 Win32 窗体
  `cpp/core/environ/win32/{MainFormUnit,WindowFormUnit,ConfigFormUnit,VersionFormUnit,…}`、
  `plugins/layerex_draw/windows/*`、`bridge/flutter_engine_bridge/{linux,windows}/…`。
- 做法：先 grep 确认无 CMake/target 引用再删，删后 Linux 预设验证一次。

### — §9. 安卓构建链防崩备忘录（2026-09 对账）
> 防"从上游合并/照搬"再次弄崩 arm64 构建。对账对象：`reAAAq/KrKr2-Next`。
- 我们的 Android 管线基本自研（上游 CMakePresets 无 Android 预设）→ 别全量合回上游 CMake/triplet/vcpkg。
- 别合并上游 `vcpkg.json`（bullet3/breakpad/libogg/opus 等差异；我们独有 oboe）。
- engine_api 用**普通链接** `krkr2core+krkr2plugin`，**别加 `--whole-archive`**（重复符号）。
- JNI 契约自洽：包 `dev.krkr2.flutter_engine_bridge` ↔ `Java_dev_krkr2_flutter_1engine_1bridge_...`
  （改包名必须同步改 JNI 方法名）。
- oboe 用 `find_library` 链接，不用 `find_package(oboe CONFIG)`。
- arm64 triplet ABI 修复是我们独有（`-DANDROID_ABI=arm64-v8a`/`CMAKE_SYSTEM_PROCESSOR=aarch64`），别覆盖。

### — §11. Android「添加文件/压缩包」死代码 bug【暂缓：功能未使用】
- `home_page.dart _addGameArchive()` 走 `pickFile`，但 Android/iOS 平台通道均无实现 → 必抛
  `MissingPluginException`。Android 目录访问用 SAF `content://` URI 得持久授权（权限模型差异，非"不能访问"）。
- 若启用再补：Android 原生 `pickFile`（`ACTION_OPEN_DOCUMENT`+SAF+copy 私有目录）或 `FilePicker.pickFiles`。

## 已解决（极简，供回溯）

- **runtime-restart 退出卡死 + 二次打开黑屏**：日志闭包未在引擎销毁前释放（`4221543`）+ 音效线程析构
  `Terminate()` 在 `WaitFor()` 后致 join 死锁（`081a9c1`）+ EGL context 重建后复用渲染器 GL 状态失效且
  `FireRendererRecreated` 从未调用（`5fd30da`）。真机复验不杀进程二次打开正常。
- **Android 启动链路**：SDL Java 层缺失（闪退）、JNI 平台层补齐（`a2d8d75`）、主界面转圈
  （`63b8537`）、日志写 `PocketKrKrLogs`（`c312272`）。
- **非标准目录（散装 xp3）启动**：`TVPAutoMountProjectXP3Archives` 支持目录内 xp3 自动挂载。
- **切换不同游戏 auto-path 清理**：见上 §3c。