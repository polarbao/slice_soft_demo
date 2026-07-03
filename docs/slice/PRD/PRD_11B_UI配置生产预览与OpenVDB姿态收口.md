# PRD_11B_UI配置生产预览与OpenVDB姿态收口

> 文档版本：v0.1  
> 文档状态：PRD / Stage 11B  
> 生成日期：2026-07-04

---

## 1. Goal

Stage 11B 目标是在不改变生产协议和不默认启用 OpenVDB 的前提下，解决当前用户在 UI 中遇到的三类问题：

```text
OpenVDB candidate 与 legacy 一键切片姿态不一致；
UI 预览与 TIFF RGB 生产数据语义不一致，导致填充/支撑/空白误判；
配置文件数量过多，用户缺少统一设置入口。
```

---

## 2. User Stories

### 2.1 OpenVDB candidate 与 legacy 使用同样摆放策略

作为调试人员，我希望同一个 OBJ 模型走 legacy 和 OpenVDB candidate 时使用同样的 scale 与 autoOrient，以便比较两种引擎，而不是先被模型姿态差异干扰。

验收：

```text
UI 生成 OpenVDB candidate 配置包含 modelTransform；
autoOrient.enabled=true；
maxHeightMm=6.0；
同一模型 inspect 后 selectedOrientation 与 legacy 一致；
模型高度 <= 6mm。
```

### 2.2 能在 UI 中区分生产 RGB、纹理 RGB、S/W/V

作为切片验证人员，我希望 UI 能显示生产 TIFF 的真实 RGBWSV 六通道值，以便判断白色、黑色、绿色等显示颜色到底代表空白、模型填充、支撑、白墨还是光油。

验收：

```text
新增生产 RGB 预览模式；
保留 texture_rgb true-color 预览；
新增六通道像素探针；
图例明确显示通道语义；
不改变 production TIFF 数据。
```

### 2.3 能通过 UI 设置替代大量手工 JSON

作为普通使用者，我希望在 UI 中选择模型、材料、支撑、预览、OpenVDB 实验开关，而不是在大量 JSON 配置里挑选。

验收：

```text
常用 Profile 进入 UI 默认列表；
测试 fixture 进入高级/测试分类；
基础/材料/支撑/预览/实验设置可以覆盖长期常用参数；
用户仍可手动打开 JSON 做高级调试。
```

### 2.4 判断 OpenVDB 何时可替代 legacy

作为项目负责人，我希望有明确 gate 判断 OpenVDB 什么时候可替代 legacy，而不是基于单次 demo 或历史阶段名称做决定。

验收：

```text
有 replacement gate；
有同模型同姿态 benchmark；
有真实模型集合；
有 texture fidelity、RIP、UI、支撑、性能和内存指标；
未满足 gate 前 OpenVDB 仍为 candidate。
```

---

## 3. Product Requirements

### P0

```text
P0-1：修复 OpenVDB candidate UI 姿态配置与 legacy 不一致；
P0-2：新增或设计生产 RGB 预览模式；
P0-3：新增或设计六通道像素探针；
P0-4：补齐 OpenVDB replacement gate 文档；
P0-5：保持 OpenVDB 默认关闭和 legacy production path 不变。
```

### P1

```text
P1-1：新增 texture.nonSurfaceRgbPolicy；
P1-2：切片设置界面覆盖基础/材料/支撑/预览/实验；
P1-3：Profile 与 fixture 在 UI 中分层展示；
P1-4：同姿态 legacy/OpenVDB benchmark 脚本化。
```

### P2

```text
P2-1：OpenVDB 支撑策略与 legacy full_vertical_projection 对齐；
P2-2：OpenVDB repair_then_strict 路线；
P2-3：OpenVDB Release benchmark 和内存预算；
P2-4：真实模型集合 replacement candidate 报告。
```

---

## 4. Non-goals

```text
不实现 RIP 半色调；
不实现设备通信；
不修改 p0.rgbwsv.2；
不默认启用 OpenVDB；
不把 OpenVDB non-production package 当 production；
不删除现有 samples/configs fixture；
不在 11B 内完成 OpenVDB 全面替代。
```

---

## 5. Acceptance

最小验收：

```powershell
.\build\Debug\slicer_cli.exe --config <legacy-generated> --inspect-model
.\build\Debug\slicer_cli.exe --config <openvdb-candidate-generated> --inspect-model
```

两者应显示：

```text
selectedOrientation 一致；
orientedBbox height <= 6mm。
```

UI/Preview 验收：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\UiSmokeLayerPreview
```

OpenVDB gate 验收：

```text
OpenVDB replacement gate 文档存在；
同姿态 benchmark 记录 legacy 和 OpenVDB candidate 的耗时、输出状态和不可比项；
报告明确 OpenVDB 当前是否满足替代条件。
```
