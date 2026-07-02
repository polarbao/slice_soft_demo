# TASKS_11A_OpenVDB_OBJ彩色纹理切片前置任务清单

> 文档版本：v0.1
> 文档状态：Codex Task List / Stage 11A
> 生成日期：2026-07-02
> 阶段定位：Stage 12 前置，OBJ 标准模板与 OpenVDB OBJ 彩色纹理候选切片计划

## 1. 总规则

每个任务开始前：

```powershell
git status --short
```

每个任务完成前：

```powershell
git status --short
git diff --check
```

生产安全规则：

```text
不修改 p0.rgbwsv.2；
不修改 RGBWSV channelOrder；
不修改 uint8 / black_is_print；
不默认启用 OpenVDB；
不把 diagnostic_only 输出当作 production package；
不绕过 ProductionAdmissionPolicy；
不让 UI 直接依赖 OpenVDB 类型。
```

## 2. 推荐顺序

```text
11A-0：标准 OBJ 模板登记；
11A-1：legacy 标准模板功能性配置与场景；
11A-2：UI 一键导入与 OpenVDB diagnostic 按钮验证；
11A-3：OpenVDB candidate 配置与 admission gate；
11A-4：OpenVDB surface-shell OBJ texture transfer 原型；
11A-5：candidate RGBWSV package writer；
11A-6：标准模板 golden / RIP / UI 验收；
11A-7：REPORT_11A。
```

## 2.1 当前执行状态

| 任务 | 状态 | 提交 | 已运行验证 |
|---|---|---|---|
| 11A-0：标准 OBJ 模板登记 | DONE | `07f50dd docs(11A): 登记标准OBJ彩色纹理模板` | `Get-ChildItem model\obj`；`Select-String mtllib/vt`；`Get-Content MTL`；`git diff --cached --check` |
| 11A-1：legacy 标准模板功能性配置与场景 | DONE | `4a07147 test(11A): 增加标准OBJ模板legacy配置` | `ConvertFrom-Json`；`slicer_cli --inspect-model`；`git diff --cached --check` |
| 11A-2：UI 一键导入与 OpenVDB diagnostic 按钮验证 | DONE | `cf7f58f docs(11A): 记录UI一键路径验证` | `cmake --build build --config Debug --target slicer_debug_ui`；`slicer_debug_ui.exe --self-test`；`slicer_cli --experimental-openvdb-shell --admission-mode diagnostic_only`；`git diff --check` |
| 11A-3：OpenVDB candidate 配置与 admission gate | DONE | `d5f3cb9 feat(11A): 增加OpenVDB候选配置门禁` | `cmake --build build --config Debug --target experimental_config_unit_tests`；`experimental_config_unit_tests.exe`；`cmake --build build --config Debug`；`ctest --test-dir build -C Debug --output-on-failure`；`slicer_cli --experimental-openvdb-shell --admission-mode diagnostic_only` |
| 11A-4：OpenVDB surface-shell OBJ texture transfer 原型 | DONE | `b66187d test(11A): 覆盖标准OBJ壳层纹理输入` | `cmake --build build --config Debug --target surface_shell_real_model_unit_tests`；`surface_shell_real_model_unit_tests.exe`；`scripts/run_surface_shell_texture_tests.ps1`；`scripts/run_surface_shell_real_model_tests.ps1` |
| 11A-5：candidate RGBWSV package writer | BLOCKED_BY_TOPOLOGY | `99f77c4 docs(11A): 记录OpenVDB候选写出阻断` | `surface_shell_real_model_demo --config samples\configs\obj_standard\standard_obj_texture_legacy.json --mesh-policy strict_closed` |
| 11A-6：标准模板 golden / RIP / UI 验收 | DONE_LIMITED | `a7ba299 test(11A): 增加标准OBJ受限验收脚本` | `scripts/run_11a_obj_standard_tests.ps1`；`scripts/run_11a_obj_openvdb_candidate_tests.ps1` |
| 11A-7：REPORT_11A | DONE | 本次报告提交 | `git status --short`；`git diff --check` |

11A-2 验证结论：

```text
Qt 调试 UI 可构建；
UI self-test PASS；
标准 OBJ 模板的 OpenVDB diagnostic report 可生成；
diagnostic report 明确 productionPackageWritten=false、productionAllowed=false；
当前环境 OpenVDB unavailable 时仍只输出诊断，不写 production RGBWSV package。
```

