# Development Tools

PocketKrKr development tools live under `tools/` and are excluded from iOS, Android, macOS application artifacts.

## XP3

`tools/xp3/` is the project XP3 extraction tool and is built with the host tools. See [tools/xp3/README.md](../../tools/xp3/README.md) for usage.

## TJS2 Analysis

`tools/tjs2dec/tjs2dec-linux-x86_64` is a prebuilt Linux x86_64 developer tool for compiled TJS2/TJS2100 scripts. It comes from AetherKiri's `tjs2Decompiler` and supports:

- `disasm`: bytecode instructions and constant-pool disassembly.
- `tjs`: high-level TJS reconstruction attempt.
- `ssa`: SSA data-flow representation.
- `ssa --hlir`: high-level intermediate representation.
- `emit-tjs`: executable low-level TJS emission.

```bash
TOOL=tools/tjs2dec/tjs2dec-linux-x86_64
$TOOL disasm path/to/script.tjs > /tmp/script.disasm.txt
$TOOL tjs path/to/script.tjs > /tmp/script.decompiled.tjs
$TOOL ssa --hlir path/to/script.tjs > /tmp/script.hlir.txt
```

For game-script analysis, inspect `SelectLayer.tjs`, `PreRenderFontEx.tjs`, `MultiResolution.tjs`, `yuzu_option.tjs`, and `TextRender.tjs` first. Use `tjs` for reading and `disasm` or `ssa --hlir` to verify call arguments, coordinates, dimensions, font settings, and scaling data. Decompilation is not guaranteed to restore comments, original variable names, or source layout.

The bundled binary covers Linux x86_64 only. Rebuild it for other host architectures using the upstream Rust project instructions. Source revision, license, and tool boundaries are documented in [tools/tjs2dec/README.md](../../tools/tjs2dec/README.md).
