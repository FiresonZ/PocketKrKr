# PocketKrKr 开发指南

改代码前先阅读 [docs/dev/README.md](docs/dev/README.md)、[docs/dev/conventions.md](docs/dev/conventions.md) 和 [docs/dev/key-references.md](docs/dev/key-references.md)。当前文档只描述长期有效的项目约束，不保存个人测试记录或历史推理过程。

## 项目结构

- `apps/flutter_app/`：Flutter 壳、页面、游戏管理和本地化。
- `bridge/engine_api/`：C ABI、引擎生命周期、帧接口和 Android JNI。
- `bridge/flutter_engine_bridge/`：Dart FFI、MethodChannel 和原生纹理桥接。
- `cpp/core/`：TJS2、存储、归档、渲染、字体、音频、视频和主循环。
- `cpp/plugins/`：PSB、PSD、motionplayer、LayerEx、Cubism 和兼容扩展。
- `build.sh`、`build/`、`CMakePresets.json`：平台构建入口与预设。
- `vcpkg.json`、`vcpkg/`：依赖、overlay port 和平台 triplet。
- `docs/`：用户文档和开发文档。

## 构建命令

```bash
./build.sh ios release
./build.sh android debug
./build.sh macos debug
cmake --preset "Linux Debug Config" && cmake --build --preset "Linux Debug Build"
```

iOS 使用静态库，Android 使用自包含 `libengine_api.so`，macOS 使用动态库；Linux 用于宿主验证。Android 插件源码通过 CMake 目标源传播进共享库，使用普通链接，禁止 `--whole-archive`。

## 硬性约束

1. `sound/win32/`、`utils/win32/` 和 `environ/win32/` 中的部分实现跨平台共享，不能按目录名称删除。
2. 不要随意删除平台守卫；删除前须核对所有目标平台的源码列表和构建结果。
3. Apple 使用 ANGLE Metal feature，Android/Linux 使用 Vulkan feature。
4. Cubism SDK 缺失时自动禁用是正常状态，不得让可选 SDK 阻断核心构建。
5. Android JNI 的 Java 包名、native 方法名和 C++ 声明必须同步。
6. EGL context 重建后不得复用旧纹理、FBO、shader 或扩展状态；GPU 对象必须绑定当前 context generation。
7. `cpp/core/visual/tvpgl.cpp` 的标量实现是像素混合正确性基准；SIMD 修改必须逐像素对比。
8. 未完成逐位验证的 PS 混合保持标量回退。
9. 探针默认关闭，高频日志必须采样或限频。
10. 文档只写当前事实、约束、解法和验收标准；不写个人设备、日志文件、临时路径、内部提交编号或外部代码复制过程。
11. 改动完成后运行对应的格式检查、类型检查、构建或测试；渲染和生命周期改动必须进行目标平台回归。

## 待办入口

当前未完成事项见 [docs/dev/todo.md](docs/dev/todo.md)，性能和结构方案见 [docs/dev/optimization-roadmap.md](docs/dev/optimization-roadmap.md)。外部格式和行为资料统一见 [docs/dev/krkrz-compat.md](docs/dev/krkrz-compat.md)。

## Git 协作

只在 `codex` 分支工作。提交前手动 `git add` 和 `git commit`，提交信息说明改动文件及原因；不修改历史、不 amend、不 rebase、不 force push、不直接 push、不创建 PR。
