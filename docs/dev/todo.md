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
- **已做**：③ 挂名完成（`cpp/plugins/zcompat/zcompat_plugin.cpp`，16 个 Z 插件名内部注册，link 不再 Failed）。
- **下一步**：① Windows 跑同游戏二分（引擎 vs 渲染链路）；② 对照 Kirikiroid2
  `src/core/visual/RenderManager_ogl.cpp`+`BasicDrawDevice.cpp` 与我们的 `RenderManager.*`，定位
  DrawBuffer 未合成原因；③ 挂名后逐个实现（squirrel/k2compat 先行）。

### P1 — §2a. motionplayer 缺 `Motion.D3DAdaptor`：千恋万花首屏后无法进入【属 motionplayer 兼容】
> `Motion.D3DAdaptor` 是 **krkr2 motionplayer** 成员（Direct3D affine），非 krkrz/Z 专属——实证
> Kirikiroid2_patch(zeas2) 给千恋万花的 patch.tjs 用 `typeof Motion.D3DAdaptor=="undefined"` 兜底，说明
> 真机 krkr2 的 motionplayer.dll 确有 D3DAdaptor。移动端标准做法=undefined+useD3D=0；我们暂用「可 new 空类」
> 让 `new Motion.D3DAdaptor(...)` 走通、由 CPU/GL motion 播 logo。
- 现况：`getD3DAdaptor` 返回 `new tTJSNativeClass("D3DAdaptor")`（`cpp/plugins/motionplayer/main.cpp`）。
- 待办：真机复验千恋万花首屏 logo 后不再崩；若还崩，补实例成员 stub 或改 Kirikiroid2 式 undefined+useD3D=0。

### P1 — §2b. motionplayer 的 `EmotePlayer`（emoteplayer.dll）未实现【NEW】
- 现况：`cpp/plugins/motionplayer/EmotePlayer.{h,cpp}` 是**空壳 stub**（仅 `_useD3D` 读写，无 emote 物理/播放）。
- 影响：M2-Emote 型作品（如 limelight-lemonade-jam 那类）派 motion 时无真正向量骨骼动画。
- 优先级：**排在 §2（krkrz 黑屏）之后**；与 §2a/§6 独立，无前置依赖。无稳定环境复现，属"有空再做"补齐。

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