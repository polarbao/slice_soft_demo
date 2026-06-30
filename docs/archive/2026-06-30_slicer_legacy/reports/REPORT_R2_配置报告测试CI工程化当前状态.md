# REPORT_R2_配置报告测试CI工程化当前状态

> 文档版本：v0.1  
> 文档状态：当前实现状态  
> 生成日期：2026-06-10  
> 适用阶段：R2

---

## 1. 阶段结论

R2 已完成第一轮配置、报告、测试、CI 工程化固化。

本阶段只做工程基础设施收口，不新增切片能力，不修改 RGBWSV 生产协议，不实现 surface_shell_texture、compensated_varnish、OpenVDB、设备通信、半色调或 ICC。

当前结论证据等级：

- [A] Debug 构建通过。
- [A] quick regression 通过。
- [A] schema tests 通过。
- [A] golden tests 通过。
- [A] UI self-test 与 overlay-load-real 通过。
- [A] `scripts/run_ci_quick.ps1` 一键通过。

---

## 2. 已完成内容

### 2.1 Config Schema Skeleton

新增：

```text
src/slicer_core/config/ConfigSchema.h
src/slicer_core/config/ConfigSchema.cpp
src/slicer_core/config/ConfigMigration.h
src/slicer_core/config/ConfigMigration.cpp
src/slicer_core/config/NormalizedConfig.h
src/slicer_core/config/NormalizedConfig.cpp
```

当前能力：

- 支持读取配置根节点 `schema`。
- 无 `schema` 时判定为 legacy config。
- 支持 `schema = slicer.config.1` 的第一版 wrapper 配置。
- `slicer.config.1` 会通过 migration 映射回当前 `SliceConfig` 可消费的 legacy DTO 形态。
- 旧配置入口继续可运行。

### 2.2 slicer.config.1 样例

新增：

```text
samples/configs/schema_v1/slicer_config_v1_basic.json
```

验证结果：

```text
packageDir: output/SchemaV1Basic
grid: 12 x 12 x 30
modelPixels: 1440
supportPixels: 2880
rip_reader_test: PASS
```

### 2.3 Report Base Helper

新增：

```text
src/slicer_core/reports/ReportBase.h
src/slicer_core/reports/ReportBase.cpp
src/slicer_core/reports/ReportSchemaValidator.h
src/slicer_core/reports/ReportSchemaValidator.cpp
```

当前能力：

- 提供 `schema/source/configSnapshot/stats/warnings/errors/timings` 基础 report helper。
- 提供 report 基础字段校验 helper。
- 已接入真实输出：`reports/package_report.json`。
- manifest 中新增 `reports.package = reports/package_report.json`。
- 既有 `model_report/slice_report/support_report/preview_report` 等旧字段保持不变。

说明：R2 只要求至少一个 report 接入 helper。既有历史 report 尚未做全量统一改写，后续可按模块逐步迁移。

### 2.4 Diagnostics 小增强

修改：

```text
src/slicer_core/diagnostics/Diagnostics.h
```

当前能力：

- `DiagnosticSeverity` 增加 `Fatal`。
- `DiagnosticMessage` 扩展为 `severity/code/message/source/context`。
- 保留 `Diagnostic` 兼容 alias。
- 未强制替换所有 legacy warning。

### 2.5 Schema Tests

新增：

```text
scripts/run_schema_tests.ps1
```

覆盖：

- `slicer.config.1` 样例可切片。
- `manifest.schema = p0.rgbwsv.2`。
- `manifest.tiff.bitDepth = 8`。
- `manifest.tiff.polarity = black_is_print`。
- `printValue = 0`，`emptyValue = 255`。
- `preview_report.schema = p0.preview_report.1`。
- `material_process_report` 关键字段与 RGB/W/V 输出统计。

### 2.6 Golden Tests

新增：

```text
scripts/run_golden_tests.ps1
tests/golden/expected/r2_golden_summaries.json
```

覆盖：

- `p0_basic`
- `material_process_top2`
- `ui_overlay_rgbwv`

比较字段：

- package manifest schema
- grid width/height/layers
- slice report total model/support pixels

当前不保存大体积二进制 golden，只保存摘要 JSON。

### 2.7 CI Quick Guard

新增：

```text
scripts/run_ci_quick.ps1
```

脚本串联：

```text
build Debug
run_regression.ps1 -Mode quick
run_schema_tests.ps1
run_golden_tests.ps1
slicer_debug_ui.exe --self-test
slicer_cli 生成 UiSmokeOverlayRgbwv fixture
slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real
```

---

## 3. 未改动边界

本阶段确认未修改：

- `p0.rgbwsv.2` manifest schema。
- RGBWSV 通道顺序 `R G B W S V`。
- 8-bit TIFF、`black_is_print`、`printValue=0`、`emptyValue=255`。
- Model > Support > Empty 优先级。
- SupportType 不编码进 TIFF channel。

本阶段确认未实现：

- surface shell texture。
- compensated varnish。
- OpenVDB。
- Qt UI 新功能。
- 设备通信。
- RIP 半色调。
- ICC/色彩管理。

---

## 4. 验证记录

已运行并通过：

```powershell
cmake --build build --config Debug
.\scripts\run_regression.ps1 -Mode quick
.\scripts\run_schema_tests.ps1
.\scripts\run_golden_tests.ps1
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --self-test
.\build\apps\slicer_debug_ui\Debug\slicer_debug_ui.exe --ui-smoke-test --case overlay-load-real --package output\UiSmokeOverlayRgbwv
.\scripts\run_ci_quick.ps1
```

关键输出：

```text
Regression complete. mode=quick
Schema tests complete.
Golden tests complete.
PASS overlay-load-real images=94 channels=rgb,support,varnish,white
CI quick complete.
```

---

## 5. 当前限制

1. `slicer.config.1` 目前是第一版 wrapper/migration 骨架，主要覆盖现有样例所需字段，不等于完整业务配置编辑协议。
2. report base helper 已接入 `package_report`，但旧 report 尚未全量迁移到统一 schema。
3. golden tests 目前只比较摘要字段，尚未覆盖每层 TIFF 内容哈希。
4. schema tests 目前通过 PowerShell/JSON 断言实现，尚未拆成独立 C++ unit test target。
5. CI quick 是本地脚本守门，尚未接入远端 CI 服务。

---

## 6. 是否进入 08

建议可以进入 08 阶段。

理由：

- R2 的核心目标是把配置版本、报告基座、schema/golden 测试和本地 CI quick 串起来；当前均已具备并通过验证。
- R2 没有修改生产 TIFF/RGBWSV 协议，已有 quick regression 和 UI smoke 均保持通过。
- 剩余限制属于后续增强项，不阻塞进入下一阶段。

进入 08 前建议保留的守门命令：

```powershell
.\scripts\run_ci_quick.ps1
```
