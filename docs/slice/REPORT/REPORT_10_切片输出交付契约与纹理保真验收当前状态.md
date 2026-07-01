# REPORT_10_切片输出交付契约与纹理保真验收当前状态

> 文档版本：v0.1
> 文档状态：Stage Report / 10
> 生成日期：2026-07-01
> 分支：`spike/09P-openvdb-experimental-pipeline`

---

## 1. 阶段目标

Stage 10 的目标是把 SliceSoft 当前输出包定义成可交付、可解释、可回归的下游契约：

```text
package / manifest；
RGBWSV TIFF 生产协议；
layer summary / channel summary；
texture fidelity 指标；
真实模型验收集；
downstream handoff checklist；
output contract schema / golden。
```

本阶段明确不实现：

```text
RIP 半色调；
设备通信；
喷头 bitstream；
ICC / 打印机校准；
OpenVDB 默认生产启用；
修改 p0.rgbwsv.2；
改变 R G B W S V 通道顺序。
```

---

## 2. 已完成任务

| Task | 状态 | 主要提交 |
|---|---|---|
| 10-0 阶段文档入口同步 | 已完成 | `50da9d4 docs(10): 同步 Stage 10 阶段入口` |
| 10-1 Output contract 字段 | 已完成 | `18ec98e docs(10): 定义输出契约字段矩阵` |
| 10-2 Layer summary / Channel summary | 已完成 | `89b05f6 docs(10): 固化层与通道统计契约` |
| 10-3 Texture fidelity 指标 | 已完成 | `2d39b3c docs(10): 定义纹理保真指标契约` |
| 10-4 真实模型验收集 | 已完成 | `6663a98 docs(10): 建立真实模型验收集` |
| 10-5 Downstream handoff checklist | 已完成 | `08ba96b docs(10): 建立下游交付清单` |
| 10-6 Output contract golden / schema | 已完成 | `4818992 test(10): 增加输出契约 golden 验证` |
| 10-7 本报告 | 本轮完成 | `REPORT_10_切片输出交付契约与纹理保真验收当前状态.md` |

---

## 3. 新增和关键修改

新增正式文档：

```text
docs/slice/DEV/DEV_10_OutputContract_FieldMatrix.md
docs/slice/DEV/DEV_10_LayerChannelSummaryContract.md
docs/slice/DEV/DEV_10_TextureFidelityMetrics.md
docs/slice/DEMO/DEMO_10_RealModelAcceptanceSet.md
docs/slice/DOC/DOC_CHECKLIST_10_DownstreamHandoff.md
docs/slice/REPORT/REPORT_10_切片输出交付契约与纹理保真验收当前状态.md
```

新增验证资产：

```text
scripts/run_10_output_contract_tests.ps1
tests/golden/expected/10_output_contract_schema.json
tests/golden/expected/10_output_contract_summary.json
```

关键修改：

```text
scripts/run_golden_tests.ps1
docs/slice/README.md
docs/slice/DEV/README.md
docs/slice/DEMO/README.md
docs/slice/DOC/README.md
docs/slice/REPORT/README.md
docs/codex_task/current/TASKS_10_切片输出交付契约与纹理保真验收任务清单.md
```

---

## 4. 当前稳定契约

### 4.1 Manifest / TIFF

Stage 10 固定下列生产协议：

```text
manifest.schema = p0.rgbwsv.2
channelOrder = R G B W S V
channelCount = 6
bitDepth = 8
sampleFormat = uint
planarConfig = contiguous
storageMode = stripped | tiled
polarity = black_is_print
printValue = 0
emptyValue = 255
```

下游可以稳定依赖：

```text
manifest.grid.*
manifest.tiff.*
manifest.layers[].index
manifest.layers[].zMm
manifest.layers[].path
manifest.layers[].widthPx / heightPx
```

### 4.2 Layer / Channel Summary

已定义统计口径：

```text
printPixels = value < 255
fullPrintPixels = value == 0
partialPrintPixels = 1..254
emptyPixels = value == 255
printPixels = fullPrintPixels + partialPrintPixels
printPixels + emptyPixels = widthPx * heightPx
```

RGB 聚合：

```text
rgbPrintPixels = count(R < 255 or G < 255 or B < 255)
```

W/S/V 聚合：

