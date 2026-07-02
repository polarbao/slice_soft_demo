# DOC_ANALYSIS_OpenVDB切片功能当前不可用原因

> 文档版本：v0.1  
> 文档状态：Analysis / Evidence  
> 生成日期：2026-07-02  
> 适用范围：解释当前 UI / CLI 中 OpenVDB 相关能力为何不能作为正式切片流程使用

---

## 1. 结论摘要

当前 OpenVDB 相关能力不是“完全没有开发”，也不是“preview 图片生成失败”。

当前真实状态是：

```text
OpenVDB 依赖接入、实验诊断、surface-shell 原型、纹理转移服务契约、准入门禁和报告 schema 已完成；
OpenVDB 正式生产切片链路未完成；
UI 的“导入模型并 OpenVDB 诊断”按钮只运行 diagnostic report，不写 production RGBWSV package；
因此不会生成 layers/、preview/、manifest.json。
```

换句话说，09 / 09P 阶段完成的是 OpenVDB experimental / diagnostic / hardening 能力，不是把 OpenVDB 切换为可直接使用的生产切片引擎。

---

## 2. 用户问题拆解

当前疑问：

```text
为什么当前 OpenVDB 相关切片功能不可用？
09 阶段不是已经完成 OpenVDB 开发了吗？
是功能没开发完，还是环境/preview 的问题？
是否需要继续输出改造文档？
```

本分析把问题拆成四层：

```text
1. OpenVDB 依赖是否可用；
2. OpenVDB CLI 是否执行 surface shell / texture transfer；
3. OpenVDB 是否写 RGBWSV production package；
4. UI 按钮是否触发生产切片流程。
```

---

## 3. 当前证据

### 3.1 默认构建轨道 OpenVDB 关闭

当前默认 build：

```text
build/CMakeCache.txt
USE_OPENVDB:BOOL=OFF
```

这符合项目红线：

```text
OpenVDB optional；
OpenVDB disabled by default；
legacy production path 不被替换。
```

因此默认 UI 使用的 `build/Debug/slicer_cli.exe` 即使运行 OpenVDB diagnostic，也会在 report 中记录 `OPENVDB_UNAVAILABLE`。

### 3.2 UI 按钮是 diagnostic，不是切片

UI 按钮名称：

```text
导入模型并 OpenVDB 诊断
```

当前代码路径：

```text
MainWindow::OnImportModelOpenVdbDiagnostic
-> MainWindow::RunOpenVdbDiagnostic
-> slicer_cli --experimental-openvdb-shell --admission-mode diagnostic_only --experimental-report <path>
```

该路径不会调用正常切片入口：

```text
RunGeneratedConfig
-> slicer_cli --config <path>
-> run_slicer
```

并且 UI 中 `RunOpenVdbDiagnostic` 会清空 `pending_package_`，因此不会加载输出包。

### 3.3 CLI experimental path 明确禁止写生产包

`apps/slicer_cli/main.cpp` 的 experimental OpenVDB path 中存在硬性安全设置：

```text
config.experimental.openvdb_pipeline.write_production_rgbwsv = false
EXPERIMENTAL_CLI_DIAGNOSTIC_ONLY
surfaceShell.generated = false
textureTransfer.executed = false
materialComposer.executed = false
legacyPath.productionPackageWritten = false
productionPackageWritten = false
writeProductionRgbwsv = false
```

该 report 的安全不变量来自：

```text
p0.experimental_openvdb_shell_cli_report.1
```

因此当前 OpenVDB CLI diagnostic 的职责是：

```text
检查 OpenVDB 可用性；
输出 topology/admission 诊断；
生成实验报告；
不写 TIFF；
不写 manifest；
不生成 preview。
```

### 3.4 配置门禁已存在，但不是写包实现

当前配置校验已经支持未来 OpenVDB candidate 的关键开关：

```text
texture.applyMode = surface_shell_from_sdf
experimental.openvdbPipeline.enabled = true
experimental.openvdbPipeline.engine = openvdb
experimental.openvdbPipeline.writeProductionRgbwsv = true
```

但门禁规则明确要求：

