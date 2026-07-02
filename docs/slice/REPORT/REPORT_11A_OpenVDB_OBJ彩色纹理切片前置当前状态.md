# REPORT_11A_OpenVDB_OBJ彩色纹理切片前置当前状态

> 文档版本：v0.1
> 文档状态：Stage Report / 11A
> 生成日期：2026-07-02
> 分支：`main`

---

## 1. 阶段目标

Stage 11A 是 Stage 12 前置阶段，目标是在不改变现有生产切片协议的前提下，为“标准 OBJ 彩色纹理模型”和“OpenVDB OBJ 表面壳层纹理候选路线”建立可验证入口：

```text
登记 model/obj 作为标准 OBJ 彩色纹理功能性测试模板；
建立 legacy 标准模板配置和 UI 场景；
确认 UI 一键切片与 OpenVDB diagnostic 路径可执行；
为 OpenVDB candidate 写出增加配置门禁和 strict admission 约束；
补 surface-shell OBJ texture transfer 原型覆盖；
建立标准模板 legacy / candidate-gate 验收脚本；
记录当前是否允许进入 Stage 12。
```

本阶段明确不做：

```text
不把 OpenVDB 默认启用；
不绕过 ProductionAdmissionPolicy；
不把 diagnostic_only 输出当 production package；
不在 strict_closed 失败时强行写 candidate RGBWSV package；
不修改 p0.rgbwsv.2 / RGBWSV channelOrder / uint8 / black_is_print。
```

---

## 2. 已完成任务

| Task | 状态 | 主要提交 |
|---|---|---|
| 11A-0 标准 OBJ 模板登记 | 已完成 | `07f50dd docs(11A): 登记标准OBJ彩色纹理模板` |
| 11A-1 legacy 标准模板功能性配置与场景 | 已完成 | `4a07147 test(11A): 增加标准OBJ模板legacy配置` |
| 11A-2 UI 一键导入与 OpenVDB diagnostic 按钮验证 | 已完成 | `cf7f58f docs(11A): 记录UI一键路径验证` |
| 11A-3 OpenVDB candidate 配置与 admission gate | 已完成 | `d5f3cb9 feat(11A): 增加OpenVDB候选配置门禁` |
| 11A-4 OpenVDB surface-shell OBJ texture transfer 原型 | 已完成 | `b66187d test(11A): 覆盖标准OBJ壳层纹理输入` |
| 11A-5 candidate RGBWSV package writer | 被拓扑门禁阻断 | `99f77c4 docs(11A): 记录OpenVDB候选写出阻断` |
| 11A-6 标准模板 golden / RIP / UI 验收 | 受限完成 | `a7ba299 test(11A): 增加标准OBJ受限验收脚本` |
| 11A-7 本报告 | 本轮完成 | `REPORT_11A_OpenVDB_OBJ彩色纹理切片前置当前状态.md` |

---

## 3. 新增和关键修改

标准 OBJ 模板：

```text
model/obj/README.md
model/obj/MF_aishen_damuzhi_L_tx02.obj
model/obj/MF_aishen_damuzhi_L_tx02.mtl
model/obj/*.png
```

配置和场景：

```text
samples/configs/obj_standard/standard_obj_texture_legacy.json
samples/scenarios/slicer_scenarios.json
```

正式文档和任务文档：

```text
docs/slice/DOC/DOC_DECISION_11A_Stage12前置_OpenVDB_OBJ彩色纹理切片计划.md
docs/slice/PRD/PRD_11A_OpenVDB_OBJ彩色纹理切片前置.md
docs/slice/DEV/DEV_11A_OpenVDB_OBJ彩色纹理切片前置设计.md
docs/slice/DEMO/DEMO_11A_OpenVDB_OBJ彩色纹理切片前置验证.md
docs/slice/ROADMAP/ROADMAP_11A_Stage12前置_OpenVDB_OBJ彩色纹理切片计划.md
docs/codex_task/current/TASKS_11A_OpenVDB_OBJ彩色纹理切片前置任务清单.md
```

验证脚本和 golden：

```text
scripts/run_11a_obj_standard_tests.ps1
scripts/run_11a_obj_openvdb_candidate_tests.ps1
tests/golden/expected/11a_obj_standard_contract.json
```

关键代码和测试：

```text
src/slicer_core/config.cpp
tests/unit/experimental_config/main.cpp
tests/unit/surface_shell_real_model/main.cpp
```

---

## 4. 当前已实现能力

### 4.1 标准 OBJ 彩色纹理模板

`model/obj` 已作为后续 OBJ 彩色纹理切片功能性测试模板目录登记。当前标准模板具备：

```text
OBJ / MTL / PNG 同目录资源；
OBJ 中存在 mtllib、vt 和 faces with UV；
MTL 中存在贴图引用；
当前 importer 可解析该模型并生成 legacy RGBWSV package。
```

legacy 配置：

```text
samples/configs/obj_standard/standard_obj_texture_legacy.json
```

legacy 输出：

