# REPORT_08_支撑形态与工艺优化当前状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 生成日期：2026-06-10  
> 适用阶段：08

---

## 1. 阶段结论

08 阶段已完成第一轮支撑形态与工艺优化。

本阶段在支撑生成之后、RGBWSV 通道合成之前新增轻量形态优化层。当前实现不修改 `p0.rgbwsv.2`，不改变 `R G B W S V` 通道顺序，不改变 8-bit / `black_is_print`，不让 `SupportType` 写入 TIFF channel。

证据等级：

- [A] 当前代码已新增 support shape 模块并可编译。
- [A] `support_shape_smoke` package 可生成。
- [A] `rip_reader_test` 可读取并严格校验 `output/SupportShapeSmoke`。
- [A] schema / golden / CI quick 均通过。
- [B] PRD_08 / DEV_08 / DEMO_08 中定义的高级支撑形态目标，本轮完成轻量版本。

---

## 2. 新增模块

新增：

```text
src/slicer_core/support/SupportShapePolicy.h
src/slicer_core/support/SupportShapePolicy.cpp
src/slicer_core/support/SupportComponentAnalysis.h
src/slicer_core/support/SupportComponentAnalysis.cpp
src/slicer_core/support/SupportShapeOptimizer.h
src/slicer_core/support/SupportShapeOptimizer.cpp
src/slicer_core/support/SupportShapeReport.h
src/slicer_core/support/SupportShapeReport.cpp
```

已加入：

```text
CMakeLists.txt
```

模块职责：

- `SupportShapePolicy`：从 legacy `SupportConfig` 生成支撑形态策略。
- `SupportComponentAnalysis`：对单层 support mask 做 4/8 连通组件分析。
- `SupportShapeOptimizer`：执行小组件过滤、dilation、简化 closing、水平/垂直短 gap bridge。
- `SupportShapeReport`：输出 `p0.support_shape_report.1` JSON。

---

## 3. 配置能力

新增解析：

```json
{
  "support": {
    "shape": {
      "enabled": true,
      "minComponentAreaPx": 16,
      "xyDilationPx": 1,
      "closingRadiusPx": 1,
      "bridgeGapPx": 2,
      "preserveModelPriority": true,
      "maxAddedSupportRatio": 2.0
    }
  }
}
```

当前字段状态：

- `enabled`：已支持。
- `minComponentAreaPx`：已支持，小于阈值的支撑组件会被清除。
- `xyDilationPx`：已支持，只向非 model 像素扩张。
- `closingRadiusPx`：已支持简化版，用邻域方向填补小间隙。
- `bridgeGapPx`：已支持水平/垂直短 gap bridge。
- `preserveModelPriority`：已支持，优化后再次确保 model 像素不被 support 覆盖。
- `maxAddedSupportRatio`：已支持，超过限制时回滚新增支撑像素并写 warning。

---

## 4. Pipeline 集成

当前集成顺序：

```text
GenerateSupport
→ OptimizeSupportShape
→ SynchronizeSupportShapeTypeMaps
→ RecalculateSupportGenerationStats
→ ComposeMaterialChannels
→ WriteRGBWSVPackage
```

关键约束：

- 只修改 support mask。
- 不覆盖 model mask。
- 新增 support 像素仅影响 S 通道打印区域。
- 新增/删除 support 后会同步 `SupportType` map。
- 优化后会重算 support pixels、support type 统计和 connectivity 统计。

---

## 5. Report 输出

新增输出：

```text
reports/support_shape_report.json
```

manifest 已登记：

```json
{
  "reports": {
    "supportShape": "reports/support_shape_report.json"
  }
}
```

report schema：

```text
p0.support_shape_report.1
```

主要字段：

```text
enabled
policy
addedSupportPixels
removedSupportPixels
filteredComponents
bridgedGaps
warnings
layers[].pre
layers[].post
layers[].filteredComponents
layers[].bridgedGaps
```

`support_report.json` 也补充了 `shape` 摘要字段，便于 UI/report viewer 读取总览。

---

## 6. Sample 与测试

新增：

```text
samples/configs/support/support_shape_smoke.json
scripts/run_support_shape_tests.ps1
```

输出：

```text
output/SupportShapeSmoke
```

smoke 当前结果：

```text
grid: 60 x 24 x 16
modelPixels: 5760
supportPixels: 13488
support_shape_report.addedSupportPixels: 816
support_shape_report.removedSupportPixels: 0
```

说明：同类旧样例 `support_bottom_plus_unsupported` 的 supportPixels 为 12672；启用形态优化后增加到 13488，证明 dilation/closing 类策略实际生效。

---

## 7. 回归接入

已接入：

```text
scripts/run_schema_tests.ps1
scripts/run_golden_tests.ps1
scripts/run_ci_quick.ps1
tests/golden/expected/r2_golden_summaries.json
```

golden 新增 case：

```text
support_shape_smoke
```

比较字段：

- manifest schema。
- grid width / height / layer count。
- modelPixels。
- supportPixels。
- support_shape_report schema。
- addedSupportPixels。
- removedSupportPixels。

---

## 8. 验证记录

已运行并通过：

```powershell
cmake --build build --config Debug
.\build\Debug\slicer_cli.exe --config samples\configs\support\support_shape_smoke.json
.\build\Debug\rip_reader_test.exe --package output\SupportShapeSmoke --summary
.\scripts\run_support_shape_tests.ps1
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
.\scripts\run_ci_quick.ps1
```

关键输出：

```text
slicer_cli: generated package
  packageDir: output/SupportShapeSmoke
  grid: 60 x 24 x 16
  modelPixels: 5760
  supportPixels: 13488

rip_reader_test: PASS
  schema: p0.rgbwsv.2
  bitDepth: 8
  channelOrder: R G B W S V
  channelPrintPixels: R=5760 G=5760 B=5760 W=0 S=13488 V=0

Support shape tests complete.
Schema tests complete.
Golden tests complete.
CI quick complete.
```

---

## 9. 当前限制

1. closing 是轻量简化版，不是完整数学形态学 closing。
2. bridge gap 只支持水平/垂直短间隙，不做任意路径搜索。
3. 支撑形态优化仍基于 2D layer mask，不引入 SDF / OpenVDB / volumetric support。
4. 当前 sample 主要验证 dilation/closing 类支撑增长，bridge gap 有代码和 report 支持，但尚未单独新增专用 fixture。
5. UI 未新增生产级界面；现有 preview/report viewer 可通过 package report 和 RGB+S overlay 观察结果。

---

## 10. 下一步建议

建议下一阶段优先做：

1. 增加专门的 bridge gap fixture，让 `bridgedGaps` 在 golden 中稳定非零。
2. 将 support shape optimizer 从 `slicer.cpp` 进一步接入正式 pipeline wrapper。
3. 为 `SupportComponentAnalysis` / `SupportShapeOptimizer` 增加 C++ unit test target。
4. 为真实 01/02/03 3MF 模型增加可选 support shape profile，避免默认影响已有真实模型回归。