```text
whitePrintPixels = channelStats.W.printPixels
supportPrintPixels = channelStats.S.printPixels
varnishPrintPixels = channelStats.V.printPixels
```

### 4.3 Texture Fidelity

已定义指标：

```text
textureResolvedRate
uvCoverageRate
fallbackPixelRate
uvOutOfRangeRate
missingTextureRate
materialBindingCoverage
colorGroupCoverage
texture2DGroupCoverage
nearestTriangleHitRate
```

已区分：

```text
OBJ / MTL / PNG；
3MF ColorGroup；
3MF Texture2DGroup；
experimental OpenVDB surface shell。
```

---

## 5. Golden / Schema 当前状态

新增 Stage 10 golden 覆盖：

```text
g4_missing_texture
g4_no_uv
g2_3mf_colorgroup
g2_3mf_texture2d_checker
g3_rgb_white_varnish_support
```

默认 golden 选择小型 fixture，避免把真实大模型放入日常快速回归。

`scripts/run_golden_tests.ps1` 当前会执行：

```text
原有 R2 golden；
Stage 10 output contract golden。
```

Stage 10 golden 会检查：

```text
manifest / TIFF 固定协议；
RIP reader 可读取 package；
layer list 与 TIFF 文件存在；
每层 / totals channelStats 守恒；
texture_report 字段和 fallback 指标；
3MF ColorGroup / Texture2DGroup 关键统计；
material_policy_report RGB/W/V 输出；
support / white / varnish / RGB printPixels 阈值。
```

---

## 6. 实际验证结果

本阶段已实际运行：

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_10_output_contract_tests.ps1
.\scripts\run_golden_tests.ps1
git diff --check
```

结果：

```text
Debug build 通过；
ctest 5/5 通过；
run_10_output_contract_tests.ps1 通过，5 个 Stage 10 fixture 通过；
run_golden_tests.ps1 通过，原有 golden 与 Stage 10 golden 均通过；
git diff --check 通过，仅有 Windows LF/CRLF 工作区提示。
```

---

## 7. 下游待确认项

需要下游 RIP 工程师确认：

```text
1. 是否只消费 manifest + layers TIFF，还是需要独立 summary JSON；
2. 是否需要把 textureFidelity summary 物化为单独 report 文件；
3. 对 partial print value 1..254 的解释是否需要更严格字段；
4. tiled / stripped 两种 storageMode 是否都作为下游必测；
5. 下游错误码是否需要映射到当前 rip_reader ValidationErrorCode；
6. large real-world model 是否进入 nightly / release gate；
7. 是否需要固定 handoff sample package 的归档位置和版本号。
```

---

## 8. 当前不支持或未完成

```text
不支持 RIP 半色调；
不支持设备 bitstream；
不支持真实打印色彩校准验收；
不把 preview PNG 当作生产输入；
不把 OpenVDB experimental report 当作 production package；
materialBindingCoverage 仍是 Candidate 指标；
真实大模型未纳入默认 golden 快速链路；
texture fidelity summary 目前由 schema/golden 脚本计算，不是独立 production report 文件。
```

---

## 9. 是否可以进入 11 阶段

建议进入 11 阶段。

理由：

```text
Stage 10 已明确 package / manifest / layer summary / channel summary / texture fidelity / handoff 的边界；
默认 golden 已覆盖输出契约核心字段；
下游生产协议未被修改；
UI layer preview 可以基于当前契约读取 report，而不是反推 preview PNG。
```

进入 11 阶段前建议保持：

```text
1. 继续禁止把 RIP SDK 引入 slicer_core；
2. 继续禁止默认启用 OpenVDB；
3. 11 阶段 UI 只消费 output contract / report，不修改生产 TIFF 语义；
4. 真实大模型验收放入 release gate 或 nightly，不进入默认 quick golden。
```

---

## 10. 下一阶段建议

优先级建议：

```text
P0：11 阶段 LayerPreview UI 按 output contract 读取 manifest/report；
P0：UI 展示 RGB/W/S/V channelStats 和 texture fidelity；
P1：补独立 texture_fidelity_summary.json 或 package_summary.json；
P1：建立真实大模型 nightly 验收脚本；
P2：和下游 RIP 工程师对齐反馈模板和错误码映射；
P2：评估 materialBindingCoverage 是否升级为 Stable。
```
