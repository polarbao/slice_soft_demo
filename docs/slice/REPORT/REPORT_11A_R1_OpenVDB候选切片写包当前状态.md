# REPORT_11A_R1_OpenVDB候选切片写包当前状态

> 文档版本：v0.1  
> 文档状态：Stage Report / 11A-R1 Partial  
> 生成日期：2026-07-02  
> 分支：`main`

---

## 1. 结论

OpenVDB 可以进入后续“候选切片”开发，但当前不能直接取代 legacy production path。

当前 09 / 09P / 11A 阶段已经证明：

```text
OpenVDB 可作为可选依赖接入；
OpenVDB OFF 默认轨道可继续通过；
surface-shell texture 原型和 topology admission gate 已存在；
diagnostic/report/prototype 链路可用于后续开发判断。
```

但当前仍未完成：

```text
OpenVDB candidate per-layer RGBWSV buffer builder；
OpenVDB candidate p0.rgbwsv.2 package writer；
candidate package RIP / UI / preview golden；
真实复杂 OBJ / 3MF 的 strict_closed 通过路径或 repair_then_strict 正式路径。
```

因此，本阶段采用“OpenVDB Candidate”路线：显式入口、严格门禁、逐步补 writer，不默认替换 legacy。

---

## 2. 本轮补齐文档

正式文档：

```text
docs/slice/DOC/DOC_DECISION_11A_R1_OpenVDB候选切片写包与Preview收口.md
docs/slice/PRD/PRD_11A_R1_OpenVDB候选切片写包与Preview收口.md
docs/slice/DEV/DEV_11A_R1_OpenVDBCandidatePipeline_RGBWSVWriter设计.md
docs/slice/DEMO/DEMO_11A_R1_OpenVDB候选包与Preview验证方案.md
docs/slice/ROADMAP/ROADMAP_11A_R1_OpenVDB候选切片开发路线.md
```

Codex 执行文档：

```text
docs/codex_task/current/TASKS_11A_R1_OpenVDB候选切片写包任务清单.md
docs/codex_task/current/CODEX_PROMPT_11A_R1_OpenVDB候选切片写包执行指令.md
```

索引已更新：

```text
docs/slice/README.md
docs/slice/DOC/README.md
docs/slice/PRD/README.md
docs/slice/DEV/README.md
docs/slice/DEMO/README.md
docs/slice/ROADMAP/README.md
docs/codex_task/README.md
```

---

## 3. 本轮已完成实现

Task 11A-R1-1 已完成：

```text
新增 slicer_cli --openvdb-candidate-slice 显式入口；
candidate writer 未完成时返回 not_implemented，不写 production package；
legacy run_slicer 遇到 surface_shell_from_sdf 配置会明确失败；
legacy run_slicer 遇到 writeProductionRgbwsv=true 配置会明确失败；
新增单测覆盖上述两个 legacy guard。
```

关键修改：

```text
apps/slicer_cli/main.cpp
src/slicer_core/slicer.cpp
tests/unit/experimental_config/main.cpp
docs/codex_task/current/TASKS_11A_R1_OpenVDB候选切片写包任务清单.md
```

Task 11A-R1-2 已完成：

```text
新增 strict_closed PASS 的小型 closed textured OBJ fixture；
新增 OpenVDB candidate config；
OpenVDB ON strict_closed probe 通过；
productionAdmission.productionAllowed=true；
admissionStatus=production_allowed；
boundaryEdges=0；
nonManifoldEdges=0；
texture sampled voxels > 0；
fallback voxels = 0。
```

关键新增文件：

```text
samples/models/openvdb_candidate/closed_textured_obj.obj
samples/models/openvdb_candidate/closed_textured_obj.mtl
samples/models/openvdb_candidate/closed_textured_obj_checker.png
samples/configs/openvdb_candidate/closed_textured_obj_candidate.json
```

Task 11A-R1-3 已完成：

```text
新增 OpenVDB candidate layer buffer builder；
将 shell_mask / interior_mask 展开为 per-layer model_mask / surface_shell_mask；
将 SurfaceTextureTransfer shell_rgb 映射到 per-layer surface_rgb；
支持外部 support / white / varnish 3D mask 映射；
默认保持 Model > Support，support 与 model 冲突时清除 support；
输出 per-layer stats；
不写 TIFF；
不写 manifest；
不改变 p0.rgbwsv.2 协议。
```

