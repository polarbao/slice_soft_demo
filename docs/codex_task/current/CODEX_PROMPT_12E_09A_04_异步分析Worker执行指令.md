# CODEX PROMPT 12E-09A-04 异步分析 Worker 执行指令

## 1. 必读

```text
docs/slice/PRD/PRD_12E_09A_SceneAware诊断UI.md
docs/slice/DEV/DEV_12E_09A_SceneAware诊断UI设计.md
docs/slice/DEMO/DEMO_12E_09A_SceneAware诊断UI验证方案.md
docs/slice/DOC/DOC_PREP_12E_09A_04_异步分析Worker准备.md
docs/codex_task/current/TASKS_12E_09A_诊断UI任务清单.md
```

## 2. 本次只执行

```text
12E-09A-04 异步分析 Worker。
```

必须完成：

```text
可取消的后台诊断；
完整 diagnostic identity/configHash；
scene/instance/revision/参数变化后的 stale 丢弃；
成功、失败、取消、重入、销毁测试；
中文开始/取消和状态显示；
状态报告与任务总览更新。
```

## 3. 禁止

```text
不得实现 09A-05 同层语义 Preview；
不得写生产 package/TIFF；
不得把诊断结果标为 production PASS；
不得默认启用 Global 生产或 OpenVDB；
不得删除 fixture 或改写历史输出；
不得夹带 12E-10 功能。
```

## 4. 验证

按准备文档的单元测试、Qt build、UI Smoke、self-test 和 `git diff --check` 执行。实际未运行的
命令不得写成 PASS。

## 5. 提交

本任务使用独立提交：

```text
feat(12E-09A-04): 【异步诊断】接通可取消分析Worker与身份防陈旧
```