11A-3 验证结论：

```text
texture.applyMode=surface_shell_from_sdf 已进入配置校验；
surface_shell_from_sdf 必须显式启用 experimental.openvdbPipeline 且 engine=openvdb；
writeProductionRgbwsv=true 必须保持 enabled=true、engine=openvdb；
writeProductionRgbwsv=true 在 diagnostic_only / warn_and_attempt / repair_then_strict 下会产生 EXPERIMENTAL_RGBWSV_REQUIRES_STRICT_ADMISSION error；
Debug 全量构建和 CTest 通过；
标准 OBJ diagnostic 仍明确 productionPackageWritten=false、productionAllowed=false。
```

11A-4 验证结论：

```text
标准 OBJ 模板已进入 surface_shell_real_model_unit_tests；
标准 OBJ 的 texcoord、faces_with_uv、texture material、adapted triangle UV/material attributes 均有测试覆盖；
OpenVDB OFF 默认轨道下小型 OBJ transfer smoke 会 SKIP 并保持 PASS；
OpenVDB ON 轨道下 run_surface_shell_texture_tests.ps1 与 run_surface_shell_real_model_tests.ps1 均通过；
open mesh / non-manifold negative cases 在脚本中按预期被 strict_closed 拒绝；
CMake OpenVDB ON 轨道存在 Boost CMP0167 dev warning，但未导致验证失败。
```

11A-5 阻断结论：

```text
标准 OBJ 模板执行 OpenVDB strict_closed probe 失败；
命令：.\build-openvdb-09b-r1\Debug\surface_shell_real_model_demo.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --voxel-mm 0.10 --shell-mm 0.15 --mesh-policy strict_closed --output output\ObjStandardTemplateOpenVdbProbe
退出码：1；
报告：output/ObjStandardTemplateOpenVdbProbe/reports/surface_shell_texture_report.json；
阻断码：MESH_DUPLICATE_FACES、MESH_OPPOSITE_DUPLICATE_FACES、MESH_BOUNDARY_EDGES、MESH_NON_MANIFOLD_EDGES；
productionAdmission.productionAllowed=false；
结论：11A-5 不允许对该标准 OBJ 写 candidate RGBWSV package，否则会违反 strict admission gate。
```

11A-5 后续前置条件：

```text
完成 mesh repair / repair_then_strict 正式实现；或
新增一个 topology strict_closed PASS 的标准 OBJ 彩色纹理模板；或
将 candidate writer 先限定在已有 closed small fixture，并明确不得代表 model/obj 标准模板通过。
```

11A-6 验收结论：

```text
新增 scripts/run_11a_obj_standard_tests.ps1；
新增 scripts/run_11a_obj_openvdb_candidate_tests.ps1；
新增 tests/golden/expected/11a_obj_standard_contract.json；
标准 OBJ legacy package 通过 RIP summary；
标准 OBJ texture_report sampledPixels > 0 且 missingTextures=0；
标准 OBJ slice_report modelPixels/supportPixels 均大于 0；
UI self-test PASS；
OpenVDB diagnostic PASS 且不写 production package；
OpenVDB candidate gate 脚本 PASS，确认该标准 OBJ 被 strict_closed 正确阻断且不写 candidate manifest。
```

## 3. Task 11A-0：标准 OBJ 模板登记

目标：

```text
确认 model/obj 为标准 OBJ 彩色纹理功能性测试模板目录；
记录 OBJ / MTL / PNG 文件关系；
不移动真实模型文件。
```

输出：

```text
model/obj/README.md
docs/slice/DOC/DOC_DECISION_11A_Stage12前置_OpenVDB_OBJ彩色纹理切片计划.md
```

验证：

```powershell
Get-ChildItem model\obj
Select-String -Path model\obj\MF_aishen_damuzhi_L_tx02.obj -Pattern "mtllib|vt "
Get-Content model\obj\MF_aishen_damuzhi_L_tx02.mtl
git diff --check
```

## 4. Task 11A-1：legacy 标准模板功能性配置与场景

目标：

```text
新增标准 OBJ 模板 legacy 配置；
加入 UI 场景索引；
验证模型路径、MTL、贴图可以被当前 importer 解析。
```

输出：

