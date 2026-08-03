# DOC_PREP 12E-10B 真实模型双模式矩阵准备

> 文档状态：COMPLETE
> 版本：v1.0
> 日期：2026-08-03
> 前置：12E-09D COMPLETE，12E-10A COMPLETE

## 1. 任务边界

12E-10B 只负责生成和校验真实 OBJ/3MF 的双模式最终闭环矩阵，不修改 RGBWSV 生产协议，
不改变 Legacy/Global 的默认选择，不把复杂浮雕的预期阻断改写为成功，也不执行 12E-10C
性能结论和 12E-10D 文档封口。

固定输出：

```text
output/benchmarks/12e_10/final_closure_matrix.json
```

固定 schema：

```text
slicesoft.stage12e.final_closure_matrix.1
```

## 2. 固定资产与身份

| 角色 | 路径 | SHA256 | 预期 |
|---|---|---|---|
| OBJ 正向基准 | `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj` | `4F2012E7D584C7D8F4E3A4467D0AF112216F93C222046F61A987880AF8820DDC` | PASS |
| OBJ 独立复核 | `model/obj/yecan/3.obj` | `A3A421005112292A71F49BED5734CE186C2B97A1379AA50E6DF8BE1A6914363D` | PASS |
| 3MF 格式控制 | `samples/models/3mf/texture2d_checker_cube.3mf` | `D7EC399818C5A1B9BDF4B5A986CA304F4113256EC0C908F951E4A308445F2C57` | PASS |
| 爱神阻断披露 | `model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj` | `5C3F2741297E687BC3E9CE34A2BF3234BA751DEDEDF09FAAC0A36E81C8F83088` | BLOCKED_EXPECTED |
| 玫瑰阻断披露 | `model/obj/meigui_fudiao/04.obj` | `5D8AFFD74C54A234084CF12ED20049B75D8032E996A306C5E9CB9460CF54D70C` | BLOCKED_EXPECTED |
| 梯田阻断披露 | `model/obj/titian_fudiao/dmz.obj` | `492CECCD47FB97362B4515EBB1CF61D17AF3AE8DA0B75173AC0749EF5E5F5022` | BLOCKED_EXPECTED |

执行前必须重新计算 hash。hash 漂移时该行记为 `FAIL`，不得静默更新基线。

## 3. 配置来源

Legacy 基线复用：

```text
samples/configs/material_process/nail_rgb_white_varnish_top2.json
```

Global 正向基线复用：

```text
samples/configs/texture_fill_partition/global_production_xiao_ma_white_fill.json
samples/configs/texture_fill_partition/global_production_yecan_white_fill.json
samples/configs/texture_fill_partition/global_production_xiao_ma_material_parity.json
samples/configs/texture_fill_partition/global_production_yecan_material_parity.json
```

3MF 格式控制以 `samples/configs/3mf/three_mf_texture2d_checker.json` 为输入配置来源。runner
只能在隔离输出目录生成临时 effective config，不修改样例配置。

宽度点固定为：

```text
minimum：生产最小纹理厚度；
intermediate：显式中间厚度；
all_texture：显式模式，不使用超大浮点数模拟。
```

## 4. Required 矩阵

正向矩阵至少包含：

```text
xiao_ma: Legacy minimum/intermediate/all_texture；
xiao_ma: Global minimum/intermediate/all_texture；
yecan: Legacy minimum/intermediate/all_texture；
yecan: Global minimum/intermediate/all_texture；
texture2d_checker_cube.3mf: Legacy format control；
texture2d_checker_cube.3mf: Global format control（仅在现有 Global 3MF 路由声明支持时 PASS，否则显式 NOT_EVALUATED 并阻断 10B 完成）。
```

阻断矩阵固定包含 aishen、meigui、titian 三行。每行必须为 strict preflight/admission
真实阻断，`productionOutputWritten=false`、`fallbackApplied=false`，并记录稳定错误码。

## 5. 每行证据

生产正向行必须同时具备：

```text
模型路径和 SHA256；
pipelineMode、productionProfileId、DPI、物理像素和宽度点；
manifest schema、layerCount 和 TIFF hash 投影；
RIP strict PASS；
TextureSurface/ModelFill/S/W/V/overlap/unassigned 统计；
Preview layerIndex/zMm 与物理纵横比 PASS；
fallbackApplied=false；
result=PASS。
```

所有 package 继续遵守 `p0.rgbwsv.2`、`R G B W S V`、`uint8`、
`black_is_print`。10B 不生成第二套生产预览图片。

## 6. Runner 设计约束

新 runner 可复用：

```text
scripts/run_12e_08d_06_release_matrix.ps1 的真实模型、计时和严格 Reader 编排；
scripts/run_12e_09d_production_texture_material_matrix.ps1 的 width/all_texture 配置变换；
12E-10A 的生产 TIFF 同层语义与精确 closure evidence。
```

不得依赖历史 `output/` 结果判定 PASS；每次执行使用独立目录，失败不得覆盖上一轮成功证据。

## 7. 验证入口

目标命令冻结为：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\run_12e_10b_final_closure_matrix.ps1 `
  -BuildDir build-slicesoft/main `
  -Config Release `
  -OutputRoot output/benchmarks/12e_10
```

完成 Gate：

```text
所有 required 行存在；
生产正向行全部 PASS；
复杂浮雕行全部 BLOCKED_EXPECTED；
schema、TIFF、manifest、report、RIP 和 no-fallback 断言通过；
git diff --check 通过。
```

## 8. 准备结论

资产、hash、配置来源、矩阵维度、输出 schema、阻断语义、复用入口和验收命令已经冻结。
12E-10B 已按本准备合同完成。`run_12e_10b_final_closure_matrix.ps1` 已生成并在 Release 下执行；
14 个生产 case PASS、3 个复杂浮雕 case 为 BLOCKED_EXPECTED，固定矩阵 schema、RIP strict 和
no-fallback Gate 均通过。实现证据见
`REPORT_12E_10B_真实OBJ_3MF双模式矩阵当前状态.md`。
