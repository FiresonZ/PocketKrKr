# 兼容性与参考资料

本文只保留当前兼容边界、能力缺口和统一资料入口。外部资料用于理解格式、接口与行为，不代表 PocketKrKr 已实现对应功能，也不直接复制外部代码。

## 当前能力矩阵

| 能力 | 当前状态 | 主要位置 | 下一步 |
|---|---|---|---|
| TJS2 与 KAG 基础运行 | 已支持 | `cpp/core/tjs2/`、`cpp/core/base/` | 按兼容性回归补齐边界行为 |
| XP3、ZIP、7z、TAR 资源读取 | 已支持 | `cpp/core/base/` | 补充加密和异常路径测试 |
| PNG、JPEG、WEBP、TLG、PSB 等图像 | 已支持部分格式 | `cpp/core/visual/`、`cpp/plugins/psbfile/` | 统一格式探测和错误回退 |
| Layer 与常规转场 | 已支持 | `cpp/core/visual/` | 补齐扩展特效边界 |
| PSB/M2 基础动画 | 已支持部分时间轴和节点 | `cpp/plugins/psbfile/`、`cpp/plugins/motionplayer/` | 完善网格、子运动、粒子和物理 |
| Live2D Cubism | 可选 | `cpp/plugins/cubism/` | SDK 存在时构建并回归 |
| Z 专用主合成路径 | 部分支持 | `cpp/core/visual/`、`cpp/plugins/zcompat/` | 核对主 DrawBuffer、目标纹理和更新链 |
| 视频显示合成 | 解码可用，显示路径待收口 | `cpp/core/movie/ffmpeg/`、`cpp/core/visual/` | 完成帧时钟、Present 和目标 Layer 更新 |
| Layer Alpha/Mosaic 扩展 | 部分接口为兼容桩 | `cpp/plugins/`、`cpp/core/visual/` | 先定义像素语义，再补实现 |
| Squirrel 等可选脚本插件 | 未完整支持 | `cpp/plugins/zcompat/` | 明确运行时依赖后按需实现 |

## 兼容边界

- 兼容性目标是尽量保持 TJS2、KAG、Layer、资源读取和常见插件接口的行为一致，不承诺所有桌面扩展均可用。
- 移动端不使用桌面 D3D9 绘制设备；需要验证的是移动端 RenderManager、LayerManager、DrawDevice 和 Flutter 纹理之间的合成关系。
- C++ 插件经 CMake 目标编入对应平台产物；纯 TJS 兼容层应保持脚本加载边界，不混入 C++ 生命周期。
- 可选 SDK、桌面专属接口和来源不明的插件应明确标记为可选或未支持，不能用“已加载”代替“功能已实现”。

## 统一参考入口

- KIRIKIRI Z 引擎与插件资料：<https://github.com/krkrz/krkrz>
- KIRIKIRI 2 插件资料：<https://github.com/krkrz/krkr2>
- Z 工具与扩展资料：<https://github.com/krkrz/krkrz_dev>
- Android 移植与平台行为资料：<https://github.com/zeas2/Kirikiroid2>
- KIRIKIRI 2 兼容脚本资料：<https://github.com/krkrz/Krkr2Compat>
- SDL 运行时和 M2 播放器资料：<https://github.com/krkrsdl3/krkrsdl3>
- TJS2 字节码工具：<https://github.com/crate-1556/tjs2-decompiler>

## 使用参考资料的规则

1. 先确认目标行为属于引擎、插件、脚本还是平台桥接层。
2. 只提取协议、数据结构和行为约束，在本项目中按现有架构重新实现。
3. 不将桌面专属依赖带入移动端；不把外部仓库的文件直接加入本项目。
4. 每项实现都要有标量基准、目标平台构建和兼容性回归。
5. 参考资料无法证明当前功能已支持，状态以代码和验证结果为准。
