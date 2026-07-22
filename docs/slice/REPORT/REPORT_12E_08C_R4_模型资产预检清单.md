# REPORT_12E-08C-R4 模型资产预检清单

> 文档状态：VERIFIED / R4 后续输入基线  
> 日期：2026-07-22
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
| `model/obj/aishen_fudiao/ai_shen_wumingzhi_L_tx03.obj` | 79,904 | 1/1 | FAIL | 5,908/0 | 需重建 | 非流形 8,914，反向重复面 3,651 |
| `model/obj/aishen_fudiao/MF_ai_shen_shizhi_L_tx03.obj` | 299,976 | 1/1 | FAIL | 32,481/23 | 需重建 | 非流形 229，反向重复面 102，退化面 4 |
| `model/obj/aishen_fudiao/MF_aishen_xiaozhi_L.obj` | 109,538 | 1/1 | FAIL | 12,325/0 | 需重建 | 非流形 24，反向重复面 8 |
| `model/obj/aishen_fudiao/MF_aishen_zhongzhi_L_tx03.obj` | 131,584 | 0/0 | FAIL | 15,280/13 | 需重建 | OBJ 引用的 MTL 文件名与现有文件不一致；同时存在完整审计确认的自相交 |
| `model/obj/caihong/5mm.obj` | 308 | 7/7 | FAIL | 0/0 | 需人工修复 | 非流形 102，反向重复面 48；策略返回 `manual_repair_required` |
| `model/obj/meigui_fudiao/02.obj` | 70,262 | 1/1 | FAIL | 3,474/0 | 需重建 | 非流形 299 |
| `model/obj/meigui_fudiao/03.obj` | 75,596 | 1/1 | FAIL | 15,730/0 | 需重建 | 完整审计确认自相交 |
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
扫描模型：21
可直接进入后续严格模块：7
需人工修复：1
需复杂重建：13
审计阻断：0
```

21 个模型均被 importer 成功读取。`MF_aishen_zhongzhi_L_tx03.obj` 的 `mtllib` 名称与目录内实际 MTL
文件名不一致，因此 importer 结果为 0 材质/0 纹理；该资源问题不会掩盖其自相交 blocker。其余新增模型
均读取到 1 材质/1 纹理。资源计数不替代后续纹理采样、颜色保真和 production writer 验收。

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

根据 `DOC_DECISION_12E_08C_R4_06_真实模型族准入替代规则.md`，R4-06 required Gate 改为
`aishen_fudiao/meigui_fudiao/titian_fudiao` 三个真实模型族各至少一个 admitted 资产。上述 clean OBJ
不属于这三个模型族，只能作为控制组，不能解除 R4-06..08 或 12E-08D 的生产阻断。

## 5. 三个真实模型族 Gate

| Required family | 已审计文件 | strict PASS | 可进入 R4-06 family 验收 | 结论 |
|---|---:|---:|---:|---|
| `required_aishen_family` | 5 | 0 | 0 | 所有候选均需修复或重建；中指模型还需修正 MTL 引用 |
| `required_meigui_family` | 3 | 0 | 0 | 所有候选均需修复或重建 |
| `required_titian_family` | 1 | 0 | 0 | `dmz.obj` 需修复或重建 |

当前无法保证三个目录各有一个无需重建的模型。R4-06 服务开发可以继续，但真实 family pass 保持 `0/3`。

## 6. 本地证据

```text
output/benchmarks/12e_08c_r4_model_inventory/model_inventory_summary.json
output/benchmarks/12e_08c_r4_model_inventory/model_inventory_summary.csv
output/benchmarks/12e_08c_r4_model_inventory/usable_repeatability.json
output/benchmarks/12e_08c_r4_model_inventory/<case>/mesh_repair_preflight_report.json
output/benchmarks/12e_08c_r4_model_inventory/<case>/mesh_repair_preflight_report_repeat.json
```

这些文件是本机生成证据，不纳入模型源资产提交。
