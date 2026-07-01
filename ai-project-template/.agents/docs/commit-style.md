# Commit Style

## Purpose

提交信息应支持人工审计、阶段回溯和 AI 接续阅读。模板项目默认推荐中文正文和 `【模块】` 功能说明；目标项目可按团队语言调整。

## Subject

推荐格式：

```text
type(scope): summary
```

常用 `type`：

```text
feat      feature or capability
fix       bug fix
docs      documentation
test      tests
build     build, dependency, packaging
chore     repository maintenance
refactor  behavior-preserving code structure change
```

`scope` 应尽量写阶段、模块或子系统。

## Body

推荐正文格式：

```text
- 【module】what changed and why
- 【validation】commands actually run and results
- 【boundary】production, protocol, dependency, or compatibility boundaries preserved
```

目标项目如果使用中文，可将 `module / validation / boundary` 替换为：

```text
【模块】
【验证】
【边界】
```

正文要求：

- 只写实际完成和实际验证过的内容。
- 不把计划执行的命令写成已通过。
- 涉及协议、生产路径、依赖或数据迁移时，说明影响面和未改变的边界。
- 如果任务来自 task card，正文应能对应任务目标、验证命令和禁改项。

## Example

```text
feat(component): 中文摘要

- 【module】新增或修改的能力，以及为什么需要它
- 【validation】运行 <command>，通过
- 【boundary】未修改生产协议、默认配置或兼容路径
```

