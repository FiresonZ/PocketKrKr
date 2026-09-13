# 插件兼容清单

## 状态定义

- **真实实现**：已进入当前 CMake 目标并包含可执行行为；仍需按接口和游戏回归确认覆盖范围。
- **兼容适配**：注册了名称或必要 API，用于让脚本继续运行；不代表完整桌面插件行为。
- **可选实现**：只有 SDK、平台或外部依赖存在时才进入产物。
- **未启用**：源码或子目录存在，但当前 CMake 没有编译进默认插件目标。
- **待审计**：需要先确认真实调用契约，再决定实现或保持桩。

## 当前插件矩阵

| 模块/脚本名 | 当前状态 | 本地入口 | 当前能力和边界 | 是否建议实现 | 验收重点 |
|---|---|---|---|---|---|
| `motionplayer.dll` | 真实实现 | `cpp/plugins/motionplayer/` | PSB/M2 图层、时间线、子运动和移动端 capture 兼容；复杂 Emote/物理仍不完整 | 是，按缺口逐项 | 时间线、skip/sync、子运动、循环、裁剪、资源生命周期 |
| `psbfile.dll` | 真实实现 | `cpp/plugins/psbfile/` | PSB 读取、图像和部分运动元数据 | 是，按格式缺口 | 多版本 PSB、WebP/raw fallback、元数据和坏资源 |
| `psdfile.dll` | 真实实现 | `cpp/plugins/psdfile/` | PSD/PBD 读取和图层资源 | 是，按格式缺口 | 图层顺序、透明度、裁剪、颜色和异常文件 |
| `layerex_draw.dll` | 真实实现 | `cpp/plugins/layerex_draw/` | 扩展路径、画刷和绘制方法 | 是，按 API 缺口 | alpha、空图层、坐标、缩放和像素基准 |
| `kagparserex.dll` | 真实实现 | `cpp/plugins/kagparserex/` | KAG 扩展解析和脚本接口 | 是，按脚本 fixture | 标签参数、异常、编码和动态属性 |
| `fstat.dll` | 真实实现 | `cpp/plugins/fstat/` | 文件状态查询 | 低优先级 | 文件、归档虚拟路径和失败返回 |
| `json.dll` | 未启用 | `cpp/plugins/json/`；`CMakeLists.txt` 中已注释 | 源码和脚本说明存在，默认目标不编译 | 按游戏调用决定 | 注册名、JSON 类型、异常和线程边界 |
| `steam.dll` | 未启用 | `cpp/plugins/steam/`；`CMakeLists.txt` 中已注释 | Steam 相关能力不进入移动端默认产物 | 通常不实现；保留兼容桩 | 游戏启动不因可选 Steam API 缺失而中断 |
| `DrawDeviceForSteam.dll` | 未启用 | `cpp/plugins/DrawDeviceForSteam/` | 桌面/Steam DrawDevice 适配 | 不建议移植到移动端 | 仅确认缺失时脚本不会误走桌面路径 |
| `krkrlive2d.dll` | 可选实现 | `cpp/plugins/krkrlive2d.cpp`、`cpp/plugins/cubism/` | SDK 存在时启用；缺失时保留 Cubism 接口和自动禁用路径 | SDK 存在的目标平台实现 | SDK 缺失、模型加载、连续动画和 context 重建 |
| `krkrgles.dll` | 真实/平台适配 | `cpp/plugins/krkrgles.cpp` | GLES/移动端相关接口和 Live2D/纹理桥接入口 | 只补已证明调用 | EGL context、纹理所有权、平台后端和销毁 |
| `textrender.dll` | 真实实现 | `cpp/plugins/textrender.cpp` | 独立文字渲染和逻辑尺寸缩放 | 是，继续做边界测试 | framebuffer 缩放、字号、透明度和目标 Layer |
| `layerExMovie.dll` | 真实实现 | `cpp/plugins/layerExMovie.cpp`、`cpp/core/movie/ffmpeg/` | 视频图层接口和 FFmpeg 连接 | 是 | 首帧、暂停、跳转、结束、重复播放和音画时钟 |
| `alphamovie.dll` | 真实实现/待审计 | `cpp/plugins/alphamovie.cpp` | Alpha movie 注册和兼容接口 | 按实际游戏调用 | 视频 alpha、像素格式和缺失解码器 |
| `layerExImage.dll` | 真实实现 | `cpp/plugins/layerExImage.cpp` | 图像扩展方法 | 按调用覆盖 | 缩放、滤镜、格式和异常资源 |
| `layerExRaster.dll` | 真实实现 | `cpp/plugins/layerExRaster.cpp` | Raster 扩展方法 | 按调用覆盖 | 空输入、边界、alpha 和性能 |
| `layerExBTOA.dll` | 真实实现 | `cpp/plugins/layerExBTOA.cpp` | 蓝色通道到 alpha 等兼容操作 | 按调用覆盖 | 通道语义、透明度和标量像素结果 |
| `layerExPerspective.dll` / `perspective.dll` | 真实实现 | `cpp/plugins/layerExPerspective.cpp` | 透视/几何扩展 | 按调用覆盖 | 变换矩阵、裁剪、边界和透明度 |
| `layerExAreaAverage.dll` | 真实实现 | `cpp/plugins/layerExAreaAverage.cpp` | 区域平均处理 | 低优先级 | 空区域、边界和像素基准 |
| `layerExLongExposure.dll` | 真实实现 | `cpp/plugins/layerExLongExposure.cpp` | 长曝光/累计效果 | 低优先级 | 累计帧、清理、alpha 和内存 |
| `extrans.dll` | 真实实现 | `cpp/plugins/extrans.cpp`、`extrans_precise/` | 转场扩展和部分精确实现 | 是，按效果补齐 | 起止帧、时间、透明度、裁剪和 SIMD/标量一致性 |
| `xp3filter.dll` | 真实实现 | `cpp/plugins/xp3filter.cpp` | XP3 过滤和扩展回调 | 按游戏需要 | 归档过滤、回调生命周期和异常 |
| `addFont.dll` | 真实实现 | `cpp/plugins/addFont.cpp` | 脚本动态字体添加 | 是，字体回归需要 | 字体注册、释放、重启和 fallback |
| `getSample.dll` | 真实实现 | `cpp/plugins/getSample.cpp` | WaveSoundBuffer 采样接口 | 低优先级 | 音频缓冲、越界和无设备回退 |
| `fftgraph.dll` | 兼容适配 | `cpp/plugins/fftgraph.cpp` | 注册脚本函数，当前主要是空/简化兼容 | 按实际调用决定 | 脚本是否读取返回值；不因缺失阻断启动 |
| `win32dialog.dll` | 兼容适配 | `cpp/plugins/win32dialog.cpp` | 用脚本或移动端 UI 代替桌面对话框 | 通常不实现桌面 UI | 返回值、取消路径和无 UI 环境 |
| `drawDeviceD2Dm.dll` | 兼容适配 | `cpp/plugins/drawDeviceD2DCompat.cpp` | 桌面 D2D 接口兼容 | 不移植到 Android/iOS | 脚本探测不失败，不误切换桌面设备 |
| `wfBasicEffect.dll` | 兼容适配 | `cpp/plugins/wfBasicEffectCompat.cpp` | Windows effect API 名称/接口兼容 | 按游戏调用决定 | 参数、返回值和空实现语义 |
| `wfTypicalDSP.dll` | 兼容适配 | `cpp/plugins/wfTypicalDSPCompat.cpp` | Windows DSP API 兼容 | 按游戏调用决定 | 音频效果调用不崩，缺失能力可识别 |
| `zcompat` | 兼容适配 | `cpp/plugins/zcompat/` | Z 相关名称、脚本和插件兼容 | 是，按真实调用 | DrawBuffer、插件名、Squirrel/扩展边界 |
| `kirikiroid2` 兼容项 | 兼容适配 | `cpp/plugins/kirikiroid2.cpp`、`motionplayer/main.cpp` | 移动端脚本和 D3DAdaptor/SeparateLayerAdaptor 兼容 | 按调用契约 | captureCanvas、canvasCaptureEnabled、属性写入和生命周期 |
| `windowEx.dll` | 真实实现/兼容 | `cpp/plugins/windowEx.cpp` | Window 扩展和移动端替代行为 | 按调用覆盖 | 输入、窗口属性和无桌面窗口路径 |
| `varfile.dll` | 真实实现 | `cpp/plugins/varfile.cpp` | 变量文件读写 | 按存档回归 | 编码、原子写入和异常恢复 |
| `saveStruct.dll` | 真实实现 | `cpp/plugins/saveStruct.cpp` | 存档结构辅助 | 是 | 版本兼容、损坏存档和顺序 |
| `getabout.dll` | 真实实现/兼容 | `cpp/plugins/getabout.cpp` | 关于/环境信息接口 | 低优先级 | 移动端字段和隐私边界 |
| `wutcwf.dll` | 待审计 | `cpp/plugins/wutcwf.cpp` | 旧作品兼容接口 | 按真实调用决定 | 调用契约和异常 |

