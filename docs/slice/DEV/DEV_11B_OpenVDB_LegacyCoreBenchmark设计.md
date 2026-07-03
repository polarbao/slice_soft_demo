# DEV_11B_OpenVDB_LegacyCoreBenchmark设计

> 文档版本：v0.1  
> 文档状态：DEV / Benchmark Plan  
> 生成日期：2026-07-04  
> 适用阶段：Stage 11B / OpenVDB replacement gate

---

## Goal

建立 legacy 与 OpenVDB 在相同模型、相同姿态、相同输出语义下的核心切片耗时对比方法。

核心目标：

```text
只比较切片核心计算；
不把 TIFF 保存、preview 图片生成、report JSON 写入、manifest 发布计入核心耗时；
同时保留可选的端到端耗时，用于评估真实用户等待时间。
```

---

## Scope

本设计覆盖：

```text
legacy relief_heightfield 核心耗时拆分；
OpenVDB candidate 核心耗时拆分；
同模型同姿态 benchmark 配置要求；
core-only benchmark CLI / script 输出契约；
后续判断 OpenVDB 是否可替代 legacy 的性能 gate。
```

---

## Non-goals

本阶段不做：

```text
默认启用 OpenVDB；
替换 legacy production path；
修改 p0.rgbwsv.2 / RGBWSV / uint8 / black_is_print；
把 non-production OpenVDB 输出当作可替代结果；
以 Debug 构建结果作为最终性能结论。
```

---

## Current State

当前已有对比只能称为探索性对比：

```text
legacy Debug 总耗时约 22.653s；
OpenVDB candidate Debug 总耗时约 40.794s；
OpenVDB candidate 为 non_production_written；
OpenVDB candidate supportPixels = 0；
两者输出语义未等价。
```

当前问题：

```text
legacy --preview-only 只是不写 TIFF，不是纯计算模式；
legacy 仍会写 report / manifest，且 preview 是否生成取决于配置；
OpenVDB candidate 当前始终写 TIFF / preview / report；
缺少统一 timing schema；
缺少 Release core-only benchmark 脚本；
缺少 I/O 与核心计算的拆分。
```

---

## Proposed Approach

### 1. 定义两个耗时口径

```text
coreComputeMs：
  只包含模型加载后到每层通道 buffer 生成完成的核心计算；
  不包含 TIFF、preview、report、manifest、目录发布。

endToEndMs：
  从 CLI 接收到 config 到全部输出完成；
  代表用户实际等待时间。
```

### 2. legacy timing 拆分

legacy 应至少记录：

```text
loadConfigMs
loadModelMs
gridMs
sampleModelOrReliefMs
texturePrepareMs
supportMs
materialComposeMs
tiffWriteMs
previewWriteMs
reportWriteMs
endToEndMs
coreComputeMs
```

其中 `coreComputeMs` 建议包含：

```text
loadModelMs
gridMs
sampleModelOrReliefMs
texturePrepareMs
supportMs
materialComposeMs
```

### 3. OpenVDB timing 拆分

OpenVDB candidate 应至少记录：

```text
loadConfigMs
loadModelMs
admissionStrictMs
levelSetMs
surfaceShellClassifyMs
textureTransferMs
layerBufferMs
materialComposeMs
tiffWriteMs
previewWriteMs
reportWriteMs
publishMs
endToEndMs
coreComputeMs
```

其中 `coreComputeMs` 建议包含：

```text
loadModelMs
admissionStrictMs
levelSetMs
surfaceShellClassifyMs
textureTransferMs
layerBufferMs
materialComposeMs
```

### 4. CLI 模式

新增 benchmark 模式时推荐使用显式参数：

```powershell
slicer_cli --config <config> --benchmark-core-only --engine legacy
slicer_cli --config <config> --benchmark-core-only --engine openvdb-candidate
```

约束：

```text
benchmark-core-only 不写 TIFF；
benchmark-core-only 不写 preview 图片；
benchmark-core-only 可只写一个 benchmark JSON，或由脚本捕获 stdout；
benchmark-core-only 不发布 package；
OpenVDB 仍必须记录 productionAllowed / nonProduction / blockerCodes；
OpenVDB 非 production 结果不得标记 replacementPass。
```

