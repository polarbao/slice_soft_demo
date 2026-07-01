# Slice Commit Style

## Purpose

提交信息需要同时服务于 Git 历史审计、阶段回溯和 AI 接续阅读。后续提交默认使用中文正文，并用 `【模块】` 标明变更类别。

## Subject

推荐格式：

```text
type(scope): 中文摘要
```

常用 `type`：

```text
feat      新功能或新实验能力
fix       bug 修复
docs      文档
test      测试
build     CMake、依赖、构建脚本
chore     仓库维护、索引、非功能性同步
refactor  不改变行为的结构调整
```

`scope` 应尽量写阶段或模块，例如：

```text
09P
slice
agents
openvdb
ui
geometry
material
```

## Body

推荐正文格式：

```text
- 【模块】说明做了什么、为什么做
- 【验证】列出实际运行过的命令和结果
- 【边界】说明没有改变哪些生产协议、安全边界或默认行为
```

正文要求：

- 使用中文描述，必要时保留英文标识符、路径、命令和 schema 名称。
- 每条说明聚焦一个功能类别，不把多个无关主题混在一条里。
- 验证命令只能写实际运行过的命令；不能把计划执行的命令写成已通过。
- 涉及 OpenVDB、RGBWSV、RIP、Qt UI、CMake 或依赖时，正文必须说明影响面。
- 涉及实验路径时，正文必须说明是否保持 `nonProduction`、是否保持默认关闭。

## Safety Notes

以下边界若未修改，应在相关提交中明确写入 `【边界】` 或 `【safety】`：

```text
p0.rgbwsv.2 未修改
channelOrder = R G B W S V 未修改
bitDepth = 8 未修改
polarity = black_is_print 未修改
OpenVDB 默认关闭
legacy slicer_cli production path 未被替代
experimental path 不写 production package
warn_and_attempt 不视为 production-safe
```

## Examples

```text
feat(09P): 固化 experimental OpenVDB report schema

- 【cli】扩展 slicer_cli experimental diagnostic report，补充 input、configSnapshot、outputContract 和 legacyPath 字段
- 【schema】新增机器可读 golden contract，并通过脚本验证 strict_closed、diagnostic_only、warn_and_attempt 三种模式
- 【验证】运行 cmake build、ctest、run_09p_cli_experimental_tests.ps1 和 run_09p_schema_tests.ps1，均通过
- 【边界】保持 p0.rgbwsv.2、R G B W S V、8-bit、black_is_print 和 experimental path 不写 production package 的安全边界
```

```text
docs(slice): 建立 09P-R2 正式文档与任务入口

- 【docs/slice】新增 PRD、DEV、DEMO、ROADMAP 和 REPORT 分类入口
- 【codex_task】新增当前任务清单与执行提示词入口
- 【治理】明确 docs/slice 是正式产品文档，docs/codex_task/current 是当前 AI 执行任务，docs/archive 是历史证据
- 【验证】运行 git diff --check，通过
```

