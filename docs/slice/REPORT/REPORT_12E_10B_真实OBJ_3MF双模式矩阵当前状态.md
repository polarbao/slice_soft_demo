# REPORT 12E-10B 真实 OBJ/3MF 双模式矩阵当前状态

> 状态：COMPLETE / 12E-10C/10D SUBSEQUENTLY COMPLETE
> 日期：2026-08-03
> 协议边界：`p0.rgbwsv.2` / `R G B W S V` / `uint8` / `black_is_print`

## 1. 阶段结论

12E-10B 已完成真实 OBJ/3MF 的 Legacy/Global 双模式闭环矩阵。新增可重复执行入口：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_12e_10b_final_closure_matrix.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -OutputRoot output/benchmarks/12e_10
```

固定输出为 `output/benchmarks/12e_10/final_closure_matrix.json`，schema 为
`slicesoft.stage12e.final_closure_matrix.1`。输出证据默认不提交 Git。

## 2. Required 矩阵结果

| 资产 | Legacy | Global | 结果 |
|---|---|---|---|
| `xiao_ma` OBJ | minimum / intermediate / all_texture | minimum / intermediate / all_texture | 6/6 PASS |
| `yecan` OBJ | minimum / intermediate / all_texture | minimum / intermediate / all_texture | 6/6 PASS |
| Texture2D checker 3MF | format control | format control | 2/2 PASS |
| `aishen_fudiao` | strict preflight | 不写生产包 | BLOCKED_EXPECTED |
| `meigui_fudiao` | strict preflight | 不写生产包 | BLOCKED_EXPECTED |
| `titian_fudiao` | strict preflight | 不写生产包 | BLOCKED_EXPECTED |

汇总结果：生产正向 14 行 PASS，复杂浮雕 3 行按预期 fail-closed，失败 0 行，silent fallback 0 行。

## 3. 验证内容

每个生产正向 case 均实际检查：

```text
固定模型 SHA-256；
requested/effective pipeline 一致且 fallbackApplied=false；
p0.rgbwsv.2、R G B W S V、uint8、black_is_print；
manifest layer list 和全部 TIFF 文件存在；
TIFF layerIndex/zMm/hash 投影；
RIP Reader strict PASS；
Texture Surface / Model Fill / S / W / V 统计；
Legacy exact material closure 或 Global production partition closure；
同层 layerIndex/zMm 对齐；
DPI 与 pixelSizeXmm/pixelSizeYmm 物理比例一致；
overlap=0、unassigned=0；
分段计时和进程峰值内存。
```

Global intermediate 请求宽度为 0.8 mm；对当前 xiao_ma/yecan 几何，其有效宽度分别受模型极限约束为
0.47 mm 和 0.42 mm，因此实际进入 all-texture，Model Fill 为 0。这是已记录的有效参数结果，不是将
intermediate case 静默改名为 all_texture。

## 4. 复杂浮雕阻断

三组复杂浮雕均重新执行了当前 `mesh_repair_preflight`，并保持：

```text
productionOutputWritten=false；
fallbackApplied=false；
result=BLOCKED_EXPECTED；
package 不存在。
```

稳定 blocker 包括 `MESH_SELF_INTERSECTION_SAMPLED`、`MESH_NON_MANIFOLD_EDGES` 和
`MESH_OPPOSITE_DUPLICATE_FACES`；爱神还包含 boundary/degenerate blocker。10B 未把历史阻断资产伪装为
生产成功，也未执行自动修复。

## 5. 实际验证

本阶段实际通过：

```text
Release 构建：slicer_cli、rip_reader_test、mesh_repair_preflight 及定向测试目标；
material_closure_report_unit_tests：PASS；
rgbwsv_production_package_writer_unit_tests：PASS；
global_surface_shell_production_pipeline_unit_tests：PASS；
12E-10B 17 行真实矩阵：PASS；
RIP strict：14/14 PASS；
模型 hash：6/6 与冻结身份一致。
```

## 6. 未改变内容

```text
Legacy 仍为默认生产路线；
Global 仍为显式候选，禁止 silent fallback；
OpenVDB 仍为可选且默认关闭；
未修改 RGBWSV 协议、TIFF 通道、位深或极性；
未生成第二套生产 Preview 图片；
未实施冻结的 12G-TCWS；
未形成 12E-10C 性能替代结论。
```

## 7. 下一任务

后续 `12E-10C` 已按同参考机、同模型、同 DPI、同层厚和同输出策略完成 36 个计量样本及中位数
汇总；`12E-10D` 也已完成最终文档封口。最新状态以
`REPORT_12E_全局纹理壳层与模型填充当前状态.md` 为准。
