# PRD_11A_R1_OpenVDB候选切片写包与Preview收口

> 文档版本：v0.1  
> 文档状态：PRD / Stage 11A-R1  
> 生成日期：2026-07-02

---

## 1. Goal

让 OpenVDB 从“实验诊断能力”推进到“候选切片能力”：

```text
输入 OBJ / 3MF 模型；
通过 OpenVDB surface-shell 生成候选几何切片；
将纹理颜色转为 RGB；
合成 RGBWSV；
在 strict_closed 准入通过后写 p0.rgbwsv.2 candidate package；
生成 preview；
通过 RIP reader 和 UI 层预览验收。
```

本阶段目标不是替换 legacy 默认路径，而是建立可验证、可回退、可比较的 OpenVDB Candidate 路径。

---

## 2. User Stories

### 2.1 开发人员执行 OpenVDB Candidate

作为切片开发人员，我希望通过 CLI 或 UI 运行 OpenVDB candidate 切片，以便判断 OpenVDB 是否能生成与 legacy 协议兼容的 RGBWSV package。

验收：

```text
必须显式启用 OpenVDB；
必须 strict_closed admission；
成功时生成 manifest / layers / reports / preview；
失败时只生成 report，不写半成品 package。
```

### 2.2 调试人员比较 Legacy 与 OpenVDB

作为调试人员，我希望 UI 能明确显示当前输出来自 Legacy 还是 OpenVDB Candidate，以便比较 RGB、支撑、白墨、光油和 preview 差异。

验收：

```text
UI 有明确 candidate 标识；
LayerPreview 可读取 candidate package；
OverlayPreview 可读取 candidate preview；
报告区显示 admission status 和 blockerCodes。
```

### 2.3 产品判断是否可逐步替换

作为产品/架构负责人，我希望基于真实验收数据判断 OpenVDB 是否可以替换部分 legacy 切片能力，而不是基于“09 阶段完成”直接替换。

验收：

```text
有 candidate package golden；
有 texture fidelity 统计；
有性能和内存统计；
有失败模型 blocker 分类；
有 legacy 回归对比。
```

---

## 3. Scope

本阶段覆盖：

```text
OpenVDB candidate pipeline 显式入口；
strict admission gate；
surface shell / interior / support 到 layer buffer 的映射；
texture RGB 到 surface shell；
MaterialChannelComposer 到 RGBWSV；
candidate package writer；
preview / report / manifest 输出；
CLI 和 UI candidate 入口；
测试 fixture / golden / smoke。
```

---

## 4. Non-goals

本阶段不做：

```text
默认启用 OpenVDB；
删除 legacy path；
让 OpenVDB 成为强制依赖；
自动修复所有真实模型 topology；
RIP 半色调；
设备通信；
喷头 bitstream；
修改 p0.rgbwsv.2 协议。
```

---

## 5. Product Requirements

### P0

```text
P0-1：OpenVDB candidate path 必须是显式入口；
P0-2：candidate 写包必须受 strict_closed admission 控制；
P0-3：strict_closed 失败不得写 package；
P0-4：PASS fixture 必须能写 p0.rgbwsv.2 package；
P0-5：candidate package 必须通过 rip_reader_test；
P0-6：candidate package 必须能生成 preview 并在 UI 读取；
P0-7：legacy 标准 OBJ package 不退化。
```

### P1

```text
P1-1：支持 OBJ/MTL/Texture 真实纹理颜色；
P1-2：输出 texture_fidelity_report；
P1-3：输出 openvdb_candidate_report；
P1-4：UI 显示 Legacy / Diagnostic / Candidate 引擎状态；
P1-5：OpenVDB ON / OFF 脚本分层稳定。
```

### P2

```text
P2-1：真实标准 OBJ topology repair_then_strict；
P2-2：3MF texture candidate package；
P2-3：Release benchmark 和内存上限；
P2-4：多模型 candidate 路线。
```

---

## 6. Acceptance

最小验收：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_11a_r1_openvdb_candidate_off_lane.ps1
.\scripts\run_11a_r1_openvdb_candidate_on_lane.ps1 -OpenVdbBuildDir build-openvdb-09p
```

候选包验收：

```powershell
.\build-openvdb-09p\Debug\slicer_cli.exe --config samples\configs\openvdb_candidate\<pass-fixture>.json --openvdb-candidate-slice
.\build-openvdb-09p\Debug\rip_reader_test.exe --package output\<candidate-package> --summary
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\<candidate-package>
```

