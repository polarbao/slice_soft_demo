# DOC_PREP_12D-R2 Repair Disabled 不变性验证准备

> 文档状态：READY / Stage 12D-R2
> 日期：2026-07-16
> 下一原子任务：12D-06 Repair Disabled 验证
> 前置完成项：12D-02、12D-03、12D-04、12D-05

## 1. 准备结论

12D-06 已具备进入开发的需求、输入、断言和验证命令。该任务只建立自动化不变性守门，不实现 repair，不修改 composer 材料优先级，不改变 TIFF writer，也不接入 Qt UI。

核心证明目标是：

```text
同一输入和同一生产配置；
仅 materialClosure.enabled 从 false 变为 true；
diagnostic + repair.enabled=false；
两份 package 的全部 RGBWSV TIFF 按 manifest layerIndex 一一对应且 SHA-256 完全相同。
```

## 2. 双配置冻结

12D-06 新增以下成对 fixture：

```text
samples/configs/material_closure/repair_disabled_baseline.json
samples/configs/material_closure/repair_disabled_diagnostic.json
```

除以下字段外，两份配置必须相同：

| 字段 | baseline | diagnostic |
|---|---|---|
| `output.packageDir` | `output/MaterialClosureRepairDisabledBaseline` | `output/MaterialClosureRepairDisabledDiagnostic` |
| `materialClosure.enabled` | `false` | `true` |

共同固定值：

```text
materialClosure.mode=diagnostic；
materialClosure.repair.enabled=false；
materialClosure.writeGapPreview=false；
preview.enabled=false；
input.modelPath=samples/models/sample.stl；
OpenVDB disabled/default OFF；
RGBWSV p0.rgbwsv.2 / uint8 / black_is_print。
```

`preview.enabled=false` 用于排除 PNG I/O 和 preview 逻辑对比较结果的干扰。

## 3. TIFF 比较算法

计划脚本入口：

```text
scripts/run_material_closure_tests.ps1
```

脚本必须：

1. 使用两个 manifest 的 `layers` 数组，不得依赖目录枚举顺序；
2. 校验 `layerCount`、`index`、`zMm`、`widthPx`、`heightPx` 一致；
3. 对每个 `layers[n].path` 计算 SHA-256；
4. 按 manifest `index` 逐层比较，不比较 package 绝对路径；
5. 任一缺层、重复 index、路径越界、hash 不同立即返回非零退出码；
6. 同时运行 `rip_reader_test --summary` 校验两个 package；
7. 输出比较层数、首个失败 layerIndex 和 baseline/diagnostic hash。

比较范围只包括生产 TIFF。manifest、slice report 和 material closure report 预期不同，因此不参与 package 整体目录 hash。

## 4. 报告断言

Baseline：

```text
material_closure_report.enabled=false；
repair.attempted=false；
repairedPixels=0。
```

Diagnostic：

```text
source=semantic_masks；
confidence=exact；
repair.enabled=false；
repair.attempted=false；
repair.repairedPixels=0；
totals.repairedPixels=0；
layers[*].repair.attempted=false。
```

## 5. “原始 Gap 仍可见”的验证拆分

生产 package 的 SHA-256 fixture 使用稳定的 gap-free sample；“gap 不因 repair disabled 消失”使用 deterministic semantic unit fixture 验证，禁止通过生产配置注入测试后门。

单元测试必须保存检测前后 evidence/layer buffer 快照，并断言：

```text
输入 buffer 和全部 semantic masks 未改变；
exact report.totalGapPixels > 0；
对应 gap reason code 存在；
repair.attempted=false；
repairedPixels=0；
remainingGapPixels=gapPixels。
```

这样分别证明“真实 TIFF 字节不变”和“已发现 gap 不被静默吞掉”。

## 6. 失败判定

以下任一条件使 12D-06 失败：

```text
任一 TIFF SHA-256 不同；
manifest layer list 不同或不能一一对应；
diagnostic report 不是 semantic_masks/exact；
任一 repair attempted/repairedPixels 非零；
gap fixture 的原始 gap 或 diagnostic code 消失；
RIP Reader 任一 package 失败；
协议不再是 R G B W S V / uint8 / black_is_print。
```

## 7. 文件边界

12D-06 预计只修改：

```text
samples/configs/material_closure/repair_disabled_baseline.json
samples/configs/material_closure/repair_disabled_diagnostic.json
scripts/run_material_closure_tests.ps1
tests/unit/material_closure_semantic_detector/main.cpp
tests/unit/material_closure_report/main.cpp（仅在报告不变性缺少断言时）
docs/codex_task/current/TASKS_12D_横截面材料无缝闭环任务清单.md
docs/slice/REPORT/REPORT_12D_材料闭环准备状态.md
```

不允许修改 `write_rgbwsv_tiff`、生产通道值、材料优先级或 repair 配置准入门禁。

## 8. 验证命令

```powershell
cmake --build build --config Debug --target slicer_cli rip_reader_test material_closure_semantic_detector_unit_tests material_closure_report_unit_tests
ctest --test-dir build -C Debug --output-on-failure
.\scripts\run_material_closure_tests.ps1 -BuildDir build -Config Debug -Mode RepairDisabled
git diff --check
```

## 9. 退出与后续准入

12D-06 完成后，12D-R2 才可封口。12D-07 的 repair-enabled 开发必须同时满足：

```text
12D-06 TIFF hash 守门通过；
repair 默认值仍为 false；
R3 repair plan 与外部背景保护契约已冻结；
用户明确指定开始 12D-07。
```
