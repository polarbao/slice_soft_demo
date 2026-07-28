# REPORT 13C-04 Preview IO 收口当前状态

> 状态：COMPLETE
> 日期：2026-07-28
> 下一任务：13C-05 TIFF 原生统一预览阶段收口

## 1. 完成范围

13C-04 已把生产 TIFF 预览和自动诊断图彻底分离：

```text
preview.outputPolicy=tiff_native：
  默认策略；
  完整写出 p0.rgbwsv.2 RGBWSV TIFF；
  不创建 package/preview；
  UI 直接从 manifest/layers TIFF 预览。

preview.outputPolicy=tiff_native_with_diagnostics：
  生产 TIFF 不变；
  额外按既有 interval/format 写 RGB/W/S/V 诊断图；
  用于显式诊断、回归和工艺检查。
```

旧 `preview.enabled` 保持兼容：没有 `outputPolicy` 时，`true` 迁移为诊断策略，`false` 或缺失迁移
为 TIFF 原生策略；两个字段冲突时以 `outputPolicy` 为准。

## 2. 代码与 UI

```text
PreviewConfig、RgbwsvProductionPreviewSpec 默认改为 tiff_native / false；
Legacy、Global Surface Shell、多模型共享 Writer 使用同一策略语义；
manifest.preview 和 preview_report 增加 outputPolicy、productionSource、
automaticDiagnosticImages；
Qt effective config 同时写一致的 outputPolicy/enabled；
配置页“生成预览”改为“自动生成诊断图”，默认关闭；
帮助说明明确关闭诊断图不影响 TIFF 或生产预览；
production_rgb_inspection Profile 继续显式开启诊断图。
```

固定协议未改变：`p0.rgbwsv.2`、`R G B W S V`、uint8、`black_is_print`、0 打印、
255 不打印。

## 3. 自动化

新增：

```text
preview_output_policy_unit_tests；
RgbwsvPackageWriter 的 tiff_native 无目录与显式诊断双策略覆盖；
tiff-native-preview-no-png UI Smoke；
scripts/run_13c_04_preview_io.ps1。
```

UI Smoke 在真实无 `preview` 目录 package 上验证首/中/末层、13 种材料模式、六通道探针和
`RGB+S+W+V`，数据源保持 `manifest/layers RGBWSV TIFF`。

## 4. IO 对比证据

同一 `material_process_top2_fixture` 的 Debug 本机功能基线：

| 指标 | tiff_native | with_diagnostics |
|---|---:|---:|
| TIFF 文件 | 25 | 25 |
| TIFF 总字节 | 178750 | 178750 |
| 诊断图文件 | 0 | 100 |
| 诊断图总字节 | 0 | 354800 |
| tiffWriteMs | 81.000 | 77.814 |
| previewWriteMs | 0.000 | 364.400 |
| outputWriteMs | 176.312 | 549.940 |
| totalMs | 257.576 | 642.033 |

两种策略的 25 层 TIFF SHA-256 逐层一致，均通过 RIP strict。时间仅代表本机单次 Debug
基线，不作为跨设备性能承诺。

可复核证据由脚本输出到：

```text
output/benchmarks/13c_04/preview_io_comparison.json
```

该目录属于运行产物，不提交仓库；脚本和验证合同提交仓库。

## 5. 验证结果

2026-07-28 实际执行：

```text
Debug 全量构建：PASS；
Debug CTest：82/82 PASS；
preview_output_policy_unit_tests：PASS；
rgbwsv_production_package_writer_unit_tests：PASS；
tiff-native-preview-no-png：PASS；
Qt self-test：PASS；
scripts/run_13c_04_preview_io.ps1：PASS；
scripts/run_ci_quick.ps1：PASS；
git diff --check：PASS，仅有工作树换行提示。
```

## 6. 结论

`13C-04 COMPLETE`。常规生产不再自动重复写逐通道 PNG/PPM，显式诊断能力和旧配置兼容均保留，
生产 TIFF、报告、RIP 和 UI TIFF 原生预览没有回归。`13C-05` 的顺序 Gate 已关闭，可以进入
Stage 13C 总收口。
