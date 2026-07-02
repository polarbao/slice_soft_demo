# DEV_11A_R1_OpenVDBCandidatePipeline_RGBWSVWriter设计

> 文档版本：v0.1  
> 文档状态：DEV / Stage 11A-R1  
> 生成日期：2026-07-02

---

## 1. Goal

实现一个显式 OpenVDB Candidate pipeline，将当前 OpenVDB surface-shell 原型能力接入 RGBWSV package 输出。

该 pipeline 必须与 legacy pipeline 并行存在，不能替换默认 `run_slicer` 行为。

---

## 2. Current State

当前已具备：

```text
OpenVDB optional adapter；
OpenVdbGeometryKernelService；
SurfaceShellRealModelPrototype；
SurfaceShellTextureService；
SurfaceTextureTransfer；
MaterialChannelComposer；
ProductionAdmissionPolicy；
RgbwsvProtocol 常量；
TIFF writer；
preview writer 逻辑；
experimental report schema。
```

当前缺口：

```text
OpenVDB CandidatePipeline 公共入口；
surface shell 3D mask -> per-layer RGBWSV layer buffer；
candidate package writer；
candidate manifest / layer list；
candidate preview report；
candidate texture fidelity report；
CLI / UI candidate action；
strict_closed PASS fixture。
```

---

## 3. Target Architecture

```text
apps/slicer_cli
  --config <path> --openvdb-candidate-slice
    -> slicer_core::RunOpenVdbCandidatePipeline

slicer_core/pipeline
  OpenVdbCandidatePipeline
    LoadConfig
    ValidateOpenVdbCandidateConfig
    LoadScene
    AdaptSceneModelToTriangleMesh
    AnalyzeTopologyAndRobustness
    EvaluateProductionAdmission
    BuildOpenVdbLevelSet
    ClassifyOpenVdbSurfaceShell
    TransferSurfaceTexture
    BuildCandidateLayerBuffers
    ComposeMaterialChannels
    WriteRgbwsvCandidatePackage
    WriteCandidateReports
```

---

## 4. Pipeline Mode Detection

新增显式模式：

```text
LegacyProduction
OpenVdbDiagnostic
OpenVdbCandidate
```

判定规则：

```text
LegacyProduction:
  默认 slicer_cli --config <path>

OpenVdbDiagnostic:
  slicer_cli --experimental-openvdb-shell

OpenVdbCandidate:
  slicer_cli --openvdb-candidate-slice
  或 config.experimental.openvdbPipeline.writeProductionRgbwsv=true 且显式 action 允许
```

禁止：

```text
仅凭 writeProductionRgbwsv=true 自动从 legacy path 写包；
UI 隐式切到 OpenVDB candidate；
diagnostic_only 写 package。
```

---

## 5. Candidate Config Contract

最小配置：

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
      "allowNonProductionOutput": false,
      "writeProductionRgbwsv": true
    }
  }
}
```

---

## 6. Data Mapping

### 6.1 Shell / Interior

```text
shell_mask -> model_mask + surface_shell_mask；
interior_mask -> model_mask；
inside_mask 可用于 occupancy / diagnostics；
```

### 6.2 RGB

```text
surface shell voxels 使用 SurfaceTextureTransfer 输出的 shell_rgb；
interior model RGB 使用 modelMaterial.rgb 或 fallbackRgb；
empty = 255；
```

### 6.3 W / V / S

阶段最小策略：

```text
W：按 MaterialPolicy / profile，默认可关闭；
V：按 top surface shell 或 top N candidate layers，默认可关闭；
S：先实现 build plate projection / lower envelope 支撑；不得覆盖 model_mask；
```

如果 W/V/S 策略不足，candidate report 必须记录 `LIMITED_MATERIAL_POLICY`。

---

## 7. Package Writer

候选输出仍使用：

```text
schema = p0.rgbwsv.2
channelOrder = R G B W S V
bitDepth = 8
polarity = black_is_print
printValue = 0
emptyValue = 255
```

输出文件：

```text
manifest.json
layers/layer_000000.tiff
reports/openvdb_candidate_report.json
reports/production_admission_report.json
reports/texture_fidelity_report.json
reports/preview_report.json
preview/*.png
```

---

## 8. Failure Behavior

| 场景 | 行为 |
|---|---|
| USE_OPENVDB=OFF | fail fast，写 diagnostic/candidate report，不写 package |
| topology blocker | fail fast，不写 package |
| admissionMode 非 strict_closed | fail fast，不写 package |
| texture missing 且策略 fail_fast | fail fast，不写 package |
| texture missing 且 warn_and_fallback | 允许 candidate，但 report 记录 fallback |
| writer 中途失败 | 删除临时 staging 目录，不留下半包 |

---

## 9. Implementation Steps

### Step 1：Pipeline entry and guard

```text
新增 OpenVDB candidate CLI flag；
新增 RunOpenVdbCandidatePipeline stub；
当 writer 未完成时返回稳定错误和 report；
legacy path 遇到 surface_shell_from_sdf 时给出明确错误。
```

### Step 2：Closed fixture

```text
新增 strict_closed PASS 的小型 OBJ/MTL/PNG fixture；
新增 candidate config；
OpenVDB ON 下可通过 topology admission。
```

### Step 3：Layer buffer builder

```text
将 shell/interior mask 按 z layer 展开；
构建 MaterialChannelComposerInput；
输出 per-layer stats。
```

### Step 4：Candidate package writer

```text
复用 TIFF writer；
写 manifest/layers/reports；
写 preview；
通过 rip_reader_test。
```

### Step 5：UI candidate action

```text
新增“导入模型并 OpenVDB 候选切片”；
OpenVDB OFF 或缺 candidate CLI 时禁用/提示；
成功后加载 package，失败后加载 report。
```

---

## 10. Validation

默认 OFF 轨道：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_11a_obj_standard_tests.ps1
```

OpenVDB ON 轨道：

```powershell
.\scripts\run_openvdb_smoke.ps1 -BuildDir build-openvdb-09p
.\scripts\run_11a_r1_openvdb_candidate_on_lane.ps1 -OpenVdbBuildDir build-openvdb-09p
```

UI：

```powershell
cmake --build build --config Debug --target slicer_debug_ui
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case layer-preview-load --package output\<candidate-package>
```

---

## 11. Rollback

```text
删除或禁用 --openvdb-candidate-slice；
保留 diagnostic；
保留 legacy；
保留 candidate report 作为失败分析；
不改 p0.rgbwsv.2。
```

