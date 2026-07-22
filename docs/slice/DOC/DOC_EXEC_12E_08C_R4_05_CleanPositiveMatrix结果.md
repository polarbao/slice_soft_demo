# DOC_EXEC_12E-08C-R4-05 Clean Positive Matrix 结果

> 文档状态：COMPLETE
> 日期：2026-07-22
> 原子任务：12E-08C-R4-05
> 下一任务：R4-06 CONTRACT READY / EXTERNAL REPAIRED ASSET BLOCKED

## 1. 结论

R4-05 已完成正常闭合 OBJ/3MF 的纹理壳层宽度、Model Fill 材料解析和内存 RGBWSV 合成正向矩阵。
三个必跑输入全部通过完整预检、互补/单调/终点不变量与材料通道检查。

本任务仍是非生产诊断：没有写 TIFF、manifest、preview、RIP 或 production package，
`requiredRepairPassCount=0`，也没有解除三个 required 修复模型和 12E-08D 的阻断。

## 2. 实现内容

```text
TextureFillPartitionPositiveMatrix：
  复用 ModelPreflight、真实 SceneModel adapter、Legacy CPU global distance、width sweep、
  texture transfer 和 diagnostic composer；
  输出 slicesoft.texture_fill_positive_matrix.12e_08c_r4.1；
  保存去重前的三个 requested width anchor，并记录去重后的有效 sample；
  显式记录 diagnosticOnly=true、productionOutputWritten=false。

ModelFillMaterialResolver：
  white -> W；varnish -> V；rgb/custom -> RGB；
  profile_default 使用显式启用的工艺 Profile；
  material_role 只接受显式注册到现有 white/varnish/rgb 语义的映射；
  未注册 C/M/Y/K 返回稳定 unavailable code，不伪造新 TIFF 通道或墨量。

Legacy CPU strict 守门：
  当快速 robustness 审计因模型规模而 sampled 时，改用完整 AABB BVH 自相交审计；
  只有 complete 且 confirmed/coplanar 均为 0 才继续，不放宽 strict。
```

## 3. 真实模型矩阵

Debug 矩阵使用 `classificationResolutionMm=0.20`。因此请求基础宽度仍为 `0.10mm`，但有效最小宽度为
`max(0.10, 2 * 0.20)=0.40mm`。

| Case | 输入 | Preflight | 有效宽度范围 | 有效 sample | 结论 |
|---|---|---|---:|---:|---|
| `clean_obj_primary` | `xiao_ma_wu_yu_new/MF_Xiao_ma_Damuzhi_ty02.obj` | PASS | 0.40..0.46mm | 3 | PASS |
| `clean_obj_independent` | `yecan/3.obj` | PASS | 0.40..0.42mm | 3 | PASS |
| `clean_3mf_texture2d` | `samples/models/3mf/texture2d_checker_cube.3mf` | PASS | 0.40..0.40mm | 1 | PASS / 三个 anchor 合并 |

3MF 薄壁 case 的 minimum/intermediate/allTexture 三个原始请求点均保留在 report，因动态上下界相同而
去重为一个有效 sample；没有伪造中间宽度。

三个 case 均满足：

```text
Texture Surface ∩ Model Fill = empty；
Texture Surface ∪ Model Fill = Model；
outside/unassigned = 0；
Texture Surface 单调非递减；
Model Fill 单调非递增；
allTexture 终点 Model Fill=0、Texture Surface=Model。
```

## 4. 材料矩阵

每个真实模型执行 8 行材料解析：`white`、`varnish`、`rgb`、`profile_default` 和 C/M/Y/K role。
前三种及启用的 profile default 完成内存合成，Model Fill 始终不占用 S 通道。

C/M/Y/K 当前没有已标定的项目级角色注册表，因此以
`E_12E_MODEL_FILL_ROLE_UNREGISTERED` 明确返回不可用。这是预期的可解释结果，不代表新增了 C/M/Y/K
物理 TIFF 通道，也不代表对应墨量已完成工艺标定。

## 5. 模型依据

R4-05 输入继续以 `REPORT_12E_08C_R4_模型资产预检清单.md` 为真源。无需重建的 7 个 OBJ 已同步到
项目上下文；本次必跑主模型为 `xiao_ma` 大拇指，独立复核为 `yecan/3.obj`，Texture2D 3MF 使用已跟踪
sample fixture。

`yecan/4.obj` 虽通过审计，但仍是用户未跟踪资产，只能本地只读扩展验证，未纳入 CI 和本次提交。
正常模型只证明 R4-05 正向能力，不替代 `nai_you/aishen/meigui` required 修复资产。

## 6. 验证结果

```text
model_fill_material_resolver_unit_tests                         PASS
texture_fill_partition_positive_matrix_unit_tests              PASS
texture_fill_partition_width_sweep_unit_tests                  PASS
texture_fill_partition_diagnostic_composer_unit_tests          PASS
run_12e_08c_r4_05_clean_positive_matrix.ps1                    PASS / 3 cases
run_ci_quick.ps1                                                FAIL / existing golden baseline
```

Quick CI 完成 Debug 构建和前序回归后，仍停在任务开始前已记录的
`material_process_top2 widthPx expected=48 actual=226`。R4-05 未修改该 production fixture 的姿态、尺寸、
writer 或 golden，因此该既有差异不改变本任务定向矩阵结论，也没有在本任务中擅自刷新 golden。

本地证据：

```text
output/benchmarks/12e_08c_r4_05_clean_positive_matrix/summary.json
output/benchmarks/12e_08c_r4_05_clean_positive_matrix/clean_obj_primary.json
output/benchmarks/12e_08c_r4_05_clean_positive_matrix/clean_obj_independent.json
output/benchmarks/12e_08c_r4_05_clean_positive_matrix/clean_3mf_texture2d.json
```

上述生成报告不纳入源代码提交。

## 7. 后续边界

```text
R4-05：COMPLETE；
R4-06：合同准备完成，但等待三个外部修复 required OBJ；
R4-07：依赖准备完成，等待 R4-06 全部 admitted；
R4-08：依赖准备完成，等待 R4-07，再刷新 08D GO/NO-GO；
12E-08D：继续 BLOCKED。
```
