# DOC_DECISION_12E-08C-R4-06 真实模型族准入替代规则

> 决策状态：ACCEPTED
> 日期：2026-07-22
> 修订对象：R4-06 Repaired Asset Intake、R4-07 Four-case Release Gate

## 1. 背景

R4 原方案把 `nai_you/aishen/meigui` 三个具体 OBJ 文件冻结为 required identity。后续 `model` 目录补充了
同一业务用途、同类纹理浮雕甲片的多个真实资产。生产 Gate 真正需要证明的是不同真实模型族在 strict、
纹理属性和全局分区链路下可复现通过，而不是绑定某一个文件名。

2026-07-22 的增量完整审计同时确认：新增的 4 个爱神 OBJ 和 2 个玫瑰 OBJ 均存在 confirmed
self-intersection；现有梯田 OBJ 也仍存在 confirmed self-intersection。因此替代规则可以解除“固定文件名”
约束，但不会降低当前 Gate，也不会把失败资产改判为通过。

## 2. 决策

R4-06 required Gate 从三个固定文件改为三个真实模型族：

```text
required_aishen_family -> model/obj/aishen_fudiao/*.obj
required_meigui_family -> model/obj/meigui_fudiao/*.obj
required_titian_family -> model/obj/titian_fudiao/*.obj
```

每个模型族至少需要一个获准资产。候选可以是：

```text
strict PASS 的原始资产；
外部人工修复资产；
独立审计重建资产。
```

`nai_you_new/MF_nai_you.obj` 保留为历史负向回归，不再是 R4-06 required family。原始失败证据和 hash
不得删除或改写。

## 3. 等价条件

同一模型族中的候选只有满足下列条件才可替代旧固定文件：

```text
位于冻结的模型族目录，或 manifest 明确声明目标 familyId；
完整资源、单位、姿态、尺寸、UV、材质和纹理来源可审计；
完整自相交审计 complete；
confirmedIntersectionPairs=0 且 coplanarOverlapPairs=0；
post-strict 无 boundary/non-manifold/duplicate/opposite-duplicate/winding blocker；
重复审计 hash 一致；
R4-07 中完成 minimum/intermediate/allTexture、full closure 和 Release 证据。
```

原始 strict PASS 资产不需要伪造“修复来源”，但必须记录 `candidateKind=strict_pass_original`；修复或重建资产
必须记录 provenance 和原始来源 hash。两类候选统一计入 `requiredFamilyPassCount`，不再使用容易误导的
`requiredRepairPassCount` 作为 R4-06 完成条件。

## 4. 当前 Gate

| Family | 已审计 OBJ | strict PASS | 当前状态 |
|---|---:|---:|---|
| `required_aishen_family` | 5 | 0 | BLOCKED |
| `required_meigui_family` | 3 | 0 | BLOCKED |
| `required_titian_family` | 1 | 0 | BLOCKED |

因此 R4-06 可以完成合同和服务开发，但真实 family matrix 仍为 `0/3`，不得进入 R4-07 Release Gate。

## 5. 安全边界

```text
不放宽 strict；
不接受 sampled/incomplete 自相交审计；
不把 xiao_ma/yecan clean control 计入三个 required family；
不自动修改或覆盖 model 资产；
不实现通用复杂自相交重建；
不写 production TIFF/package；
不修改 p0.rgbwsv.2、RGBWSV、uint8、black_is_print；
global 不 silent fallback 到 legacy。
```

## 6. 对原文档的影响

R4 文档中“固定 `nai_you/aishen/meigui` 三文件不可替代”的条款由本决策取代。其他关于属性审计、完整
自相交、post-strict、Release 预算和 12E-08D 用户明确授权的要求继续有效。
