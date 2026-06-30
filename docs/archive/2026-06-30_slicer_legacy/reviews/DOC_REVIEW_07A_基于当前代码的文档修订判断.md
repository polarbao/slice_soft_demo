# DOC_REVIEW_07A_基于当前代码的文档修订判断

> 文档版本：v0.2  
> 文档状态：Review / 基于当前 07 代码的 07A 文档修订判断  
> 适用阶段：REPORT_07 之后 / 07A 执行前  
> 建议提交目录：`docs/slicer/`

---

## 1. 结论

07A 阶段文档不需要推翻重写，但建议做小幅修订。

原 07A 文档的主方向仍然正确：

```text
Config form editor
MaterialProcessProfile editor
MaterialPolicy editor
MaterialRoleMapping editor
Channel chart
Preview overlay
Save / Save As
Self-test 增强
```

但当前代码已经落地了 07 基础 UI，因此 07A 文档应从“抽象规划”修订为“基于现有 slicer_debug_ui 结构的增量改造”。

---

## 2. 当前代码基线

当前 07 代码已经具备：

```text
apps/slicer_debug_ui/
  CMakeLists.txt
  main.cpp
  MainWindow.*
  services/
    PackageLoader.*
    ProcessRunner.*
    ReportLoader.*
    ToolPaths.*
  widgets/
    LogPanel.*
    MaterialProcessPanel.*
    PreviewPanel.*
    ReportPanel.*
```

`MainWindow` 当前布局是：

```text
Left:
  config / package / profile A / profile B
  run buttons

Center:
  Preview tab
  Reports tab

Right:
  Material tab
  Warnings tab
  Compare tab

Bottom:
  LogPanel
```

因此 07A 不应重新设计主架构，而应在现有 MainWindow 上增量增加：

```text
Config tab
Profile editor tab
Charts tab
Overlay tab
```

---

## 3. 需要修订的点

### 3.1 CMake 表述修订

原文档中“新增 Qt target”应改为：

```text
当前 Qt target 已存在。
07A 只需要把新增 widgets/services 加入 apps/slicer_debug_ui/CMakeLists.txt。
```

不要重新创建 target。

---

### 3.2 MainWindow 改造方式修订

原文档应明确：

```text
保留现有 MainWindow / ProcessRunner / PackageLoader / ReportLoader / PreviewPanel / ReportPanel。
```

新增 UI 应挂接到当前 tab 体系：

```text
Center tabs:
  Preview
  Reports
  Config
  Charts
  Overlay

Right tabs:
  Material
  Warnings
  Compare
  Profile Editor
```

或等价布局。

---

### 3.3 MaterialProcessPanel 与 Editor 分离

当前 `MaterialProcessPanel` 是 report summary viewer，不是编辑器。

07A 应新增：

```text
MaterialProcessProfileEditor
```

而不是把 `MaterialProcessPanel` 改成编辑器。

推荐职责：

```text
MaterialProcessPanel:
  继续读取 output package 中的 material_process_report.json

MaterialProcessProfileEditor:
  编辑 config JSON 中的 materialProcessProfile
```

---

### 3.4 Report / Preview 不替换

当前已有：

```text
ReportPanel
PreviewPanel
```

07A 应新增：

```text
ChannelChartPanel
PreviewOverlayPanel
```

并复用现有 PackageLoader / PreviewLoader 思路，而不是替换原 report/preview 功能。

---

### 3.5 Tool Path 与可执行路径修订

文档中统一使用当前真实路径：

```text
build/apps/slicer_debug_ui/Debug/slicer_debug_ui.exe
```

而不是：

```text
build/Debug/slicer_debug_ui.exe
```

---

### 3.6 Config 保存策略补充

07A 必须保护现有配置：

```text
Save As 优先；
Save 覆盖前弹确认；
保存非法 JSON 禁止；
dirty config 运行前提示保存。
```

---

### 3.7 Preview overlay 数据源补充

当前 PreviewPanel 通过 preview 目录扫描工作。

07A overlay 应采用两级策略：

```text
1. 优先读取 preview_report.json 中的 layer/channel metadata；
2. 如果缺失，则沿用当前文件名 token 粗分类 fallback。
```

---

## 4. 不需要改的点

以下原 07A 文档内容仍然有效：

```text
不改 slicer_core 输出协议
不做设备通信
不做 RIP 半色调
不做 ICC/CMYK
不做 OpenVDB
不做完整 3D viewport
不做生产级任务系统
```

---

## 5. 建议

建议将 07A 文档升级为 v0.2，重点不是改变目标，而是明确：

```text
07A = 基于现有 slicer_debug_ui 的增量增强
```

不是重新创建 UI 工程。