## 缺失和优先级

### 优先实现或补齐

1. `motionplayer.dll` 的 skip/sync、自动进度、command-list 和标题动画回归。
2. `layerExMovie.dll`、FFmpeg 视频显示合成和帧时钟。
3. `addFont.dll` 与预渲染字体/运行时字体的生命周期测试。
4. `zcompat` 的真实游戏调用缺口和主 DrawBuffer 路径。
5. 关键插件注册、参数和返回值的独立 fixture。

### 保持兼容桩，不应直接实现

- `steam.dll`
- `DrawDeviceForSteam.dll`
- `drawDeviceD2Dm.dll`
- `win32dialog.dll` 的桌面 UI 部分
- `wfBasicEffect.dll` 和 `wfTypicalDSP.dll` 的桌面专属能力

这些模块只要满足“脚本探测不崩、可选功能缺失可识别、不会错误切入桌面路径”即可。

## 可借鉴的实现和资料入口

| 主题 | 入口 | 用途 |
|---|---|---|
| AetherKiri 同源实现 | <https://github.com/AetherKiri/AetherKiri/tree/main/cpp/plugins> | 插件状态机、兼容适配、测试和诊断实现参考 |
| AetherKiri 插件缺口审计 | <https://github.com/AetherKiri/AetherKiri/blob/main/tools/plugin_gap_audit.py> | real/compat_stub/empty_stub 分类方式 |
| AetherKiri 插件注册测试 | <https://github.com/AetherKiri/AetherKiri/tree/main/tests/unit-tests/plugins> | 注册名、参数、返回值和兼容桩 fixture |
| KIRIKIRI Z 插件 | <https://github.com/krkrz/krkrz/tree/master/plugins> | Z API、插件接口和数据格式 |
| KIRIKIRI 2 插件 | <https://github.com/krkrz/krkr2/tree/master/plugins> | 原始 KiriKiri2 插件行为参考 |
| Kirikiroid2 移动端实现 | <https://github.com/zeas2/Kirikiroid2> | Android/移动端兼容行为和 D3DAdaptor 替代思路 |
| krkr2-tools | <https://github.com/xiaocongyu66/krkr2-tools> | XP3、TJS 和脚本资源分析 |
| TJS2 反编译器 | <https://github.com/crate-1556/tjs2-decompiler> | 编译脚本调用链和兼容变量分析 |
| SDL M2 播放器 | <https://github.com/krkrsdl3/krkrsdl3> | M2/PSB 时间轴和播放行为对照 |

## 规则

1. “已注册”不等于“完整实现”；必须区分真实实现、兼容适配和空桩。
2. 外部地址只用于协议、行为和测试设计参考，不直接复制文件。
3. 每个待实现插件必须先拿到真实调用契约，再定义最小行为和验收 fixture。
4. 插件缺口完成后，应从本清单移动到“已验证”或删除对应待办，而不是永久堆积。
5. 桌面专属插件不因 AetherKiri 或 Kirikiroid2 存在就进入 Android/iOS 产物。