```text
samples/configs/obj_standard/standard_obj_texture_legacy.json
samples/scenarios/slicer_scenarios.json
```

验证：

```powershell
Get-Content -Raw samples\configs\obj_standard\standard_obj_texture_legacy.json | ConvertFrom-Json | Out-Null
Get-Content -Raw samples\scenarios\slicer_scenarios.json | ConvertFrom-Json | Out-Null
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --inspect-model
git diff --check
```

## 5. Task 11A-2：UI 一键导入与 OpenVDB diagnostic 按钮验证

目标：

```text
确认 UI 可通过按钮执行 non-OpenVDB legacy 一键切片；
确认 UI 可通过按钮执行 OpenVDB diagnostic；
确认两条路径文案和输出路径不混淆。
```

输出：

```text
apps/slicer_debug_ui/MainWindow.*
docs/user_guides/QT_DEBUG_UI_操作手册.md
```

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --experimental-openvdb-shell --admission-mode diagnostic_only --experimental-report output\ObjStandardTemplateOpenVdbDiagnostic\reports\experimental_openvdb_shell_report.json
git diff --check
```

## 6. Task 11A-3：OpenVDB candidate 配置与 admission gate

目标：

```text
新增 surface_shell_from_sdf 配置枚举；
新增 writeProductionRgbwsv gate；
确保 OpenVDB unavailable / diagnostic_only / warn_and_attempt 不写 package；
补单测。
```

建议修改：

```text
src/slicer_core/config.*
src/slicer_core/diagnostics/ProductionAdmissionPolicy.*
tests/unit/experimental_config 或新增 11A 单测
```

验证：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --experimental-openvdb-shell --admission-mode diagnostic_only --experimental-report output\ObjStandardTemplateOpenVdbDiagnostic\reports\experimental_openvdb_shell_report.json
git diff --check
```

## 7. Task 11A-4：OpenVDB surface-shell OBJ texture transfer 原型

目标：

```text
基于 OBJ/MTL/Texture 和 TriangleTextureInfo；
执行 OpenVDB shell sample -> nearest triangle -> UV -> texture RGB；
输出稳定 texture transfer stats 和 ValidationIssue。
```

建议修改：

```text
src/slicer_core/materials/texture_application/*
src/slicer_core/geometry/*
tests/unit/surface_shell_texture*
```

验证：

```powershell
cmake --build build --config Debug
.\scripts\run_surface_shell_texture_tests.ps1
.\scripts\run_surface_shell_real_model_tests.ps1
git diff --check
```

## 8. Task 11A-5：candidate RGBWSV package writer

目标：

```text
在 strict_closed 且无 blocker 时，将 OpenVDB candidate buffer 写为 p0.rgbwsv.2 package；
复用 RGBWSV writer / manifest / report 规则；
不影响 legacy run_slicer。
```

建议修改：

```text
src/slicer_core/pipeline/*
src/slicer_core/output/*
src/slicer_core/slicer.cpp 或 wrapper API
apps/slicer_cli/main.cpp
```

验证：

```powershell
cmake --build build --config Debug
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_openvdb_candidate.json
.\build\Debug\rip_reader_test.exe --package output\ObjStandardTemplateOpenVdbCandidate --summary
ctest --test-dir build -C Debug --output-on-failure
git diff --check
```

## 9. Task 11A-6：标准模板 golden / RIP / UI 验收

目标：

```text
为 model/obj 标准模板建立 legacy 与 OpenVDB candidate 验收脚本；
补 texture fidelity / package summary / UI smoke。
```

建议输出：

```text
scripts/run_11a_obj_standard_tests.ps1
scripts/run_11a_obj_openvdb_candidate_tests.ps1
tests/golden/expected/11a_obj_standard_*.json
```

验证：

```powershell
.\scripts\run_11a_obj_standard_tests.ps1
.\scripts\run_11a_obj_openvdb_candidate_tests.ps1
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
git diff --check
```

## 10. Task 11A-7：REPORT_11A

目标：

```text
生成 Stage 11A 状态报告；
记录已完成能力、未完成风险、验证命令、是否允许进入 Stage 12。
```

输出：

```text
docs/slice/REPORT/REPORT_11A_OpenVDB_OBJ彩色纹理切片前置当前状态.md
```

验证：

```powershell
git status --short
git diff --check
```