```text
output/ObjStandardTemplateLegacy
```

### 4.2 UI 一键切片与 OpenVDB diagnostic 路径

Stage 11 已提供 UI 一键导入 / 一键切片 / OpenVDB 诊断入口；Stage 11A 对标准 OBJ 模板进行了路径验证。

当前结论：

```text
Qt 调试 UI 可构建；
UI self-test 通过；
标准 OBJ legacy 一键路径可通过配置和场景进入；
OpenVDB diagnostic 路径只写诊断 report；
diagnostic_only 下 productionPackageWritten=false；
diagnostic_only 下 productionAllowed=false。
```

### 4.3 OpenVDB candidate 配置门禁

`texture.applyMode` 已支持：

```text
surface_shell_from_sdf
```

配置门禁规则：

```text
surface_shell_from_sdf 必须显式启用 experimental.openvdbPipeline.enabled=true；
surface_shell_from_sdf 必须使用 experimental.openvdbPipeline.engine=openvdb；
writeProductionRgbwsv=true 必须保持 OpenVDB pipeline enabled 且 engine=openvdb；
writeProductionRgbwsv=true 在 diagnostic_only / warn_and_attempt / repair_then_strict 下会产生 EXPERIMENTAL_RGBWSV_REQUIRES_STRICT_ADMISSION error。
```

这表示 OpenVDB candidate RGBWSV 写出只能在 strict admission 满足时继续，不能通过 diagnostic 或 warn 路径偷跑。

### 4.4 surface-shell OBJ texture transfer 原型覆盖

当前已有的 surface-shell 原型链路覆盖：

```text
OpenVDB shell sample；
nearest triangle；
barycentric UV；
texture RGB sample；
texture transfer stats；
strict_closed negative case。
```

Stage 11A 补充了标准 OBJ 模板属性回归，覆盖：

```text
triangles；
texcoords；
faces_with_uv；
texture material；
adapted triangle UV/material attributes。
```

默认 OpenVDB OFF 轨道下，小型 OBJ transfer smoke 会 SKIP 并保持 PASS；OpenVDB ON 轨道下，真实 smoke 可执行并验证 shell texture transfer。

---

## 5. 11A-5 阻断结论

标准 OBJ 模板执行 OpenVDB strict_closed probe 时被拓扑诊断正确阻断。

实际运行命令：

```powershell
.\build-openvdb-09b-r1\Debug\surface_shell_real_model_demo.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --voxel-mm 0.10 --shell-mm 0.15 --mesh-policy strict_closed --output output\ObjStandardTemplateOpenVdbProbe
```

结果：

```text
退出码：1
报告：output/ObjStandardTemplateOpenVdbProbe/reports/surface_shell_texture_report.json
productionAdmission.productionAllowed=false
```

阻断码：

```text
MESH_DUPLICATE_FACES
MESH_OPPOSITE_DUPLICATE_FACES
MESH_BOUNDARY_EDGES
MESH_NON_MANIFOLD_EDGES
```

结论：

```text
当前不能对 model/obj 标准 OBJ 模板写出 OpenVDB candidate RGBWSV package；
如果强行写包，会违反 strict admission gate 和 ProductionAdmissionPolicy；
11A-5 因真实模型拓扑问题被阻断，不应标记为 candidate 写出能力完成。
```

---

## 6. 11A-6 受限验收结论

已新增两个验收脚本：

```text
scripts/run_11a_obj_standard_tests.ps1
scripts/run_11a_obj_openvdb_candidate_tests.ps1
```

`run_11a_obj_standard_tests.ps1` 验证 legacy 标准模板：

```text
slicer_cli / rip_reader_test / slicer_debug_ui 可构建；
标准 OBJ legacy package 可生成；
manifest schema = p0.rgbwsv.2；
bitDepth = 8；
polarity = black_is_print；
channelOrder = R G B W S V；
texture_report sampledPixels > 0；
texture_report missingTextures = 0；
slice_report modelPixels > 0；
slice_report supportPixels > 0；
RIP summary 可读取；
UI self-test 通过；
OpenVDB diagnostic 不写 production package。
```

`run_11a_obj_openvdb_candidate_tests.ps1` 验证 OpenVDB candidate gate：

```text
标准 OBJ strict_closed probe 预期失败；
surface_shell_texture_report schema = p0.surface_shell_texture_report.2；
productionAllowed=false；
报告包含 boundary / non-manifold blocker；
不生成 output/ObjStandardTemplateOpenVdbCandidate/manifest.json。
```

因此，11A-6 的状态是“受限完成”：legacy/RIP/UI/diagnostic 验收通过，OpenVDB candidate 写包验收以“正确阻断”为通过条件。

---

## 7. 生产协议符合情况

Stage 11A 未修改生产 RGBWSV 协议：

```text
manifest.schema = p0.rgbwsv.2
channelOrder = R G B W S V
channelCount = 6
bitDepth = 8
sampleFormat = uint
polarity = black_is_print
printValue = 0
emptyValue = 255
```