```text
surface_shell_from_sdf 必须显式启用 OpenVDB；
writeProductionRgbwsv 必须由 ProductionAdmissionPolicy 控制；
writeProductionRgbwsv 不能在 diagnostic_only / warn_and_attempt / repair_then_strict 下运行；
writeProductionRgbwsv 需要 admissionMode=strict_closed。
```

这说明当前已完成的是“禁止误写包”和“候选切片前置门禁”，不是完整的 OpenVDB production package writer。

---

## 4. 09 / 09P 阶段到底完成了什么

### 4.1 已完成能力

根据 `REPORT_09P_R2_OpenVDB实验生产管线Hardening当前状态.md`，09P-R2 已完成：

```text
experimental report schema；
ProductionAdmissionPolicy；
topology admission gate；
OpenVDB OFF / ON CI matrix；
Qt UI 读取 experimental report；
service data contract；
golden / downstream output contract / texture fidelity 容器；
OpenVDB unavailable / blocker / warning 的稳定报告。
```

这些能力的价值是：

```text
让 OpenVDB experimental path 可解释；
让失败原因可回归；
让 UI 可展示 OpenVDB 状态；
防止 diagnostic output 被误当 production output。
```

### 4.2 未完成能力

09P-R2 明确非目标包括：

```text
不替代 legacy slicer_cli production path；
不从 experimental path 写真实 OBJ/3MF production RGBWSV TIFF；
不默认启用 OpenVDB；
不让 OpenVDB 成为强制依赖。
```

因此以下能力仍未完成：

```text
OpenVDB production RGBWSV package writer branch；
OBJ / 3MF surface-shell 正式切片；
strict admission 通过后的 package 写出；
OpenVDB per-layer RGBWSV layer list；
OpenVDB package preview 生成；
rip_reader_test 对 OpenVDB package 的 golden 验收；
真实模型 topology repair_then_strict。
```

---

## 5. 为什么会产生“09 已完成但现在不可用”的误解

原因是“OpenVDB 开发完成”在历史阶段里指向多个不同层级：

| 层级 | 当前状态 | 是否等于可切片 |
|---|---|---|
| 依赖锁定 / Smoke | 已完成 | 否 |
| OpenVDB SDF / surface-shell 原型 | 已完成 | 否 |
| 真实模型诊断 / report | 已完成 | 否 |
| topology admission gate | 已完成 | 否 |
| UI 读取 diagnostic report | 已完成 | 否 |
| OpenVDB candidate 配置门禁 | 已完成 | 否 |
| OpenVDB production RGBWSV 写包 | 未完成 | 是，必须完成 |
| OpenVDB preview / RIP / golden 验收 | 未完成 | 是，必须完成 |

所以当前不可用不是单点 bug，而是阶段边界：

```text
09 / 09P 完成了“OpenVDB 可诊断、可准入判断、可防误用”；
尚未完成“OpenVDB 可正式生成切片包”。
```

---

## 6. 与 Preview 的关系

当前 OpenVDB diagnostic 不生成 preview 的原因：

```text
preview 是 production package 的派生产物；
production package 需要 manifest.json、layers/*.tiff、reports/*.json；
diagnostic path 不写 production package；
因此没有 preview 源数据。
```

这不是 preview writer 本身失败。

如果未来 OpenVDB candidate 写包完成，preview 生成应复用现有生产包规则：

```text
preview.enabled；
preview.channels；
preview.interval；
preview.onlyNonEmptyLayers；
preview.pseudoColors。
```

---

## 7. 标准 OBJ 模板的额外阻断

Stage 11A 已把 `model/obj` 登记为标准 OBJ 彩色纹理功能性测试模板。

该模板走 legacy path 已可生成 production package。

但 OpenVDB strict_closed probe 当前被拓扑门禁阻断，已知 blocker 包括：

```text
MESH_DUPLICATE_FACES
MESH_OPPOSITE_DUPLICATE_FACES
MESH_BOUNDARY_EDGES
MESH_NON_MANIFOLD_EDGES
```

这意味着即使实现 OpenVDB candidate writer，也不能绕过 admission gate 直接对该真实模板写包。必须满足以下任一条件：

```text
1. 新增 strict_closed PASS 的 OBJ 彩色纹理 fixture；
2. 实现 mesh repair / repair_then_strict，并在 repair 后重新诊断；
3. 将该模型仅作为 diagnostic / legacy 验收模型，不作为 OpenVDB production candidate PASS 模型。
```

