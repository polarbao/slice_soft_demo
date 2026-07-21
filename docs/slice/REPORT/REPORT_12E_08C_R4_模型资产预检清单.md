# REPORT_12E-08C-R4 模型资产预检清单

> 文档状态：VERIFIED / R4 后续输入基线  
> 日期：2026-07-21  
> 扫描范围：`model/**/*.{obj,3mf}`  
> 生产边界：本报告只判定模型能否在“不重建”条件下进入后续严格模块，不代表已取得生产 TIFF 准入

## 1. 校验方法

统一使用 Release `mesh_repair_preflight`，在最终 `autoOrient` 变换后执行 topology、strict closed 和完整
AABB BVH 自相交审计：

```powershell
.\build\Release\mesh_repair_preflight.exe `
  --config <generated-config> `
  --output <report.json> `
  --voxel-mm 0.10 `
  --require-openvdb-off `
  --analyze-r3-01a `
  --complete-self-intersection-max-candidates 5000000
```

分类规则：

```text
可直接进入后续模块：strictPass=true，完整自相交审计 complete，confirmed/coplanar 均为 0；
需人工修复：无自相交，但存在非流形、反向重复面等 strict blocker，且自动修复策略拒绝；
需重建：存在 confirmed self-intersection 或 coplanar overlap；
审计阻断：解析失败、资源缺失、预算不足或完整审计未完成。
```

## 2. 模型校验表

| 模型 | 三角面 | 材质/纹理 | strict | 自相交（确认/共面） | 结论 | 主要问题 |
|---|---:|---:|---|---:|---|---|
| `model/3mf/haiyang_fudiao/01.3mf` | 99,538 | 3/3 | FAIL | 12,196/0 | 需重建 | 边界 6，非流形 10,179，反向重复面 6,623 |
| `model/3mf/meigui_fudiao/02.3mf` | 28,910 | 2/2 | FAIL | 54/64 | 需重建 | 非流形 9,631，反向重复面 6,255 |
| `model/3mf/meigui_fudiao/03.3mf` | 75,596 | 3/3 | FAIL | 5,136/0 | 需重建 | 非流形 10,939，反向重复面 7,190 |
| `model/obj/aishen_fudiao/MF_aishen_damuzhi_L_tx02.obj` | 84,533 | 1/1 | FAIL | 19,270/20 | 需重建 | 边界 3，非流形 59，反向重复面 2，退化面 1 |
| `model/obj/caihong/5mm.obj` | 308 | 7/7 | FAIL | 0/0 | 需人工修复 | 非流形 102，反向重复面 48；策略返回 `manual_repair_required` |
| `model/obj/meigui_fudiao/04.obj` | 76,926 | 1/1 | FAIL | 5,592/0 | 需重建 | 非流形 10,940，反向重复面 7,192 |
| `model/obj/nai_you_new/MF_nai_you.obj` | 117,705 | 2/1 | FAIL | 8,409/0 | 需重建 | 边界 113，退化面 1 |
| `model/obj/titian_fudiao/dmz.obj` | 95,590 | 1/1 | FAIL | 48,831/28 | 需重建 | 非流形 27,069，反向重复面 17,247 |
| `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj` | 12,736 | 5/5 | PASS | 0/0 | **可直接进入** | 完整审计通过 |
| `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Shizhi_ty03.obj` | 11,680 | 5/5 | PASS | 0/0 | **可直接进入** | 完整审计通过 |
| `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Wumingzhi_ty03.obj` | 11,680 | 5/5 | PASS | 0/0 | **可直接进入** | 完整审计通过 |
| `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Xiaozhi_ty03.obj` | 11,680 | 5/5 | PASS | 0/0 | **可直接进入** | 完整审计通过 |
| `model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Zhongzhi_ty03.obj` | 11,680 | 5/5 | PASS | 0/0 | **可直接进入** | 完整审计通过 |
| `model/obj/yecan/3.obj` | 14,606 | 1/1 | PASS | 0/0 | **可直接进入** | 完整审计通过 |
| `model/obj/yecan/4.obj` | 14,606 | 1/1 | PASS | 0/0 | **可直接进入** | 完整审计通过 |

## 3. 汇总结论

```text
扫描模型：15
可直接进入后续严格模块：7
需人工修复：1
需复杂重建：7
审计阻断：0
```

15 个模型均被 importer 成功读取，报告中的 `missingTextureResources=0`。这只说明引用资源在本次读取中
可用，不替代后续纹理采样、颜色保真和生产 writer 验收。

7 个直接准入候选已执行第二次完整审计；strict 状态、拓扑计数、candidate pair hash、geometry hash 和
attribute hash 均保持一致。

## 4. 后续输入选择

R4 正向开发优先使用：

```text
主彩色 OBJ：model/obj/xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj；
独立复核 OBJ：model/obj/yecan/3.obj；
同系列覆盖：xiao_ma_wu_yu_new 其余四个 OBJ 与 yecan/4.obj；
Texture2D 3MF：model 目录当前无 strict PASS 文件，继续使用
samples/models/3mf/texture2d_checker_cube.3mf 作为正向 fixture。
```

`nai_you/aishen/meigui` 的 required 身份不变。上述 clean OBJ 只能推进 R4-01..05，不得替代 required
模型解除 R4-06..08 或 12E-08D 的生产阻断。

## 5. 本地证据

```text
output/benchmarks/12e_08c_r4_model_inventory/model_inventory_summary.json
output/benchmarks/12e_08c_r4_model_inventory/model_inventory_summary.csv
output/benchmarks/12e_08c_r4_model_inventory/usable_repeatability.json
output/benchmarks/12e_08c_r4_model_inventory/<case>/mesh_repair_preflight_report.json
output/benchmarks/12e_08c_r4_model_inventory/<case>/mesh_repair_preflight_report_repeat.json
```

这些文件是本机生成证据，不纳入模型源资产提交。
