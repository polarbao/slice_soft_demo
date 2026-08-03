# REPORT_12E-09D-06 Release 矩阵与阶段收口当前状态

> 状态：COMPLETE / PASS
> 日期：2026-08-03

## 1. Release 矩阵

新增 `scripts/run_12e_09d_production_texture_material_matrix.ps1`，在隔离输出目录验证：

| 组别 | Case | 结果 |
|---|---|---|
| Legacy | topSurfaceLayers=1/3/10 | RGB 生效层单调，W/S 不变，PASS |
| Global | minimum/middle/explicit all_texture | Model Fill 反向单调，all_texture 语义 Model Fill=0，PASS |
| 单材料 | W 白墨 / V 光油 | W/V 互斥，层数和 S 支撑一致，PASS |

所有 case 均满足 `p0.rgbwsv.2`、`R G B W S V`、uint8、`black_is_print` 并通过 RIP strict。

## 2. 实际验证

```powershell
.\scripts\run_12e_09d_production_texture_material_matrix.ps1 `
  -BuildDir build-slicesoft/main -Config Release
```

结果：

```text
6/6 定向 CTest PASS；
Qt self-test PASS；
production-texture-controls PASS；
diagnostic-settings-controls PASS；
8 个生产 package 与 RIP strict PASS。
```

本地摘要：`output/benchmarks/12e_09d/production_texture_material_matrix_summary.json`。

## 3. 阶段结论

12E-09D-01..06 全部完成。Legacy 层数、Global 物理宽度/显式 all_texture、诊断宽度和单材料 W/V 已形成互不混写的生产闭环。下一任务为 `12E-10A`。
