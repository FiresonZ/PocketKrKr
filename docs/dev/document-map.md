# 文档分类地图

本页是 `docs/dev/` 的唯一分类入口。新增文档前先判断主题归属，再选择该分类下的现有页面；同一事实只保留一个规范来源。

## 分类

| 分类 | 用途 | 当前入口 |
|---|---|---|
| 入门与事实 | 项目结构、技术栈、当前架构事实 | [getting-started.md](getting-started.md)、[tech-stack.md](tech-stack.md)、[architecture.md](architecture.md) |
| 源码与接口 | 文件职责、关键符号、C ABI 和模块边界 | [source-map.md](source-map.md)、[key-references.md](key-references.md)、[api-contracts.md](api-contracts.md) |
| 构建与平台 | 工具链、预设、产物、CI 和平台差异 | [build.md](build.md) |
| 开发规则 | 修改前约束、生命周期、渲染、链接和文档规则 | [conventions.md](conventions.md)、[documentation-rules.md](documentation-rules.md) |
| 兼容性 | 当前能力、游戏回归、插件缺口和外部实现对照 | [compatibility.md](compatibility.md)、[plugin-compatibility.md](plugin-compatibility.md)、[aetherkiri-audit.md](aetherkiri-audit.md)、[krkrz-compat.md](krkrz-compat.md) |
| 测试与验收 | 可重复的 fixture、回归矩阵和验收条件 | [test-fixtures.md](test-fixtures.md)、[compatibility.md](compatibility.md) |
| 诊断与问题 | 探针、渲染诊断和结构化问题记录 | [probes.md](probes.md)、[rendering-diagnosis.md](rendering-diagnosis.md)、[incident-reports.md](incident-reports.md) |
| 工具与分析 | 解包、TJS2 反编译和开发辅助工具 | [tools.md](tools.md) |
| 规划与优化 | 当前待办、已批准路线和性能优化 | [todo.md](todo.md)、[optimization-roadmap.md](optimization-roadmap.md)、[perf-optimization.md](perf-optimization.md) |
| 发布与变更 | 面向版本的变更摘要和兼容性说明 | [release-notes.md](release-notes.md) |

## 近似主题的边界

- `compatibility.md` 记录测试闭环和回归要求，不记录外部仓库资料总表。
- `krkrz-compat.md` 记录外部参考入口和项目兼容边界，不记录单个游戏排查过程。
- `plugin-compatibility.md` 只记录插件状态、缺口、兼容桩和插件参考入口。
- `aetherkiri-audit.md` 只记录与 AetherKiri 的逐项实现对照，不替代通用兼容性文档。
- `todo.md` 只记录尚未完成的工作；已验证事项应移除或转入事实文档。
- `optimization-roadmap.md` 记录已确认的长期优化方向；`perf-optimization.md` 记录性能评估原则和验收方法。
- `probes.md` 记录探针清单；`rendering-diagnosis.md` 记录如何使用探针判断问题。
- `architecture.md` 记录系统数据流；`source-map.md` 记录文件定位；`key-references.md` 记录关键符号。

## 双语约定

稳定的开发文档必须同时提供 `.md` 中文页和 `.en.md` 英文页。两页结构、标题、链接和事实保持一致；代码标识、路径、命令和外部项目名不翻译。
