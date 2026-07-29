# TASKS 13D Qt 工作台布局收口任务清单

> 文档状态：13D-01..04 COMPLETE
> 版本：v1.0
> 日期：2026-07-29

## 1. 任务

### 13D-01 顶部作业栏

状态：`COMPLETE`

复用 13B-08 主动作，建立导入、保存、模式/Profile、切片当前场景、取消和状态摘要的固定顶部作业栏。

原子准备：
`docs/slice/DOC/DOC_PREP_13D_01_顶部作业栏准备.md`

### 13D-02 单一 Context Inspector

状态：`COMPLETE`

把模型列表、变换、排版、切片设置和预检重组为单一右侧检查器；保持实例 identity 和现有业务
controller。

原子准备：
`docs/slice/DOC/DOC_PREP_13D_02_单一ContextInspector准备.md`

### 13D-03 项目区和诊断 Dock 收口

状态：`COMPLETE`

左侧路径/兼容工具移入可折叠项目区；右侧诊断/工艺对比迁入底部 DiagnosticsDock；迁移后删除重复
壳层，不删除能力。

原子准备：
`docs/slice/DOC/DOC_PREP_13D_03_项目区与诊断Dock准备.md`

### 13D-04 响应式与阶段收口

状态：`COMPLETE`

完成布局状态版本化、1280x720/150% 缩放、中文长文本、tab order、截图/UI Smoke、用户手册和
`REPORT_13D_Qt工作台布局收口当前状态.md`。

原子准备：
`docs/slice/DOC/DOC_PREP_13D_04_响应式与阶段收口准备.md`

## 2. 顺序

```text
13C-05 PASS -> 13D-01 READY；
13D-01 -> 13D-02 -> 13D-03 -> 13D-04；
13D-04 PASS -> 12E-09A-03 恢复为推荐入口。
```
## 3. 固定边界

```text
不改变 SceneDocument、切片引擎或生产协议；
不重新实现 13C 预览；
不删除高级工具，只降低默认视觉权重；
不引入第三方 3D 库；
每个任务必须独立 Smoke 和提交。
```
