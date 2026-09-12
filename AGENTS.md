# PocketKrKr 开发指南

这是代理首屏规则。先读本文件；涉及具体模块时，再读 [开发文档索引](docs/dev/README.md)、[开发约定](docs/dev/conventions.md) 和 [关键引用](docs/dev/key-references.md)。本文只保存长期有效的事实与约束，不保存个人测试、临时路径、提交编号或历史推理。

## 项目边界

- `apps/flutter_app/`：Flutter 壳、页面、游戏管理和本地化。
- `bridge/`：C ABI、生命周期、帧接口、Dart FFI、MethodChannel 和原生纹理桥接。
- `cpp/core/`：TJS2、存储、归档、渲染、字体、音频、视频和主循环。
- `cpp/plugins/`：PSB、PSD、motionplayer、LayerEx、Cubism 和兼容扩展。
- `build.sh`、`build/`、`CMakePresets.json`：构建入口；`vcpkg.json`、`vcpkg/`：依赖配置；`docs/`：项目文档。

## 常用构建

```bash
./build.sh ios release
./build.sh android debug
./build.sh macos debug
cmake --preset "Linux Debug Config" && cmake --build --preset "Linux Debug Build"
```

iOS 使用静态库，macOS 使用动态库，Android 使用自包含 `libengine_api.so`，Linux 用于宿主验证。Android 插件通过 CMake 目标源进入共享库，使用普通链接，禁止 `--whole-archive`。

## 必须遵守

1. 只在 `codex` 分支工作。先运行 `git status` 并保留用户已有改动；禁止 `reset --hard`、`checkout`、`clean -f` 等破坏性操作。
2. 改代码前确认模块边界、调用方、错误处理、配置和现有测试；不为小改动引入新库，不顺手重构无关代码。
3. `sound/win32/`、`utils/win32/`、`environ/win32/` 中的部分实现跨平台共享；平台守卫也不得未经核对移除。
4. Apple 使用 ANGLE Metal feature，Android/Linux 使用 Vulkan feature；不得混用 triplet 或图形后端。
5. Cubism SDK 是可选依赖。缺失时自动禁用属于正常状态，不得阻断核心构建；启用路径见 [开发约定](docs/dev/conventions.md)。
6. Android JNI 的 Java 包名、native 方法名和 C++ 声明必须同步；修改公共 C ABI 时同步检查 Dart binding、生命周期和版本约束。
7. EGL context 重建后不得复用旧纹理、FBO、shader 或扩展状态；GPU 对象必须绑定当前 context generation。异步回调、线程和资源必须有明确所有权。
8. `cpp/core/visual/tvpgl.cpp` 的标量实现是像素混合基准。SIMD 修改须逐像素覆盖透明度、边界、溢出和负值路径；未验证的 PS 混合保持标量回退。
9. 诊断探针统一使用 `KRKR_RENDER_PROBE` 且默认关闭；高频日志必须采样、限频、去重或只记录状态边沿，诊断专用扫描在宏关闭时不得执行。
10. 探针不得改变正常结果、时序、生命周期或性能；日志不得记录完整用户文本、个人路径或设备隐私。新增或修改探针时同步更新 `docs/dev/probes.md` 及英文版，并验证 ON/OFF 重配置。
11. 注释只解释非显然的约束、算法和生命周期；复杂源码注释使用简短中英双语，避免复述代码。
12. 文档只写当前事实、约束、解法和验收标准；外部资料统一从 [兼容性与参考资料](docs/dev/krkrz-compat.md) 进入，不描述逐段复制或个人验证过程。

## 执行与验证

- 先读取目标文件上下文，搜索调用方、配置和测试；跨模块改动先确认影响范围。
- 若源文件与预期状态、当前任务或已读取上下文不一致，暂停该文件的修改并报告差异；不得覆盖、强行合并或猜测性修复。
- 修改后运行匹配的格式检查、类型检查、构建或测试；渲染、生命周期、JNI 和平台代码尽量做目标平台回归。
- 工具或依赖不可用时，不伪称通过；说明未执行的检查、原因和可复现命令。
- 完成后检查 `git diff --check`、`git status` 和最终 diff，排除临时文件、密钥、构建产物及无关改动。
- 文档或接口变化同步更新对应入口，避免重复复制架构、构建和兼容性说明。
- 每次改动完成并验证后必须提交；未完成、验证失败或存在未解决差异时不得提交。

## 当前入口

- 待办：[docs/dev/todo.md](docs/dev/todo.md)
- 优化：[docs/dev/optimization-roadmap.md](docs/dev/optimization-roadmap.md)
- 架构与源码：[docs/dev/README.md](docs/dev/README.md)
- 构建：[docs/dev/build.md](docs/dev/build.md)
- 兼容性与参考：[docs/dev/krkrz-compat.md](docs/dev/krkrz-compat.md)

## Git 协作

每次改动完成并通过可用验证后，手动执行 `git add <明确文件>` 和 `git commit`。提交信息必须同时说明改动对象、目的和原因，禁止使用无法表达目的的模糊标题；提交前确认只包含本次改动。不得 amend、rebase、force push、push 或创建 PR。
