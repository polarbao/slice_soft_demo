# DOC_EXEC_12E-08D-05 Global 材料等价结果

> 状态：COMPLETE
> 日期：2026-07-23
> 范围：Global lower/internal-void support、surface/outer varnish、S/V 通道与 full closure

## 1. 结论

12E-08D-05 已完成。新增显式候选 Profile：

```text
global_surface_shell_material_parity_candidate
```

该 Profile 在原有 RGB Texture Surface + W Model Fill 基础上，增加：

```text
S：lower projection support；
S：模型列内部空洞支撑；
V：模型域内 surface varnish；
V：按 thicknessMm 扩张 XY 的 outer varnish；
优先级：Model > OuterVarnishShell > Support > Empty。
```

两个 strict-PASS 真实模型族均生成正式 TIFF package，完整 material closure 与 RIP strict
通过。08D-04 restricted Profile 回归通过。

## 2. 实现边界

新增 `GlobalSurfaceShellMaterialEvidence`，只消费 production raster mapping 和已验证配置，
输出与真实 layerIndex/zMm 对齐的：

```text
supportFillMask；
internalVoidSupportMask；
surfaceVarnishMask；
outerVarnishShellMask；
modelEnvelopeMask；
supportRequiredMask；
RGBWSV channel bytes。
```

该服务不写文件。最终输出继续经过：

```text
TextureFillPartitionFullClosureAdapter；
GlobalSurfaceShellProductionLayerAdapter；
共享 RGBWSV package writer；
RIP Reader strict。
```

未新增第二个 TIFF encoder。

## 3. 几何与材料规则

### 3.1 支撑

首版只准入 `bottom_projection + placement=lower`。对每个 XY 列，从当前打印域底部填充到
最高 model/outer-varnish 需求点；模型区和 outer-varnish 区按优先级移除支撑。位于同一列两个
模型区间之间的支撑另标记为 `internalVoidSupportMask`。

以下支撑模式继续 fail-closed：

```text
upper；
both；
full_vertical_projection；
offset；
shape/bridge/dilation。
```

### 3.2 光油

`surfaceVarnish` 使用三维 6 邻域和外部空域 flood fill 区分外表面/内部空洞表面，并支持
`thicknessPx` 向模型内部扩展。V 与 RGB/W 可在同一模型像素叠加。

`outerVarnish` 只扩张 XY。production raster 按厚度预留边界，V 壳层不覆盖模型，也不与 S
重叠。

## 4. Release 真实模型证据

固定条件：

```text
Release；
600 dpi；
layerThicknessMm=0.2；
stripped TIFF；
OpenVDB OFF；
support lower；
surface varnish 1 px；
outer varnish 0.05 mm。
```

| Case | Grid | R/G/B | W | S | V | Slice | Write | Total | RIP |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---|
| xiao_ma | 302 x 567 x 30 | 428431 / 428431 / 428506 | 5028 | 2453268 | 317680 | 1159.582 ms | 1377.940 ms | 2618.036 ms | PASS |
| yecan | 297 x 723 x 31 | 412820 / 442249 / 442462 | 87546 | 3286174 | 406422 | 1074.474 ms | 1377.796 ms | 2537.746 ms | PASS |

机器可读证据：

```text
output/benchmarks/12e_08d_05_global_material_parity/
  global_material_parity_matrix_summary.json
```

该目录为忽略的本机验证产物。

## 5. 负向与回归

```text
upper support：E_12E_PIPELINE_GLOBAL_NOT_ADMITTED；
负向 case 不写 package；
fallbackApplied=false；
08D-04 restricted Profile 两真实模型回归 PASS；
四项定向 Release CTest PASS；
RIP Reader strict 2/2 PASS。
```

## 6. 阶段判断

```text
08D-05：COMPLETE；
0.2 mm Global material-parity candidate：GO；
普通 Global 最终全功能等价：仍等待 08D-06 的 0.01 mm Release 矩阵。
```

复杂自相交模型与未准入支撑模式继续保持 fail-closed。
