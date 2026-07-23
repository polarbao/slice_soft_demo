# DOC_EXEC_12E-08D-06 0.01 mm Release 矩阵与最终分层结论

> 任务状态：COMPLETE
> 执行日期：2026-07-23
> 构建类型：Release
> 固定条件：600 dpi、0.01 mm、OpenVDB OFF

## 1. 任务结论

12E-08D-06 已完成两个 strict-PASS 真实模型族、三种生产路径共六个 case 的 Release
验证：

```text
legacy；
global_surface_shell_restricted_candidate；
global_surface_shell_material_parity_candidate。
```

六个 case 均生成完整 RGBWSV TIFF package，layer list 完整，RIP Reader strict PASS，
且没有从 Global 静默回退到 Legacy。

最终结论必须分层表达：

```text
Legacy 默认生产：GO；
Global restricted 显式候选：GO；
Global material-parity 显式候选：GO；
Global 取代 Legacy 成为默认引擎：NO-GO。
```

最后一项 NO-GO 的原因不是协议或材料正确性失败，而是当前 Global 实现在 0.01 mm 下的耗时和
峰值内存明显高于 Legacy。

## 2. 验证入口

```powershell
pwsh -File scripts/run_12e_08d_06_release_matrix.ps1 `
  -BuildDir build `
  -Config Release
```

机器可读摘要：

```text
output/benchmarks/12e_08d_06_release_matrix/release_matrix_summary.json
```

该目录属于可再生 benchmark 输出，不提交 Git。

## 3. Release 实测

| case | grid | sliceProcessing | outputWrite | total | peak working set |
|---|---:|---:|---:|---:|---:|
| legacy xiao_ma | 284x550x551 | 6.087 s | 3.615 s | 9.820 s | 684.9 MiB |
| legacy yecan | 283x709x576 | 10.074 s | 4.035 s | 14.300 s | 868.8 MiB |
| Global restricted xiao_ma | 286x552x564 | 30.269 s | 16.920 s | 47.284 s | 5610.9 MiB |
| Global restricted yecan | 285x711x585 | 87.019 s | 30.643 s | 117.839 s | 7470.3 MiB |
| Global parity xiao_ma | 290x556x564 | 66.557 s | 17.781 s | 84.392 s | 5715.8 MiB |
| Global parity yecan | 289x715x585 | 50.285 s | 23.165 s | 73.532 s | 7597.2 MiB |

同模型比较：

| 模型族 | restricted/legacy total | parity/legacy total | restricted/legacy memory | parity/legacy memory |
|---|---:|---:|---:|---:|
| xiao_ma | 4.82x | 8.59x | 8.19x | 8.35x |
| yecan | 8.24x | 5.14x | 8.60x | 8.74x |

这些数据是当前参考机器候选证据，不是产品 SLA。Global writer 当前只在
`outputWriteMs` 中提供完整写包耗时，`tiffWriteMs/previewWriteMs` 的细分值尚未单独采集，因此
不能把其零值解释为“没有写 TIFF”。

## 4. 通道与协议结果

所有 case 均满足：

```text
schema = p0.rgbwsv.2；
channelOrder = R G B W S V；
bitDepth = 8；
polarity = black_is_print；
printValue = 0；
emptyValue = 255；
TIFF count = manifest layerCount；
RIP Reader strict PASS。
```

Global restricted 两个 case 的 `S=0、V=0` 符合受限 Profile；Global parity 两个 case 的
`R/G/B/W/S/V` 均大于零。关键统计如下：

| case | W | S | V |
|---|---:|---:|---:|
| Global restricted xiao_ma | 118278 | 0 | 0 |
| Global restricted yecan | 1763922 | 0 | 0 |
| Global parity xiao_ma | 118278 | 46809583 | 1768929 |
| Global parity yecan | 1763922 | 62766378 | 2370402 |

## 5. 能力边界

Global material-parity 候选当前只准入：

```text
lower support；
internal void support；
surface varnish；
XY outer varnish；
Model > OuterVarnishShell > Support > Empty；
strict-PASS 闭合 OBJ；
repair disabled。
```

下列能力仍不因本任务自动准入：

```text
upper/both/full_vertical_projection support；
support shape/offset/dilation/bridge；
复杂 confirmed self-intersection 模型；
自动 repair 后绕过 strict；
OpenVDB production backend；
Global 默认替换 Legacy。
```

## 6. 后续决策

12E-08D 原子任务已收口。后续可进入 12E-09B，但必须遵守：

```text
UI 默认选择 Legacy；
Global 作为显式候选模式；
UI 按 Profile 锁定不支持的工艺选项；
展示 admission、无回退、0.01 mm 高内存/高耗时提示；
不把 OpenVDB backend 暴露为第三种产品切片模式。
```

Global 后续若要替换 Legacy，必须进入独立性能优化专项，至少解决 3D mask/evidence 全体积常驻、
重复 mask 副本和逐层 writer 前一次性持有全部 RGBWSV layer 的峰值内存问题。
