# 工具（tools）

命令行工具，随宿主构建（macOS / Linux）编译，移动端构建（iOS/Android）不包含：

- `xp3/`：XP3 归档命令行工具（`./build.sh` 构建；Linux CI 中做冒烟测试）
- `tjs2dec/`：预编译 TJS2/TJS2100 分析工具，支持反汇编、SSA/HLIR、高层反编译和 TJS 发射；仅随仓库提供给 Linux x86_64 开发环境使用，不参与应用构建。