### 5. benchmark report schema

推荐输出：

```json
{
  "schema": "p0.openvdb_legacy_core_benchmark.1",
  "engine": "legacy",
  "buildType": "Release",
  "modelPath": "...",
  "pose": {
    "scale": [0.8, 0.8, 0.8],
    "autoOrientEnabled": true,
    "selectedOrientation": "rotate_x_90",
    "orientedHeightMm": 4.97729
  },
  "grid": {
    "widthPx": 229,
    "heightPx": 455,
    "layerCount": 498
  },
  "outputPolicy": {
    "writeTiff": false,
    "writePreview": false,
    "writeReports": "benchmark_only"
  },
  "productionAdmission": {
    "productionAllowed": true,
    "status": "allowed"
  },
  "stats": {
    "modelPixels": 0,
    "supportPixels": 0,
    "shellPixels": 0
  },
  "timingsMs": {
    "coreCompute": 0.0,
    "endToEnd": 0.0
  },
  "memory": {
    "peakWorkingSetBytes": 0
  },
  "replacementGate": {
    "performanceComparable": false,
    "outputSemanticsComparable": false,
    "replacementPass": false
  }
}
```

---

## Steps

### Step 1：文档和任务口径固化

完成标准：

```text
11B 任务清单明确 core-only benchmark；
ROADMAP 明确不能用 end-to-end Debug 结果直接判断替代；
REPORT 记录当前缺口。
```

### Step 2：legacy core-only runner

完成标准：

```text
legacy 可在不写 TIFF / preview 的情况下完成核心 buffer 计算；
输出 benchmark JSON；
默认生产路径不受影响。
```

### Step 3：OpenVDB candidate core-only runner

完成标准：

```text
OpenVDB 可在不写 TIFF / preview 的情况下完成 SDF、shell、transfer、layer buffer、composer；
输出 benchmark JSON；
strict/non-production 状态清晰记录。
```

### Step 4：Release benchmark 脚本

完成标准：

```text
脚本强制使用 Release 构建；
同模型、同 scale、同 autoOrient、同 layerThickness、同 dpi；
分别输出 legacy 与 OpenVDB benchmark JSON；
自动判断两者是否具备可比性。
```

### Step 5：replacement gate 判定

完成标准：

```text
如果 outputSemanticsComparable=false，则不判定性能替代；
如果 OpenVDB nonProduction=true，则 replacementPass=false；
只有 productionAllowed=true 且语义可比时，才比较 coreComputeMs。
```

---

## Risks

```text
core-only 路径可能与 production 路径发生漂移；
跳过 TIFF / preview 后无法验证下游 package；
OpenVDB 非 production fallback 会让性能数字看起来可用但语义不可比；
Debug 构建容易误导性能判断。
```

缓解：

```text
core-only 只用于 benchmark；
production 回归仍必须跑完整 package；
benchmark report 必须输出 outputSemanticsComparable；
Release benchmark 才能进入 replacement gate。
```

---

## Validation

最小验证：

```powershell
cmake --build build --config Debug --target slicer_cli
.\build\Debug\slicer_cli.exe --config <legacy-config> --inspect-model
```

正式验证：

```powershell
cmake --build build-release --config Release --target slicer_cli
cmake --build build-openvdb-release --config Release --target slicer_cli
.\scripts\run_11b_core_benchmark.ps1 -Model model\obj\nai_you_new\MF_nai_you.obj
```

通过标准：

```text
benchmark report 能拆分 coreComputeMs 和 endToEndMs；
报告明确是否写 TIFF / preview；
OpenVDB 非 production 时 replacementPass=false；
legacy 默认完整切片不受影响。
```

---

## Rollback

```text
如果 benchmark-core-only 引入风险，保留文档和脚本设计，撤回 CLI 参数；
如果 OpenVDB core-only 结果与 candidate package 结果不一致，停止替代评估；
任何 benchmark 改造不得影响默认 legacy slicer_cli --config 路径。
```
