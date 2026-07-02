# DEV_11A_OpenVDB_OBJ彩色纹理切片改造计划

> 文档版本：v0.1
> 文档状态：DEV / Stage 11A
> 生成日期：2026-07-02
> 阶段定位：Stage 12 前置技术计划

## 1. Goal

以 `model/obj` 标准 OBJ/MTL/PNG 模板为真实功能性测试模型，完成 legacy 与 OpenVDB 两条 OBJ 彩色纹理处理路线的开发计划。

## 2. Current State

当前已具备：

```text
OBJ / MTL / Texture importer；
legacy relief_heightfield 纹理采样；
UI 一键导入模型并切片；
UI OpenVDB diagnostic 按钮；
OpenVDB optional experimental service；
ProductionAdmissionPolicy；
SurfaceShellTextureService / MaterialChannelComposer 契约；
experimental OpenVDB report schema。
```

当前不具备：

```text
OpenVDB production RGBWSV package writer branch；
OBJ 彩色纹理 surface-shell 正式切片；
strict admission 后的 package 写出；
真实 OBJ 模板的 OpenVDB golden package；
OpenVDB candidate UI 入口。
```

## 3. 标准模板接入

新增 legacy 标准配置：

```text
samples/configs/obj_standard/standard_obj_texture_legacy.json
```

对应输出：

```text
output/ObjStandardTemplateLegacy
```

标准功能性配置使用：

```text
modelTransform.scale = [0.8, 0.8, 0.8]
autoOrient.maxHeightMm = 6.0
```

这是测试 profile 的标准化处理，不代表真实模型尺寸验收结论。

UI 场景索引新增：

```text
obj_standard_template_legacy
```

## 4. Proposed Approach

### 4.1 保持两条路径隔离

```text
Legacy path:
  slicer_cli --config <legacy_config>
  -> run_slicer
  -> RGBWSV package

OpenVDB diagnostic path:
  slicer_cli --experimental-openvdb-shell
  -> diagnostic report only

OpenVDB candidate path:
  future explicit config + explicit CLI/UI action
  -> strict admission
  -> RGBWSV candidate package
```

OpenVDB candidate 不得替换 legacy default。

### 4.2 新增配置策略

未来新增：

```json
{
  "texture": {
    "enabled": true,
    "applyMode": "surface_shell_from_sdf"
  },
  "experimental": {
    "openvdbPipeline": {
      "enabled": true,
      "engine": "openvdb",
      "admissionMode": "strict_closed",
      "failurePolicy": "fail_fast",
      "writeProductionRgbwsv": true
    }
  }
}
```

配置校验规则：

```text
surface_shell_from_sdf 只能在 OpenVDB enabled 时使用；
writeProductionRgbwsv=true 必须 admissionMode=strict_closed；
OpenVDB unavailable 时不得写 package；
diagnostic_only / warn_and_attempt 不得写 production package。
```

### 4.3 Pipeline 改造

建议新增显式分支：

```text
RunSlicer
  -> DetectPipelineMode(config)
  -> LegacySlicePipeline
  -> OpenVdbSurfaceShellCandidatePipeline
```

完成标准：

```text
legacy 路径原样保留；
OpenVDB candidate 使用独立函数/类承载；
OpenVDB candidate 失败时不污染 legacy 输出；
所有失败通过 ValidationIssue / report 表达。
```

### 4.4 SurfaceShell texture transfer

OpenVDB OBJ 彩色纹理候选切片需要：

```text
TriangleMeshData 保留 triangle -> material -> UV；
OpenVDB shell sample 找最近 triangle；
nearest triangle barycentric -> UV；
UV -> texture RGB；
RGB 写入 shell surface buffer；
interior / support / W / V 由 MaterialChannelComposer 合成。
```

Seam 策略：

```text
命中哪个 triangle，就使用该 triangle 的 material 和 UV；
不跨 UV seam 平均；
不跨 material seam 混色。
```

### 4.5 Output package

OpenVDB candidate 输出仍必须复用：

```text
p0.rgbwsv.2
R G B W S V
uint8
black_is_print
manifest/layers/reports/preview
```

新增 report：

```text
reports/openvdb_candidate_report.json
reports/texture_fidelity_report.json
reports/production_admission_report.json
```

## 5. Steps

### Task 11A-1：标准 OBJ 模板基线

完成：

```text
model/obj/README.md；
standard_obj_texture_legacy.json；
场景索引；
inspect-model 验证；
```

### Task 11A-2：UI 按钮路径收口

完成：

```text
导入模型并切片；
导入模型并 OpenVDB 诊断；
按钮状态/报告路径/输出包路径展示；
操作手册。
```

### Task 11A-3：OpenVDB candidate 配置与 admission 规则

完成：

```text
surface_shell_from_sdf 配置枚举；
writeProductionRgbwsv gate；
OpenVDB unavailable fail-fast；
diagnostic_only 禁止写 package 测试。
```

### Task 11A-4：OpenVDB candidate pipeline 原型

完成：

```text
OpenVDB level set + shell mask；
OBJ UV texture transfer；
MaterialChannelComposer 合成 RGBWSV；
candidate package writer；
candidate report。
```

### Task 11A-5：标准模板验收

完成：

```text
model/obj 标准模板 legacy package PASS；
OpenVDB diagnostic report PASS；
OpenVDB candidate package PASS；
rip_reader_test PASS；
texture fidelity 指标 PASS；
legacy regression PASS。
```

## 6. Risks

| 风险 | 控制 |
|---|---|
| 真实 OBJ 拓扑阻断 OpenVDB strict admission | 先 diagnostic，必要时进入 mesh repair 专项 |
| OpenVDB ON 环境不可用 | OpenVDB OFF 默认轨道必须继续可用 |
| texture seam 串色 | triangle-local UV/material 策略，不跨 seam 混合 |
| candidate 写出误作 production | report 和 UI 标识 production candidate，必须 strict gate |
| 性能不可控 | 标准模板记录 timing/memory，Release benchmark 作为后续门槛 |

## 7. Validation

Stage 11A 最小验证：

```powershell
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --inspect-model
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json
.\build\Debug\rip_reader_test.exe --package output\ObjStandardTemplateLegacy --summary
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --experimental-openvdb-shell --admission-mode diagnostic_only --experimental-report output\ObjStandardTemplateOpenVdbDiagnostic\reports\experimental_openvdb_shell_report.json
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

OpenVDB candidate 完成后的新增验证：

```powershell
.\scripts\run_openvdb_smoke.ps1
.\scripts\run_surface_shell_real_model_tests.ps1
.\scripts\run_surface_shell_texture_tests.ps1
.\scripts\run_11a_obj_openvdb_candidate_tests.ps1
```

## 8. Rollback

若 OpenVDB candidate 出现不可控风险：

```text
保留 legacy 标准模板；
禁用 OpenVDB candidate UI 入口；
保留 OpenVDB diagnostic；
不影响 p0.rgbwsv.2 legacy 输出。
```
