# 开发约定

## 平台与目录

- `cpp/core/sound/win32/`、`cpp/core/utils/win32/` 和 `cpp/core/environ/win32/` 中的部分文件是跨平台共享实现，不能按目录名称删除或改为 Windows 专属。
- 共享源文件中的平台守卫可能承载尚未启用的平台路径。删除前必须确认所有目标平台的 CMake 源列表和构建结果。
- Live2D SDK 是可选依赖。SDK 不存在时应保持自动禁用，不能把缺少 SDK 当作核心构建错误。
- `*.md` 属于正常版本控制文件；构建输出和平台产物按现有忽略规则处理。

## Live2D Cubism SDK（可选）

Live2D SDK 不通过 vcpkg 管理，相关商业 SDK 文件也不会提交到仓库。需要启用 Live2D 时，从官方 SDK 包中将以下内容放入 `cpp/plugins/cubism/`：

```text
cpp/plugins/cubism/
├── Core/
│   ├── include/Live2DCubismCore.h        已随仓库保留
│   └── lib/
│       ├── ios/Release-iphoneos/libLive2DCubismCore.a
│       └── macos/
│           ├── arm64/libLive2DCubismCore.a
│           └── x86_64/libLive2DCubismCore.a
└── Framework/                            SDK Framework 源码
```

- `Framework/` 应包含 Cubism Framework 的 C++ 源码，OpenGL ES 2 渲染器由当前 CMake 配置使用。
- iOS 使用 `Core/lib/ios/Release-iphoneos/` 下的静态库；macOS 根据架构使用对应目录下的静态库。
- 配置时若同时检测到 `Framework/*.cpp` 和目标平台的 `libLive2DCubismCore.a`，CMake 会编译 `krkrlive2d.cpp` 并定义 `KRKR2_LIVE2D`。
- 缺少任一部分时，CMake 会保留空的 `CubismFramework` 接口并禁用 Live2D 插件；其他平台的核心构建仍可继续。
- `Core/lib/` 和 `Framework/` 已加入 `.gitignore`，重新获取 SDK 后无需修改忽略规则。

检查方式：确认上述目录和静态库存在后，重新配置目标平台；配置输出不再出现 `Live2D Cubism SDK not found`，并在构建目标中看到 `krkrlive2d.cpp`。

## 构建与链接

- Apple 使用 ANGLE 的 Metal feature；Android 和 Linux 使用 Vulkan feature，不要跨平台混用。
- iOS 产物是静态库，macOS 产物是动态库，Android 产物是自包含 `libengine_api.so`。
- Android 插件源码通过 CMake 目标源传播进共享库，使用普通链接 `krkr2core` 和 `krkr2plugin`；不要使用 `--whole-archive`。
- Android JNI 的 Java 包名、native 方法名和 C++ 声明必须同步修改。
- 修改 vcpkg manifest、overlay port 或 triplet 后，必须重新验证对应平台的依赖缓存和 ABI。

## 生命周期

- 引擎重启必须分别处理脚本、存储、插件、字体、音频、窗口、渲染器和 GPU 资源。
- 进程级单例不能把一次性进程退出清理误当作引擎周期清理；需要显式 reset 时，为其定义独立的初始化和复位接口。
- EGL context 重建后不能复用旧 context 的纹理、FBO、shader 或扩展状态；所有 GPU 对象必须绑定当前 context generation。
- 异步启动、销毁和帧回调要明确所有权，不能让旧引擎实例持有回调或资源。

## 渲染与 SIMD

- `cpp/core/visual/tvpgl.cpp` 的标量实现是像素混合的正确性基准。
- 修改 SIMD 公式前必须进行标量与 SIMD 的逐像素对比，覆盖透明度、边界值、溢出和负值路径。
- 当前未完成逐位验证的 PS 混合保持标量回退；只有通过验证后才能重新注册 SIMD 派发。
- 渲染目标、FBO、纹理和 Layer 的坐标系必须在接口处明确，禁止在不同层重复应用缩放、偏移或旋转。
- 诊断探针统一使用 `KRKR_RENDER_PROBE`，默认关闭；禁止新增默认开启的模块级探针开关。
- 高频日志必须采样、限频、去重或只记录状态边沿；诊断专用扫描在宏关闭时不得执行。
- 探针不得改变渲染结果、事件顺序、线程调度、资源生命周期或正常性能；日志不得记录完整用户文本、个人路径和设备隐私。
- 新增或修改探针时同步更新 [探针清单](probes.md)，并验证探针 ON/OFF 会触发构建目录重新配置。

## 代码与文档

- 新增代码遵循现有模块边界和错误处理风格，不引入未在项目中使用的库。
- 注释只解释非显然的约束、算法和生命周期；复杂逻辑可使用中英双语短注释，避免重复描述代码本身。
- 文档描述当前事实、约束和验收条件，不记录个人设备、日志文件名、临时目录、内部提交编号或逐次试错过程。
- 外部资料只作为协议和行为入口，不在文档中将某段实现描述为直接复制，也不把参考实现当作本项目能力声明。
- 改动完成后至少运行对应的格式检查、类型检查、构建或测试；渲染和生命周期改动必须覆盖目标平台回归。
