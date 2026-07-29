# REPORT 12E-09A-03 中文参数控件与状态区当前状态

> 日期：2026-07-29
> 状态：COMPLETE
> 下一任务：12E-09A-04 异步分析 Worker

## 1. 结论

12E-09A-03 已把 Texture Surface Layer 宽度、Model Fill 材料、诊断对象、派生边界、
backend 可用状态和阻断原因加入 13D 单一 `ContextInspector` 的“切片设置”页。

该页面是 diagnostic-only：编辑不会直接修改生产 Profile，不写 Package/TIFF，也不会把
OpenVDB 候选工具存在解释为生产准入。

## 2. 已实现

```text
纹理宽度 SpinBox + Slider 双向同步；
单位 mm、步长 0.01 mm、两位小数；
预分析上限 6.00 mm，最小值按 DPI/层高两单元规则动态计算且不低于 0.10 mm；
白墨、光油、RGB 三种当前已支持的模型填充材料；
场景 id、scene revision 和 current instance 摘要；
最小/最大/全纹理阈值，缺失值明确显示“未评估”；
Legacy CPU 可用状态与 OpenVDB 候选工具发现状态，不用文件存在冒充能力探测；
无模型、导入中、ready、blocked、failed 中文状态；
每个控件和状态提供中文 tooltip。
```

## 3. UI 位置

```text
主窗口 -> 右侧上下文检查器 -> 切片设置 -> 纹理与填充诊断
```

右侧检查器隐藏时可通过：

```text
视图 -> 上下文检查器
Ctrl+Alt+I
```

## 4. 边界

本任务没有实现：

```text
09A-04 异步分析、取消和 stale 丢弃；
09A-05 同层 Texture/Fill/Partition 诊断叠加；
动态模型最大宽度和全纹理阈值计算；
生产 Profile 或生产 TIFF 修改；
独立 C/M/Y/K 模型填充材料。
```

## 5. 验证入口

```text
diagnostic-settings-controls：
  中文控件；
  0.01 mm；
  SpinBox/Slider 双向同步；
  white/varnish/rgb；
  无模型禁用；
  available 状态和最长中文；
  1280x720、1440x900、1920x1080。
```

## 6. 下一步

12E-09A-04 必须建立可取消 Worker，并在启动前通过 09A-02
`DiagnosticEffectiveConfig` 冻结 subject identity、requested 参数和 configHash。Worker 返回 UI
前必须再次检查 scene/instance/revision/configHash，拒绝 stale 结果。
