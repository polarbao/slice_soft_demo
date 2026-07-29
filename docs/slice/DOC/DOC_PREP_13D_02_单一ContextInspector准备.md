# DOC PREP 13D-02 单一 Context Inspector 准备

> 文档状态：COMPLETE
> 版本：v1.0
> 日期：2026-07-29

## 1. 目标

把模型列表、变换、排版、切片设置和模型预检包装到唯一右侧 `ContextInspector`，保持现有 widget、
controller、SceneDocument identity 和信号语义。

## 2. 迁移映射

| 现有组件 | 目标页 | 处理 |
|---|---|---|
| `ModelListPanel` | 场景 | 复用实例 |
| `ModelTransformPanel` | 变换 | 复用实例 |
| `SceneLayoutPanel` | 排版 | 复用实例 |
| `ConfigEditorPanel` 常用参数入口 | 切片设置 | 只提供摘要与跳转，不复制编辑器 |
| `ModelPreflightPanel` | 预检 | 复用实例 |

## 3. 固定边界

```text
只允许一个右侧检查器常驻；
迁移不得复制 QObject 所有权或 controller；
selection、sceneRevision、transformRevision 必须保持；
右侧页切换不得触发配置写入、模型重载或预检；
13D-01 顶部作业栏保持不变。
```

## 4. 验收

新增 `workbench-context-inspector` Smoke，覆盖五页可达、实例选择一致、页切换无 scene revision
变化、右侧只有一个顶层检查器。完成后才解锁 13D-03。

## 5. 实施结果

2026-07-29 已完成：

```text
模型画布不再内嵌第二套模型侧栏；
模型列表、变换、排版、切片设置和预检迁入唯一 ContextInspector；
切片设置页可跳转中央完整配置；
参数、诊断和工艺对比暂存在同一检查器的高级诊断页，等待 13D-03 迁移；
workbench-context-inspector PASS；
multi-model-list、scene-grid-layout、workspace-layout-sizes PASS。
```
