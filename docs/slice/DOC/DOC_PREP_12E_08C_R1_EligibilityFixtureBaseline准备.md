# DOC_PREP_12E-08C-R1 Eligibility、Fixture 与真实模型 Baseline 准备

> 文档状态：COMPLETE
> 日期：2026-07-20
> 范围：12E-08C-R1-02、R1-03、R1-04

## 1. 准备结论

R1-02 至 R1-04 的输入、输出、职责边界、fixture、真实模型证据和退出标准已明确。R1 只建立修复前契约与
资格判断，不修改网格、不执行 repair、不写 TIFF/package。

执行顺序固定为：

```text
R1-01 DTO/Hash/Report Contract
  -> R1-02 Eligibility Policy
  -> R1-03 Generated Fixtures/Golden
  -> R1-04 Real Model Pre-Repair Baseline
```

## 2. R1-02 Eligibility Policy

输入复用 `MeshTopologyDiagnostics`、`MeshRobustnessDiagnostics` 和 R1-01 canonical hashes。不得重新实现一套
拓扑统计，也不得在 policy 内修改 mesh。

输出分类：

```text
eligible：存在唯一、局部、属性可保持的候选操作；
conditional：需要预算、几何或属性条件进一步判定；
manual_only：无法证明唯一自动修复；
fail_fast：confirmed self-intersection 或输入契约失效。
```

R1-02 至少冻结以下稳定错误码：

```text
E_12E_REPAIR_INPUT_INVALID
E_12E_REPAIR_NOT_ENABLED
E_12E_REPAIR_AMBIGUOUS_TOPOLOGY
E_12E_REPAIR_ATTRIBUTE_CONFLICT
E_12E_REPAIR_SELF_INTERSECTION
E_12E_REPAIR_MANUAL_REQUIRED
E_12E_REPAIR_BUDGET_EXCEEDED
```

同一种 issue 必须只有一个最高优先级结论；优先级为 `fail_fast > manual_only > conditional > eligible`。
Eligibility 仅提出建议，不创建 operation，不改变 admission。

## 3. R1-03 Generated Fixtures

| Fixture | 最小几何 | 预期分类 | 关键断言 |
|---|---|---|---|
| clean_closed | 闭合四面体/盒体 | no repair | hash 重复稳定 |
| degenerate_face | 闭合体附退化面 | eligible | affectedCount 稳定 |
| duplicate_same_attributes | 同向重复面 | eligible | 属性一致 |
| duplicate_uv_conflict | 重复面 UV 不同 | manual_only | attribute conflict |
| opposite_duplicate | 正反重复面 | conditional/manual | 不自动承诺修复 |
| winding_only | 唯一可定向局部壳 | conditional | 组件和传播范围可报告 |
| simple_planar_boundary | 单一简单平面孔 | conditional | loop hash/size 稳定 |
| non_planar_boundary | 非平面或超预算孔 | manual_only | 无 operation |
| separable_edge_fan | 可分 fan | conditional | pattern 稳定 |
| ambiguous_edge_fan | 歧义 fan | manual_only | 不伪 PASS |
| self_intersection | 已确认相交三角形 | fail_fast | 不继续 repair |

Golden 固定 status、code、hash、affectedCount 和 schema 结构，不冻结平台相关耗时。

## 4. R1-04 真实模型 Baseline

固定输入：

```text
model/obj/nai_you_new；
model/obj/aishen_fudiao；
model/obj/meigui_fudiao；
仓库内 Texture2D/ColorGroup 闭合 3MF fixture。
```

每个 case 输出：输入相对路径、config/model/attribute hash、顶点/三角形/组件数量、topology/robustness issue、
eligibility、人工建议和 `productionOutputWritten=false`。不要求 R1 阶段把真实模型修好。

已知 baseline 只能作为首个 golden 候选；实施时必须从当前代码重新计算，不把历史报告数值硬编码进 policy。

## 5. 代码所有权

```text
geometry/repair：Eligibility DTO 与纯 policy；
geometry diagnostics：提供事实，不决定 repair；
diagnostics/report：序列化证据，不做业务判断；
tests：fixture builder、golden 和真实模型 baseline 驱动；
apps/scripts：只负责调用和归档，不复制 policy。
```

## 6. Gate 与停止条件

R1 完成要求：

```text
所有 generated fixture 有唯一稳定结论；
四个真实模型有可审计 baseline；
repairAttempted=false；
productionOutputWritten=false；
legacy/Profile/TIFF 不变；
OpenVDB OFF 可独立构建。
```

## 8. 实际完成结果

R1-01 至 R1-04 已完成。R1-04 使用三个真实 OBJ 与闭合 Texture2D 3MF，生成 source/config/geometry/
attribute hash、拓扑、eligibility 和非生产报告，每个 case 连续两次稳定一致。详细结果见
`DOC_EXEC_12E_08C_R1_04_真实模型PreRepairBaseline结果.md`。

R1 Gate 已关闭；R2-01 可以在用户明确启动后执行。真实模型 sampled self-intersection 缺口已进入独立
R3-01A 准备，不在 R2-01 中顺手扩大范围。

遇到需要修改 required-case matrix、放宽 strict 或新增第三方库时停止并另行决策。

## 7. 计划验证

```text
R1-02：eligibility unit tests；
R1-03：fixture/golden/schema tests；
R1-04：真实模型 baseline 脚本与重复 hash 检查；
每个任务：定向 build/CTest + git diff --check。
```