OpenVDB 仍保持：

```text
optional；
默认关闭；
experimental；
diagnostic_only 不写 production package；
strict_closed 不通过时不写 candidate RGBWSV package。
```

---

## 8. 实际验证结果

本阶段实际运行过：

```powershell
Get-ChildItem model\obj
Select-String -Path model\obj\MF_aishen_damuzhi_L_tx02.obj -Pattern "mtllib|vt "
Get-Content model\obj\MF_aishen_damuzhi_L_tx02.mtl
Get-Content -Raw samples\configs\obj_standard\standard_obj_texture_legacy.json | ConvertFrom-Json | Out-Null
Get-Content -Raw samples\scenarios\slicer_scenarios.json | ConvertFrom-Json | Out-Null
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --inspect-model
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --experimental-openvdb-shell --admission-mode diagnostic_only --experimental-report output\ObjStandardTemplateOpenVdbDiagnostic\reports\experimental_openvdb_shell_report.json
cmake --build build --config Debug --target experimental_config_unit_tests
.\build\Debug\experimental_config_unit_tests.exe
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
cmake --build build --config Debug --target surface_shell_real_model_unit_tests
.\build\Debug\surface_shell_real_model_unit_tests.exe
.\scripts\run_surface_shell_texture_tests.ps1
.\scripts\run_surface_shell_real_model_tests.ps1
.\build-openvdb-09b-r1\Debug\surface_shell_real_model_demo.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json --voxel-mm 0.10 --shell-mm 0.15 --mesh-policy strict_closed --output output\ObjStandardTemplateOpenVdbProbe
.\scripts\run_11a_obj_standard_tests.ps1
.\scripts\run_11a_obj_openvdb_candidate_tests.ps1
Get-Content -Raw tests\golden\expected\11a_obj_standard_contract.json | ConvertFrom-Json | Out-Null
git diff --check
git diff --cached --check
```

已知验证输出：

```text
Debug 全量构建通过；
CTest 5/5 通过；
UI self-test 通过 startup / experimental-report-summary；
standard OBJ legacy package 通过 RIP summary；
legacy 输出 grid = 226 x 425 x 573；
legacy 输出 modelPixels = 7055867；
legacy 输出 supportPixels = 20911855；
RIP channelPrintPixels：R=6961174, G=7006345, B=7053475, W=0, S=20911855, V=0；
OpenVDB candidate gate 脚本通过，验证 strict_closed 正确阻断；
OpenVDB ON 轨道存在 Boost CMP0167 dev warning，但未导致脚本失败。
```

备注：

```text
output/* 为本地验证产物，不提交到仓库；
git diff --check / git diff --cached --check 通过时仅出现 Windows LF/CRLF 工作区提示。
```

---

## 9. 当前未完成和风险

```text
标准 OBJ 模板不是 strict_closed 拓扑，存在 duplicate faces / opposite duplicate faces / boundary edges / non-manifold edges；
OpenVDB candidate RGBWSV package writer 未对该标准 OBJ 模板完成生产写包；
surface_shell_from_sdf 当前是配置和原型门禁能力，不等于正式生产切片能力；
repair_then_strict 尚未形成可让该真实 OBJ 通过 admission 的正式 mesh repair 链路；
OpenVDB ON 轨道仍有 Boost CMP0167 CMake dev warning，需要后续依赖收口；
Stage 12 如果依赖 OpenVDB candidate package，必须先解决拓扑修复或新增 strict_closed PASS 的标准模型。
```

---

## 10. 是否可以进入 Stage 12

可以进入 Stage 12 的前提：

```text
Stage 12 不把 OpenVDB OBJ candidate RGBWSV package 当作已完成基础能力；
Stage 12 继续使用 legacy production path 作为标准 OBJ 彩色纹理的生产基线；
OpenVDB 继续作为 experimental diagnostic / prototype path；
所有 OpenVDB production 写出仍受 strict_closed + ProductionAdmissionPolicy 约束。
```

如果 Stage 12 要正式推进 OpenVDB OBJ 彩色纹理生产切片，则建议先补一个 Stage 11A-R1：

```text
P0：mesh repair / repair_then_strict 正式实现和验收；
P0：或新增 strict_closed PASS 的标准 OBJ 彩色纹理 fixture；
P1：candidate RGBWSV writer 只在 productionAllowed=true 时写包；
P1：RIP summary / layer preview / texture fidelity 对 candidate package 建立 golden；
P2：OpenVDB ON 依赖 warning 收口。
```

推荐判断：

```text
Stage 11A 已完成 Stage 12 前置文档、标准模板、legacy 验收和 OpenVDB 安全门禁；
OpenVDB OBJ 彩色纹理生产写包尚未完成；
下一阶段若目标是产品功能，应优先把 legacy OBJ 彩色纹理链路继续产品化；
下一阶段若目标是 OpenVDB 生产化，应先处理 11A-R1 拓扑修复/closed fixture/candidate writer。
```
