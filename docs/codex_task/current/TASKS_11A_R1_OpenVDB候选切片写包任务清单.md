# TASKS_11A_R1_OpenVDB候选切片写包任务清单

> 文档版本：v0.1  
> 文档状态：Codex Task List / Stage 11A-R1  
> 生成日期：2026-07-02

---

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
不修改 R G B W S V；
不修改 uint8 / black_is_print；
不默认启用 OpenVDB；
不替换 legacy production path；
不绕过 ProductionAdmissionPolicy；
strict_closed 失败不得写 candidate package；
diagnostic_only 不得写 package；
OpenVDB OFF 默认轨道必须继续通过。
```

---

## 2. 当前执行状态

| 任务 | 状态 | 说明 |
|---|---|---|
| 11A-R1-0 文档包补齐 | DONE | 已新增 DOC_DECISION / PRD / DEV / DEMO / ROADMAP / TASKS / CODEX_PROMPT |
| 11A-R1-1 Pipeline 入口与防误用 guard | DONE | 已新增 candidate CLI flag / legacy path guard；writer 未完成时不写 package |
| 11A-R1-2 strict_closed PASS fixture | TODO | 新增小型 closed OBJ/MTL/PNG fixture 和 candidate config |
| 11A-R1-3 Candidate layer buffer builder | TODO | shell/interior/support -> RGBWSV per-layer input |
| 11A-R1-4 Candidate package writer | TODO | 写 manifest / TIFF / reports / preview |
| 11A-R1-5 Candidate RIP / UI smoke | TODO | rip_reader_test、LayerPreview、OverlayPreview |
| 11A-R1-6 UI OpenVDB Candidate 按钮 | TODO | 新增显式按钮和状态显示 |
| 11A-R1-7 REPORT_11A_R1 | TODO | 记录完成范围和实际验证 |

---

## 3. Task 11A-R1-0：文档包补齐

目标：

```text
确认 OpenVDB 可以作为 candidate 进入开发；
明确不能直接替换 legacy；
补齐需求、设计、验证、路线图和任务清单。
```

输出：

```text
docs/slice/DOC/DOC_DECISION_11A_R1_OpenVDB候选切片写包与Preview收口.md
docs/slice/PRD/PRD_11A_R1_OpenVDB候选切片写包与Preview收口.md
docs/slice/DEV/DEV_11A_R1_OpenVDBCandidatePipeline_RGBWSVWriter设计.md
docs/slice/DEMO/DEMO_11A_R1_OpenVDB候选包与Preview验证方案.md
docs/slice/ROADMAP/ROADMAP_11A_R1_OpenVDB候选切片开发路线.md
docs/codex_task/current/TASKS_11A_R1_OpenVDB候选切片写包任务清单.md
docs/codex_task/current/CODEX_PROMPT_11A_R1_OpenVDB候选切片写包执行指令.md
```

验证：

```powershell
git diff --check
```

---

## 4. Task 11A-R1-1：Pipeline 入口与防误用 guard

目标：

```text
新增明确的 OpenVDB Candidate CLI 入口；
legacy run_slicer 遇到 surface_shell_from_sdf / writeProductionRgbwsv 时给出明确错误；
当前 writer 未完成时不写 package。
```

建议修改：

```text
apps/slicer_cli/main.cpp
src/slicer_core/pipeline/SlicePipeline.*
src/slicer_core/slicer.cpp
tests/unit/experimental_config 或新增 pipeline unit test
```

验证：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\slicer_cli.exe --config samples\configs\obj_standard\standard_obj_texture_legacy.json
```

---

## 5. Task 11A-R1-2：strict_closed PASS fixture

目标：

```text
新增小型 closed textured OBJ fixture；
新增 OpenVDB candidate config；
OpenVDB ON 下能通过 topology/admission。
```

输出：

```text
samples/models/openvdb_candidate/closed_textured_obj.*
samples/configs/openvdb_candidate/closed_textured_obj_candidate.json
```

验证：

```powershell
.\build-openvdb-09p\Debug\surface_shell_real_model_demo.exe --config samples\configs\openvdb_candidate\closed_textured_obj_candidate.json --mesh-policy strict_closed --output output\OpenVdbCandidateClosedTexturedObjProbe
```

---

## 6. Task 11A-R1-3：Candidate layer buffer builder

目标：

```text
将 OpenVDB shell/interior 3D mask 转成 per-layer MaterialChannelComposerInput；
输出 layer stats；
不写 TIFF。
```

验证：

```powershell
cmake --build build --config Debug --target material_channel_composer_unit_tests
.\build\Debug\material_channel_composer_unit_tests.exe
```

---

## 7. Task 11A-R1-4：Candidate package writer

目标：

```text
将 candidate per-layer RGBWSV buffer 写成 p0.rgbwsv.2 package；
写 manifest/layers/reports/preview；
失败时使用 staging 目录，避免半包。
```

验证：

```powershell
.\build-openvdb-09p\Debug\slicer_cli.exe --config samples\configs\openvdb_candidate\closed_textured_obj_candidate.json --openvdb-candidate-slice
.\build-openvdb-09p\Debug\rip_reader_test.exe --package output\OpenVdbCandidateClosedTexturedObj --summary
```

---

## 8. Task 11A-R1-5：Candidate RIP / UI smoke

目标：

```text
新增 candidate 验收脚本；
验证 RIP、LayerPreview、OverlayPreview。
```

输出：

```text
scripts/run_11a_r1_openvdb_candidate_on_lane.ps1
scripts/run_11a_r1_openvdb_candidate_off_lane.ps1
tests/golden/expected/11a_r1_openvdb_candidate_contract.json
```

---

## 9. Task 11A-R1-6：UI OpenVDB Candidate 按钮

目标：

```text
新增“导入模型并 OpenVDB 候选切片”；
OpenVDB OFF 时提示需要 OpenVDB ON build；
成功后加载 package；
失败后加载 report。
```

验证：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
```

---

## 10. Task 11A-R1-7：REPORT_11A_R1

目标：

```text
记录实际完成范围；
记录 OpenVDB 是否具备替换条件；
记录仍需 repair / 性能 / 真实模型验证的风险。
```
