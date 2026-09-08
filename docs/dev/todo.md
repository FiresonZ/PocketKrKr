# 待办 / 已知问题

> 本文件维护当前未完成或待验证的工程事项，供 AI Agent 与开发者交接。
> 每完成一项，把该条目移到「已完成」或直接删除，并在 [README.md](README.md) 的
> 目录索引中保持本文件引用。
> 完成某项并验证后，请同步更新根目录 [AGENTS.md](https://github.com/FiresonZ/PocketKrKr/blob/main/AGENTS.md) 的「当前状态」。

## ✅ 已解决（保留经验摘记）

### Android 启动即闪退（缺 SDL Java 层）— 已修复并真机复验
- 根因：`libengine_api.so` 静态链入 SDL2，其安卓原生 `JNI_OnLoad` 里 `FindClass("org/libsdl/app/SDLActivity")`
  失败留下 pending exception → ART `abort()`。
- 修复：补 `apps/flutter_app/android/app/src/main/java/org/libsdl/app/` 9 个 Java 源
  （SDL/SDLActivity/…，版本常量 2.32.10 与 vcpkg sdl2 一致）；engine_api 链接加
  `--allow-multiple-definition` 让我们的 `JNI_OnLoad` 生效。
- 状态：真机不再闪退、主界面正常。

### 对齐上游 Android JNI 平台层 — 已通过并可跑
- 完整嵌入 `KrkrJniHelper`/`AndroidUtils`/`engine_api_android_jni.cpp`/`KR2Activity` 等。
- 本次补齐此前缺失的 Android 平台符号：`TVPGetMemoryInfo`/`TVPRelinquishCPU`/`TVP_utime`
  （`AndroidUtils.cpp` 用 bionic 实现）+ `krkr_GetApplicationContext`（引用声明改 `extern "C"`
  对齐定义），提交 `a2d8d75`。
- 状态：CI 编译通过、APK 可跑，进入真机日志筛查游戏兼容性阶段。

### Android 主界面永久转圈 — 已修复
- 根因：静态排查无死循环，怀疑某 await 在平台通道静默挂起。
- 修复：`_loadGames()` 包 try-catch-finally 强制 `_loading=false`（提交 `63b8537`）。
- 状态：真机不再永久转圈，可进入游戏。

## 进行中 / 待验证

### 2a. motionplayer 缺 `Motion.D3DAdaptor` — 首屏后点击退出的兼容根因
- 旧现象（千恋万花高压 真机日志）：开场播放 yuzulogo logo 后，`custom.ks:89`
  访问 `Motion.D3DAdaptor`（`(property getter) motionD3DAdaptor`），早期报
  `Member "D3DAdaptor" does not exist` → 脚本致命错误。
- 根因：Z 的 D3D 版 motionplayer（motionplayer_nod3d/drawdeviceD3DZ，无开源）专有成员，
  我们 motionplayer 未实现。
- **新增现象（2026-09-08 00:32 日志）**：给 `D3DAdaptor` 返回 `undefined` 后，`Member
  does not exist` 消失，但 krkrz 的 `affinesourcemotion.tjs drawAffine` 把
  `Motion.D3DAdaptor` **当作 Object 传参/赋值**（`new MotionXX(D3DAdaptor, …)`）→ 报
  `Cannot convert the variable type (() to Object)`，`custom.ks:89` 致命 → logo 后崩溃+卡死。
- 修复（本轮，`cpp/plugins/motionplayer/main.cpp`）：`getD3DAdaptor` 改为返回 **stub 字典
  对象**（与 `Motion.enableD3D` 的 `getEnableD3D` 同一模式）而非 undefined，避免
  `() → Object` 转换错误。后续若脚本访问 stub 的具体成员再逐项补。
- **再次复验（2026-09-08 01:51 真机日志）**：字典 stub 仍崩溃——`affinesourcemotion.tjs`
  `drawAffine` 实际执行 **`new Motion.D3DAdaptor(…, w/2, h/2)`**（字节码
  `45 new %1, %9(%-1, %2, %3, %4, %6)`，%9 = D3DAdaptor 值），字典对象不是函数/类 →
  `Not a function or invalid method/property type` 致命错误 → 主循环终止 → 退出。
- **二修（本轮）**：`getD3DAdaptor` 改为返回**可 new 的空类**（`new tTJSNativeClass(TJS_W("D3DAdaptor"))`），
  让 `new Motion.D3DAdaptor(...)` 成功创建实例；后续若脚本访问实例成员再逐项补 stub。
- 待验证：真机复验千恋万花首屏 logo 后不再崩溃；若还崩，看是否要继续补 D3DAdaptor 实例
  成员（如 drawAffine 里对实例的 getter/方法调用）的 stub。

### 2. Z（krkrz/KIRIKIRI Z）插件兼容 — 移动端黑屏根因【高优】
> 移植清单/参考源：见 [krkrz-compat.md](krkrz-compat.md)（已确认各插件源码来源，含
> krkrz/krkr2/Kirikiroid2 本地参考副本）。
- **现象**（真机日志 `sabbat_kr`/魔女的夜宴，目录版）：游戏正常启动到
  `startup→Initialize→first.ks→title.ks`，XP3 全挂载，脚本/图层照常（事件 2 万对象、
  9k ICC、内存 230MB+），但合成源纹理始终 `(0,0,0,255)` 纯黑、draw 计数卡死不再增长。
- **已排除**：黑屏探针 `VideoOverlay: total=0 active=0 playing=0` → **不是视频/krmovie**。
  源纹理 = 主 LayerManager 的 `DrawBuffer`（初始即 `0xFF000000`），等于**从未被合成过**
  → Z 游戏主画面走的是 Z 那条路径（D3D drawdevice / Z 层），我们没接入。
- **关联插件缺口**（real 游戏 `plugin/` 里这些全部 `Failed`，引擎无任何实现/stub）：
  `drawdeviceD3D.dll`、`drawdeviceD3DZ.dll`、`kztouch.dll`、`k2compat.dll`、
  `kagexopt.dll`、`multiimage.dll`、`squirrel.dll`、`PackinOne.dll`。
- **与主线目标的关联**：这正是 AGENTS 里"补全解析引擎、插件，目标与 Z 闭源版兼容持平"
  的核心项。
- **下一步**：
  1. 用 **Windows**（我们 CMakePresets 有 Windows MinGW 预设）跑同一游戏二分：
     Windows 也黑 → 引擎/Z 层问题；Windows 正常 → 才转 iOS 纹理链路。
  2. 对照 **Kirikiroid2** `src/core/visual/RenderManager_ogl.cpp` + `BasicDrawDevice.cpp`
     与我们的 `cpp/core/visual/RenderManager.*`，定位 Z 主层 `DrawBuffer` 未合成原因。
  3. 在 `ncbAutoRegister` 内部编表里给 Z 插件**挂名**（先能 link 成功），再实现功能。
- 参考：引擎主合成链 `BasicDrawDevice::Show→GetDrawBuffer→UpdateDrawBuffer`、
  `CompleteForWindow→InternalComplete2(_GPU)`、`__captureBaseDrawDevice` 包装挂点
  （见 `cpp/plugins/drawDeviceD2DCompat.cpp`）。

### 2. krmovie Present 未实现（视频解码 → 画面合成）【一般，非本次黑屏根因】
- 现状：ffmpeg 解码链路完整（`cpp/core/movie/ffmpeg/`），但
  `VideoPresentOverlay::PresentPicture` 及 overlay 合成到场景/纹理的路径仍是 stub
  （只打一条 warn，不渲染）。
- 备注：**已证实不是魔女的夜宴黑屏的原因**（`VideoOverlay total=0`）；保留为通用待办，
  供真正的视频 OP/影片游戏使用。
- 待办：评估并把解码帧 RGBA 合成到引擎场景（Mixer/Layer）或 Flutter 纹理。
- 参考实现：Kirikiroid2 `src/core/movie/krmovie.cpp` + `ffmpeg/KRMovie*.{h,cpp}`、krkrz `movie/win32/krmovie.cpp`（见 [krkrz-compat.md](krkrz-compat.md)）。
- 参考：`cpp/core/visual/impl/VideoOvlImpl.cpp` 的 `EC_UPDATE` 处理与
  `iTVPVideoOverlay::PresentVideoImage` / `GetFrontBuffer` 契约。

### 3. runtime-restart 不支持（退出后无法直接开另一个游戏）—— ✅ 已全部解决
> **✅ 已全部解决（2026-09-08，真机复验）**：
> - **退出卡死**根治：①`Application::OnExit` 在销毁脚本引擎前 `TVPClearLoggingHandlers()`
>   （`DebugIntf.cpp`，提前释放日志闭包，避免引擎死后 finalizer 命中失效 TJS 态）
>   → 提交 `4221543`；② 真正卡死点是音效线程析构：`~tTVPWaveSoundBufferThread`
>   原在 `WaitFor()`（join）后才 `Terminate()`，`Execute()` 是 `while(!GetTerminated())`
>   无限循环 → join 永久阻塞（进程退出被 OS 回收掩盖，进程内 teardown 必现）。
>   已改为先 `Terminate()`+`Event.Set()` 再 `WaitFor()` → 提交 `081a9c1`。
> - **二次打开黑屏/乱屏**根治（首开正常、仅 reset 后乱、杀进程重开正常）：EGL context
>   在重启时被 `EngineBootstrap::Shutdown` 销毁重建，但复用渲染器单例的 shader/共享
>   `_FBO`/模板 FBO 都是旧 context 的失效 id；`FireRendererRecreated` 全仓零调用（死代码）。
>   修复：`EngineBootstrap::InitializeGraphics` 在 context（重）建后真正调用
>   `krkr::gl::FireRendererRecreated()`，`RenderManager_ogl.cpp` 回调内除重建 shader 外
>   还重建 `_FBO`/`_stencil_FBO` 并复位 FBO 状态 → 提交 `5fd30da`。
> - 状态：两游戏不杀进程二次打开渲染正常，无黑屏/乱屏；杀进程重开亦正常。
> - 下文为排查/移植历史，保留作经验摘记。
>
> **📍历史排查状态（2026-09-08）**：teardown 已按上游 PR#12 完整对照（engine_api.cpp：
> `OnExit→TVPSystemUninit→删 scene/loop→13 项 Reset→Bootstrap→g_runtime_started_once=false`）。
> 装真机复验（01:51）两游戏（krkr2 IINCHO-Re.co 与 Z 千恋万花）退出均**永久卡死在
> `TVPSystemUninit begin` 之后的 `TVPCauseAtExit()` 内某个 at-exit handler**；项 C 端
> `g_runtime_started_once` 不复位的根因已修。已给 `TVPCauseAtExit()` 循环加逐 handler 打点
> （`SysInitIntf.cpp`，每 handler `begin/end` + flush）。
> **最新一轮（09:35 安卓日志）定位**：卡在 `TVPCauseAtExit: handler[2] pri=10 begin`
> （有 begin 无 end）——即 **`PREPARE(10)` 优先级的一个 at-exit handler**。Android 上
> `pri=10` 共 4 个：`TVPDestroyEventQueue`、`TVPDestroyContinuousHandlerVector`、
> `TVPShutdownVideoOverlay`、`TVPUnmapAllPrerenderedFonts`；`std::sort` 不稳定，handler[2]
> 具体是哪一个无法仅凭 index 判定。三个是纯 Release 循环（事件/连续 handler/字体），
> 唯一与线程/媒体交互的是 `TVPShutdownVideoOverlay`（疑点最大：teardown 跑 Flutter UI 线程，
> 视频/渲染子线程若仍在等主循环推进可死锁）。
> **本轮动作**：`TVPCauseAtExit` 打点改用 `dladdr` 附带 handler **符号名**；但 release APK
> stripped，真机 `sym=?` 解析不出（10:16 日志）。**改在各 PREPARE(10) handler 内部打显式标记**
> （`at-exit PREPARE[1/4] DestroyEventQueue / [2/4] DestroyContinuousHandlerVector /
> [3/4] ShutdownVideoOverlay / [4/4] UnmapAllPrerenderedFonts`，与 stripped 无关）。
> **10:56 日志已基本排除到单个 handler**：handler[0]=DestEventQueue、handler[1]=
> DestroyContinuousHandlerVector 均完成；handler[2] 卡死，且**既无 video 也无 font 的标记**
> （它们是第一句就打标记）→ 排除二者。pri=10 还有第 5 个未被标记的
> **`TVPDestroyLoggingHandlerVector`（DebugIntf.cpp，释放日志 handler 闭包，归零跑 TJS
> finalizer）**，判定它就是卡死的 handler[2]。
> **本轮**：给 `TVPDestroyLoggingHandlerVector` 加标记 + 逐 closure 打点
> （`at-exit PREPARE LoggingHandler begin(count=N)` / `release closure[k] begin/end`），下次
> 真机退出即可确认并定位到具体闭包/finalizer。
> 盲改任一候选有破坏二次重启的风险，故等真机日志对症下药。
>
> **❗真机 2026-09-08 00:31 复验：退出即卡死（非干净的 restart），reset 未根治**。日志
> `pocketkrkr_engine(1).log` 里 IINCHO-Re.co（krkr2）到 title 屏后退出：第一个游戏的最后一行
> 日志停在 `00:31:46 journal title.ks:@s`，其后**完全没有 engine_destroy / TVPSystemUninit 任何
> 日志**，也**没有 `runtime teardown complete`** → `engine_destroy` 在 `OnDeactivate` /
> `TVPSystemUninit` / `scene·loop delete` / `Bootstrap::Shutdown` 之一处**静默挂死**（Flutter
> UI 线程 await engineDestroy 阻塞 → 整机无响应）。
> **本轮动作**：给 `engine_destroy` 逐级打点（进入/OnDeactivate/TVPSystemUninit/MainScene/
> EngineLoop/Bootstrap/Reset 链/完成），下次真机退出即可据最后的点定位卡在哪一步。
> 注意：engine_api 有 `#if ENGINE_API_USE_KRKR2_RUNTIME` 双实现，Android 联动 krkr2core/
> plugin 走的是**带完整 teardown 的那份**（engine_api.cpp:757 附近），打点也加在这份。
>
> **最新决策（2026-09-08）：按上游 PR#12 完全对照做「安全退出」**（`engine_api.cpp` engine_destroy）。
> - 关键修正：不再跳过 `TVPSystemUninit`，而是**先 `Application->OnExit()`**（内部
>   `TVPUninitScriptEngine` + delete `TVPSystemControl`，让脚本引擎在安全上下文退出），
>   再调 `TVPSystemUninit()`（其内部 `TVPUninitScriptEngine` 因守卫标志变 no-op）。
>   裸调 `TVPSystemUninit` 会在 TJS 栈内销毁脚本引擎 = krkrz host 模式自声明的
>   undefined behavior（hang），是此前退出即静默卡死的根因（参考 `SysInitImpl.cpp`
>   `TVPTerminateSync` 注释）。打点顺序已按上游重排：OnExit→TVPSystemUninit→
>   scene·EngineLoop→Reset 链→Bootstrap::Shutdown→`g_runtime_started_once=false`。
> - ✅ **已全部对照上游移植（本轮）**：`TVPResetWindowListForRestart`（WindowIntf）、
>   `TVPResetLayerBitmapImplForRestart`（LayerBitmapImpl）、`TVPResetFontImplForRestart`
>   （FontImpl，并把 `TVPFontNamesInit` 提升为文件作用域）、`TVPResetTransIntfForRestart`
>   （TransIntf）、`tTVPBitmapBitsAlloc::ResetForRestart`（BitmapBitsAlloc）、
>   `TVPResetPluginSystemForRestart` + `TVPUnregisterInternalPluginsForRestart`
>   （PluginImpl/ncbind，含 `ncbAutoRegister::ResetModuleStateForRestart`）。
>   本地适配：本地无 `s_ProxyStorageMedia`（`TVPRegisterProxyFsStub` 不保存 media
>   句柄），故 `TVPResetPluginFallbackStubsForRestart` 只复位 `s_ProxyStorageMap`。
> - ✅ **打点保留，engine_destroy 复位链严格对照上游顺序**
>   （Unregister→OnExit→TVPSystemUninit→scene·EngineLoop→13 项 Reset→Bootstrap→started_once）。
> - ⏳ **待真机**：编译通过 + 退出不再静默卡死（走到 `runtime teardown complete`）+
>   不杀进程能再开另一款游戏。
>
> **❗二次复验（2026-09-08 01:51 真机日志 `pocketkrkr_engine(3).log`）：仍卡死，且已精确定位**。
> - IINCHO-Re.co（krkr2）与千恋万花（Z）两个游戏退出**均永久卡死在 `engine_destroy` 的
>   `TVPSystemUninit begin` 之后**（第一个游戏用户等 13.6 秒无返回后手动杀进程；第二个游戏
>   日志止于 `TVPSystemUninit begin` 不再前进）。
> - 已排除：`TVPUninitTVPGL`（= `TVPDestroyTable` 空操作）、`TVPUninitScriptEngine`（OnExit
>   已调过，守卫标志 → no-op）→ **卡死点在 `TVPCauseAtExit()` 内部某个 at-exit handler**。
> - **本轮动作**：给 `TVPCauseAtExit()` 循环加逐 handler 打点（`handler[i] pri=… begin/end`，
>   `SysInitIntf.cpp`），下次真机退出据最后一条日志直接定位卡在哪个 at-exit handler
>   （可疑候选：线程 join 类 `TVPWatchThreadUninit`/`TVPTimerThreadUninit`/`ContinuousHandlerCallLimit`，
>   或 GL 相关 `TVPReleaseTexture2D` glFlush——engine_destroy 在 Flutter UI 线程，可能无 EGL 上下文）。
> - ⏳ 待真机：据新打点定位具体 handler 后修。
> **C 端根因已定位**（这篇日志复现确认）：`engine_api.cpp` 的 `g_runtime_started_once`
> 一旦置 true 从不复位；`engine_destroy` 只清 `g_runtime_active/owner`，漏了它 → 第二次
> `engine_open_game` 必命中 `runtime restart is not supported yet`。Dart shutdown 是前置，
> 绕不过这个 C 端标志。
>
> **进度**：engine_api.cpp `engine_destroy` 已按 PR#12 加入热重启 teardown
> （TVPSystemUninit + 销毁 MainScene/EngineLoop 单例 + Bootstrap::Shutdown +
> `g_runtime_started_once=false`）。
> **Reset 函数移植进度**（PR#12 C++ 侧，保证重初始化干净）：
> **🟡 已编译通过（Android + iOS），待真机验证**：三批 Reset 函数已全部接入
> `engine_destroy`，两平台编译均绿；下一步真机复验"不杀进程退出→再开另一款游戏"。
> - ✅ 已提交并接入 `engine_destroy`（`05d1254` + `682d87e`，codex 分支）：
>   `TVPResetScriptEngineForRestart`（ScriptMgnIntf）、
>   `TVPResetRuntimeForRestart`（SysInitIntf，含 TVPProjectDir/DataPath 清空）、
>   `TVPResetSysInitImplForRestart`（SysInitImpl，含 TVPSystemControlAlive 复位）、
>   `TVPResetApplicationForRestart`（Application：删图像线程/清事件队列/复位标志）、
>   `TVPResetStorageImplForRestart`（StorageImpl：TVPGetAppPath 缓存提文件作用域再清）、
>   `TVPResetExtensionClassInstallStateForRestart`（Extension：复位待装类标志）。
> - ⏳ 剩余待移植：Plugin/ncbind 类 `TVPResetPluginSystemForRestart`/
>   `TVPUnregisterInternalPluginsForRestart`/`ncbAutoRegister::ResetModuleStateForRestart`
>   （仅清 `TVPRegisteredPlugins` 集合放行重新 Regist，勿清 `_internal_plugins`，且
>   二次 init 的 AllRegist 会重复 append 注册器、需随 LoadAllModules 守卫校验）；本地
>   沙箱无 vcpkg 不能直接编译，暂缓待真机确认前三批是否已根治再评估。
> - visual 级（RenderManager/Font/Trans/Window/Bitmap/LayerBitmap/OpenGL）为第二批。
> - ✅ visual 级部分（`TVPResetVisualForRestart` 等价，接入 engine_destroy）：
>   - 修 `TVPCauseAtExit` 空指针（二次 destroy 时 `TVPAtExitInfos` 已删，tTVPAtExit
>     static 进程只注册一次）——`SysInitIntf.cpp`。
>   - `TVPUninitializeFontRasterizers` 复位 `TVPFontRasterizersInit` 标志，使二次
>     open_game 重建 FontSystem（否则二次绘制文字崩）——`LayerBitmapImpl.cpp`。
>   - `_RenderManager` 由函数局部 static 提升文件作用域，新增
>     `TVPResetRenderManagerForRestart()` 清主单例+释放已建渲染器——`RenderManager.{h,cpp}`。
>   - `engine_destroy` 复位链末尾调 `TVPClearGraphicCache()` + `TVPResetRenderManagerForRestart()`。
>   - 未处理（评估低风险/需真机验证）：Trans provider、位图分配器、Texture2D 回收队列、
>     OpenGL 扩展探测 static、`TVPGetSoftwareRenderManager` 软件单例。
>   - 参考：Kirikiroid2 无 Reset*ForRestart 批函数，靠 TVPSystemUninit 全量拆；我们的
>     visual Reset 是自研，针对 tTVPAtExit 一次性注册的二次不重跑问题。
- 现象：首次开游戏正常；不杀进程、退出后再开另一款游戏报
  `Engine Error engine_open_game_async failed: result=-3, error=runtime restart is not supported yet`。
- 已做：Dart 侧 `_exitGame` 现在先 `engineDestroy()` 等销毁完成再 `pop`。
- **上游参考（vcdlk PR#12「make runtime restartable after engine_destroy」,
  reAAAq/KrKr2-Next，2026-06-16）**：此 PR 正是修"杀后台/无法重启"的根因。
  病根是我们本地 [game_page.dart](https://github.com/FiresonZ/PocketKrKr/blob/main/apps/flutter_app/lib/pages/game_page.dart)：
  `_exitGame()` 直接 `pop` 不完整释放；Retry 把 `engineDestroy()` 用 `unawaited()` 丢出、
  立刻 `engineCreate()` → 引擎未销毁完就 recreate，全局状态残留 → 下次起不来，只能杀进程。
- PR#12 的改法（纯 Dart）：
  1. `_shutdownEngine()` 用 `_shutdownRequested` + `_shutdownFuture` 去重，严格 await 顺序：
     `surface.release() → engineDestroy() → finalizePlaySession → restoreOrientation`。
  2. `_autoStart()`/`_exitGame`/`_retryAutoStart` 全程检查 `_shutdownRequested`，
     销毁未完就不再重新 create。
  3. Retry 改走 `_retryAutoStart()`（完整 shutdown 后再重建 bridge + autoStart）。
- 待办（真机阶段）：已移植 PR#12 中 `game_page.dart` 的 shutdown 重构（只搬这段，不整体
  cherry-pick——该 PR 还夹带 launch_args/归档/.gitignore/CMake `TVP_SOURCE_ROOT` 等噪声）。
  移植后真机验证是否根治"杀后台/无法再开游戏"。若仍失败，再评估引擎热重启或新进程形态。

### 3b. 小 bug：快速 skip 时消息框（聊天框）偶发变黑色色块【待查】
- 现象（2026-09-08 真机）：快速 skip 模式下，本应透明的消息框有时整块变成**黑色色块**
  （消息框背景层/遮罩变成不透明黑）。
- 初步方向（未验证）：疑似消息框透明层在 skip 抖动的**快速帧间**经某混合/预乘路径偶发
  写成不透明黑（alpha 通道被置 0xFF 的黑色），或边框/遮罩层未随 skip 正确刷新；
  与 runtime-restart 无关（单次运行即现）。待抓 skip 瞬间 + 渲染探针日志复核混合/SIMD 路径。

### 4. SIMD 公式逐模式修到位级一致（保正确回归）
> **❗更正（2026-09-08，CI 实证）**：下方 P2–P6 的「已放回 0 mismatch」对 **11 个 PS 混合
> 模式不成立**。这些模式的 SIMD 用**逐字节 u16 + `OrderedDemote2To`**，而标量
> `ps_alpha_blend_func` 用 **32 位打包 R/B 算术（跨字节借位，`%2^32`）**：
>   - `OrderedDemote2To` 对 16 位中间值**饱和**到 0xFF，标量是 `&0xFF` **截断**；
>   - 标量打包算术里 B 通道下溢会**借位**影响 R 通道，逐字节通道无法复现。
> 二者**结构性不等**。Linux CI `tvpgl_simd_compare` 实测 **16 处 mismatch**
> （如 `PsAlphaBlend scalar=005E8946 simd=00FF8946`、`PsMulBlend scalar=00752B57 simd=00FFFFFF`）。
>
> **对策（已提交 8ff8760）**：把 Alpha/Add/Sub/Mul/Screen/Lighten/Darken/Diff/Overlay/
> HardLight/Exclusion 共 11 个 PS 模式在 `tvpgl_simd_init.cpp` **不再注册**（回退生产标量），
> 与 conventions §9「SIMD≠标量→回退标量保正确」一致。修复后 Linux CI `tvpgl_simd_compare`
> **全绿**。
>
> 待办（放回前提）：已用 [harness_ps.cpp](https://github.com/FiresonZ/PocketKrKr/blob/main/harness_ps.cpp) 实证**唯一能位级一致**的算法
> = **u8 混合核心 + u32 打包 alpha**（每像素 32 位打包复现标量跨字节借位，再 `&0xFF` 截断），
> 11 模式 × 4 变体 × 2M 随机矢量 0 mismatch。需把它改写成 Highway **u32 lane** 后再放回注册。
> 功能正确性已由标量保证；性能上 PS 混合暂为标量（VN 中少用，可接受）。

- 背景：`tests/tvpgl_simd_compare` 用于标量 vs SIMD 逐像素比对；早期证实
  PS 全系混合 / SubBlend_o / ScreenBlend 等 SIMD ≠ 标量。<以下保留各模式修复历史/经验摘记>
- **P2 ✅（PsApplyAlpha 舍入序已修）**：标量 `TVPPS_ALPHABLEND`（tvpps.inc）实际是
  `result = ((s - d) * a >> 8) + d`；旧 SIMD 用了 `(s*a>>8) + (d*(255-a)>>8)`，30M
  随机矢量 29.85M 不一致。已改为 `((s-d)*a >> 8) + d`（u16 包减/包乘，`OrderedDemote2To`
  只留低字节故无碍），`out/p2_check/check.c` 验证 30M 全一致（0 mismatch）。修在
  `tvpgl_simd_ps_blend.cpp` 与 `tvpgl_simd_ps_blend2.cpp` 的 `PsApplyAlpha`。
  → evaluation：Alpha/Add/Sub/Mul/Lighten/Darken/Diff/Exclusion（P 系）+ Overlay/HardLight
  的 alpha 应用段已校正。
- **P3 ✅（TVPSubBlend_o）**：标量 `TVPSubBlend_o_c` 只按 opa 缩放字节0-2 并把 alpha
  强制为 0xFF，饱和减后 alpha 保留 dst 原值；SIMD 原对 alpha 也做缩放 => 偏离。已在
  `SubBlend_o_HWY` 加 alpha_mask 保留 dst alpha（`tvpgl_simd_arithmetic_blend.cpp`），
  `out/p34_check/check.c` 30M 全一致；`tvpgl_simd_init.cpp` 已放回。
- **P4 ✅（TVPScreenBlend base）**：标量 `TVPScreenBlend_c` 的 packed 乘积只写字节0-2，
  alpha 字节恒为 0xFF；SIMD base 原来对所有 4 字节做 screen。已在 `ScreenBlend_HWY`
  OR 上 alpha_ff=0xFF，`out/p34_check/check.c` 30M 全一致；`tvpgl_simd_init.cpp` 已放回。
- **P5 ✅（PsOverlay/PsHardLight core）**：标量表是 `unsigned char`（精确 `/255`），
  SIMD 原用 `>>7`(≈/128) 且 NORM/_o 不写 alpha。已把 `OverlayCore`/`HardLightCore`
  改为精确 `floor(2*s*d/255)`（p=s*d, k=p>>7, t=k+2*(p&127), m=k+(t>=255)+(t>=510)，
  demote 前 `&0xFF` 复现 uchar 截断），并在 `MAKE_PS_4V` 的 NORM/_o 加 rgb_mask 置
  alpha=0（HDA 走 ApplyHDA 保留 dst alpha）。`out/p5_check/check2.c` 20M 全一致。
  `tvpgl_simd_init.cpp` 已放回 Overlay/HardLight。
- **P6 ✅（剩余 9 个 PS 模式全部核对放回）**：Alpha/Add/Sub/Mul/Screen/Lighten/Darken/
  Diff/Exclusion 的 core（溢出/alpha 分支）逐一与标量 `tvpgl.cpp` 宏逐位核对，已全部
  `REGISTER_PS_BLEND_4V` 放回（`tvpgl_simd_init.cpp`）。
  - 验证：`out/simd9_check/check.c`（自包含，标量 packed 复刻 vs per-channel u16-wrap
    复刻，9 模式 × 4 变体 × 30M 随机像素）**0 mismatch**；Exclusion 定向借位 case 亦一致。
  - 发现并修复一个真实 bug：`PsScreenBlend_o_HWY` 缺 `rgb_mask`（`_o` 的 alpha 字节
    未被清零，而标量 `_o` 写 alpha=0）——已补（`tvpgl_simd_ps_blend.cpp`）。
  - 关键结论：标量 Screen 的 alpha 段是 `((s-sd)*a>>8)+d`（**非**标准 alpha blend
    `((blended-d)*a>>8)+d`），SIMD 实现已按此对齐；`sd<s` 恒成立故无跨通道借位。
- ✅ 全部 16 种 PS 模式（含表驱动 5 种保持标量）已定案：8 个已核对 + Overlay/HardLight/
  Exclusion 已放回，SoftLight/ColorDodge/ColorBurn/ColorDodge5/Diff5 因查表无 SIMD
  收益保留标量（注释内）。待 CI（`tvpgl_simd_compare`）真机确认无回归。

### 5. KAGEX / KAG 差异兼容（kagexopt 相关）
- 待办：调研并规划对依赖较新 KAG/KAGEX 行为或未登官方插件的游戏做兼容（确切需求待明确）。

### 6. multiimage（多图/psd 相关）支持
- 待办：规划 `multiimage`（多图像/图层处理）相关能力；确切范围与用例待明确后拆解。

### 7. 非标准目录结构（散装 xp3 启动定位）— ✅ 已成功
- 现状：已支持标准 `data.xp3`/同目录 xp3 自动挂载（`TVPAutoMountProjectXP3Archives`）。
- 验证：形如 `D:\...\委員界の異端者體驗版` 这种目录内散装文件（非标准
  gameexe.dat/data.xp3 布局）的目录已能识别并启动，本次无需再处理。

### 8. 去除桌面端残余文件【工程清理，未来执行】
- **目标**：本项目专注移动端（iOS/Android，macOS 为 Apple 开发目标）。逐步清理从上
  游继承/回填的桌面端残留，避免读者/AI 误以为支持桌面。
- **约定**：`win32/` 目录是**跨平台共享实现**（音频/线程/系统控制），**不能删**
  （conventions §1）；macOS runner 保留作 Apple 开发。
- **待清理方向**（对照上游 `reAAAq/KrKr2-Next` 我们有而它独有的 180 文件）：
  1. 桌面应用 runner：`platforms/windows`、`platforms/linux/main.cpp`（若保留会暗示桌面支持）。
  2. 纯 Windows UI 窗体实现：`cpp/core/environ/win32/{MainFormUnit,WindowFormUnit,ConfigFormUnit,
     VersionFormUnit,TouchPoint,MouseCursor,TouchPoint,ImeControl}` 等与 UI 框架类型无关的窗体；
     保留却属于跨平台共享的（如 `CompatibleNativeFuncs`/`WindowsUtil` 中供其它平台复用的部分）先核对再动。
  3. `plugins/layerex_draw/windows/*`（桌面渲染后端）。
  4. `bridge/flutter_engine_bridge/{linux,windows}/…` 桌面平台插件与 `example/*`。
- **做法**：先确认没有 CMake/target 引用（grep），再删除；删后本地 `cmake --preset` Linux 验证一次。
  这是"以后"低优先清理，不阻塞当前 iOS/Android 发布。

### 9. 安卓构建链 —— 与上游的差异防崩备忘录（2026-09 对账）
> 防止"从上游合并/照搬"再次把安卓构建弄崩。对账对象：`reAAAq/KrKr2-Next`（main/master）。
- **结论：我们的安卓管线基本自研**。上游 `CMakePresets.json` **没有 Android 预设**（走它自己的
  `cmake/vcpkg_android.cmake` 交叉）；我们的 CMakePresets Android 预设 + JNI 胶水 + 壳层是自建。
  所以**不要把上游 CMake/triplet/vcpkg 全量合回来**，会破坏已修好的 arm64 链路。
- **别从上游合并 `vcpkg.json`**：
  - 上游含而我们已删/不同的：`bullet3`（提速已删）、`breakpad`/`libogg`/`opus`(android, 崩溃上报/音频)、
    `dirent`(windows)、`libgdiplus` 平台范围、platform 表达式写法不同。
  - 我们独有的：`oboe`（android 低延迟音频，见下）。
  - 若要功能（崩溃上报等）只按需补单项，不要整体替换。
- **engine_api 链接**：上游 Android 用**普通链接** `krkr2core+krkr2plugin`（**无 `--whole-archive`**）。
  我们已对齐；**别再加 whole-archive**（会把 psbfile/motionplayer 对象再拉一份 → ld.lld 重复符号）。
- **JNI 契约必须自洽**：我们包 `dev.krkr2.flutter_engine_bridge` ↔
  `Java_dev_krkr2_flutter_1engine_1bridge_FlutterEngineBridgePlugin_native{SetSurface,DetachSurface}`
  已核对自洽（上游是 `org.github.krkr2`，各自自洽）。**改包名必须同步改 JNI 方法名**，否则运行时 `UnsatisfiedLinkError`。
- **oboe**：上游 `WaveMixer.cpp` 也 `#include "oboe/Oboe.h"` 但上游 `vcpkg.json` 没有 oboe
  （上游 Android 未必能编到那步）。我们已加 oboe 依赖 + `find_library` 链接（vcpkg 1.8.0 端口
  只给 pkg-config、不给 CMake config，不能用 `find_package(oboe CONFIG)`）。**别因"上游没加"就移除**。
- **arm64 triplet ABI 修复是我们独有**（`VCPKG_CMAKE_CONFIGURE_OPTIONS -DANDROID_ABI=arm64-v8a`、
  `CMAKE_ANDROID_ARCH_ABI`/`CMAKE_SYSTEM_PROCESSOR=aarch64`），上游 triplet 是原版，别覆盖。

### 11. Android「添加文件/压缩包」死代码 bug + 目录访问平台差异 【暂缓：功能未使用】
> 2026-09 结论：「添加文件/压缩包」入口当前无 UI 使用场景，**暂不需要验证/修复**。
- 问题 1（死代码）：`home_page.dart _addGameArchive()` Android 分支调用
  `_platformChannel.invokeMethod('pickFile')`（`flutter_engine_bridge` channel），但
  **Android `FlutterEngineBridgePlugin.onMethodCall` 与 iOS Swift 均无 `pickFile` 实现**
  → 必抛 `MissingPluginException`，「添加压缩包/XP3」在 Android 上直接失败。
- 问题 2（平台差异）：Android 用 SAF（`file_picker.getDirectoryPath`）拿 `content://` URI +
  持久授权，iOS/macOS 拿真实路径。这是**权限模型差异，不是"目录不能访问"**。
- 若后续要启用该功能再做：Android 原生补 `pickFile`（`ACTION_OPEN_DOCUMENT` + SAF +
  持久授权 copy 到 app 私有目录，或 `FilePicker.pickFiles` + SAF），注意 APK 体积与引擎
  只认真实路径的读取方式。