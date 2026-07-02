# DEMO_11A_R1_OpenVDB候选包与Preview验证方案

> 文档版本：v0.1  
> 文档状态：DEMO / Stage 11A-R1  
> 生成日期：2026-07-02

---

## 1. Goal

证明 OpenVDB Candidate path 可以在严格准入下生成 RGBWSV package，并能被 RIP reader 和 UI preview 消费。

---

## 2. Demo Cases

### Case 1：默认 OFF 轨道安全

命令：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

期望：

```text
OpenVDB OFF 默认构建通过；
legacy tests 通过；
candidate 功能不影响默认生产路径。
```

### Case 2：OpenVDB ON smoke

命令：

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p
```

期望：

```text
openvdb.enabled=true；
openvdb.available=true；
activeVoxels > 0。
```

### Case 3：strict_closed PASS fixture candidate package

命令占位：

```powershell
.\build-openvdb-09p\Debug\slicer_cli.exe --config samples\configs\openvdb_candidate\closed_textured_obj_candidate.json --openvdb-candidate-slice
.\build-openvdb-09p\Debug\rip_reader_test.exe --package output\OpenVdbCandidateClosedTexturedObj --summary
```

期望：

```text
生成 manifest.json；
生成 layers/*.tiff；
manifest.schema = p0.rgbwsv.2；
RIP summary PASS；
RGB channel 有 texture pixels；
reports/openvdb_candidate_report.json 存在。
```

### Case 4：真实标准 OBJ 正确阻断

命令：

```powershell
.\scripts\run_11a_obj_openvdb_candidate_tests.ps1
```

期望：

```text
model/obj 标准 OBJ 因 topology blocker 被拒绝；
不生成 candidate manifest；
报告包含 blockerCodes。
```

### Case 5：UI 读取 candidate package

命令：

```powershell
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\OpenVdbCandidateClosedTexturedObj
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\OpenVdbCandidateClosedTexturedObj
```

期望：

```text
LayerPreview 可读取 RGB/S/W/V/occupancy；
OverlayPreview 可读取同层 RGB + S；
preview_report 去重后不重复显示。
```

---

## 3. Exit Criteria

```text
Case 1 PASS；
Case 2 PASS；
Case 3 PASS；
Case 4 PASS；
Case 5 PASS；
legacy 标准 OBJ package 不退化；
git diff --check 通过。
```

