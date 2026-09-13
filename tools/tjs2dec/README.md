# tjs2dec

PocketKrKr ships a prebuilt Linux x86_64 copy of `tjs2dec` for offline analysis of compiled TJS2/TJS2100 scripts. It is a developer tool only and is not part of the iOS, Android, macOS, or engine runtime build.

Source project: https://github.com/AetherKiri/tjs2Decompiler
Source revision: `df497c8a8801971a20ccc681ffd3bbe54e5c0657`
License: MPL-2.0. See `MPL-2.0.txt` in this directory.

## Usage

```bash
TOOL=tools/tjs2dec/tjs2dec-linux-x86_64
$TOOL --help
$TOOL disasm path/to/script.tjs > /tmp/script.disasm.txt
$TOOL tjs path/to/script.tjs > /tmp/script.decompiled.tjs
$TOOL ssa path/to/script.tjs > /tmp/script.ssa.txt
$TOOL ssa --hlir path/to/script.tjs > /tmp/script.hlir.txt
$TOOL emit-tjs path/to/script.tjs > /tmp/script.emitted.tjs
```

Use `tjs` for a readable reconstruction attempt, `disasm` for bytecode and constant-pool details, and `ssa --hlir` when tracing coordinate, size, font, or scaling data flow. Output is an approximation and does not restore comments or the original source layout.

The bundled binary targets Linux x86_64. Build the upstream Rust project separately when another host architecture is required.
