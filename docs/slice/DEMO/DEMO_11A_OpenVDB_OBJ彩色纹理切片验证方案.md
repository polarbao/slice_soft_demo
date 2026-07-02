# DEMO_11A_OpenVDB_OBJ彩色纹理切片验证方案

> 文档版本：v0.1
> 文档状态：DEMO / Stage 11A
> 生成日期：2026-07-02

## 1. Goal

验证 `model/obj` 标准 OBJ 彩色纹理模板可用于：

```text
legacy 一键切片；
OpenVDB experimental diagnostic；
后续 OpenVDB production candidate 切片。
```

## 2. Fixture

标准 fixture：

```text
model/obj/MF_aishen_damuzhi_L_tx02.obj
model/obj/MF_aishen_damuzhi_L_tx02.mtl
model/obj/T_aishen_damuzhi_L_tx02.png
```

legacy config：

```text
samples/configs/obj_standard/standard_obj_texture_legacy.json
```

legacy package：

```text
output/ObjStandardTemplateLegacy
```

## 3. Demo Cases

### Case 1：模型导入检查

命令：

```powershell
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --inspect-model
```

期望：

```text
format = obj
vertices > 0
triangles > 0
autoOrient 输出合理
不报 model file does not exist
```

### Case 2：legacy 标准模板切片

命令：

```powershell
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json
.\build\Debug\rip_reader_test.exe --package output\ObjStandardTemplateLegacy --summary
```

期望：

```text
生成 manifest.json
生成 layers/*.tiff
生成 reports/texture_report.json
rip_reader_test PASS
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
```

### Case 3：UI 一键导入模型

操作：

```text
打开 slicer_debug_ui；
点击“导入模型并切片”；
选择 model/obj/MF_aishen_damuzhi_L_tx02.obj；
等待切片完成；
查看层预览和报告。
```

期望：

```text
自动生成 output/ui_sessions/<name>/slice_config.generated.json；
自动加载 output/ui_sessions/<name>/package；
层预览可查看 texture_rgb；
报告页可查看 texture_report。
```

### Case 4：OpenVDB 诊断

命令：

```powershell
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --experimental-openvdb-shell --admission-mode diagnostic_only --experimental-report output\ObjStandardTemplateOpenVdbDiagnostic\reports\experimental_openvdb_shell_report.json
```

期望：

```text
生成 p0.experimental_openvdb_shell_cli_report.1；
productionPackageWritten = false；
productionAllowed = false；
status = diagnostic_only；
UI 能读取该 report。
```

### Case 5：OpenVDB candidate 验收（后续实现后启用）

命令占位：

```powershell
.\scripts\run_11a_obj_openvdb_candidate_tests.ps1
```

期望：

```text
strict_closed 通过才写 candidate package；
rip_reader_test PASS；
texture fidelity sampledPixels > 0；
fallbackPixels / uvOutOfRangePixels 有稳定统计；
legacy package 与 OpenVDB candidate 均可在 UI 中查看。
```

## 4. Demo Exit Criteria

Stage 11A 进入 Stage 12 前至少满足：

```text
Case 1 PASS；
Case 2 PASS；
Case 3 可手工演示；
Case 4 PASS；
OpenVDB candidate 的实现任务已拆分并排期。
```

若 Stage 11A 同时完成 OpenVDB candidate，则 Case 5 也必须 PASS。