关键新增/修改文件：

```text
src/slicer_core/pipeline/OpenVdbCandidateLayerBufferBuilder.h
src/slicer_core/pipeline/OpenVdbCandidateLayerBufferBuilder.cpp
tests/unit/material_channel_composer/main.cpp
CMakeLists.txt
```

---

## 4. 当前未完成任务

11A-R1 后续仍需按任务清单继续：

```text
11A-R1-4 Candidate package writer；
11A-R1-5 Candidate RIP / UI smoke；
11A-R1-6 UI OpenVDB Candidate 按钮；
11A-R1-7 完整阶段报告。
```

其中最关键的技术闭环是：

```text
OpenVDB shell/interior/support mask
-> MaterialChannelComposerInput
-> per-layer RGBWSV uint8 buffer
-> p0.rgbwsv.2 manifest/TIFF/reports/preview
-> rip_reader_test strict validation
-> Qt LayerPreview / OverlayPreview 可加载。
```

---

## 5. 实际验证结果

本轮已运行：

```powershell
cmake --build build --config Debug --target slicer_cli experimental_config_unit_tests
.\build\Debug\experimental_config_unit_tests.exe
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json
git diff --check
Get-Content -Raw samples\configs\openvdb_candidate\closed_textured_obj_candidate.json | ConvertFrom-Json | Out-Null
.\build\Debug\slicer_cli.exe --config samples\configs\openvdb_candidate\closed_textured_obj_candidate.json --inspect-model
cmake --build build-openvdb-09p --config Debug --target surface_shell_real_model_demo
.\build-openvdb-09p\Debug\surface_shell_real_model_demo.exe --config samples\configs\openvdb_candidate\closed_textured_obj_candidate.json --mesh-policy strict_closed --output output\OpenVdbCandidateClosedTexturedObjProbe
cmake --build build --config Debug --target material_channel_composer_unit_tests
.\build\Debug\material_channel_composer_unit_tests.exe
```

结果：

```text
slicer_cli / experimental_config_unit_tests 构建通过；
experimental_config_unit_tests 全部 PASS；
ctest 5/5 PASS；
legacy standard OBJ package 可继续生成；
legacy 输出 packageDir = output/ObjStandardTemplateLegacy；
legacy 输出 grid = 226 x 425 x 573；
legacy 输出 modelPixels = 7055867；
legacy 输出 supportPixels = 20911855。
git diff --check 退出码为 0，仅出现 Windows LF/CRLF 工作区提示。
candidate fixture JSON 解析通过；
candidate fixture 模型检查通过，bbox = 3 x 3 x 0.6 mm；
OpenVDB ON strict_closed probe 退出码为 0；
probe report schema = p0.surface_shell_texture_report.2；
productionAllowed = true；
admissionStatus = production_allowed；
boundaryEdges = 0；
nonManifoldEdges = 0；
activeVoxels = 111099；
shellVoxels = 19132；
interiorVoxels = 29241；
sampledTextureVoxels = 19132；
fallbackVoxels = 0；
errors = 0。
material_channel_composer_unit_tests 构建通过；
新增 candidate_layer_buffer_builder_maps_shell_interior_and_support PASS；
新增 candidate_layer_buffer_builder_rejects_invalid_masks PASS；
```

---

## 6. 风险与下一步

风险：

```text
candidate writer 尚未实现，OpenVDB 不能被 UI 当作正式切片按钮使用；
真实 OBJ / 3MF 拓扑质量不稳定，strict_closed 可能继续阻断；
closed_textured_obj 仅作为 writer 正向 fixture，不代表真实甲片模型已经 strict_closed 可生产；
OpenVDB ON 轨道和默认 OFF 轨道需要持续分层验证；
后续 writer 必须继续遵守 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print。
当前 builder 尚未接入 CLI candidate path，仍需 11A-R1-4 writer 和 pipeline glue。
```

下一步建议：

```text
优先执行 11A-R1-4，完成 candidate package writer；
writer 通过 RIP / UI / preview smoke 之前，不允许替换 legacy production path。
```