---

## 8. 当前是否需要进一步改造文档

需要，但不应重复 09P-R2。

当前已有文档：

```text
docs/slice/DOC/DOC_DECISION_11A_Stage12前置_OpenVDB_OBJ彩色纹理切片计划.md
docs/slice/PRD/PRD_11A_OpenVDB_OBJ彩色纹理切片前置计划.md
docs/slice/DEV/DEV_11A_OpenVDB_OBJ彩色纹理切片改造计划.md
docs/slice/DEMO/DEMO_11A_OpenVDB_OBJ彩色纹理切片验证方案.md
docs/codex_task/current/TASKS_11A_OpenVDB_OBJ彩色纹理切片前置任务清单.md
```

这些文档已经覆盖“为什么要做 OpenVDB OBJ 彩色纹理前置计划”和“候选路线怎么做”。

但如果目标从“诊断与前置计划”升级为“让 UI 按钮真正完成 OpenVDB 切片并生成 preview”，建议新增一个小阶段文档包：

```text
Stage 11A-R1：OpenVDB Candidate RGBWSV 写包与 Preview 收口
```

建议新增文档：

```text
docs/slice/DOC/DOC_DECISION_11A_R1_OpenVDB候选切片写包与Preview收口.md
docs/slice/PRD/PRD_11A_R1_OpenVDB候选切片写包与Preview收口.md
docs/slice/DEV/DEV_11A_R1_OpenVDBCandidatePipeline_RGBWSVWriter设计.md
docs/slice/DEMO/DEMO_11A_R1_OpenVDB候选包与Preview验证方案.md
docs/codex_task/current/TASKS_11A_R1_OpenVDB候选切片写包任务清单.md
```

---

## 9. 建议的改造目标

### 9.1 UI 层目标

保留现有按钮：

```text
导入模型并切片
导入模型并 OpenVDB 诊断
```

新增第三个按钮：

```text
导入模型并 OpenVDB 候选切片
```

按钮行为：

```text
默认隐藏或禁用，只有检测到 OpenVDB ON build 后启用；
运行 strict_closed admission；
productionAllowed=false 时只显示报告，不写包；
productionAllowed=true 时写 candidate RGBWSV package；
成功后加载 layer preview / overlay preview；
UI 明确显示 Candidate / Experimental，不伪装为默认生产路径。
```

### 9.2 CLI / Pipeline 目标

新增显式 pipeline 分支：

```text
LegacySlicePipeline
OpenVdbDiagnosticPipeline
OpenVdbCandidatePipeline
```

其中 `OpenVdbCandidatePipeline` 必须：

```text
只在 USE_OPENVDB=ON 下可执行；
只在 strict_closed 且无 blocker 时写 RGBWSV；
复用 p0.rgbwsv.2 输出协议；
写 manifest / layers / reports / preview；
失败时只写 report，不写半成品 package。
```

### 9.3 验收目标

候选能力完成后至少需要：

```text
OpenVDB OFF 默认轨道仍通过；
OpenVDB ON smoke 通过；
strict_closed PASS fixture 生成 candidate package；
rip_reader_test PASS；
LayerPreview / OverlayPreview 可读取 candidate package；
真实标准 OBJ 若 topology blocker 未修复，则保持“正确阻断”；
legacy 标准 OBJ 输出不退化。
```

---

## 10. 推荐下一步

当前若只是回答“为什么不可用”，本分析文档已经足够。

如果下一步要真正开发 OpenVDB 切片按钮和 preview 输出，建议先做：

```text
1. 新增 Stage 11A-R1 文档包；
2. 明确 candidate writer 是 experimental candidate，不替代 legacy；
3. 新增 strict_closed PASS 的小型 OBJ 彩色纹理 fixture；
4. 实现 OpenVdbCandidatePipeline 最小写包；
5. 再考虑真实 model/obj 模板的 mesh repair / repair_then_strict。
```

不建议直接把现有“OpenVDB 诊断”按钮改成写包按钮。

原因：

```text
会破坏 09P-R2 report schema 的安全不变量；
会混淆 diagnostic 与 production candidate；
会在真实模型 topology blocker 未解决时诱发错误输出；
会增加 legacy production path 回归风险。
```

