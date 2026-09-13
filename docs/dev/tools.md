# 开发工具

PocketKrKr 的开发工具位于 `tools/`，不参与 iOS、Android、macOS 应用产物构建。

## XP3

`tools/xp3/` 是项目内的 XP3 解包工具，随宿主工具构建。用法见 [tools/xp3/README.md](../../tools/xp3/README.md)。

## TJS2 分析

`tools/tjs2dec/tjs2dec-linux-x86_64` 是预编译的 Linux x86_64 开发工具，用于分析 TJS2/TJS2100 编译脚本。它来自 AetherKiri 的 `tjs2Decompiler`，支持：

- `disasm`：指令和常量池反汇编。
- `tjs`：高层 TJS 源码重建尝试。
- `ssa`：生成 SSA 数据流表示。
- `ssa --hlir`：生成高层中间表示。
- `emit-tjs`：发射可执行的低层 TJS 表示。

```bash
TOOL=tools/tjs2dec/tjs2dec-linux-x86_64
$TOOL disasm path/to/script.tjs > /tmp/script.disasm.txt
$TOOL tjs path/to/script.tjs > /tmp/script.decompiled.tjs
$TOOL ssa --hlir path/to/script.tjs > /tmp/script.hlir.txt
```

分析游戏脚本时，优先检查 `SelectLayer.tjs`、`PreRenderFontEx.tjs`、`MultiResolution.tjs`、`yuzu_option.tjs` 和 `TextRender.tjs`。`tjs` 输出用于阅读，`disasm` 和 `ssa --hlir` 用于核对调用参数、坐标、宽高、字体和缩放数据。反编译结果不保证恢复注释、原变量名或原始源码布局。

预编译文件只覆盖 Linux x86_64。其他架构应按上游仓库的 Rust 构建说明重新构建。来源版本、许可证和工具边界见 [tools/tjs2dec/README.md](../../tools/tjs2dec/README.md)。
